/*
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

#include "config.h"

#include <glib/gi18n.h>

#include "ptyxis-custom-link-editor.h"
#include "ptyxis-custom-link-row.h"

struct _PtyxisCustomLinkRow
{
  AdwActionRow    parent_instance;

  PtyxisProfile   *profile;
  PtyxisCustomLink *custom_link;
};

enum {
  PROP_0,
  PROP_PROFILE,
  PROP_CUSTOM_LINK,
  N_PROPS
};


typedef struct
{
  PtyxisProfile     *profile;
  PtyxisCustomLink  *custom_link;
  guint              index;
} RemovedCustomlinkUndoInfo;

G_DEFINE_FINAL_TYPE (PtyxisCustomLinkRow, ptyxis_custom_link_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

/* Methods for RemovedCustomlinkUndoInfo helper object */

static void 
removed_custom_link_undo_info_free (RemovedCustomlinkUndoInfo *info)
{
  g_object_unref (info->custom_link);
  g_object_unref (info->profile);
  g_free (info);
}

static RemovedCustomlinkUndoInfo * 
removed_custom_link_undo_info_new (PtyxisProfile    *profile,
                                   PtyxisCustomLink *custom_link,
                                   guint             index)
{
  RemovedCustomlinkUndoInfo *info = g_new0 (RemovedCustomlinkUndoInfo, 1);

  info->custom_link = g_object_ref (custom_link);
  info->profile = g_object_ref (profile);
  info->index = index;

  return info;
}

/* Methods for PtyxisCustomLinkRow */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
ptyxis_custom_link_row_edit (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  AdwPreferencesWindow *window;
  PtyxisCustomLinkEditor *editor;
  PtyxisCustomLinkRow *self = PTYXIS_CUSTOM_LINK_ROW (widget);

  g_assert (PTYXIS_IS_CUSTOM_LINK_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  editor = ptyxis_custom_link_editor_new (self->profile, self->custom_link);

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (window),
                                       ADW_NAVIGATION_PAGE (editor));
}

static void
ptyxis_custom_link_row_undo_clicked_cb (AdwToast                 *toast,
                                        RemovedCustomlinkUndoInfo *info)
{
  g_assert (ADW_IS_TOAST (toast));

  ptyxis_profile_undo_remove_custom_link (info->profile,
                                          info->custom_link,
                                          info->index);

  ptyxis_profile_save_custom_link_changes (info->profile);
}

static void
ptyxis_custom_link_row_remove (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *param)
{
  PtyxisCustomLinkRow *self = (PtyxisCustomLinkRow *)widget;
  AdwPreferencesWindow *window;
  AdwToast *toast;
  guint index;

  g_assert (PTYXIS_IS_CUSTOM_LINK_ROW (self));

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));

  if (ptyxis_profile_remove_custom_link (self->profile, self->custom_link, &index))
    {
      ptyxis_profile_save_custom_link_changes (self->profile);

      toast = adw_toast_new_format (_("Removed custom link “%s”"),
      ptyxis_custom_link_dup_pattern (self->custom_link));
      adw_toast_set_button_label (toast, _("Undo"));

      g_signal_connect_data (toast,
                             "button-clicked",
                             G_CALLBACK (ptyxis_custom_link_row_undo_clicked_cb),
                             removed_custom_link_undo_info_new (self->profile,
                                                                self->custom_link,
                                                                index),
                             (GClosureNotify)removed_custom_link_undo_info_free,
                             0);

      adw_preferences_window_add_toast (window, toast);
    }
}

G_GNUC_END_IGNORE_DEPRECATIONS

static void
ptyxis_custom_link_row_constructed (GObject *object)
{
  PtyxisCustomLinkRow *self = (PtyxisCustomLinkRow *)(object);

  G_OBJECT_CLASS (ptyxis_custom_link_row_parent_class)->constructed (object);

  g_object_bind_property (self->custom_link, "pattern", self, "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->custom_link, "target", self, "subtitle",
                          G_BINDING_SYNC_CREATE);
}

static void
ptyxis_custom_link_row_dispose (GObject *object)
{
  PtyxisCustomLinkRow *self = (PtyxisCustomLinkRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_CUSTOM_LINK_ROW);

  g_clear_object (&self->custom_link);
  g_clear_object (&self->profile);

  G_OBJECT_CLASS (ptyxis_custom_link_row_parent_class)->dispose (object);
}

static void
ptyxis_custom_link_row_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  PtyxisCustomLinkRow *self = PTYXIS_CUSTOM_LINK_ROW (object);

  switch (prop_id)
    {
    case PROP_CUSTOM_LINK:
      g_value_set_object (value, self->custom_link);
      break;

    case PROP_PROFILE:
      g_value_set_object (value, self->profile);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_custom_link_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  PtyxisCustomLinkRow *self = PTYXIS_CUSTOM_LINK_ROW (object);

  switch (prop_id)
    {
    case PROP_CUSTOM_LINK:
      self->custom_link = g_value_dup_object (value);
      break;
  
    case PROP_PROFILE:
      self->profile = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_custom_link_row_class_init (PtyxisCustomLinkRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_custom_link_row_constructed;
  object_class->dispose = ptyxis_custom_link_row_dispose;
  object_class->get_property = ptyxis_custom_link_row_get_property;
  object_class->set_property = ptyxis_custom_link_row_set_property;

  properties[PROP_CUSTOM_LINK] =
    g_param_spec_object ("custom-link", NULL, NULL,
                         PTYXIS_TYPE_CUSTOM_LINK,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         PTYXIS_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class,
                                   "custom_link.edit",
                                   NULL,
                                   ptyxis_custom_link_row_edit);
  gtk_widget_class_install_action (widget_class,
                                   "custom_link.remove",
                                   NULL,
                                   ptyxis_custom_link_row_remove);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-custom-link-row.ui");
}

static void
ptyxis_custom_link_row_init (PtyxisCustomLinkRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ptyxis_custom_link_row_new (PtyxisProfile    *profile,
                            PtyxisCustomLink *custom_link)
{
  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (custom_link), NULL);

  return g_object_new (PTYXIS_TYPE_CUSTOM_LINK_ROW,
                       "profile", profile,
                       "custom-link", custom_link,
                       NULL);
}

