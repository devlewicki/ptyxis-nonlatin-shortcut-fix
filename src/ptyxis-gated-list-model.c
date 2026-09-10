/*
 * ptyxis-gated-list-model.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#include "ptyxis-gated-list-model.h"

struct _PtyxisGatedListModel
{
  GObject parent_instance;
  GListModel *model;
  guint gated : 1;
};

enum {
  PROP_0,
  PROP_GATED,
  PROP_MODEL,
  N_PROPS
};

static GType
ptyxis_gated_list_model_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
ptyxis_gated_list_model_get_n_items (GListModel *model)
{
  PtyxisGatedListModel *self = PTYXIS_GATED_LIST_MODEL (model);

  if (self->gated || self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
ptyxis_gated_list_model_get_item (GListModel *model,
                                  guint       position)
{
  PtyxisGatedListModel *self = PTYXIS_GATED_LIST_MODEL (model);

  if (self->gated || self->model == NULL)
    return NULL;

  return g_list_model_get_item (self->model, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ptyxis_gated_list_model_get_item_type;
  iface->get_item = ptyxis_gated_list_model_get_item;
  iface->get_n_items = ptyxis_gated_list_model_get_n_items;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtyxisGatedListModel, ptyxis_gated_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
ptyxis_gated_list_model_items_changed_cb (PtyxisGatedListModel *self,
                                          guint                 position,
                                          guint                 removed,
                                          guint                 added,
                                          GListModel           *model)
{
  g_assert (PTYXIS_IS_GATED_LIST_MODEL (self));

  if (self->gated)
    return;

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
ptyxis_gated_list_model_set_model (PtyxisGatedListModel *self,
                                   GListModel           *model)
{
  guint old_n_items = 0;
  guint new_n_items = 0;

  g_assert (PTYXIS_IS_GATED_LIST_MODEL (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  if (model == self->model)
    return;

  if (model)
    {
      g_object_ref (model);
      new_n_items = g_list_model_get_n_items (model);
      g_signal_connect_object (model,
                               "items-changed",
                               G_CALLBACK (ptyxis_gated_list_model_items_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

  if (self->model)
    {
      old_n_items = g_list_model_get_n_items (self->model);
      g_signal_handlers_disconnect_by_func (self->model,
                                            G_CALLBACK (ptyxis_gated_list_model_items_changed_cb),
                                            self);
      g_clear_object (&self->model);
    }

  self->model = model;

  if (!self->gated && (old_n_items || new_n_items))
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

static void
ptyxis_gated_list_model_set_gated (PtyxisGatedListModel *self,
                                   gboolean              gated)
{
  guint old_n_items = 0;
  guint new_n_items = 0;

  g_assert (PTYXIS_IS_GATED_LIST_MODEL (self));

  gated = !!gated;

  if (gated == self->gated)
    return;

  if (self->model != NULL)
    {
      if (gated)
        old_n_items = g_list_model_get_n_items (self->model);
      else
        new_n_items = g_list_model_get_n_items (self->model);
    }

  self->gated = gated;

  if (old_n_items || new_n_items)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GATED]);
}

static void
ptyxis_gated_list_model_dispose (GObject *object)
{
  PtyxisGatedListModel *self = (PtyxisGatedListModel *)object;

  self->gated = TRUE;
  ptyxis_gated_list_model_set_model (self, NULL);

  G_OBJECT_CLASS (ptyxis_gated_list_model_parent_class)->dispose (object);
}

static void
ptyxis_gated_list_model_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  PtyxisGatedListModel *self = PTYXIS_GATED_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_GATED:
      g_value_set_boolean (value, self->gated);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_gated_list_model_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  PtyxisGatedListModel *self = PTYXIS_GATED_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ptyxis_gated_list_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_GATED:
      ptyxis_gated_list_model_set_gated (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_gated_list_model_class_init (PtyxisGatedListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_gated_list_model_dispose;
  object_class->get_property = ptyxis_gated_list_model_get_property;
  object_class->set_property = ptyxis_gated_list_model_set_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_GATED] =
    g_param_spec_boolean ("gated", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ptyxis_gated_list_model_init (PtyxisGatedListModel *self)
{
}
