/* ptyxis-container-provider.c
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

#include "ptyxis-container-provider.h"

typedef struct
{
  GPtrArray *containers;
} PtyxisContainerProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PtyxisContainerProvider, ptyxis_container_provider, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (PtyxisContainerProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  ADDED,
  REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
ptyxis_container_provider_real_added (PtyxisContainerProvider *self,
                                      PtyxisIpcContainer      *container)
{
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);
  guint position;

  g_assert (PTYXIS_IS_CONTAINER_PROVIDER (self));
  g_assert (PTYXIS_IPC_IS_CONTAINER (container));

  g_debug ("Added container \"%s\"",
           ptyxis_ipc_container_get_id (container));

  position = priv->containers->len;

  g_ptr_array_add (priv->containers, g_object_ref (container));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
ptyxis_container_provider_real_removed (PtyxisContainerProvider *self,
                                        PtyxisIpcContainer      *container)
{
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);
  guint position;

  g_assert (PTYXIS_IS_CONTAINER_PROVIDER (self));
  g_assert (PTYXIS_IPC_IS_CONTAINER (container));

  g_debug ("Removed container \"%s\"",
           ptyxis_ipc_container_get_id (container));

  if (g_ptr_array_find (priv->containers, container, &position))
    {
      g_ptr_array_remove_index (priv->containers, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }
}

static void
ptyxis_container_provider_dispose (GObject *object)
{
  PtyxisContainerProvider *self = (PtyxisContainerProvider *)object;
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  if (priv->containers->len > 0)
    g_ptr_array_remove_range (priv->containers, 0, priv->containers->len);

  G_OBJECT_CLASS (ptyxis_container_provider_parent_class)->dispose (object);
}

static void
ptyxis_container_provider_finalize (GObject *object)
{
  PtyxisContainerProvider *self = (PtyxisContainerProvider *)object;
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  g_clear_pointer (&priv->containers, g_ptr_array_unref);

  G_OBJECT_CLASS (ptyxis_container_provider_parent_class)->finalize (object);
}

static void
ptyxis_container_provider_class_init (PtyxisContainerProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_container_provider_dispose;
  object_class->finalize = ptyxis_container_provider_finalize;

  klass->added = ptyxis_container_provider_real_added;
  klass->removed = ptyxis_container_provider_real_removed;

  signals[ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PtyxisContainerProviderClass, added),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, PTYXIS_IPC_TYPE_CONTAINER);

  signals[REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PtyxisContainerProviderClass, removed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, PTYXIS_IPC_TYPE_CONTAINER);
}

static void
ptyxis_container_provider_init (PtyxisContainerProvider *self)
{
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  priv->containers = g_ptr_array_new_with_free_func (g_object_unref);
}

void
ptyxis_container_provider_emit_added (PtyxisContainerProvider *self,
                                      PtyxisIpcContainer      *container)
{
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);
  const char *id;

  g_return_if_fail (PTYXIS_IS_CONTAINER_PROVIDER (self));
  g_return_if_fail (PTYXIS_IPC_IS_CONTAINER (container));

  id = ptyxis_ipc_container_get_id (container);

  for (guint i = 0; i < priv->containers->len; i++)
    {
      if (g_strcmp0 (id, ptyxis_ipc_container_get_id (g_ptr_array_index (priv->containers, i))) == 0)
        {
          g_warning ("Container \"%s\" already added", id);
          return;
        }
    }

  g_signal_emit (self, signals[ADDED], 0, container);
}

void
ptyxis_container_provider_emit_removed (PtyxisContainerProvider *self,
                                        PtyxisIpcContainer      *container)
{
  g_return_if_fail (PTYXIS_IS_CONTAINER_PROVIDER (self));
  g_return_if_fail (PTYXIS_IPC_IS_CONTAINER (container));

  g_signal_emit (self, signals[REMOVED], 0, container);
}

static gboolean
compare_by_id (gconstpointer a,
               gconstpointer b)
{
  return 0 == g_strcmp0 (ptyxis_ipc_container_get_id ((PtyxisIpcContainer *)a),
                         ptyxis_ipc_container_get_id ((PtyxisIpcContainer *)b));
}

void
ptyxis_container_provider_merge (PtyxisContainerProvider *self,
                                 GPtrArray               *containers)
{
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  g_return_if_fail (PTYXIS_IS_CONTAINER_PROVIDER (self));
  g_return_if_fail (containers != NULL);

  /* First remove any containers not in the set, or replace them with
   * the new version of the object. Scan in reverse so that we can
   * have stable indexes.
   */
  for (guint i = priv->containers->len; i > 0; i--)
    {
      PtyxisIpcContainer *container = g_ptr_array_index (priv->containers, i-1);
      guint position;

      if (g_ptr_array_find_with_equal_func (containers, container, compare_by_id, &position))
        {
          g_ptr_array_index (priv->containers, i-1) = g_object_ref (g_ptr_array_index (containers, position));
          g_list_model_items_changed (G_LIST_MODEL (self), i-1, 1, 1);
          continue;
        }

      ptyxis_container_provider_emit_removed (self, container);
    }

  for (guint i = 0; i < containers->len; i++)
    {
      PtyxisIpcContainer *container = g_ptr_array_index (containers, i);
      guint position;

      if (!g_ptr_array_find_with_equal_func (priv->containers, container, compare_by_id, &position))
        ptyxis_container_provider_emit_added (self, container);
    }
}

static GType
ptyxis_container_provider_get_item_type (GListModel *model)
{
  return PTYXIS_IPC_TYPE_CONTAINER;
}

static guint
ptyxis_container_provider_get_n_items (GListModel *model)
{
  PtyxisContainerProvider *self = PTYXIS_CONTAINER_PROVIDER (model);
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  return priv->containers->len;
}

static gpointer
ptyxis_container_provider_get_item (GListModel *model,
                                    guint       position)
{
  PtyxisContainerProvider *self = PTYXIS_CONTAINER_PROVIDER (model);
  PtyxisContainerProviderPrivate *priv = ptyxis_container_provider_get_instance_private (self);

  if (position < priv->containers->len)
    return g_object_ref (g_ptr_array_index (priv->containers, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ptyxis_container_provider_get_item_type;
  iface->get_n_items = ptyxis_container_provider_get_n_items;
  iface->get_item = ptyxis_container_provider_get_item;
}
