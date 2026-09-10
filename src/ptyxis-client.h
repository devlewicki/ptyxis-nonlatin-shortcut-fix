/*
 * ptyxis-client.h
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
#include <vte/vte.h>

#include "ptyxis-agent-ipc.h"
#include "ptyxis-profile.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_CLIENT (ptyxis_client_get_type())

G_DECLARE_FINAL_TYPE (PtyxisClient, ptyxis_client, PTYXIS, CLIENT, GObject)

PtyxisClient       *ptyxis_client_new                        (gboolean              use_sandbox,
                                                              GError              **error);
const char         *ptyxis_client_get_user_data_dir          (PtyxisClient         *self);
void                ptyxis_client_force_exit                 (PtyxisClient         *self);
VtePty             *ptyxis_client_create_pty                 (PtyxisClient         *self,
                                                              GError              **error);
int                 ptyxis_client_create_pty_producer        (PtyxisClient         *self,
                                                              VtePty               *pty,
                                                              GError              **error);
char              **ptyxis_client_discover_proxy_environment (PtyxisClient         *self,
                                                              GCancellable         *cancellable,
                                                              GError              **error);
void                ptyxis_client_discover_shell_async       (PtyxisClient         *self,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
char               *ptyxis_client_discover_shell_finish      (PtyxisClient         *client,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                ptyxis_client_spawn_async                (PtyxisClient         *self,
                                                              PtyxisIpcContainer   *container,
                                                              PtyxisProfile        *profile,
                                                              const char           *default_shell,
                                                              const char           *last_working_directory_uri,
                                                              VtePty               *pty,
                                                              const char * const   *alt_argv,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
PtyxisIpcProcess   *ptyxis_client_spawn_finish               (PtyxisClient         *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
PtyxisIpcContainer *ptyxis_client_discover_current_container (PtyxisClient         *self,
                                                              VtePty               *pty);
const char         *ptyxis_client_get_os_name                (PtyxisClient         *self);
gboolean            ptyxis_client_ping                       (PtyxisClient         *self,
                                                              int                   timeout_msec,
                                                              GError              **error);

G_END_DECLS
