/*
 * ptyxis-profile-dialog.c
 *
 * Copyright 2025 Mikel Olasagasti Uranga <mikel@olasagasti.info>
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

#include "ptyxis-application.h"
#include "ptyxis-profile-dialog.h"
#include "ptyxis-profile.h"
#include "ptyxis-tab.h"

struct _PtyxisProfileDialog
{
  AdwDialog     parent_instance;

  /* Owned references */
  PtyxisTab    *tab;

  /* Template Widgets */
  AdwComboRow  *profile_combo;
};

enum {
  PROP_0,
  PROP_TAB,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisProfileDialog, ptyxis_profile_dialog, ADW_TYPE_DIALOG)

static GParamSpec *properties [N_PROPS];

static void ptyxis_profile_dialog_apply_profile (PtyxisProfileDialog *self,
                                                 PtyxisProfile       *new_profile);

static guint
find_profile_index (GListModel *model,
                    const char *uuid)
{
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PtyxisProfile) profile = g_list_model_get_item (model, i);
      const char *profile_uuid = ptyxis_profile_get_uuid (profile);

      if (g_strcmp0 (profile_uuid, uuid) == 0)
        return i;
    }

  return 0;
}

static void
ptyxis_profile_dialog_selected_changed_cb (PtyxisProfileDialog *self)
{
  guint selected;
  GListModel *model;
  g_autoptr(PtyxisProfile) profile = NULL;

  g_assert (PTYXIS_IS_PROFILE_DIALOG (self));

  selected = adw_combo_row_get_selected (self->profile_combo);
  model = adw_combo_row_get_model (self->profile_combo);

  if (model != NULL && selected < g_list_model_get_n_items (model))
    {
      profile = g_list_model_get_item (model, selected);
      ptyxis_profile_dialog_apply_profile (self, profile);
    }
}

static void
ptyxis_profile_dialog_apply_profile (PtyxisProfileDialog *self,
                                     PtyxisProfile       *new_profile)
{
  g_assert (PTYXIS_IS_PROFILE_DIALOG (self));
  g_assert (PTYXIS_IS_PROFILE (new_profile));
  g_assert (PTYXIS_IS_TAB (self->tab));

  ptyxis_tab_apply_profile (self->tab, new_profile);
}

static void
ptyxis_profile_dialog_constructed (GObject *object)
{
  PtyxisProfileDialog *self = (PtyxisProfileDialog *)object;
  PtyxisApplication *app;
  g_autoptr(GListModel) profiles = NULL;
  PtyxisProfile *current_profile;
  const char *current_uuid;
  guint selected_index;

  g_assert (PTYXIS_IS_PROFILE_DIALOG (self));

  G_OBJECT_CLASS (ptyxis_profile_dialog_parent_class)->constructed (object);

  app = PTYXIS_APPLICATION_DEFAULT;
  profiles = ptyxis_application_list_profiles (app);
  adw_combo_row_set_model (self->profile_combo, profiles);

  if ((current_profile = ptyxis_tab_get_profile (self->tab)))
    {
      current_uuid = ptyxis_profile_get_uuid (current_profile);
      selected_index = find_profile_index (profiles, current_uuid);
      adw_combo_row_set_selected (self->profile_combo, selected_index);
    }

  g_signal_connect_object (self->profile_combo,
                           "notify::selected",
                           G_CALLBACK (ptyxis_profile_dialog_selected_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ptyxis_profile_dialog_dispose (GObject *object)
{
  PtyxisProfileDialog *self = (PtyxisProfileDialog *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_PROFILE_DIALOG);

  g_clear_object (&self->tab);

  G_OBJECT_CLASS (ptyxis_profile_dialog_parent_class)->dispose (object);
}

static void
ptyxis_profile_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PtyxisProfileDialog *self = PTYXIS_PROFILE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TAB:
      g_value_set_object (value, self->tab);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profile_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PtyxisProfileDialog *self = PTYXIS_PROFILE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TAB:
      self->tab = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profile_dialog_class_init (PtyxisProfileDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_profile_dialog_constructed;
  object_class->dispose = ptyxis_profile_dialog_dispose;
  object_class->get_property = ptyxis_profile_dialog_get_property;
  object_class->set_property = ptyxis_profile_dialog_set_property;

  properties[PROP_TAB] =
    g_param_spec_object ("tab", NULL, NULL,
                         PTYXIS_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-profile-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileDialog, profile_combo);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
ptyxis_profile_dialog_init (PtyxisProfileDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

