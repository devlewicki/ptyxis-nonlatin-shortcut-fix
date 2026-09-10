/*
 * ptyxis-profile-editor.c
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

#include <math.h>

#include <glib/gi18n.h>

#include "ptyxis-agent-ipc.h"

#include "ptyxis-add-button-list-item.h"
#include "ptyxis-add-button-list-model.h"
#include "ptyxis-application.h"
#include "ptyxis-custom-link-editor.h"
#include "ptyxis-custom-link-row.h"
#include "ptyxis-profile-editor.h"
#include "ptyxis-preferences-list-item.h"
#include "ptyxis-util.h"

struct _PtyxisProfileEditor
{
  AdwNavigationPage  parent_instance;

  PtyxisProfile     *profile;

  AdwEntryRow       *label;
  AdwSwitchRow      *bold_is_bright;
  AdwSpinRow        *cell_height_scale;
  AdwSpinRow        *cell_width_scale;
  AdwComboRow       *containers;
  AdwSwitchRow      *use_custom_commmand;
  AdwSwitchRow      *login_shell;
  AdwSpinRow        *scrollback_lines;
  AdwSwitchRow      *limit_scrollback;
  AdwSwitchRow      *scroll_on_keystroke;
  AdwSwitchRow      *scroll_on_output;
  AdwComboRow       *exit_action;
  GListModel        *exit_actions;
  AdwComboRow       *palette;
  AdwComboRow       *preserve_container;
  GListModel        *preserve_containers;
  AdwComboRow       *preserve_directory;
  GListModel        *preserve_directories;
  AdwEntryRow       *custom_commmand;
  GtkScale          *opacity;
  GtkAdjustment     *opacity_adjustment;
  GtkLabel          *opacity_label;
  AdwSwitchRow      *use_proxy;
  AdwActionRow      *uuid_row;
  GListStore        *erase_bindings;
  AdwComboRow       *backspace_binding;
  AdwComboRow       *delete_binding;
  AdwComboRow       *cjk_ambiguous_width;
  GListStore        *cjk_ambiguous_widths;
  GtkListBox        *custom_links_list_box;
};

enum {
  PROP_0,
  PROP_PROFILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisProfileEditor, ptyxis_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties[N_PROPS];

static gboolean
ptyxis_profile_editor_spin_row_show_decimal_cb (PtyxisProfileEditor *self,
                                                AdwSpinRow          *spin)
{
  g_autofree char *text = NULL;
  double value;

  g_assert (PTYXIS_IS_PROFILE_EDITOR (self));
  g_assert (ADW_IS_SPIN_ROW (spin));

  value = adw_spin_row_get_value (spin);
  text = g_strdup_printf ("%.1f", value);
  gtk_editable_set_text (GTK_EDITABLE (spin), text);

  return TRUE;
}

static gboolean
string_to_index (GValue   *value,
                 GVariant *variant,
                 gpointer  user_data)
{
  GListModel *model = G_LIST_MODEL (user_data);
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PtyxisPreferencesListItem) item = PTYXIS_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, i));
      GVariant *item_value = ptyxis_preferences_list_item_get_value (item);

      if (g_variant_equal (variant, item_value))
        {
          g_value_set_uint (value, i);
          return TRUE;
        }
    }

  return FALSE;
}

static GVariant *
index_to_string (const GValue       *value,
                 const GVariantType *type,
                 gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  GListModel *model = G_LIST_MODEL (user_data);
  g_autoptr(PtyxisPreferencesListItem) item = PTYXIS_PREFERENCES_LIST_ITEM (g_list_model_get_item (model, index));

  if (item != NULL)
    return g_variant_ref (ptyxis_preferences_list_item_get_value (item));

  return NULL;
}

static void
ptyxis_profile_editor_uuid_copy (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  PtyxisProfileEditor *self = (PtyxisProfileEditor *)widget;
  GdkClipboard *clipboard;

  g_assert (PTYXIS_IS_PROFILE_EDITOR (self));

  clipboard = gtk_widget_get_clipboard (widget);
  gdk_clipboard_set_text (clipboard, ptyxis_profile_get_uuid (self->profile));

  gtk_widget_activate_action_variant (widget, "toast.add", ptyxis_variant_new_toast (_("Copied to clipboard"), 3));
}

static void
undo_profile_delete (AdwToast      *toast,
                     PtyxisProfile *profile)
{
  g_assert (ADW_IS_TOAST (toast));
  g_assert (PTYXIS_IS_PROFILE (profile));

  ptyxis_application_add_profile (PTYXIS_APPLICATION_DEFAULT, profile);
}

static void
ptyxis_profile_editor_profile_delete (GtkWidget  *widget,
                                      const char *action_name,
                                      GVariant   *param)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  PtyxisProfileEditor *self = (PtyxisProfileEditor *)widget;
  AdwPreferencesWindow *window;
  AdwToast *toast;

  g_assert (PTYXIS_IS_PROFILE_EDITOR (self));

  g_object_ref (self);

  window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
  toast = adw_toast_new_format (_("Removed profile “%s”"),
                                ptyxis_profile_dup_label (self->profile));
  adw_toast_set_button_label (toast, _("Undo"));
  g_signal_connect_data (toast,
                         "button-clicked",
                         G_CALLBACK (undo_profile_delete),
                         g_object_ref (self->profile),
                         (GClosureNotify)g_object_unref,
                         0);

  ptyxis_application_remove_profile (PTYXIS_APPLICATION_DEFAULT, self->profile);

  adw_preferences_window_add_toast (window, toast);

  adw_preferences_window_pop_subpage (window);

  g_object_unref (self);

G_GNUC_END_IGNORE_DEPRECATIONS
}

static char *
get_container_title (PtyxisIpcContainer *container)
{
  const char *display_name;
  const char *provider;

  g_assert (!container || PTYXIS_IPC_IS_CONTAINER (container));

  provider = ptyxis_ipc_container_get_provider (container);
  display_name = ptyxis_ipc_container_get_display_name (container);

  if (g_strcmp0 (provider, "session") == 0)
    return g_strdup (_("My Computer"));

  return g_strdup (display_name);
}

static gpointer
map_container_to_list_item (gpointer item,
                            gpointer user_data)
{
  g_autoptr(PtyxisIpcContainer) container = PTYXIS_IPC_CONTAINER (item);
  g_autoptr(GVariant) value = g_variant_take_ref (g_variant_new_string (ptyxis_ipc_container_get_id (container)));

  return g_object_new (PTYXIS_TYPE_PREFERENCES_LIST_ITEM,
                       "title", ptyxis_ipc_container_get_display_name (container),
                       "value", value,
                       NULL);
}

static gboolean
opacity_to_label (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  double opacity;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_DOUBLE (from_value));
  g_assert (G_VALUE_HOLDS_STRING (to_value));

  opacity = g_value_get_double (from_value);
  g_value_take_string (to_value, g_strdup_printf ("%3.0lf%%", floor (100.*opacity)));

  return TRUE;
}

static GtkWidget *
ptyxis_profile_editor_create_custom_link_row_cb (gpointer item,
                                                 gpointer user_data)
{
  PtyxisAddButtonListItem *add_button_item = PTYXIS_ADD_BUTTON_LIST_ITEM (item);
  GObject *wrapped_item;

  g_assert (PTYXIS_IS_ADD_BUTTON_LIST_ITEM (add_button_item));

  if ((wrapped_item = ptyxis_add_button_list_item_get_item (add_button_item)))
    {
      g_autoptr(PtyxisProfile) profile = ptyxis_application_dup_default_profile (PTYXIS_APPLICATION_DEFAULT);

      return ptyxis_custom_link_row_new (profile, PTYXIS_CUSTOM_LINK (wrapped_item));
    }

  return g_object_new (ADW_TYPE_BUTTON_ROW,
                       "title", _("Add Link"),
                       "start-icon-name", "list-add-symbolic",
                       "action-name", "custom-link.add",
                       NULL);
}

static void
ptyxis_profile_editor_constructed (GObject *object)
{
  PtyxisProfileEditor *self = (PtyxisProfileEditor *)object;
  PtyxisApplication *app = PTYXIS_APPLICATION_DEFAULT;
  g_autoptr(GSettings) gsettings = NULL;
  g_autoptr(GListModel) containers = NULL;
  g_autoptr(GtkMapListModel) mapped_containers = NULL;
  g_autoptr(GListModel) custom_links_list = NULL;
  g_autoptr(PtyxisAddButtonListModel) custom_links_list_model = NULL;

  G_OBJECT_CLASS (ptyxis_profile_editor_parent_class)->constructed (object);

  containers = ptyxis_application_list_containers (app);
  mapped_containers = gtk_map_list_model_new (g_object_ref (containers),
                                              map_container_to_list_item,
                                              NULL, NULL);

  adw_combo_row_set_model (self->containers, containers);

  adw_combo_row_set_model (self->palette,
                           ptyxis_palette_list_model_get_default ());

  gsettings = ptyxis_profile_dup_settings (self->profile);

  g_object_bind_property (self->profile, "uuid",
                          self->uuid_row, "subtitle",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->profile, "label",
                          self->label, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "limit-scrollback",
                          self->limit_scrollback, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scrollback-lines",
                          self->scrollback_lines, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scroll-on-keystroke",
                          self->scroll_on_keystroke, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "scroll-on-output",
                          self->scroll_on_output, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "bold-is-bright",
                          self->bold_is_bright, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "cell-height-scale",
                          self->cell_height_scale, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "cell-width-scale",
                          self->cell_width_scale, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "login-shell",
                          self->login_shell, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "use-custom-command",
                          self->use_custom_commmand, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "use-proxy",
                          self->use_proxy, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "custom-command",
                          self->custom_commmand, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->profile, "opacity",
                          self->opacity_adjustment, "value",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property_full (self->profile, "opacity",
                               self->opacity_label, "label",
                               G_BINDING_SYNC_CREATE,
                               opacity_to_label, NULL, NULL, NULL);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_DEFAULT_CONTAINER,
                                self->containers,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (mapped_containers),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_PALETTE,
                                self->palette,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (ptyxis_palette_list_model_get_default ()),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_BACKSPACE_BINDING,
                                self->backspace_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_DELETE_BINDING,
                                self->delete_binding,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->erase_bindings),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH,
                                self->cjk_ambiguous_width,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->cjk_ambiguous_widths),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_PRESERVE_CONTAINER,
                                self->preserve_container,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->preserve_containers),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_PRESERVE_DIRECTORY,
                                self->preserve_directory,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->preserve_directories),
                                g_object_unref);

  g_settings_bind_with_mapping (gsettings,
                                PTYXIS_PROFILE_KEY_EXIT_ACTION,
                                self->exit_action,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_index,
                                index_to_string,
                                g_object_ref (self->exit_actions),
                                g_object_unref);

  custom_links_list = ptyxis_profile_list_custom_links(self->profile);
  custom_links_list_model = ptyxis_add_button_list_model_new (custom_links_list);

  gtk_list_box_bind_model (self->custom_links_list_box,
                           G_LIST_MODEL (custom_links_list_model),
                           ptyxis_profile_editor_create_custom_link_row_cb,
                           g_object_ref(self),
                           g_object_unref);
}


static void
ptyxis_profile_editor_dispose (GObject *object)
{
  PtyxisProfileEditor *self = (PtyxisProfileEditor *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_PROFILE_EDITOR);

  g_clear_object (&self->profile);

  G_OBJECT_CLASS (ptyxis_profile_editor_parent_class)->dispose (object);
}

static void
ptyxis_profile_editor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PtyxisProfileEditor *self = PTYXIS_PROFILE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROFILE:
      g_value_set_object (value, ptyxis_profile_editor_get_profile (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profile_editor_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PtyxisProfileEditor *self = PTYXIS_PROFILE_EDITOR (object);

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
ptyxis_profile_editor_add_custom_link (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *param)
{
  PtyxisProfileEditor *self = (PtyxisProfileEditor *)widget;
  g_autoptr(PtyxisCustomLink) custom_link = NULL;

  g_assert (PTYXIS_IS_PROFILE_EDITOR (self));

  custom_link = ptyxis_custom_link_new ();
  ptyxis_profile_add_custom_link (self->profile, custom_link);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS {
    AdwPreferencesWindow *window;
    PtyxisCustomLinkEditor *editor;

    window = ADW_PREFERENCES_WINDOW (gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_WINDOW));
    editor = ptyxis_custom_link_editor_new (self->profile, custom_link);

    adw_preferences_window_push_subpage (window, ADW_NAVIGATION_PAGE (editor));
  } G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
ptyxis_profile_editor_class_init (PtyxisProfileEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_profile_editor_constructed;
  object_class->dispose = ptyxis_profile_editor_dispose;
  object_class->get_property = ptyxis_profile_editor_get_property;
  object_class->set_property = ptyxis_profile_editor_set_property;

  properties[PROP_PROFILE] =
    g_param_spec_object ("profile", NULL, NULL,
                         PTYXIS_TYPE_PROFILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-profile-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, backspace_binding);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, bold_is_bright);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, cell_height_scale);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, cell_width_scale);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, cjk_ambiguous_width);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, cjk_ambiguous_widths);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, containers);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, custom_commmand);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, custom_links_list_box);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, delete_binding);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, erase_bindings);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, exit_action);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, exit_actions);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, label);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, limit_scrollback);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, login_shell);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, opacity);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, opacity_adjustment);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, opacity_label);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, palette);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, preserve_containers);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, preserve_container);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, preserve_directories);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, preserve_directory);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, scroll_on_keystroke);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, scroll_on_output);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, scrollback_lines);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, use_custom_commmand);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, use_proxy);
  gtk_widget_class_bind_template_child (widget_class, PtyxisProfileEditor, uuid_row);

  gtk_widget_class_bind_template_callback (widget_class, get_container_title);
  gtk_widget_class_bind_template_callback (widget_class, ptyxis_profile_editor_spin_row_show_decimal_cb);

  gtk_widget_class_install_action (widget_class, "uuid.copy", NULL, ptyxis_profile_editor_uuid_copy);
  gtk_widget_class_install_action (widget_class, "profile.delete", NULL, ptyxis_profile_editor_profile_delete);
  gtk_widget_class_install_action (widget_class, "custom-link.add", NULL, ptyxis_profile_editor_add_custom_link);
}

static void
ptyxis_profile_editor_init (PtyxisProfileEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

PtyxisProfileEditor *
ptyxis_profile_editor_new (PtyxisProfile *profile)
{
  g_return_val_if_fail (PTYXIS_IS_PROFILE (profile), NULL);

  return g_object_new (PTYXIS_TYPE_PROFILE_EDITOR,
                       "profile", profile,
                       NULL);
}

PtyxisProfile *
ptyxis_profile_editor_get_profile (PtyxisProfileEditor *self)
{
  g_return_val_if_fail (PTYXIS_IS_PROFILE_EDITOR (self), NULL);

  return self->profile;
}
