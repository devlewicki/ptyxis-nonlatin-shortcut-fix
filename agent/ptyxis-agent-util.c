/*
 * ptyxis-agent-util.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdlib.h>

#ifndef __linux__
# include <fcntl.h>
#endif

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>

#include <glib-unix.h>

#include "ptyxis-agent-compat.h"
#include "ptyxis-agent-util.h"

#ifdef __linux__
static gboolean
_linux_check_version (int major,
                      int minor)
{
  struct utsname u;

  if (uname (&u) == 0)
    {
      g_auto(GStrv) parts = g_strsplit (u.release, ".", 3);
      guint64 u_major = parts[0] ? g_ascii_strtoull (parts[0], NULL, 10) : 0;
      guint64 u_minor = parts[0] && parts[1] ? g_ascii_strtoull (parts[1], NULL, 10) : 0;

      return (u_major > major) || ((u_major == major) && (u_minor >= minor));
    }

  return FALSE;
}

/* This just allows us to work around fstat() being different on old
 * glibc and new glibc yet we need to support both.
 */
static gboolean
gio_stat (const char *path,
          guint      *st_dev,
          guint64    *st_ino)
{
  g_autoptr(GFile) file = g_file_new_for_path (path);
  g_autoptr(GFileInfo) info = g_file_query_info (file, "unix::inode,unix::device", 0, NULL, NULL);

  if (info == NULL)
    return FALSE;

  *st_dev = g_file_info_get_attribute_uint32 (info, "unix::device");
  *st_ino = g_file_info_get_attribute_uint64 (info, "unix::inode");

  return TRUE;
}
#endif

static int
ptyxis_pty_create_producer (int      consumer_fd,
                            gboolean blocking)
{
  _g_autofd int ret = -1;
  int extra = blocking ? 0 : O_NONBLOCK;
#if defined(HAVE_PTSNAME_R) || defined(__FreeBSD__)
  char name[256] = {0};
#else
  const char *name = NULL;
#endif

  g_return_val_if_fail (consumer_fd != -1, -1);

  if (grantpt (consumer_fd) != 0)
    return -1;

  if (unlockpt (consumer_fd) != 0)
    return -1;

#ifdef __linux__
  /* TIOCGPTPEER was added in Linux 4.13. We are not guaranteed to
   * have that if we're in a Flatpak on something like CentOS 7.
   * Handle that by doing a minimal kernel check first.
   */
  if (ret == -1 && _linux_check_version (4, 13))
    ret = ioctl (consumer_fd, TIOCGPTPEER, O_NOCTTY | O_RDWR | O_CLOEXEC | extra);
#endif

  if (ret == -1)
    {
#ifdef HAVE_PTSNAME_R
      if (ptsname_r (consumer_fd, name, sizeof name - 1) != 0)
        return -1;
      name[sizeof name - 1] = '\0';
#elif defined(__FreeBSD__)
      if (fdevname_r (consumer_fd, name + 5, sizeof name - 6) == NULL)
        return -1;
      memcpy (name, "/dev/", 5);
      name[sizeof name - 1] = '\0';
#else
      if (NULL == (name = ptsname (consumer_fd)))
        return -1;
#endif

      ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC | extra);
    }

#ifndef __linux__
  if (ret == -1 && errno == EINVAL)
    {
      gint flags;

      ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC);
      if (ret == -1 && errno == EINVAL)
        ret = open (name, O_NOCTTY | O_RDWR);

      if (ret == -1)
        return -1;

      /* Add FD_CLOEXEC if O_CLOEXEC failed */
      flags = fcntl (ret, F_GETFD, 0);
      if ((flags & FD_CLOEXEC) == 0)
        {
          if (fcntl (ret, F_SETFD, flags | FD_CLOEXEC) < 0)
            return -1;
        }

      if (!blocking)
        {
          if (!g_unix_set_fd_nonblocking (ret, TRUE, NULL))
            return -1;
        }
    }
#endif

#ifdef TIOCPKT
  if (ret != -1)
    {
      static int one = 1;
      ioctl(ret, TIOCPKT, &one);
    }
#else
# warning "Compiled without TIOCPKT support, this is discouraged"
#endif

