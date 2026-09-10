/* ptyxis-util.c
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
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#include <glib-unix.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include "gconstructor.h"

#include "line-reader-private.h"

#include "ptyxis-util.h"

#define APP_ID_DESKTOP APP_ID ".desktop"

/* XDG Default Terminal Execution Specification allows selecting a
   Desktop Action as a suffix delimited by ':'
 * https://gitlab.freedesktop.org/Vladimir-csp/xdg-terminal-exec/-/tree/v0.13.2?ref_type=tags#default-terminal-execution-specification
 */
#define XDG_TERMINAL_EXEC_ENTRY APP_ID_DESKTOP ":new-window"

static PtyxisProcessKind kind = PTYXIS_PROCESS_KIND_HOST;

G_DEFINE_CONSTRUCTOR(ptyxis_init_ctor)

PtyxisProcessKind
ptyxis_get_process_kind (void)
{
  return kind;
}

/**
 * ptyxis_shell_supports_dash_l:
 * @shell: the name of the shell, such as `sh` or `/bin/sh`
 *
 * Checks if the shell is known to support login semantics. Originally,
 * this meant `--login`, but now is meant to mean `-l` as more shells
 * support `-l` than `--login` (notably dash).
 *
 * Returns: %TRUE if @shell likely supports `-l`.
 */
