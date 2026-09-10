/* ptyxis-run-context.h
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

#include "ptyxis-unix-fd-map.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_RUN_CONTEXT (ptyxis_run_context_get_type())

G_DECLARE_FINAL_TYPE (PtyxisRunContext, ptyxis_run_context, PTYXIS, RUN_CONTEXT, GObject)

/**
 * PtyxisRunContextShell:
 * @PTYXIS_RUN_CONTEXT_SHELL_DEFAULT: A basic shell with no user scripts
 * @PTYXIS_RUN_CONTEXT_SHELL_LOGIN: A user login shell similar to `bash -l`
 * @PTYXIS_RUN_CONTEXT_SHELL_INTERACTIVE: A user interactive shell similar to `bash -i`
 *
 * Describes the type of shell to be used within the context.
 */
typedef enum _PtyxisRunContextShell
{
  PTYXIS_RUN_CONTEXT_SHELL_DEFAULT     = 0,
  PTYXIS_RUN_CONTEXT_SHELL_LOGIN       = 1,
  PTYXIS_RUN_CONTEXT_SHELL_INTERACTIVE = 2,
} PtyxisRunContextShell;

/**
 * PtyxisRunContextHandler:
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error must be set.
 */
typedef gboolean (*PtyxisRunContextHandler) (PtyxisRunContext    *run_context,
                                             const char * const  *argv,
                                             const char * const  *env,
                                             const char          *cwd,
                                             PtyxisUnixFDMap     *unix_fd_map,
                                             gpointer             user_data,
                                             GError             **error);

PtyxisRunContext    *ptyxis_run_context_new                     (void);
void                 ptyxis_run_context_push                    (PtyxisRunContext         *self,
                                                                 PtyxisRunContextHandler   handler,
                                                                 gpointer                  handler_data,
                                                                 GDestroyNotify            handler_data_destroy);
void                 ptyxis_run_context_push_host               (PtyxisRunContext         *self);
void                 ptyxis_run_context_push_at_base            (PtyxisRunContext         *self,
                                                                 PtyxisRunContextHandler   handler,
                                                                 gpointer                  handler_data,
                                                                 GDestroyNotify            handler_data_destroy);
void                 ptyxis_run_context_push_error              (PtyxisRunContext         *self,
                                                                 GError                   *error);
void                 ptyxis_run_context_push_scope              (PtyxisRunContext         *self);
void                 ptyxis_run_context_push_shell              (PtyxisRunContext         *self,
                                                                 PtyxisRunContextShell     shell);
const char * const  *ptyxis_run_context_get_argv                (PtyxisRunContext         *self);
void                 ptyxis_run_context_set_argv                (PtyxisRunContext         *self,
                                                                 const char * const       *argv);
const char * const  *ptyxis_run_context_get_environ             (PtyxisRunContext         *self);
void                 ptyxis_run_context_set_environ             (PtyxisRunContext         *self,
                                                                 const char * const       *environ);
void                 ptyxis_run_context_add_environ             (PtyxisRunContext         *self,
                                                                 const char * const       *environ);
void                 ptyxis_run_context_add_minimal_environment (PtyxisRunContext         *self);
void                 ptyxis_run_context_environ_to_argv         (PtyxisRunContext         *self);
const char          *ptyxis_run_context_get_cwd                 (PtyxisRunContext         *self);
void                 ptyxis_run_context_set_cwd                 (PtyxisRunContext         *self,
                                                                 const char               *cwd);
void                 ptyxis_run_context_take_fd                 (PtyxisRunContext         *self,
                                                                 int                       source_fd,
                                                                 int                       dest_fd);
gboolean             ptyxis_run_context_merge_unix_fd_map       (PtyxisRunContext         *self,
                                                                 PtyxisUnixFDMap          *unix_fd_map,
                                                                 GError                  **error);
void                 ptyxis_run_context_prepend_argv            (PtyxisRunContext         *self,
                                                                 const char               *arg);
void                 ptyxis_run_context_prepend_args            (PtyxisRunContext         *self,
                                                                 const char * const       *args);
void                 ptyxis_run_context_append_argv             (PtyxisRunContext         *self,
                                                                 const char               *arg);
void                 ptyxis_run_context_append_args             (PtyxisRunContext         *self,
                                                                 const char * const       *args);
gboolean             ptyxis_run_context_append_args_parsed      (PtyxisRunContext         *self,
                                                                 const char               *args,
                                                                 GError                  **error);
void                 ptyxis_run_context_append_formatted        (PtyxisRunContext         *self,
                                                                 const char               *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
const char          *ptyxis_run_context_getenv                  (PtyxisRunContext         *self,
                                                                 const char               *key);
void                 ptyxis_run_context_setenv                  (PtyxisRunContext         *self,
                                                                 const char               *key,
                                                                 const char               *value);
void                 ptyxis_run_context_unsetenv                (PtyxisRunContext         *self,
                                                                 const char               *key);
GIOStream           *ptyxis_run_context_create_stdio_stream     (PtyxisRunContext         *self,
                                                                 GError                  **error);
GSubprocessLauncher *ptyxis_run_context_end                     (PtyxisRunContext         *self,
                                                                 GError                  **error);
GSubprocess         *ptyxis_run_context_spawn                   (PtyxisRunContext         *self,
                                                                 GError                  **error);
GSubprocess         *ptyxis_run_context_spawn_with_flags        (PtyxisRunContext         *self,
                                                                 GSubprocessFlags          flags,
                                                                 GError                  **error);

G_END_DECLS
