/* ptyxis-shortcuts.c
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

#include <gtk/gtk.h>

#include "ptyxis-shortcuts.h"

struct _PtyxisShortcuts
{
  GObject    parent_instance;
  GSettings *settings;
};

enum {
  PROP_0,
  PROP_SETTINGS,
#define PTYXIS_SHORTCUT_DEFINE(NAME, name) PROP_##NAME,
# include "ptyxis-shortcuts.defs"
#undef PTYXIS_SHORTCUT_DEFINE
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisShortcuts, ptyxis_shortcuts, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
transform_string_to_trigger (const GValue *src_value,
                             GValue       *dest_value)
{
  const char *str = g_value_get_string (src_value);

  if (str != NULL && str[0] != 0)
    g_value_take_object (dest_value, gtk_shortcut_trigger_parse_string (str));
}

static void
ptyxis_shortcuts_settings_changed_cb (PtyxisShortcuts *self,
                                      const char      *key,
                                      GSettings       *settings)
{
  g_assert (PTYXIS_IS_SHORTCUTS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  g_object_notify (G_OBJECT (self), key);
}

static void
ptyxis_shortcuts_constructed (GObject *object)
{
  PtyxisShortcuts *self = (PtyxisShortcuts *)object;

  G_OBJECT_CLASS (ptyxis_shortcuts_parent_class)->constructed (object);

  if (self->settings == NULL)
    self->settings = g_settings_new (APP_SCHEMA_SHORTCUTS_ID);

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (ptyxis_shortcuts_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SETTINGS]);
}

static void
ptyxis_shortcuts_dispose (GObject *object)
{
  PtyxisShortcuts *self = (PtyxisShortcuts *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ptyxis_shortcuts_parent_class)->dispose (object);
}

static void
ptyxis_shortcuts_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PtyxisShortcuts *self = PTYXIS_SHORTCUTS (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

#define PTYXIS_SHORTCUT_DEFINE(NAME, name) \
    case PROP_##NAME: \
      g_value_take_string (value, g_settings_get_string (self->settings, name)); \
      break;
# include "ptyxis-shortcuts.defs"
#undef PTYXIS_SHORTCUT_DEFINE

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

#undef PROXY_PROPERTY
}

static void
ptyxis_shortcuts_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PtyxisShortcuts *self = PTYXIS_SHORTCUTS (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

#define PTYXIS_SHORTCUT_DEFINE(NAME, name) \
    case PROP_##NAME: \
      g_settings_set_string (self->settings, name, g_value_get_string (value)); \
      break;
# include "ptyxis-shortcuts.defs"
#undef PTYXIS_SHORTCUT_DEFINE

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

#undef PROXY_PROPERTY
}

static void
ptyxis_shortcuts_class_init (PtyxisShortcutsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ptyxis_shortcuts_constructed;
  object_class->dispose = ptyxis_shortcuts_dispose;
  object_class->get_property = ptyxis_shortcuts_get_property;
  object_class->set_property = ptyxis_shortcuts_set_property;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

#define PTYXIS_SHORTCUT_DEFINE(NAME, name) \
  properties[PROP_##NAME] = \
    g_param_spec_string (name, NULL, NULL, \
                         NULL, \
                         (G_PARAM_READWRITE | \
                          G_PARAM_EXPLICIT_NOTIFY | \
                          G_PARAM_STATIC_STRINGS));
# include "ptyxis-shortcuts.defs"
#undef PTYXIS_SHORTCUT_DEFINE

  g_object_class_install_properties (object_class, N_PROPS, properties);

  if (!g_value_type_transformable (G_TYPE_STRING, GTK_TYPE_SHORTCUT_TRIGGER))
    g_value_register_transform_func (G_TYPE_STRING,
                                     GTK_TYPE_SHORTCUT_TRIGGER,
                                     transform_string_to_trigger);
}

static void
ptyxis_shortcuts_init (PtyxisShortcuts *self)
{
}

PtyxisShortcuts *
ptyxis_shortcuts_new (GSettings *settings)
{
  g_return_val_if_fail (!settings || G_IS_SETTINGS (settings), NULL);

  return g_object_new (PTYXIS_TYPE_SHORTCUTS,
                       "settings", settings,
                       NULL);
}

static void
model_copy_attributes_to_item (GMenuModel *model,
                               gint        item_index,
                               GMenuItem  *item)
{
  g_autoptr(GMenuAttributeIter) iter = NULL;
  const gchar *attr_name;
  GVariant *attr_value;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (item_index >= 0);
  g_assert (G_IS_MENU_ITEM (item));

  if (!(iter = g_menu_model_iterate_item_attributes (model, item_index)))
    return;

  while (g_menu_attribute_iter_get_next (iter, &attr_name, &attr_value))
    {
      g_menu_item_set_attribute_value (item, attr_name, attr_value);
      g_variant_unref (attr_value);
    }
}

static void
model_copy_links_to_item (GMenuModel *model,
                          guint       position,
                          GMenuItem  *item)
{
  g_autoptr(GMenuLinkIter) link_iter = NULL;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (G_IS_MENU_ITEM (item));

  link_iter = g_menu_model_iterate_item_links (model, position);

  while (g_menu_link_iter_next (link_iter))
    {
      g_autoptr(GMenuModel) link_model = NULL;
      const gchar *link_name;

      link_name = g_menu_link_iter_get_name (link_iter);
      link_model = g_menu_link_iter_get_value (link_iter);

      g_menu_item_set_link (item, link_name, link_model);
    }
}

static GMenuItem *
ptyxis_shortcuts_copy_menu_item (GMenu *menu,
                                 guint  index)
{
  GMenuItem *copy;

  copy = g_menu_item_new (NULL, NULL);
  model_copy_attributes_to_item (G_MENU_MODEL (menu), index, copy);
  model_copy_links_to_item (G_MENU_MODEL (menu), index, copy);

  return copy;
}

static void
ptyxis_shortcuts_replace_key (GMenu      *menu,
                              guint       index,
                              const char *key,
                              const char *value)
{
  g_autoptr(GMenuItem) copy = NULL;

  g_assert (G_IS_MENU (menu));
  g_assert (index < g_menu_model_get_n_items (G_MENU_MODEL (menu)));
  g_assert (key != NULL);

  if (value == NULL)
    value = "";

  copy = ptyxis_shortcuts_copy_menu_item (menu, index);
  g_menu_item_set_attribute (copy, key, "s", value);

  g_menu_remove (menu, index);
  g_menu_insert_item (menu, index, copy);
}

/**
 * ptyxis_shortcuts_update_menu:
 * @self: a #PtyxisShortcuts
 * @menu: a #GMenu
 *
 * Will recursively dive into @menu and update the accel
 * property for that item so it shows up in GtkPopoverMenu.
 */
