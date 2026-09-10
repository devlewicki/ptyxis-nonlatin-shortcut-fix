/*
 * ptyxis-agent-util.h
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

#pragma once

#include <gio/gio.h>

#include "ptyxis-run-context.h"

G_BEGIN_DECLS

int      ptyxis_agent_pty_new                   (GError             **error);
int      ptyxis_agent_pty_new_producer          (int                  consumer_fd,
                                                 GError             **error);
void     ptyxis_agent_push_spawn                (PtyxisRunContext    *run_context,
                                                 GUnixFDList         *fd_list,
                                                 const char          *cwd,
                                                 const char * const  *argv,
                                                 GVariant            *fds,
                                                 GVariant            *env);
gboolean ptyxis_agent_is_sandboxed              (void) G_GNUC_CONST;
gint64   ptyxis_agent_get_default_rlimit_nofile (void);

G_END_DECLS
