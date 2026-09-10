/*
 * ptyxis-client.c
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

#include <errno.h>
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif
#include <sys/socket.h>

#include <glib/gstdio.h>
#include <glib-unix.h>

#include "ptyxis-application.h"
#include "ptyxis-client.h"
#include "ptyxis-util.h"

struct _PtyxisClient
{
  GObject          parent_instance;
  GPtrArray       *containers;
  GSubprocess     *subprocess;
  GDBusConnection *bus;
  PtyxisIpcAgent  *proxy;
};

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

enum {
  CLOSED,
  PROCESS_EXITED,
  N_SIGNALS
};

static GType
ptyxis_client_get_item_type (GListModel *model)
{
  return PTYXIS_IPC_TYPE_CONTAINER;
}

static guint
ptyxis_client_get_n_items (GListModel *model)
{
  return PTYXIS_CLIENT (model)->containers->len;
}

static gpointer
ptyxis_client_get_item (GListModel *model,
                        guint       position)
{
  PtyxisClient *self = PTYXIS_CLIENT (model);

  if (position < self->containers->len)
    return g_object_ref (g_ptr_array_index (self->containers, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ptyxis_client_get_item_type;
  iface->get_n_items = ptyxis_client_get_n_items;
  iface->get_item = ptyxis_client_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtyxisClient, ptyxis_client, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
ptyxis_client_dispose (GObject *object)
{
  PtyxisClient *self = (PtyxisClient *)object;

  if (self->containers->len > 0)
    g_ptr_array_remove_range (self->containers, 0, self->containers->len);

  g_clear_object (&self->bus);
  g_clear_object (&self->proxy);
  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (ptyxis_client_parent_class)->dispose (object);
}

static void
ptyxis_client_finalize (GObject *object)
{
  PtyxisClient *self = (PtyxisClient *)object;

  g_clear_pointer (&self->containers, g_ptr_array_unref);

  G_OBJECT_CLASS (ptyxis_client_parent_class)->finalize (object);
}

static void
ptyxis_client_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PtyxisClient *self = PTYXIS_CLIENT (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, self->containers->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_client_class_init (PtyxisClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_client_dispose;
  object_class->finalize = ptyxis_client_finalize;
  object_class->get_property = ptyxis_client_get_property;

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  signals[PROCESS_EXITED] =
    g_signal_new ("process-exited",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_INT);
}

static void
ptyxis_client_init (PtyxisClient *self)
{
  self->containers = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
ptyxis_client_process_exited_cb (PtyxisClient   *self,
                                 const char     *process_object_path,
                                 int             exit_code,
                                 PtyxisIpcAgent *agent)
{
  g_assert (PTYXIS_IS_CLIENT (self));
  g_assert (PTYXIS_IPC_IS_AGENT (agent));

  g_signal_emit (self, signals[PROCESS_EXITED], 0, process_object_path, exit_code);
}

static void
ptyxis_client_containers_changed_cb (PtyxisClient       *self,
                                     guint               position,
                                     guint               removed,
                                     const char * const *added,
                                     PtyxisIpcAgent     *agent)
{
  guint added_len;

  g_assert (PTYXIS_IS_CLIENT (self));
  g_assert (PTYXIS_IPC_IS_AGENT (agent));

  if (removed > 0)
    g_ptr_array_remove_range (self->containers, position, removed);

  added_len = g_strv_length ((char **)added);

  for (guint i = 0; i < added_len; i++)
    {
      g_autoptr(PtyxisIpcContainer) container = NULL;
      g_autoptr(GError) error = NULL;
      const char *provider;
      const char *id;

      container = ptyxis_ipc_container_proxy_new_sync (self->bus,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       added[i],
                                                       NULL,
                                                       &error);

      provider = ptyxis_ipc_container_get_provider (container);
      id = ptyxis_ipc_container_get_id (container);

      g_debug ("Container %s:%s added at position %u",
               provider, id, position + i);

      g_ptr_array_insert (self->containers, position + i, g_steal_pointer (&container));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added_len);

  if (removed != added_len)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static char *
find_ptyxis_agent_path (gboolean in_sandbox)
{
  if (!in_sandbox &&
      ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
    {
      g_autofree char *contents = NULL;
      gsize len;

      if (g_file_get_contents ("/.flatpak-info", &contents, &len, NULL))
        {
          g_autoptr(GKeyFile) key_file = g_key_file_new ();
          g_autofree char *str = NULL;

          if (g_key_file_load_from_data (key_file, contents, len, 0, NULL) &&
              (str = g_key_file_get_string (key_file, "Instance", "app-path", NULL)))
            return g_build_filename (str, "libexec", "ptyxis-agent", NULL);
        }
    }

  return g_build_filename (LIBEXECDIR, "ptyxis-agent", NULL);
}

static void
ptyxis_client_wait_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(PtyxisClient) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (PTYXIS_IS_CLIENT (self));

  /* TODO:
   *
   * There isn't much we can do to recover here because without the peer we
   * can't monitor when processes exit/etc. Depending on how things go, we may
   * want to allow still interacting locally with whatever tabs are open, but
   * notify the user that there is pretty much no ability to continue.
   *
   * Another option could be a "terminal crashed" view in the PtyxisTab.
   */

  if (!g_subprocess_wait_check_finish (subprocess, result, &error))
    g_critical ("Client exited: %s", error->message);

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
ptyxis_client_child_setup_func (gpointer data)
{
  setsid ();
  setpgid (0, 0);

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGKILL);
#endif
}