void
ptyxis_shortcuts_update_menu (PtyxisShortcuts *self,
                              GMenu           *menu)
{
  GObjectClass *klass;
  guint n_items;

  g_return_if_fail (PTYXIS_IS_SHORTCUTS (self));
  g_return_if_fail (!menu || G_IS_MENU (menu));

  if (menu == NULL)
    return;

  klass = G_OBJECT_GET_CLASS (self);
  n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  for (guint i = 0; i < n_items; i++)
    {
      g_autofree char *id = NULL;
      g_autoptr(GMenuModel) section = NULL;
      g_autoptr(GMenuModel) submenu = NULL;
      GParamSpec *pspec;

      if ((section = g_menu_model_get_item_link (G_MENU_MODEL (menu), i, G_MENU_LINK_SECTION)) && G_IS_MENU (section))
        ptyxis_shortcuts_update_menu (self, G_MENU (section));

      if ((submenu = g_menu_model_get_item_link (G_MENU_MODEL (menu), i, G_MENU_LINK_SUBMENU)) && G_IS_MENU (submenu))
        ptyxis_shortcuts_update_menu (self, G_MENU (submenu));

      if (!g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "id", "s", &id))
        continue;

      if ((pspec = g_object_class_find_property (klass, id)) &&
          G_IS_PARAM_SPEC_STRING (pspec))
        {
          g_autofree char *accel = NULL;
          g_object_get (G_OBJECT (self), id, &accel, NULL);
          ptyxis_shortcuts_replace_key (menu, i, "accel", accel);
        }
    }
}

/**
 * ptyxis_shortcuts_get_default_accelerator:
 * @shortcut_name: the name of the shortcut (property name)
 *
 * Returns the default accelerator value for the given shortcut name
 * as defined in the GSettings schema.
 *
 * Returns: (transfer full): the default accelerator string
 */
char *
ptyxis_shortcuts_get_default_accelerator (const char *shortcut_name)
{
  g_autoptr(GSettingsSchemaKey) key = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GVariant) default_value = NULL;
  GSettingsSchemaSource *source;

  g_return_val_if_fail (shortcut_name != NULL, NULL);

  source = g_settings_schema_source_get_default ();

  if (!(schema = g_settings_schema_source_lookup (source, APP_SCHEMA_SHORTCUTS_ID, TRUE)))
    return NULL;

  if (!(key = g_settings_schema_get_key (schema, shortcut_name)))
    return NULL;

  if (!(default_value = g_settings_schema_key_get_default_value (key)))
    return NULL;

  return g_variant_dup_string (default_value, NULL);
}
