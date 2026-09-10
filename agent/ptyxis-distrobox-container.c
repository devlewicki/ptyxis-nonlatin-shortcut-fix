/* ptyxis-distrobox-container.c
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

#include <json-glib/json-glib.h>

#include "ptyxis-distrobox-container.h"

struct _PtyxisDistroboxContainer
{
  PtyxisPodmanContainer parent_instance;
  gboolean has_unshared_groups;
};

G_DEFINE_TYPE (PtyxisDistroboxContainer, ptyxis_distrobox_container, PTYXIS_TYPE_PODMAN_CONTAINER)

static gboolean
ptyxis_distrobox_container_deserialize (PtyxisPodmanContainer  *self,
                                       JsonObject             *object,
                                       GError                **error)
{
  PtyxisDistroboxContainer *distrobox_container = PTYXIS_DISTROBOX_CONTAINER (self);
  gboolean has_unshared_groups = FALSE;
  const char *label_val_str;

  g_assert (PTYXIS_IS_DISTROBOX_CONTAINER (self));
  g_assert (object != NULL);

  if (!PTYXIS_PODMAN_CONTAINER_CLASS (ptyxis_distrobox_container_parent_class)->deserialize (self, object, error))
    return FALSE;

  /* If the distrobox has been created with --init or --unshare-groups,
   * then we do not want to do our --tty redirection workaround.
   *
   * See: #477 #545
   */
  if ((label_val_str = ptyxis_podman_container_lookup_label(self, "distrobox.unshare_groups")))
    has_unshared_groups = g_strcmp0 (label_val_str, "1") == 0;

  distrobox_container->has_unshared_groups = has_unshared_groups;

  return TRUE;
}

static gboolean
ptyxis_distrobox_container_run_context_cb (PtyxisRunContext    *run_context,
                                           const char * const  *argv,
                                           const char * const  *env,
                                           const char          *cwd,
                                           PtyxisUnixFDMap     *unix_fd_map,
                                           gpointer             user_data,
                                           GError             **error)
{
  PtyxisDistroboxContainer *self = user_data;
  g_autoptr(GString) additional_flags = NULL;
  const char *name;
  int max_dest_fd;

  g_assert (PTYXIS_IS_DISTROBOX_CONTAINER (self));
  g_assert (PTYXIS_IS_RUN_CONTEXT (run_context));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (PTYXIS_IS_UNIX_FD_MAP (unix_fd_map));

  name = ptyxis_ipc_container_get_display_name (PTYXIS_IPC_CONTAINER (self));

  ptyxis_run_context_append_argv (run_context, "distrobox");
  ptyxis_run_context_append_argv (run_context, "enter");

  if (!self->has_unshared_groups)
    ptyxis_run_context_append_argv (run_context, "--no-tty");

  ptyxis_run_context_append_argv (run_context, name);

  additional_flags = g_string_new (NULL);

  if (!self->has_unshared_groups)
    g_string_append (additional_flags, "--tty");

  /* From podman-exec(1):
   *
   * Pass down to the process N additional file descriptors (in addition to
   * 0, 1, 2).  The total FDs will be 3+N.
   */
  if ((max_dest_fd = ptyxis_unix_fd_map_get_max_dest_fd (unix_fd_map)) > 2)
    g_string_append_printf (additional_flags, " --preserve-fds=%d", max_dest_fd-2);

  /* Make sure we can pass the FDs down */
  if (!ptyxis_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  if (additional_flags->len)
    {
      ptyxis_run_context_append_argv (run_context, "--additional-flags");
      ptyxis_run_context_append_argv (run_context, additional_flags->str);
    }

  ptyxis_run_context_append_argv (run_context, "--");
  ptyxis_run_context_append_argv (run_context, "env");

  /* TODO: We need to find a way to propagate directory safely.
   *       env --chrdir= is an option if we know it's already there.
   */
  if (cwd != NULL && cwd[0] && g_file_test (cwd, G_FILE_TEST_EXISTS))
    ptyxis_run_context_set_cwd (run_context, cwd);
  else
    ptyxis_run_context_append_formatted (run_context, "--chdir=%s", cwd);

  /* Append environment if we have it */
  if (env != NULL)
    ptyxis_run_context_append_args (run_context, env);

  /* Finally, propagate the upper layer's command arguments */
  ptyxis_run_context_append_args (run_context, argv);

  return TRUE;
}

static void
ptyxis_distrobox_container_prepare_run_context (PtyxisPodmanContainer *container,
                                                PtyxisRunContext      *run_context)
{
  g_assert (PTYXIS_IS_DISTROBOX_CONTAINER (container));
  g_assert (PTYXIS_IS_RUN_CONTEXT (run_context));

  /* These seem to be needed for distrobox-enter */
  ptyxis_run_context_setenv (run_context, "HOME", g_get_home_dir ());
  ptyxis_run_context_setenv (run_context, "USER", g_get_user_name ());

  /* In case we got sandboxed due to incompatible host */
  ptyxis_run_context_push_host (run_context);

  ptyxis_run_context_push (run_context,
                           ptyxis_distrobox_container_run_context_cb,
                           g_object_ref (container),
                           g_object_unref);

  ptyxis_run_context_add_minimal_environment (run_context);

  /* But don't allow it to be overridden inside the environment, that
   * should be setup for us by the distrobox.
   */
  ptyxis_run_context_setenv (run_context, "HOME", NULL);
}

static void
ptyxis_distrobox_container_class_init (PtyxisDistroboxContainerClass *klass)
{
  PtyxisPodmanContainerClass *podman_container_class = PTYXIS_PODMAN_CONTAINER_CLASS (klass);

  podman_container_class->deserialize = ptyxis_distrobox_container_deserialize;
  podman_container_class->prepare_run_context = ptyxis_distrobox_container_prepare_run_context;
}

static void
ptyxis_distrobox_container_init (PtyxisDistroboxContainer *self)
{
  ptyxis_ipc_container_set_provider (PTYXIS_IPC_CONTAINER (self), "distrobox");
}