static gpointer
do_timeout_on_thread (gpointer data)
{
  g_autoptr(GCancellable) cancellable = data;
  g_usleep (G_USEC_PER_SEC);
  g_cancellable_cancel (cancellable);
  return NULL;
}

static void
timeout_on_thread (GCancellable *cancellable)
{
  GThread *thread = g_thread_new ("[ptyxis-client-timeout]",
                                  do_timeout_on_thread,
                                  g_object_ref (cancellable));
  g_thread_unref (thread);
}

PtyxisClient *
ptyxis_client_new (gboolean   in_sandbox,
                   GError   **error)
{
  g_autofree char *ptyxis_agent_path = find_ptyxis_agent_path (in_sandbox);
  PtyxisClient *self = g_object_new (PTYXIS_TYPE_CLIENT, NULL);
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (0);
  g_autoptr(GPtrArray) argv = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GSocketConnection) stream = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_auto(GStrv) object_paths = NULL;
  g_autofree char *guid = NULL;
  gint64 default_rlimit_nofile;
  int pair[2];
  int res;

  if (!in_sandbox &&
      ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
    {
      g_ptr_array_add (argv, g_strdup ("flatpak-spawn"));
      g_ptr_array_add (argv, g_strdup ("--host"));
      g_ptr_array_add (argv, g_strdup ("--watch-bus"));
      g_ptr_array_add (argv, g_strdup_printf ("--forward-fd=3"));
    }

  g_ptr_array_add (argv, g_strdup (ptyxis_agent_path));
  g_ptr_array_add (argv, g_strdup ("--socket-fd=3"));
  if ((default_rlimit_nofile = ptyxis_application_get_default_rlimit_nofile ()) > 0)
    g_ptr_array_add (argv, g_strdup_printf ("--rlimit-nofile=%"G_GINT64_FORMAT, default_rlimit_nofile));
  g_ptr_array_add (argv, NULL);

#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  res = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, pair);
#else
  res = socketpair (AF_UNIX, SOCK_STREAM, 0, pair);

  if (res == 0)
    {
      fcntl (pair[0], F_SETFD, FD_CLOEXEC);
      fcntl (pair[1], F_SETFD, FD_CLOEXEC);
      g_unix_set_fd_nonblocking (pair[0], TRUE, NULL);
      g_unix_set_fd_nonblocking (pair[1], TRUE, NULL);
    }
#endif

  if (res != 0)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return NULL;
    }

  if (!(socket = g_socket_new_from_fd (pair[0], error)))
    {
      close (pair[0]);
      close (pair[1]);
      return NULL;
    }

  g_subprocess_launcher_take_fd (launcher, pair[1], 3);

  pair[0] = -1;
  pair[1] = -1;

  g_subprocess_launcher_set_child_setup (launcher,
                                         ptyxis_client_child_setup_func,
                                         NULL, NULL);
  if (!(subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)argv->pdata, error)))
    return NULL;

  g_set_object (&self->subprocess, subprocess);

  guid = g_dbus_generate_guid ();
  stream = g_socket_connection_factory_create_connection (socket);

  /* This can lock-up if the other side crashes when spawning.
   * Particularly if we flatpak-spawn on a host without glibc or
   * something like that.
   *
   * To handle that, we create a new GCancellable that will
   * timeout on a thread in short order so we don't lockup.
   */
  cancellable = g_cancellable_new ();
  timeout_on_thread (cancellable);
  if (!(bus = g_dbus_connection_new_sync (G_IO_STREAM (stream), guid,
                                          (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS |
                                           G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER),
                                          NULL, cancellable, error)))
    return NULL;

  g_set_object (&self->bus, bus);

  if (!(self->proxy = ptyxis_ipc_agent_proxy_new_sync (bus,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "/org/gnome/Ptyxis/Agent",
                                                       NULL,
                                                       error)))
    return NULL;

  g_signal_connect_object (self->proxy,
                           "containers-changed",
                           G_CALLBACK (ptyxis_client_containers_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->proxy,
                           "process-exited",
                           G_CALLBACK (ptyxis_client_process_exited_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_subprocess_wait_check_async (subprocess,
                                 NULL,
                                 ptyxis_client_wait_cb,
                                 g_object_ref (self));

  if (!ptyxis_ipc_agent_call_list_containers_sync (self->proxy, &object_paths, NULL, error))
    return NULL;

  for (guint i = 0; object_paths[i]; i++)
    {
      g_autoptr(PtyxisIpcContainer) container = NULL;
      const char *provider = NULL;
      const char *id = NULL;

      container = ptyxis_ipc_container_proxy_new_sync (self->bus,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       object_paths[i],
                                                       NULL,
                                                       error);

      if (container != NULL)
        {
          provider = ptyxis_ipc_container_get_provider (container);
          id = ptyxis_ipc_container_get_id (container);

          g_debug ("Container %s:%s added at position %u",
                   provider, id, i);

          g_ptr_array_add (self->containers, g_steal_pointer (&container));
        }

      g_clear_error (error);
    }

  return g_steal_pointer (&self);
}

void
ptyxis_client_force_exit (PtyxisClient *self)
{
  g_return_if_fail (PTYXIS_IS_CLIENT (self));

  g_subprocess_force_exit (self->subprocess);
}

VtePty *
ptyxis_client_create_pty (PtyxisClient  *self,
                          GError       **error)
{
  g_autoptr(GVariant) out_fd = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(VtePty) pty = NULL;
  int fd;
  int handle;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  if (self->subprocess == NULL || self->proxy == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "The connection to the agent has closed");
      return NULL;
    }

  if (!ptyxis_ipc_agent_call_create_pty_sync (self->proxy, NULL, &out_fd, &out_fd_list,
                                              NULL, error))
    return NULL;

  handle = g_variant_get_handle (out_fd);
  fd = g_unix_fd_list_get (out_fd_list, handle, error);
  if (fd == -1)
    return NULL;

  if ((pty = vte_pty_new_foreign_sync (fd, NULL, error)))
    vte_pty_set_utf8 (pty, TRUE, NULL);

  return g_steal_pointer (&pty);
}

static void
ptyxis_client_new_process_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr(PtyxisIpcProcess) process = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(process = ptyxis_ipc_process_proxy_new_finish (result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&process), g_object_unref);
}

