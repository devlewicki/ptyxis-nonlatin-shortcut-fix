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

#include <math.h>

#include <glib/gi18n.h>

#include "ptyxis-custom-link-editor.h"

struct _PtyxisCustomLinkEditor
{
  AdwNavigationPage parent_instance;

  AdwEntryRow       *pattern;
  AdwEntryRow       *target;

  PtyxisCustomLink   *custom_link;
  PtyxisProfile     *profile;

  gboolean           unsaved_changes;
};

enum {
  PROP_0,
  PROP_PROFILE,
  PROP_CUSTOM_LINK,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisCustomLinkEditor, ptyxis_custom_link_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties[N_PROPS];

static void
ptyxis_custom_link_editor_custom_link_changed_cb (GObject *object)
{
  PtyxisCustomLinkEditor *self = (PtyxisCustomLinkEditor *)object;

  self->unsaved_changes = TRUE;
}

static void
ptyxis_custom_link_editor_constructed (GObject *object)
{
  PtyxisCustomLinkEditor *self = (PtyxisCustomLinkEditor *)object;

  g_object_bind_property (self->custom_link, "pattern",
    self->pattern, "text",
    G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self->custom_link, "target",
    self->target, "text",
    G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  
  g_signal_connect_object (self->custom_link,
    "notify::pattern",
    G_CALLBACK (ptyxis_custom_link_editor_custom_link_changed_cb),
    self,
    G_CONNECT_SWAPPED);

  g_signal_connect_object (self->custom_link,
    "notify::target",
    G_CALLBACK (ptyxis_custom_link_editor_custom_link_changed_cb),
    self,
    G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (ptyxis_custom_link_editor_parent_class)->constructed (object);
}

static void
ptyxis_custom_link_editor_dispose (GObject *object)
{
  PtyxisCustomLinkEditor *self = (PtyxisCustomLinkEditor *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_CUSTOM_LINK_EDITOR);
  
  if (self->unsaved_changes)
    ptyxis_profile_save_custom_link_changes (self->profile);

  g_clear_object (&self->profile);
  g_clear_object (&self->custom_link);

  G_OBJECT_CLASS (ptyxis_custom_link_editor_parent_class)->dispose (object);
}

static void
ptyxis_custom_link_editor_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  PtyxisCustomLinkEditor *self = PTYXIS_CUSTOM_LINK_EDITOR (object);

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
ptyxis_custom_link_editor_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  PtyxisCustomLinkEditor *self = PTYXIS_CUSTOM_LINK_EDITOR (object);

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
ptyxis_custom_link_editor_class_init (PtyxisCustomLinkEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_custom_link_editor_constructed;
  object_class->dispose = ptyxis_custom_link_editor_dispose;
  object_class->get_property = ptyxis_custom_link_editor_get_property;
  object_class->set_property = ptyxis_custom_link_editor_set_property;

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-custom-link-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PtyxisCustomLinkEditor, pattern);
  gtk_widget_class_bind_template_child (widget_class, PtyxisCustomLinkEditor, target);
}

static void
ptyxis_custom_link_editor_init (PtyxisCustomLinkEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->unsaved_changes = FALSE;
}

PtyxisCustomLinkEditor *
ptyxis_custom_link_editor_new (PtyxisProfile    *profile,
                               PtyxisCustomLink *custom_link)
{
  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (custom_link), NULL);

  return g_object_new (PTYXIS_TYPE_CUSTOM_LINK_EDITOR,
                       "profile", profile,
                       "custom-link", custom_link,
                       NULL);
}

