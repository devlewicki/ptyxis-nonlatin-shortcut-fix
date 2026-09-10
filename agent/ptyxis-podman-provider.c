/* ptyxis-podman-provider.c
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

#include <fcntl.h>
#include <stdio.h>

#include <json-glib/json-glib.h>

#include "ptyxis-agent-compat.h"
#include "ptyxis-agent-util.h"
#include "ptyxis-podman-container.h"
#include "ptyxis-podman-provider-private.h"
#include "ptyxis-run-context.h"

#define PODMAN_RELOAD_DELAY_SECONDS 3

typedef struct _LabelToType
{
  const char *label;
  const char *value;
  GType type;
} LabelToType;

struct _PtyxisPodmanProvider
{
  PtyxisContainerProvider parent_instance;
  GFileMonitor *storage_monitor;
  GFileMonitor *monitor;
  GArray *label_to_type;
  guint queued_update;
  guint is_updating : 1;
};

G_DEFINE_TYPE (PtyxisPodmanProvider, ptyxis_podman_provider, PTYXIS_TYPE_CONTAINER_PROVIDER)

static void
ptyxis_podman_provider_storage_dir_changed_cb (PtyxisPodmanProvider *self,
                                               GFile                *file,
                                               GFile                *other_file,
                                               GFileMonitorEvent     event,
                                               GFileMonitor         *monitor)
{
  g_autofree char *name = NULL;

  g_assert (PTYXIS_IS_PODMAN_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  name = g_file_get_basename (file);

  if (g_strcmp0 (name, "db.sql") == 0)
    ptyxis_podman_provider_queue_update (self);
}

static void
ptyxis_podman_provider_constructed (GObject *object)
{
  PtyxisPodmanProvider *self = (PtyxisPodmanProvider *)object;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) storage_dir = NULL;
  g_autofree char *data_dir = NULL;
  g_autofree char *parent_dir = NULL;

  G_OBJECT_CLASS (ptyxis_podman_provider_parent_class)->constructed (object);

  _g_set_str (&data_dir, g_get_user_data_dir ());
  if (data_dir == NULL)
    data_dir = g_build_filename (g_get_home_dir (), ".local", "share", NULL);

  g_assert (data_dir != NULL);

  storage_dir = g_file_new_build_filename (data_dir, "containers", "storage", NULL);
  parent_dir = g_build_filename (data_dir, "containers", "storage", "overlay-containers", NULL);
  file = g_file_new_build_filename (parent_dir, "containers.json", NULL);

  /* If our parent directory does not exist, we won't be able to monitor
   * for changes to the podman json file. Just create it upfront in the
   * same form that it'd be created by podman (mode 0700).
   */
  g_mkdir_with_parents (parent_dir, 0700);

  /* We have two files to monitor for potential updates. The containers.json
   * file is primarily how we've done it. But if we have hopes to track the
   * creation of containers via quadlet, we must monitor db.sql for changes.
   */

  if ((self->monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL)))
    g_signal_connect_object (self->monitor,
                             "changed",
                             G_CALLBACK (ptyxis_podman_provider_queue_update),
                             self,
                             G_CONNECT_SWAPPED);

  /*
   *
   * Since db.sql might not exist if we're the first to set things up, we
   * track changes to the @storage_dir directory. We filter on what files
   * are changed there, because we can see spurious unlreated updates just
   * from checking the state.
   */
  if ((self->storage_monitor = g_file_monitor_directory (storage_dir, G_FILE_MONITOR_NONE, NULL, NULL)))
    g_signal_connect_object (self->storage_monitor,
                             "changed",
                             G_CALLBACK (ptyxis_podman_provider_storage_dir_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  ptyxis_podman_provider_queue_update (self);
}

static void
ptyxis_podman_provider_dispose (GObject *object)
{
  PtyxisPodmanProvider *self = (PtyxisPodmanProvider *)object;

  if (self->label_to_type->len > 0)
    g_array_remove_range (self->label_to_type, 0, self->label_to_type->len);

  g_clear_object (&self->storage_monitor);
  g_clear_object (&self->monitor);
  g_clear_handle_id (&self->queued_update, g_source_remove);

  G_OBJECT_CLASS (ptyxis_podman_provider_parent_class)->dispose (object);
}

static void
ptyxis_podman_provider_finalize (GObject *object)
{
  PtyxisPodmanProvider *self = (PtyxisPodmanProvider *)object;

  g_clear_pointer (&self->label_to_type, g_array_unref);

  G_OBJECT_CLASS (ptyxis_podman_provider_parent_class)->finalize (object);
}

static void
ptyxis_podman_provider_class_init (PtyxisPodmanProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ptyxis_podman_provider_constructed;
  object_class->dispose = ptyxis_podman_provider_dispose;
  object_class->finalize = ptyxis_podman_provider_finalize;
}

static void
ptyxis_podman_provider_init (PtyxisPodmanProvider *self)
{
  self->label_to_type = g_array_new (FALSE, FALSE, sizeof (LabelToType));
}

PtyxisContainerProvider *
ptyxis_podman_provider_new (void)
{
  return g_object_new (PTYXIS_TYPE_PODMAN_PROVIDER, NULL);
}

void
ptyxis_podman_provider_set_type_for_label (PtyxisPodmanProvider *self,
                                           const char           *key,
                                           const char           *value,
                                           GType                 container_type)
{
  LabelToType map;

  g_return_if_fail (PTYXIS_IS_PODMAN_PROVIDER (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (g_type_is_a (container_type, PTYXIS_TYPE_PODMAN_CONTAINER));

  map.label = g_intern_string (key);
  map.value = g_intern_string (value);
  map.type = container_type;

  g_array_append_val (self->label_to_type, map);
}

static gboolean
label_matches (JsonNode          *node,
               const LabelToType *l_to_t)
{
  if (l_to_t->value != NULL)
    return JSON_NODE_HOLDS_VALUE (node) &&
           json_node_get_value_type (node) == G_TYPE_STRING &&
           g_strcmp0 (l_to_t->value, json_node_get_string (node)) == 0;

  return TRUE;
}

static PtyxisPodmanContainer *
ptyxis_podman_provider_deserialize (PtyxisPodmanProvider *self,
                                    JsonObject           *object)
{
  g_autoptr(PtyxisPodmanContainer) container = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *labels_object;
  JsonNode *labels;
  GType gtype;

  g_assert (PTYXIS_IS_PODMAN_PROVIDER (self));
  g_assert (object != NULL);

  gtype = PTYXIS_TYPE_PODMAN_CONTAINER;

  if (json_object_has_member (object, "Labels") &&
      (labels = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels) &&
      (labels_object = json_node_get_object (labels)))
    {
      for (guint i = 0; i < self->label_to_type->len; i++)
        {
          const LabelToType *l_to_t = &g_array_index (self->label_to_type, LabelToType, i);

          if (json_object_has_member (labels_object, l_to_t->label))
            {
              JsonNode *match = json_object_get_member (labels_object, l_to_t->label);

              if (label_matches (match, l_to_t))
                {
                  gtype = l_to_t->type;
                  break;
                }
            }
        }
    }

  container = g_object_new (gtype, NULL);

  if (!ptyxis_podman_container_deserialize (container, object, &error))
    {
      g_critical ("Failed to deserialize container JSON: %s", error->message);
      return NULL;
    }

  return g_steal_pointer (&container);
}

static gboolean
container_is_infra (JsonObject *object)
{
  JsonNode *is_infra;
  g_assert (object != NULL);

  return json_object_has_member (object, "IsInfra") &&
      (is_infra = json_object_get_member (object, "IsInfra")) &&
      json_node_get_value_type (is_infra) == G_TYPE_BOOLEAN &&
      json_node_get_boolean (is_infra);
}

gboolean
_ptyxis_podman_provider_parse_json (PtyxisPodmanProvider  *self,
                                    const char            *json,
                                    GError               **error)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GPtrArray) containers = NULL;
  JsonArray *root_array;
  JsonNode *root;

  g_assert (PTYXIS_IS_PODMAN_PROVIDER (self));
  g_assert (json != NULL);

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, json, -1, error))
    return FALSE;

  containers = g_ptr_array_new_with_free_func (g_object_unref);

  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_ARRAY (root) &&
      (root_array = json_node_get_array (root)))
    {
      guint n_elements = json_array_get_length (root_array);

      g_printerr ("Looking through %d elements\n", n_elements);

      for (guint i = 0; i < n_elements; i++)
        {
          g_autoptr(PtyxisPodmanContainer) container = NULL;
          JsonNode *element = json_array_get_element (root_array, i);
          JsonObject *element_object;

          if (JSON_NODE_HOLDS_OBJECT (element) &&
              (element_object = json_node_get_object (element)) &&
              !container_is_infra (element_object) &&
              (container = ptyxis_podman_provider_deserialize (self, element_object)))
            g_ptr_array_add (containers, g_steal_pointer (&container));
        }
    }

  ptyxis_container_provider_merge (PTYXIS_CONTAINER_PROVIDER (self), containers);

  return TRUE;
}