#ifdef __linux__
  {
    static GFile *run_host_ptmx;

    if (run_host_ptmx == NULL)
      run_host_ptmx = g_file_new_for_path ("/run/host/dev/pts/ptmx");

    /* We want to try to open a PTY for the child that will be available
     * at the same location in and outside of the container. This helps
     * ensure that various TTY operations can do the right thing.
     *
     * Since we created the PTY, we know that came from /dev so just see
     * if /run/host/dev/pts/ptmx is the same as the host.
     */
    if (g_file_query_exists (run_host_ptmx, NULL))
      {
        char tty_name[64];

        if (ttyname_r (ret, tty_name, sizeof tty_name) == 0)
          {
            g_autofree char *path = g_build_filename ("/run/host", tty_name, NULL);
            _g_autofd int alt_fd = -1;
            guint old_st_dev, new_st_dev;
            guint64 old_st_ino, new_st_ino;

            alt_fd = open (path, O_NOCTTY | O_RDWR | O_CLOEXEC | extra);

            if (alt_fd != -1 &&
                gio_stat (tty_name, &old_st_dev, &old_st_ino) &&
                gio_stat (path, &new_st_dev, &new_st_ino) &&
                old_st_dev == new_st_dev &&
                old_st_ino == new_st_ino)
              {
                _g_clear_fd (&ret, NULL);
                ret = _g_steal_fd (&alt_fd);
              }
          }
      }
  }
#endif

  return _g_steal_fd (&ret);
}

int
ptyxis_agent_pty_new_producer (int      consumer_fd,
                               GError **error)
{
  int fd;

  if (-1 == (fd = ptyxis_pty_create_producer (consumer_fd, FALSE)))
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
    }

  return fd;
}

int
ptyxis_agent_pty_new (GError **error)
{
  int pty_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);

#ifndef __linux__
  if (pty_fd == -1 && errno == EINVAL)
    {
      int flags;

      pty_fd = posix_openpt (O_RDWR | O_NOCTTY);
      if (pty_fd == -1)
        return -1;

      flags = fcntl (pty_fd, F_GETFD, 0);
      if (flags < 0)
        return -1;

      if (fcntl (pty_fd, F_SETFD, flags | FD_CLOEXEC) < 0)
        return -1;
    }
#endif

  g_unix_set_fd_nonblocking (pty_fd, TRUE, NULL);

  return pty_fd;
}

void
ptyxis_agent_push_spawn (PtyxisRunContext   *run_context,
                         GUnixFDList        *fd_list,
                         const char         *cwd,
                         const char * const *argv,
                         GVariant           *fds,
                         GVariant           *env)
{
  GVariantIter iter;

  g_return_if_fail (PTYXIS_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (G_IS_UNIX_FD_LIST (fd_list));

  if (cwd == NULL || cwd[0] == 0)
    cwd = g_get_home_dir ();

  ptyxis_run_context_setenv (run_context, "PWD", cwd);
  ptyxis_run_context_set_cwd (run_context, cwd);

  ptyxis_run_context_append_args (run_context, argv);

  ptyxis_run_context_setenv (run_context, "COLORTERM", "truecolor");
  ptyxis_run_context_setenv (run_context, "TERM", "xterm-256color");
  ptyxis_run_context_setenv (run_context, "FLATPAK_TTY_PROGRESS", "1");

  if (env && g_variant_iter_init (&iter, env) > 0)
    {
      char *key;
      char *value;

      while (g_variant_iter_loop (&iter, "{&s&s}", &key, &value))
        ptyxis_run_context_setenv (run_context, key, value);
    }

  if (fds && g_variant_iter_init (&iter, fds) > 0)
    {
      guint dest_fd_num;
      guint handle;

      while (g_variant_iter_loop (&iter, "{uh}", &dest_fd_num, &handle))
        {
          g_autoptr(GError) error = NULL;
          int fd;

          if (-1 == (fd = g_unix_fd_list_get (fd_list, handle, &error)))
            {
              ptyxis_run_context_push_error (run_context, g_steal_pointer (&error));
              break;
            }

          ptyxis_run_context_take_fd (run_context, fd, dest_fd_num);
        }
    }
}

gboolean
ptyxis_agent_is_sandboxed (void)
{
  static gboolean initialized;
  static gboolean sandboxed;

  if (!initialized)
    {
      sandboxed = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
      initialized = TRUE;
    }

  return sandboxed;
}
