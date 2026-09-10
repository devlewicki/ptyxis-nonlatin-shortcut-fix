/*
 * ptyxis-agent-compat.h
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

#include <unistd.h>

#include <gio/gio.h>

G_BEGIN_DECLS

static inline void
_ptyxis_clear_fd_ignore_error (int *fd_ptr)
{
  int errsv = errno;
  if (*fd_ptr != -1)
    {
      int fd = *fd_ptr;
      *fd_ptr = -1;
      close (fd);
    }
  errno = errsv;
}

#define _g_autofd _GLIB_CLEANUP(_ptyxis_clear_fd_ignore_error)

static inline int
_g_steal_fd (int *fdptr)
{
  int fd = *fdptr;
  *fdptr = -1;
  return fd;
}

static inline void
_g_clear_fd (int     *fdptr,
             GError **error)
{
  if (*fdptr != -1)
    {
      int fd = *fdptr;

      *fdptr = -1;

      if (close (fd) != 0)
        {
          int errsv = errno;

          if (error)
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 g_io_error_from_errno (errsv),
                                 g_strerror (errsv));
        }
    }
}

static inline GList *
_g_list_insert_before_link (GList *list,
                            GList *sibling,
                            GList *link_)
{
  g_return_val_if_fail (link_ != NULL, list);

  if (!list)
    {
      g_return_val_if_fail (sibling == NULL, list);
      return link_;
    }
  else if (sibling)
    {
      link_->prev = sibling->prev;
      link_->next = sibling;
      sibling->prev = link_;
      if (link_->prev)
        {
          link_->prev->next = link_;
          return list;
        }
      else
        {
          g_return_val_if_fail (sibling == list, link_);
          return link_;
        }
    }
  else
    {
      GList *last;

      last = list;
      while (last->next)
        last = last->next;

      last->next = link_;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}

static inline void
_g_queue_insert_before_link (GQueue *queue,
                             GList  *sibling,
                             GList  *link_)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (link_ != NULL);

  if G_UNLIKELY (sibling == NULL)
    {
      /* We don't use g_list_insert_before_link() with a NULL sibling because it
       * would be a O(n) operation and we would need to update manually the tail
       * pointer.
       */
      g_queue_push_tail_link (queue, link_);
    }
  else
    {
      queue->head = _g_list_insert_before_link (queue->head, sibling, link_);
      queue->length++;
    }
}

static inline gboolean
_g_set_str (char       **str_pointer,
            const char  *new_str)
{
  char *copy;

  if (*str_pointer == new_str ||
      (*str_pointer && new_str && strcmp (*str_pointer, new_str) == 0))
    return FALSE;

  copy = g_strdup (new_str);
  g_free (*str_pointer);
  *str_pointer = copy;

  return TRUE;
}

G_END_DECLS