static void
ptyxis_podman_provider_communicate_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  PtyxisPodmanProvider *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  self->is_updating = FALSE;

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      g_debug ("Failed to run podman ps: %s", error->message);
      g_task_return_boolean (task, FALSE);
      return;
    }

  if (!_ptyxis_podman_provider_parse_json (self, stdout_buf, &error))
    {
      g_critical ("Failed to load podman JSON: %s", error->message);
      g_task_return_boolean (task, FALSE);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static gboolean
ptyxis_podman_provider_update_source_func (gpointer user_data)
{
  PtyxisPodmanProvider *self = user_data;
  g_autoptr(PtyxisRunContext) run_context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;

  g_assert (PTYXIS_IS_PODMAN_PROVIDER (self));

  self->queued_update = 0;

  run_context = ptyxis_run_context_new ();

  ptyxis_run_context_push_host (run_context);

  ptyxis_run_context_append_argv (run_context, "podman");
  ptyxis_run_context_append_argv (run_context, "ps");
  ptyxis_run_context_append_argv (run_context, "--all");
  ptyxis_run_context_append_argv (run_context, "--format=json");

  subprocess = ptyxis_run_context_spawn_with_flags (run_context, G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error);
  if (subprocess == NULL)
    return G_SOURCE_REMOVE;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, ptyxis_podman_provider_update_source_func);

  self->is_updating = TRUE;

  g_subprocess_communicate_utf8_async (subprocess,
                                       NULL,
                                       NULL,
                                       ptyxis_podman_provider_communicate_cb,
                                       g_steal_pointer (&task));

  return G_SOURCE_REMOVE;
}