static void
ptyxis_client_spawn_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  PtyxisIpcContainer *container = (PtyxisIpcContainer *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autofree char *object_path = NULL;
  PtyxisClient *self;

  g_assert (PTYXIS_IPC_IS_CONTAINER (container));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  g_assert (PTYXIS_IS_CLIENT (self));

  if (!ptyxis_ipc_container_call_spawn_finish (container, &object_path, NULL, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    ptyxis_ipc_process_proxy_new (self->bus,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  object_path,
                                  g_task_get_cancellable (task),
                                  ptyxis_client_new_process_cb,
                                  g_object_ref (task));
}

static const char *
maybe_extract_path_from_uri (GFile      *file,
                             const char *original_uri)
{
  if (file == NULL)
    return NULL;

  /*
   * Special-case handling for URIs which are file:// but GVFS
   * translated them into something like `sftp://.../` even though
   * they are local on disk (`/run/user/1000/gvfs/.../`).
   */
  if (!g_file_is_native (file))
    {
      if (original_uri == NULL)
        return NULL;

      if (!(original_uri[0] != '/' || g_str_has_prefix (original_uri, "file://")))
        return NULL;
    }

  return g_file_peek_path (file);
}

void
ptyxis_client_spawn_async (PtyxisClient        *self,
                           PtyxisIpcContainer  *container,
                           PtyxisProfile       *profile,
                           const char          *default_shell,
                           const char          *last_working_directory_uri,
                           VtePty              *pty,
                           const char * const  *alt_argv,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autofree char *custom_command = NULL;
  g_autofree char *arg0 = NULL;
  g_autoptr(GVariantBuilder) fd_builder = NULL;
  g_autoptr(GVariantBuilder) env_builder = NULL;
  g_autoptr(GStrvBuilder) argv_builder = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) last_directory = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GTask) task = NULL;
  g_auto(GStrv) env = NULL;
  g_auto(GStrv) full_argv = NULL;
  g_autofd int pty_fd = -1;
  const char *cwd = NULL;
  char vte_version[32];
  int handle;

  g_return_if_fail (PTYXIS_IS_CLIENT (self));
  g_return_if_fail (PTYXIS_IPC_IS_CONTAINER (container));
  g_return_if_fail (PTYXIS_IS_PROFILE (profile));
  g_return_if_fail (VTE_IS_PTY (pty));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (default_shell != NULL && default_shell[0] == 0)
    default_shell = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ptyxis_client_spawn_async);

  if (self->subprocess == NULL || self->proxy == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "The connection to the agent has closed");
      return;
    }

  if (-1 == (pty_fd = ptyxis_client_create_pty_producer (self, pty, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Make sure that the child PTY FD is blocking as most things
   * will expect that by default. We continue to keep our consumer
   * FD non-blocking for how we use it.
   */
  if (!g_unix_set_fd_nonblocking (pty_fd, FALSE, &error))
    {
      g_warning ("Failed to set child PTY FD non-blocking: %s",
                 error->message);
      g_clear_error (&error);
    }

  if (ptyxis_profile_get_use_proxy (profile))
    env = ptyxis_client_discover_proxy_environment (self, NULL, NULL);

  env = g_environ_setenv (env, "PTYXIS_PROFILE", ptyxis_profile_get_uuid (profile), TRUE);
  env = g_environ_setenv (env, "PTYXIS_VERSION", PACKAGE_VERSION, TRUE);
  env = g_environ_setenv (env, "COLORTERM", "truecolor", TRUE);
  env = g_environ_setenv (env, "TERM", "xterm-256color", TRUE);

  g_snprintf (vte_version, sizeof vte_version, "%u", VTE_VERSION_NUMERIC);
  env = g_environ_setenv (env, "VTE_VERSION", vte_version, TRUE);

  argv_builder = g_strv_builder_new ();

  if (alt_argv != NULL && alt_argv[0] != NULL)
    {
      arg0 = NULL;
      g_strv_builder_addv (argv_builder, (const char **)alt_argv);
    }
  else if (ptyxis_profile_get_use_custom_command (profile))
    {
      g_auto(GStrv) argv = NULL;

      custom_command = ptyxis_profile_dup_custom_command (profile);

      if (!g_shell_parse_argv (custom_command, NULL, &argv, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      g_strv_builder_addv (argv_builder, (const char **)argv);
      arg0 = g_strdup (argv[0]);
    }
  else if (default_shell != NULL)
    {
      arg0 = g_strdup (default_shell);
      g_strv_builder_add (argv_builder, arg0);
    }
  else
    {
      arg0 = g_strdup ("");
      g_strv_builder_add (argv_builder, "sh");
      g_strv_builder_add (argv_builder, "-c");
      g_strv_builder_add (argv_builder, "if [ -x \"$(getent passwd $(whoami) | cut -d : -f 7)\" ]; then exec $(getent passwd $(whoami) | cut -d : -f 7); else exec sh; fi");
    }

  if (arg0 != NULL &&
      ptyxis_profile_get_login_shell (profile) &&
      ptyxis_shell_supports_dash_l (arg0))
    g_strv_builder_add (argv_builder, "-l");

  if (last_working_directory_uri != NULL)
    last_directory = g_file_new_for_uri (last_working_directory_uri);

  if (alt_argv != NULL && alt_argv[0] != NULL)
    {
      if (last_directory != NULL)
        cwd = g_file_peek_path (last_directory);
    }

  if (cwd == NULL)
    {
      switch (ptyxis_profile_get_preserve_directory (profile))
        {
        case PTYXIS_PRESERVE_DIRECTORY_NEVER:
          break;

        case PTYXIS_PRESERVE_DIRECTORY_SAFE:
          /* TODO: We might want to check with the container that this
           * is a shell (as opposed to one available on the host).
           */
          if (!arg0 || !ptyxis_is_shell (arg0))
            break;
          G_GNUC_FALLTHROUGH;

        case PTYXIS_PRESERVE_DIRECTORY_ALWAYS:
          cwd = maybe_extract_path_from_uri (last_directory, last_working_directory_uri);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (cwd == NULL)
    cwd = "";

  full_argv = g_strv_builder_end (argv_builder);

  fd_list = g_unix_fd_list_new ();

  if (-1 == (handle = g_unix_fd_list_append (fd_list, pty_fd, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  fd_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{uh}"));
  g_variant_builder_add (fd_builder, "{uh}", 0, handle);
  g_variant_builder_add (fd_builder, "{uh}", 1, handle);
  g_variant_builder_add (fd_builder, "{uh}", 2, handle);

  env_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
  for (guint i = 0; env[i]; i++)
    {
      const char *pair = env[i];
      const char *eq = strchr (pair, '=');
      const char *val = eq ? eq + 1 : "";
      g_autofree char *key = eq ? g_strndup (pair, eq - pair) : g_strdup (pair);

      g_variant_builder_add (env_builder, "{ss}", key, val);
    }

  ptyxis_ipc_container_call_spawn (container,
                                   cwd,
                                   (const char * const *)full_argv,
                                   g_variant_builder_end (g_steal_pointer (&fd_builder)),
                                   g_variant_builder_end (g_steal_pointer (&env_builder)),
                                   fd_list,
                                   cancellable,
                                   ptyxis_client_spawn_cb,
                                   g_steal_pointer (&task));
}

PtyxisIpcProcess *
ptyxis_client_spawn_finish (PtyxisClient  *client,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (client), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ptyxis_client_discover_shell_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  PtyxisIpcAgent *agent = (PtyxisIpcAgent *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autofree char *default_shell = NULL;

  g_assert (PTYXIS_IPC_IS_AGENT (agent));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ptyxis_ipc_agent_call_get_preferred_shell_finish (agent, &default_shell, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&default_shell), g_free);
}

void
ptyxis_client_discover_shell_async (PtyxisClient        *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (PTYXIS_IS_CLIENT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ptyxis_client_discover_shell_async);

  if (self->subprocess == NULL || self->proxy == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "The connection to the agent has closed");
      return;
    }

  ptyxis_ipc_agent_call_get_preferred_shell (self->proxy,
                                             cancellable,
                                             ptyxis_client_discover_shell_cb,
                                             g_steal_pointer (&task));
}

char *
ptyxis_client_discover_shell_finish (PtyxisClient  *client,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (client), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

int
ptyxis_client_create_pty_producer (PtyxisClient  *self,
                                   VtePty        *pty,
                                   GError       **error)
{
  g_autoptr(GUnixFDList) in_fd_list = NULL;
  g_autoptr(GUnixFDList) out_fd_list = NULL;
  g_autoptr(GVariant) out_fd = NULL;
  g_autofd int fd = -1;
  int in_handle;
  int out_handle;
  int pty_fd;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), -1);
  g_return_val_if_fail (VTE_IS_PTY (pty), -1);

  if (self->subprocess == NULL || self->proxy == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "The connection to the agent has closed");
      return -1;
    }

  pty_fd = vte_pty_get_fd (pty);

  in_fd_list = g_unix_fd_list_new ();
  if (-1 == (in_handle = g_unix_fd_list_append (in_fd_list, pty_fd, error)))
    return -1;

  if (!ptyxis_ipc_agent_call_create_pty_producer_sync (self->proxy,
                                                       g_variant_new_handle (in_handle),
                                                       in_fd_list,
                                                       &out_fd,
                                                       &out_fd_list,
                                                       NULL,
                                                       error))
    return -1;

  out_handle = g_variant_get_handle (out_fd);
  if (-1 == (fd = g_unix_fd_list_get (out_fd_list, out_handle, error)))
    return -1;

  return g_steal_fd (&fd);
}

PtyxisIpcContainer *
ptyxis_client_discover_current_container (PtyxisClient *self,
                                          VtePty       *pty)
{
  g_autofree char *object_path = NULL;
  g_autoptr(GUnixFDList) in_fd_list = NULL;
  int in_handle;
  int pty_fd;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);
  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  pty_fd = vte_pty_get_fd (pty);

  in_fd_list = g_unix_fd_list_new ();
  if (-1 == (in_handle = g_unix_fd_list_append (in_fd_list, pty_fd, NULL)))
    return NULL;

  if (ptyxis_ipc_agent_call_discover_current_container_sync (self->proxy,
                                                             g_variant_new_handle (in_handle),
                                                             in_fd_list,
                                                             &object_path,
                                                             NULL,
                                                             NULL,
                                                             NULL))
    {
      for (guint i = 0; i < self->containers->len; i++)
        {
          PtyxisIpcContainer *container = g_ptr_array_index (self->containers, i);

          if (g_strcmp0 (object_path,
                         g_dbus_proxy_get_object_path (G_DBUS_PROXY (container))) == 0)
            return g_object_ref (container);
        }
    }

  return NULL;
}

const char *
ptyxis_client_get_os_name (PtyxisClient *self)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  return ptyxis_ipc_agent_get_os_name (self->proxy);
}

const char *
ptyxis_client_get_user_data_dir (PtyxisClient *self)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  return ptyxis_ipc_agent_get_user_data_dir (self->proxy);
}

char **
ptyxis_client_discover_proxy_environment (PtyxisClient  *self,
                                          GCancellable  *cancellable,
                                          GError       **error)
{
  char **ret = NULL;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  if (ptyxis_ipc_agent_call_discover_proxy_environment_sync (self->proxy, &ret, cancellable, error))
    return ret;

  return NULL;
}

gboolean
ptyxis_client_ping (PtyxisClient  *self,
                    int            timeout_msec,
                    GError       **error)
{
  g_autoptr(GVariant) ret = NULL;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), FALSE);

  ret = g_dbus_connection_call_sync (self->bus,
                                     NULL,
                                     "/org/gnome/Ptyxis/Agent",
                                     "org.freedesktop.DBus.Peer",
                                     "Ping",
                                     NULL,
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     timeout_msec,
                                     NULL,
                                     error);

  return ret != NULL;
}
