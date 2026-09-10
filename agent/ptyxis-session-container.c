/*
 * ptyxis-session-container.c
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

#include "ptyxis-agent-compat.h"
#include "ptyxis-agent-util.h"
#include "ptyxis-process-impl.h"
#include "ptyxis-session-container.h"

struct _PtyxisSessionContainer
{
  PtyxisIpcContainerSkeleton parent_instance;
  char **command_prefix;
};

static void container_iface_init (PtyxisIpcContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (PtyxisSessionContainer, ptyxis_session_container, PTYXIS_IPC_TYPE_CONTAINER_SKELETON,
                         G_IMPLEMENT_INTERFACE (PTYXIS_IPC_TYPE_CONTAINER, container_iface_init))

static void
ptyxis_session_container_finalize (GObject *object)
{
  PtyxisSessionContainer *self = (PtyxisSessionContainer *)object;

  g_clear_pointer (&self->command_prefix, g_strfreev);

  G_OBJECT_CLASS (ptyxis_session_container_parent_class)->finalize (object);
}

static void
ptyxis_session_container_class_init (PtyxisSessionContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ptyxis_session_container_finalize;
}

static void
ptyxis_session_container_init (PtyxisSessionContainer *self)
{
  ptyxis_ipc_container_set_id (PTYXIS_IPC_CONTAINER (self), "session");
  ptyxis_ipc_container_set_provider (PTYXIS_IPC_CONTAINER (self), "session");
}

PtyxisSessionContainer *
ptyxis_session_container_new (void)
{
  return g_object_new (PTYXIS_TYPE_SESSION_CONTAINER, NULL);
}

static gboolean
ptyxis_session_container_handle_spawn (PtyxisIpcContainer    *container,
                                       GDBusMethodInvocation *invocation,
                                       GUnixFDList           *in_fd_list,
                                       const char            *cwd,
                                       const char * const    *argv,
                                       GVariant              *in_fds,
                                       GVariant              *in_env)
{
  PtyxisSessionContainer *self = (PtyxisSessionContainer *)container;
  g_autoptr(PtyxisRunContext) run_context = NULL;
  g_autoptr(PtyxisIpcProcess) process = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) env = NULL;
  GDBusConnection *connection;
  g_autofree char *object_path = NULL;
  g_autofree char *guid = NULL;

  g_assert (PTYXIS_IS_SESSION_CONTAINER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_UNIX_FD_LIST (in_fd_list));
  g_assert (cwd != NULL);
  g_assert (argv != NULL);
  g_assert (in_fds != NULL);
  g_assert (in_env != NULL);

  /* Make sure CWD exists within the user session, it might have
   * come from another container that isn't the same or at a path
   * that is not accessible to the user (say from a sudo shell).
   */
  if (cwd[0] == 0 || !g_file_test (cwd, G_FILE_TEST_IS_DIR) ||
      !g_file_test (cwd, G_FILE_TEST_IS_EXECUTABLE))
    cwd = g_get_home_dir ();

  env = g_get_environ ();

  run_context = ptyxis_run_context_new ();

  /* If we had to run within Flatpak, escape to host */
  ptyxis_run_context_push_host (run_context);

  /* Place the process inside a new scope similar to what VTE would do. */
  ptyxis_run_context_push_scope (run_context);

  /* For the default session, we'll just inherit whatever the session gave us
   * as our environment. For other types of containers, that may be different
   * as you likely want to filter some stateful things out.
   */
  if (ptyxis_agent_is_sandboxed ())
    ptyxis_run_context_add_minimal_environment (run_context);
  else
    ptyxis_run_context_set_environ (run_context, (const char * const *)env);

  /* If a command prefix was specified, add that now */
  if (self->command_prefix != NULL)
    ptyxis_run_context_append_args (run_context, (const char * const *)self->command_prefix);

  /* Use the spawn helper to copy everything that matters after marshaling
   * out of GVariant format. This will be very much the same for other
   * container providers.
   */
  ptyxis_agent_push_spawn (run_context, in_fd_list, cwd, argv, in_fds, in_env);

  /* Spawn and export our object to the bus. Note that a weak reference is used
   * for the object on the bus so you must keep the object alive to ensure that
   * it is not removed from the bus. The default PtyxisProcessImpl does that for
   * us by automatically waiting for the child to exit.
   */
  guid = g_dbus_generate_guid ();
  object_path = g_strdup_printf ("/org/gnome/Ptyxis/Process/%s", guid);
  out_fd_list = g_unix_fd_list_new ();
  connection = g_dbus_method_invocation_get_connection (invocation);
  if (!(subprocess = ptyxis_run_context_spawn (run_context, &error)) ||
      !(process = ptyxis_process_impl_new (connection, subprocess, object_path, &error)))
    g_dbus_method_invocation_return_gerror (g_steal_pointer (&invocation), error);
  else
    ptyxis_ipc_container_complete_spawn (container,
                                         g_steal_pointer (&invocation),
                                         out_fd_list,
                                         object_path);

  return TRUE;
}

static gboolean
ptyxis_session_container_handle_find_program_in_path (PtyxisIpcContainer    *container,
                                                      GDBusMethodInvocation *invocation,
                                                      const char            *program)
{
  g_autofree char *path = NULL;

  g_assert (PTYXIS_IS_SESSION_CONTAINER (container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if ((path = g_find_program_in_path (program)))
    ptyxis_ipc_container_complete_find_program_in_path (container,
                                                        g_steal_pointer (&invocation),
                                                        path);
  else
    g_dbus_method_invocation_return_error_literal (g_steal_pointer (&invocation),
                                                   G_IO_ERROR,
                                                   G_IO_ERROR_NOT_FOUND,
                                                   "Not Found");

  return TRUE;
}

static gboolean
ptyxis_session_container_handle_translate_uri (PtyxisIpcContainer    *container,
                                               GDBusMethodInvocation *invocation,
                                               const char            *uri)
{
  g_assert (PTYXIS_IS_SESSION_CONTAINER (container));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  ptyxis_ipc_container_complete_translate_uri (container,
                                               g_steal_pointer (&invocation),
                                               uri);

  return TRUE;
}

static void
container_iface_init (PtyxisIpcContainerIface *iface)
{
  iface->handle_find_program_in_path = ptyxis_session_container_handle_find_program_in_path;
  iface->handle_spawn = ptyxis_session_container_handle_spawn;
  iface->handle_translate_uri = ptyxis_session_container_handle_translate_uri;
}

void
ptyxis_session_container_set_command_prefix (PtyxisSessionContainer *self,
                                             const char * const     *command_prefix)
{
  char **copy;

  g_return_if_fail (PTYXIS_IS_SESSION_CONTAINER (self));

  copy = g_strdupv ((char **)command_prefix);
  g_strfreev (self->command_prefix);
  self->command_prefix = copy;
}
