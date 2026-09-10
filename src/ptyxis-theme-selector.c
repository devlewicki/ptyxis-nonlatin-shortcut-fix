/* ptyxis-theme-selector.c
 *
 * Copyright 2021-2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include <adwaita.h>

#include "ptyxis-theme-selector.h"

struct _PtyxisThemeSelector
{
  GtkWidget        parent_instance;

  /* Template widgets */
  GtkWidget       *box;
  GtkToggleButton *dark;
  GtkToggleButton *light;
  GtkToggleButton *follow;

  char            *action_name;
};

G_DEFINE_TYPE (PtyxisThemeSelector, ptyxis_theme_selector, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ACTION_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ptyxis_theme_selector_new:
 *
 * Create a new #ThemeSelector.
 *
 * Returns: a newly created #PtyxisThemeSelector.
 */
GtkWidget *
ptyxis_theme_selector_new (void)
{
  return g_object_new (PTYXIS_TYPE_THEME_SELECTOR, NULL);
}

static void
on_notify_system_supports_color_schemes_cb (PtyxisThemeSelector *self,
                                            GParamSpec          *pspec,
                                            AdwStyleManager     *style_manager)
{
  gboolean visible;

  g_assert (PTYXIS_IS_THEME_SELECTOR (self));
  g_assert (ADW_IS_STYLE_MANAGER (style_manager));

  visible = adw_style_manager_get_system_supports_color_schemes (style_manager);
  gtk_widget_set_visible (GTK_WIDGET (self->follow), visible);
}

static void
on_notify_dark_cb (PtyxisThemeSelector *self,
                   GParamSpec          *pspec,
                   AdwStyleManager     *style_manager)
{
  g_assert (PTYXIS_IS_THEME_SELECTOR (self));
  g_assert (ADW_IS_STYLE_MANAGER (style_manager));

  style_manager = adw_style_manager_get_default ();

  if (adw_style_manager_get_dark (style_manager))
    gtk_widget_add_css_class (GTK_WIDGET (self), "dark");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "dark");
}

static void
ptyxis_theme_selector_dispose (GObject *object)
{
  PtyxisThemeSelector *self = (PtyxisThemeSelector *)object;

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_pointer (&self->action_name, g_free);

  G_OBJECT_CLASS (ptyxis_theme_selector_parent_class)->dispose (object);
}

static void
ptyxis_theme_selector_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PtyxisThemeSelector *self = PTYXIS_THEME_SELECTOR (object);

  switch (prop_id)
    {
    case PROP_ACTION_NAME:
      g_value_set_string (value, ptyxis_theme_selector_get_action_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_theme_selector_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PtyxisThemeSelector *self = PTYXIS_THEME_SELECTOR (object);

  switch (prop_id)
    {
    case PROP_ACTION_NAME:
      ptyxis_theme_selector_set_action_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_theme_selector_class_init (PtyxisThemeSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ptyxis_theme_selector_dispose;
  object_class->get_property = ptyxis_theme_selector_get_property;
  object_class->set_property = ptyxis_theme_selector_set_property;

  /**
   * PtyxisThemeSelector:action-name
   *
   * The name of the action activated on activation.
   */
  properties [PROP_ACTION_NAME] =
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "themeselector");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-theme-selector.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, PtyxisThemeSelector, box);
  gtk_widget_class_bind_template_child (widget_class, PtyxisThemeSelector, dark);
  gtk_widget_class_bind_template_child (widget_class, PtyxisThemeSelector, light);
  gtk_widget_class_bind_template_child (widget_class, PtyxisThemeSelector, follow);
}

static void
ptyxis_theme_selector_init (PtyxisThemeSelector *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  gboolean dark;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (style_manager,
                           "notify::system-supports-color-schemes",
                           G_CALLBACK (on_notify_system_supports_color_schemes_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (style_manager,
                           "notify::dark",
                           G_CALLBACK (on_notify_dark_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dark = adw_style_manager_get_dark (style_manager);
  self->action_name = g_strdup (dark ? "dark" : "light");

  on_notify_system_supports_color_schemes_cb (self, NULL, style_manager);
  on_notify_dark_cb (self, NULL, style_manager);
}

/**
 * ptyxis_theme_selector_get_action_name:
 * @self: a #PtyxisThemeSelector
 *
 * Gets the name of the action that will be activated.
 *
 * Returns: (transfer none): the name of the action.
 */
const char *
ptyxis_theme_selector_get_action_name (PtyxisThemeSelector *self)
{
  g_return_val_if_fail (PTYXIS_IS_THEME_SELECTOR (self), NULL);

  return self->action_name;
}

/**
 * ptyxis_theme_selector_set_action_name:
 * @self: a #PtyxisThemeSelector
 * @action_name: (transfer none): the action name.
 *
 * Sets the name of the action that will be activated.
 */
void
ptyxis_theme_selector_set_action_name (PtyxisThemeSelector *self,
                                       const char          *action_name)
{
  g_return_if_fail (PTYXIS_IS_THEME_SELECTOR (self));

  if (g_set_str (&self->action_name, action_name))
    {
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->dark), action_name);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->light), action_name);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->follow), action_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTION_NAME]);
    }
}
