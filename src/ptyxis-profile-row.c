/*
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

#include <glib/gi18n.h>

#include "ptyxis-application.h"
#include "ptyxis-preferences-window.h"
#include "ptyxis-profile-editor.h"
#include "ptyxis-profile-row.h"

struct _PtyxisProfileRow
{
  AdwActionRow    parent_instance;

  GtkImage       *checkmark;

  PtyxisProfile *profile;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisProfileRow, ptyxis_profile_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
ptyxis_profile_row_duplicate (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  PtyxisProfileRow *self = (PtyxisProfileRow *)widget;
  g_autoptr(PtyxisProfile) profile = NULL;

  g_assert (PTYXIS_IS_PROFILE_ROW (self));

  profile = ptyxis_profile_duplicate (self->profile);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
ptyxis_profile_row_edit (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  AdwPreferencesWindow *window;
  PtyxisProfileEditor *editor;
  PtyxisProfileRow *self = (PtyxisProfileRow *)widget;

  g_assert (PTYXIS_IS_PROFILE_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  editor = ptyxis_profile_editor_new (self->profile);

  adw_preferences_window_pop_subpage (ADW_PREFERENCES_WINDOW (window));
  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (window),
                                       ADW_NAVIGATION_PAGE (editor));
}

static void
ptyxis_profile_row_undo_clicked_cb (AdwToast      *toast,
                                    PtyxisProfile *profile)
{
  g_assert (ADW_IS_TOAST (toast));
  g_assert (PTYXIS_IS_PROFILE (profile));

  ptyxis_application_add_profile (PTYXIS_APPLICATION_DEFAULT, profile);
}

static void
ptyxis_profile_row_remove (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *param)
{
  PtyxisProfileRow *self = (PtyxisProfileRow *)widget;
  AdwPreferencesWindow *window;
  AdwToast *toast;

  g_assert (PTYXIS_IS_PROFILE_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  toast = adw_toast_new_format (_("Removed profile “%s”"),
                                ptyxis_profile_dup_label (self->profile));
  adw_toast_set_button_label (toast, _("Undo"));
  g_signal_connect_data (toast,
                         "button-clicked",
                         G_CALLBACK (ptyxis_profile_row_undo_clicked_cb),
                         g_object_ref (self->profile),
                         (GClosureNotify)g_object_unref,
                         0);

  ptyxis_application_remove_profile (PTYXIS_APPLICATION_DEFAULT, self->profile);

  adw_preferences_window_add_toast (window, toast);
}

G_GNUC_END_IGNORE_DEPRECATIONS

static void
ptyxis_profile_row_make_default (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  PtyxisProfileRow *self = PTYXIS_PROFILE_ROW (widget);

  ptyxis_application_set_default_profile (PTYXIS_APPLICATION_DEFAULT, self->profile);
}

static void
ptyxis_profile_row_default_profile_changed_cb (PtyxisProfileRow *self,
                                               GParamSpec       *pspec,
                                               PtyxisSettings   *settings)
{
  const char *default_uuid;
  gboolean is_default;

  g_assert (PTYXIS_IS_PROFILE_ROW (self));
  g_assert (PTYXIS_IS_SETTINGS (settings));

  default_uuid = ptyxis_settings_dup_default_profile_uuid (settings);

  is_default = g_strcmp0 (default_uuid, ptyxis_profile_get_uuid (self->profile)) == 0;

  gtk_widget_set_visible (GTK_WIDGET (self->checkmark), is_default);
}

static void
ptyxis_profile_row_constructed (GObject *object)
{
  PtyxisProfileRow *self = (PtyxisProfileRow *)object;
  PtyxisApplication *app = PTYXIS_APPLICATION_DEFAULT;
  PtyxisSettings *settings = ptyxis_application_get_settings (app);

  G_OBJECT_CLASS (ptyxis_profile_row_parent_class)->constructed (object);

  g_signal_connect_object (settings,
                           "notify::default-profile-uuid",
                           G_CALLBACK (ptyxis_profile_row_default_profile_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ptyxis_profile_row_default_profile_changed_cb (self, NULL, settings);

  g_object_bind_property (self->profile, "label", self, "title",
                          G_BINDING_SYNC_CREATE);
}

static void
ptyxis_profile_row_dispose (GObject *object)
{
  PtyxisProfileRow *self = (PtyxisProfileRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_PROFILE_ROW);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (ptyxis_profile_row_parent_class)->dispose (object);
}

static void
ptyxis_profile_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PtyxisProfileRow *self = PTYXIS_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, ptyxis_profile_row_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profile_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PtyxisProfileRow *self = PTYXIS_PROFILE_ROW (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profile_row_class_init (PtyxisProfileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_profile_row_constructed;
  object_class->dispose = ptyxis_profile_row_dispose;
  object_class->get_property = ptyxis_profile_row_get_property;
  object_class->set_property = ptyxis_profile_row_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         PTYXIS_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class,
                                   "profile.duplicate",
                                   NULL,
                                   ptyxis_profile_row_duplicate);
  gtk_widget_class_install_action (widget_class,
                                   "profile.edit",
                                   NULL,
                                   ptyxis_profile_row_edit);
  gtk_widget_class_install_action (widget_class,
                                   "profile.remove",
                                   NULL,
                                   ptyxis_profile_row_remove);
  gtk_widget_class_install_action (widget_class,
                                   "profile.make-default",
                                   NULL,
                                   ptyxis_profile_row_make_default);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileRow, checkmark);
}

static void
ptyxis_profile_row_init (PtyxisProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ptyxis_profile_row_new (PtyxisProfile *profile)
{
  g_return_val_if_fail (PTYXIS_IS_PROFILE (profile), NULL);

  return g_object_new (PTYXIS_TYPE_PROFILE_ROW,
                       "profile", profile,
                       NULL);
}

PtyxisProfile *
ptyxis_profile_row_get_profile (PtyxisProfileRow *self)
{
  g_return_val_if_fail (PTYXIS_IS_PROFILE_ROW (self), NULL);

  return self->profile;
}