void
ptyxis_podman_provider_queue_update (PtyxisPodmanProvider *self)
{
  g_return_if_fail (PTYXIS_IS_PODMAN_PROVIDER (self));

  if (self->queued_update == 0 && !self->is_updating)
    self->queued_update = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                      PODMAN_RELOAD_DELAY_SECONDS,
                                                      ptyxis_podman_provider_update_source_func,
                                                      self, NULL);
}

const char *
ptyxis_podman_provider_get_version (void)
{
  static char *version;

  if (version == NULL)
    {
      g_autoptr(PtyxisRunContext) run_context = NULL;
      g_autoptr(GSubprocess) subprocess = NULL;
      g_autoptr(JsonParser) parser = NULL;
      g_autofree char *stdout_buf = NULL;
      JsonObject *obj;
      JsonNode *node;

      run_context = ptyxis_run_context_new ();

      ptyxis_run_context_push_host (run_context);

      ptyxis_run_context_append_argv (run_context, "podman");
      ptyxis_run_context_append_argv (run_context, "version");
      ptyxis_run_context_append_argv (run_context, "--format=json");

      subprocess = ptyxis_run_context_spawn_with_flags (run_context, G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
      if (subprocess == NULL)
        return NULL;

      if (!g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
        return NULL;

      parser = json_parser_new ();
      if (!json_parser_load_from_data (parser, stdout_buf, -1, NULL))
        return NULL;

      if ((node = json_parser_get_root (parser)) &&
          JSON_NODE_HOLDS_OBJECT (node) &&
          (obj = json_node_get_object (node)) &&
          json_object_has_member (obj, "Client") &&
          (node = json_object_get_member (obj, "Client")) &&
          JSON_NODE_HOLDS_OBJECT (node) &&
          (obj = json_node_get_object (node)) &&
          json_object_has_member (obj, "Version") &&
          (node = json_object_get_member (obj, "Version")) &&
          JSON_NODE_HOLDS_VALUE (node))
        version = g_strdup (json_node_get_string (node));
    }

  return version;
}

gboolean
ptyxis_podman_provider_check_version (guint major,
                                      guint minor,
                                      guint micro)
{
  const char *version = ptyxis_podman_provider_get_version ();
  guint pmaj, pmin, pmic;

  if (version == NULL)
    return FALSE;

  if (sscanf (version, "%u.%u.%u", &pmaj, &pmin, &pmic) != 3)
    return FALSE;

  return (pmaj > major) ||
         ((pmaj == major) && (pmin > minor)) ||
         ((pmaj == major) && (pmin == minor) && (pmic >= micro));
}

gboolean
ptyxis_podman_provider_update_sync (PtyxisPodmanProvider  *self,
                                    GCancellable          *cancellable,
                                    GError               **error)
{
  g_autoptr(PtyxisRunContext) run_context = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GPtrArray) containers = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autofree char *stdout_buf = NULL;
  JsonArray *root_array;
  JsonNode *root;

  g_return_val_if_fail (PTYXIS_IS_PODMAN_PROVIDER (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  g_clear_handle_id (&self->queued_update, g_source_remove);

  run_context = ptyxis_run_context_new ();

  ptyxis_run_context_push_host (run_context);

  ptyxis_run_context_append_argv (run_context, "podman");
  ptyxis_run_context_append_argv (run_context, "ps");
  ptyxis_run_context_append_argv (run_context, "--all");
  ptyxis_run_context_append_argv (run_context, "--format=json");

  subprocess = ptyxis_run_context_spawn_with_flags (run_context, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);
  if (subprocess == NULL)
    return FALSE;

  if (!g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdout_buf, NULL, error))
    return FALSE;

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, stdout_buf, -1, error))
    return FALSE;

  containers = g_ptr_array_new_with_free_func (g_object_unref);

  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_ARRAY (root) &&
      (root_array = json_node_get_array (root)))
    {
      guint n_elements = json_array_get_length (root_array);

      for (guint i = 0; i < n_elements; i++)
        {
          g_autoptr(PtyxisPodmanContainer) container = NULL;
          JsonNode *element = json_array_get_element (root_array, i);
          JsonObject *element_object;

          if (JSON_NODE_HOLDS_OBJECT (element) &&
              (element_object = json_node_get_object (element)) &&
              !container_is_infra (element_object) &&
              (container = ptyxis_podman_provider_deserialize (self, element_object)))
            g_ptr_array_add (containers, g_steal_pointer (&container));
        }
    }

  ptyxis_container_provider_merge (PTYXIS_CONTAINER_PROVIDER (self), containers);

  return TRUE;
}
