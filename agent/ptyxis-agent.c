/*
 * ptyxis-agent.c
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

#ifdef LIBC_COMPAT
# include "libc-compat.h"
#endif

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include "ptyxis-agent-impl.h"
#include "ptyxis-distrobox-container.h"
#include "ptyxis-podman-provider.h"
#include "ptyxis-session-container.h"
#include "ptyxis-toolbox-container.h"
#include "ptyxis-agent-util.h"

gint64 default_rlimit_nofile = 0;

typedef struct _PtyxisAgent
{
  PtyxisAgentImpl   *impl;
  GSocket           *socket;
  GSocketConnection *stream;
  GDBusConnection   *bus;
  GMainLoop         *main_loop;
  int                exit_code;
} PtyxisAgent;

G_GNUC_UNUSED
static void
ptyxis_agent_quit (PtyxisAgent *agent,
                   int          exit_code)
{
  agent->exit_code = exit_code;
  g_main_loop_quit (agent->main_loop);
}

G_GNUC_UNUSED
static gboolean
ptyxis_agent_init (PtyxisAgent  *agent,
                   int           socket_fd,
                   GError      **error)
{
  g_autoptr(PtyxisSessionContainer) session = NULL;
  g_autoptr(PtyxisContainerProvider) podman = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GFile) jhbuildrc = NULL;

  memset (agent, 0, sizeof *agent);

  if (socket_fd <= 2)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "socket-fd must be set to a FD > 2");
      return FALSE;
    }

  agent->main_loop = g_main_loop_new (NULL, FALSE);

  if (!(agent->socket = g_socket_new_from_fd (socket_fd, error)))
    {
      close (socket_fd);
      return FALSE;
    }

  agent->stream = g_socket_connection_factory_create_connection (agent->socket);

  g_assert (agent->stream != NULL);
  g_assert (G_IS_SOCKET_CONNECTION (agent->stream));

  if (!(agent->bus = g_dbus_connection_new_sync (G_IO_STREAM (agent->stream),
                                                 NULL,
                                                 (G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING |
                                                  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                                 NULL,
                                                 NULL,
                                                 error)) ||
      !(agent->impl = ptyxis_agent_impl_new (error)) ||
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (agent->impl),
                                         agent->bus,
                                         "/org/gnome/Ptyxis/Agent",
                                         error))
    return FALSE;

  session = ptyxis_session_container_new ();
  ptyxis_agent_impl_add_container (agent->impl, PTYXIS_IPC_CONTAINER (session));

  jhbuildrc = g_file_new_build_filename (g_get_home_dir (), ".config", "jhbuildrc", NULL);
  if (g_file_query_exists (jhbuildrc, NULL))
    {
      g_autoptr(PtyxisSessionContainer) jhbuild_container = ptyxis_session_container_new ();
      ptyxis_ipc_container_set_id (PTYXIS_IPC_CONTAINER (jhbuild_container), "jhbuild");
      ptyxis_ipc_container_set_provider (PTYXIS_IPC_CONTAINER (jhbuild_container), "jhbuild");
      ptyxis_ipc_container_set_display_name (PTYXIS_IPC_CONTAINER (jhbuild_container), "JHBuild");
      ptyxis_ipc_container_set_icon_name (PTYXIS_IPC_CONTAINER (jhbuild_container), "container-jhbuild-symbolic");
      ptyxis_session_container_set_command_prefix (PTYXIS_SESSION_CONTAINER (jhbuild_container),
                                                   (const char * const []) { "jhbuild", "run", NULL });
      ptyxis_agent_impl_add_container (agent->impl, PTYXIS_IPC_CONTAINER (jhbuild_container));
    }

  podman = ptyxis_podman_provider_new ();

  /* Prioritize "manager":"distrobox" above toolbox because it erroniously
   * can add com.github.containers.toolbox too! See #245 for details.
   */
  ptyxis_podman_provider_set_type_for_label (PTYXIS_PODMAN_PROVIDER (podman),
                                             "manager",
                                             "distrobox",
                                             PTYXIS_TYPE_DISTROBOX_CONTAINER);

  ptyxis_podman_provider_set_type_for_label (PTYXIS_PODMAN_PROVIDER (podman),
                                             "manager",
                                             "apx",
                                             PTYXIS_TYPE_DISTROBOX_CONTAINER);

  ptyxis_podman_provider_set_type_for_label (PTYXIS_PODMAN_PROVIDER (podman),
                                             "com.github.containers.toolbox",
                                             NULL,
                                             PTYXIS_TYPE_TOOLBOX_CONTAINER);

  if (!ptyxis_podman_provider_update_sync (PTYXIS_PODMAN_PROVIDER (podman), NULL, &local_error))
    {
      g_debug ("Failed to process podman containers: %s", local_error->message);

      /* Sometimes podman seems to crap out on us. Try a second time and see
       * if that works any better. See #62.
       */
      ptyxis_podman_provider_update_sync (PTYXIS_PODMAN_PROVIDER (podman), NULL, NULL);
    }

  ptyxis_agent_impl_add_provider (agent->impl, podman);

  g_dbus_connection_start_message_processing (agent->bus);

  return TRUE;
}

G_GNUC_UNUSED
static int
ptyxis_agent_run (PtyxisAgent *agent)
{
  g_main_loop_run (agent->main_loop);
  return agent->exit_code;
}

G_GNUC_UNUSED
static void
ptyxis_agent_destroy (PtyxisAgent *agent)
{
  g_clear_object (&agent->impl);
  g_clear_object (&agent->socket);
  g_clear_object (&agent->stream);
  g_clear_object (&agent->bus);
  g_clear_pointer (&agent->main_loop, g_main_loop_unref);
}

gint64
ptyxis_agent_get_default_rlimit_nofile (void)
{
  return default_rlimit_nofile;
}

#ifndef DISABLE_AGENT_ENTRY_POINT
int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  PtyxisAgent agent;
  int socket_fd = -1;
  int ret;

  const GOptionEntry entries[] = {
    { "socket-fd", 0, 0, G_OPTION_ARG_INT, &socket_fd, "The socketpair to communicate over", "FD" },
    { "rlimit-nofile", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT64, &default_rlimit_nofile },
    { NULL }
  };

  g_set_prgname ("ptyxis-agent");
  g_set_application_name ("ptyxis-agent");

  context = g_option_context_new ("- terminal container agent");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error) ||
      !ptyxis_agent_init (&agent, socket_fd, &error))
    {
      g_printerr ("usage: %s --socket-fd=FD\n", argv[0]);
      g_printerr ("\n");
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  ret = ptyxis_agent_run (&agent);
  ptyxis_agent_destroy (&agent);

  return ret;
}
#endif
