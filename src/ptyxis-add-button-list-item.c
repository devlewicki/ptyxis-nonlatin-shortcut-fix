/* ptyxis-add-button-list-item.c
 *
 * Copyright 2025 Marco Mastropaolo <marco@mastropaolo.com>
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

#include "ptyxis-add-button-list-item.h"

struct _PtyxisAddButtonListItem
{
  GObject  parent_instance;

  GObject *item;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (PtyxisAddButtonListItem, ptyxis_add_button_list_item, G_TYPE_OBJECT)

static void
ptyxis_add_button_list_item_dispose (GObject *object)
{
  PtyxisAddButtonListItem *self = PTYXIS_ADD_BUTTON_LIST_ITEM (object);

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ptyxis_add_button_list_item_parent_class)->dispose (object);
}

static void
ptyxis_add_button_list_item_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  PtyxisAddButtonListItem *self = PTYXIS_ADD_BUTTON_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_add_button_list_item_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  PtyxisAddButtonListItem *self = PTYXIS_ADD_BUTTON_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_set_object (&self->item, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_add_button_list_item_class_init (PtyxisAddButtonListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_add_button_list_item_dispose;
  object_class->get_property = ptyxis_add_button_list_item_get_property;
  object_class->set_property = ptyxis_add_button_list_item_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ptyxis_add_button_list_item_init (PtyxisAddButtonListItem *self)
{
}

PtyxisAddButtonListItem *
ptyxis_add_button_list_item_new (GObject *item)
{
  return g_object_new (PTYXIS_TYPE_ADD_BUTTON_LIST_ITEM,
                       "item", item,
                       NULL);
}

gpointer
ptyxis_add_button_list_item_get_item (PtyxisAddButtonListItem *self)
{
  g_return_val_if_fail (PTYXIS_IS_ADD_BUTTON_LIST_ITEM (self), NULL);

  return self->item;
}

gboolean
ptyxis_add_button_list_item_is_add_button (PtyxisAddButtonListItem *self)
{
  g_return_val_if_fail (PTYXIS_IS_ADD_BUTTON_LIST_ITEM (self), FALSE);

  return self->item == NULL;
}