gboolean
ptyxis_shell_supports_dash_l (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  /* So here is the deal. Typically we would be able to "-bash" as the argv0 to
   * "/bin/bash" which is what determines a login shell. But since we may be
   * tunneling through various layers to get environment applied correctly, we
   * may not have that level of control over argv0.
   *
   * Additionally, things like "exec -a -bash bash" don't work unless you first
   * have a shell to do the exec as most distros don't ship an actual "exec"
   * binary.
   *
   * So there we have it, just sniff for the shell to see if we can fake it
   * till we make it.
   */

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "dash") == 0 || g_str_has_suffix (shell, "/dash") ||
         strcmp (shell, "tcsh") == 0 || g_str_has_suffix (shell, "/tcsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

static char **
get_environ_from_stdout (GSubprocess *subprocess)
{
  g_autofree char *stdout_buf = NULL;

  if (g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    {
      g_auto(GStrv) lines = g_strsplit (stdout_buf, "\n", 0);
      g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);

      for (guint i = 0; lines[i]; i++)
        {
          const char *line = lines[i];

          if (!g_ascii_isalpha (*line) && *line != '_')
            continue;

          for (const char *iter = line; *iter; iter = g_utf8_next_char (iter))
            {
              if (*iter == '=')
                {
                  g_ptr_array_add (env, g_strdup (line));
                  break;
                }

              if (!g_ascii_isalnum (*iter) && *iter != '_')
                break;
            }
        }

      if (env->len > 0)
        {
          g_ptr_array_add (env, NULL);
          return (char **)g_ptr_array_free (g_steal_pointer (&env), FALSE);
        }
    }

  return NULL;
}

const char * const *
ptyxis_host_environ (void)
{
  static char **host_environ;

  if (host_environ == NULL)
    {
      if (ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
        {
          g_autoptr(GSubprocessLauncher) launcher = NULL;
          g_autoptr(GSubprocess) subprocess = NULL;
          g_autoptr(GError) error = NULL;

          launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
          subprocess = g_subprocess_launcher_spawn (launcher, &error,
                                                    "flatpak-spawn", "--host", "printenv", NULL);
          if (subprocess != NULL)
            host_environ = get_environ_from_stdout (subprocess);
        }

      if (host_environ == NULL)
        host_environ = g_get_environ ();
    }

  return (const char * const *)host_environ;
}

/**
 * ptyxis_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
 */
char *
ptyxis_path_expand (const char *path)
{
  wordexp_t state = { 0 };
  char *replace_home = NULL;
  char *ret = NULL;
  char *escaped;
  int r;

  if (path == NULL)
    return NULL;

  /* Special case some path prefixes */
  if (path[0] == '~')
    {
      if (path[1] == 0)
        path = g_get_home_dir ();
      else if (path[1] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[1]);
    }
  else if (strncmp (path, "$HOME", 5) == 0)
    {
      if (path[5] == 0)
        path = g_get_home_dir ();
      else if (path[5] == G_DIR_SEPARATOR)
        path = replace_home = g_strdup_printf ("%s%s", g_get_home_dir (), &path[5]);
    }

  escaped = g_shell_quote (path);
  r = wordexp (escaped, &state, WRDE_NOCMD);
  if (r == 0 && state.we_wordc > 0)
    ret = g_strdup (state.we_wordv [0]);
  wordfree (&state);

  if (!g_path_is_absolute (ret))
    {
      g_autofree char *freeme = ret;

      ret = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  g_free (replace_home);
  g_free (escaped);

  return ret;
}

/**
 * ptyxis_path_collapse:
 *
 * This function will collapse a path that starts with the users home
 * directory into a shorthand notation using ~/ for the home directory.
 *
 * If the path does not have the home directory as a prefix, it will
 * simply return a copy of @path.
 *
 * Returns: (transfer full): A new path, possibly collapsed.
 */
char *
ptyxis_path_collapse (const char *path)
{
  g_autofree char *expanded = NULL;

  if (path == NULL)
    return NULL;

  expanded = ptyxis_path_expand (path);

  if (g_str_has_prefix (expanded, g_get_home_dir ()))
    return g_build_filename ("~",
                             expanded + strlen (g_get_home_dir ()),
                             NULL);

  return g_steal_pointer (&expanded);
}

static void
ptyxis_init_ctor (void)
{
  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    kind = PTYXIS_PROCESS_KIND_FLATPAK;
}

gboolean
ptyxis_is_shell (const char *arg0)
{
  static const char * const builtin_shells[] = {
    "sh", "bash", "dash", "zsh", "fish", "tcsh", "csh", "tmux",
  };
  const char *etc_shells_path = "/etc/shells";
  const char *slash = strrchr (arg0, '/');
  g_autofree char *etc_shells = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (builtin_shells); i++)
    {
      if (g_str_equal (arg0, builtin_shells[i]))
        return TRUE;

      if (slash != NULL && g_str_equal (slash + 1, builtin_shells[i]))
        return TRUE;
    }

  if (ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
    etc_shells_path = "/var/run/host/etc/shells";

  if (g_file_get_contents (etc_shells_path, &etc_shells, NULL, NULL))
    {
      g_auto(GStrv) lines = g_strsplit (etc_shells, "\n", 0);

      for (guint i = 0; lines[i]; i++)
        {
          if (g_str_equal (g_strstrip (lines[i]), arg0))
            return TRUE;
        }
    }

  return FALSE;
}

GListModel *
ptyxis_parse_shells (const char *etc_shells)
{
  g_auto(GStrv) split = NULL;

  if (ptyxis_str_empty0 (etc_shells))
    return G_LIST_MODEL (g_list_store_new (GTK_TYPE_STRING_OBJECT));

  split = g_strsplit (etc_shells, "\n", 0);

  return G_LIST_MODEL (gtk_string_list_new ((const char * const *)split));
}

const char *
ptyxis_app_name (void)
{
#if APP_IS_BUILDER
  /* translators: Builder Terminal means this is a terminal bundled with GNOME Builder */
  return _("Builder Terminal");
#elif APP_IS_GENERIC
  return _("Terminal");
#else
  return _("Ptyxis");
#endif
}

GVariant *
ptyxis_variant_new_toast (const char *title,
                          guint       timeout)
{
  GVariantBuilder b;

  g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&b, "{sv}", "title", g_variant_new_string (title));
  g_variant_builder_add (&b, "{sv}", "timeout", g_variant_new_uint32 (timeout));

  return g_variant_builder_end (&b);
}

static gboolean
line_is_ptyxis (const char *line)
{
  return strcmp (line, XDG_TERMINAL_EXEC_ENTRY) == 0 && line[strlen(XDG_TERMINAL_EXEC_ENTRY)] == 0;
}

static char **
get_xdg_terminals_list_paths (void)
{
  g_autofree char *fallback = g_build_filename (g_get_user_config_dir (), "xdg-terminals.list", NULL);
  const char *session = g_getenv ("XDG_CURRENT_DESKTOP");
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new ();

  if (session != NULL)
    {
      g_auto(GStrv) split = g_strsplit (session, ":", 0);

      for (guint i = 0; split[i]; i++)
        {
          g_autofree char *path = NULL;
          g_autofree char *down = NULL;
          g_autofree char *base = NULL;

          g_strstrip (split[i]);

          if (split[i][0] == 0)
            continue;

          down = g_utf8_strdown (split[i], -1);

          if (strchr (down, G_DIR_SEPARATOR))
            continue;

          base = g_strdup_printf ("%s-xdg-terminals.list", down);
          path = g_build_filename (g_get_user_config_dir (), base, NULL);

          g_strv_builder_add (builder, path);
        }
    }

  g_strv_builder_add (builder, fallback);

  return g_strv_builder_end (builder);
}

gboolean
ptyxis_is_default (void)
{
  g_auto(GStrv) paths = get_xdg_terminals_list_paths ();

  for (guint i = 0; paths[i]; i++)
    {
      const char *path = paths[i];
      g_autofree char *contents = NULL;
      LineReader reader;
      gsize contents_len = 0;
      char *line;
      gsize len;

      if (!g_file_get_contents (path, &contents, &contents_len, NULL))
        return FALSE;

      line_reader_init (&reader, contents, contents_len);

      while ((line = line_reader_next (&reader, &len)))
        {
          line[len] = 0;

          g_strchug (line);

          if (line[0] == '#')
            continue;

          if (line[0] == 0)
            continue;

          return line_is_ptyxis (line);
        }
    }

  return FALSE;
}

static void
ptyxis_make_default_in_file (const char *path)
{
  g_autoptr(GString) replace = g_string_new (XDG_TERMINAL_EXEC_ENTRY "\n");
  g_autofree char *contents = NULL;
  gsize contents_len = 0;

  if (g_file_get_contents (path, &contents, &contents_len, NULL))
    {
      LineReader reader;
      gsize len;
      char *line;

      line_reader_init (&reader, contents, contents_len);

      while ((line = line_reader_next (&reader, &len)))
        {
          line[len] = 0;

          g_strchug (line);

          if (line_is_ptyxis (line))
            continue;

          g_string_append (replace, line);
          g_string_append_c (replace, '\n');
        }
    }

  g_file_set_contents (path, replace->str, replace->len, NULL);
}

gboolean
ptyxis_make_default (void)
{
  g_auto(GStrv) paths = get_xdg_terminals_list_paths ();

  for (guint i = 0; paths[i]; i++)
    ptyxis_make_default_in_file (paths[i]);

  return ptyxis_is_default ();
}

char *
ptyxis_escape_underline (const char *str)
{
  GString *gstr;

  if (str == NULL)
    return NULL;

  gstr = g_string_new (NULL);

  for (const char *c = str;
       *c != 0;
       c = g_utf8_next_char (c))
    {
      gunichar ch = g_utf8_get_char (c);

      if (ch == '_')
        g_string_append_c (gstr, '_');

      g_string_append_unichar (gstr, ch);
    }

  return g_string_free (gstr, FALSE);
}
