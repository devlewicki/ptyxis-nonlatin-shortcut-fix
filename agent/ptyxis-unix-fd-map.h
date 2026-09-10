/* ptyxis-unix-fd-map.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_UNIX_FD_MAP (ptyxis_unix_fd_map_get_type())

G_DECLARE_FINAL_TYPE (PtyxisUnixFDMap, ptyxis_unix_fd_map, PTYXIS, UNIX_FD_MAP, GObject)

PtyxisUnixFDMap *ptyxis_unix_fd_map_new             (void);
guint            ptyxis_unix_fd_map_get_length      (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_peek_stdin      (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_peek_stdout     (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_peek_stderr     (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_steal_stdin     (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_steal_stdout    (PtyxisUnixFDMap  *self);
int              ptyxis_unix_fd_map_steal_stderr    (PtyxisUnixFDMap  *self);
gboolean         ptyxis_unix_fd_map_steal_from      (PtyxisUnixFDMap  *self,
                                                     PtyxisUnixFDMap  *other,
                                                     GError          **error);
int              ptyxis_unix_fd_map_peek            (PtyxisUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd);
int              ptyxis_unix_fd_map_get             (PtyxisUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd,
                                                     GError          **error);
int              ptyxis_unix_fd_map_steal           (PtyxisUnixFDMap  *self,
                                                     guint             index,
                                                     int              *dest_fd);
void             ptyxis_unix_fd_map_take            (PtyxisUnixFDMap  *self,
                                                     int               source_fd,
                                                     int               dest_fd);
gboolean         ptyxis_unix_fd_map_open_file       (PtyxisUnixFDMap  *self,
                                                     const char       *filename,
                                                     int               mode,
                                                     int               dest_fd,
                                                     GError          **error);
int              ptyxis_unix_fd_map_get_max_dest_fd (PtyxisUnixFDMap  *self);
gboolean         ptyxis_unix_fd_map_stdin_isatty    (PtyxisUnixFDMap  *self);
gboolean         ptyxis_unix_fd_map_stdout_isatty   (PtyxisUnixFDMap  *self);
gboolean         ptyxis_unix_fd_map_stderr_isatty   (PtyxisUnixFDMap  *self);
GIOStream       *ptyxis_unix_fd_map_create_stream   (PtyxisUnixFDMap  *self,
                                                     int               dest_read_fd,
                                                     int               dest_write_fd,
                                                     GError          **error);
gboolean         ptyxis_unix_fd_map_silence_fd      (PtyxisUnixFDMap  *self,
                                                     int               dest_fd,
                                                     GError          **error);

G_END_DECLS
