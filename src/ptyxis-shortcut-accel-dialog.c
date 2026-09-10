/* ptyxis-shortcut-accel-dialog.c
 *
 * Copyright (C) 2016 Endless, Inc
 *           (C) 2017 Christian Hergert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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
 * Authors: Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *          Christian Hergert <chergert@redhat.com>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "ptyxis-shortcut-accel-dialog.h"

struct _PtyxisShortcutAccelDialog
{
  AdwDialog             parent_instance;

  GtkButton            *accept_button;
  GtkButton            *reset_button;
  GtkStack             *stack;
  GtkLabel             *display_label;
  GtkShortcutLabel     *display_shortcut;
  GtkLabel             *selection_label;

  char                 *shortcut_title;
  char                 *default_accelerator;

  guint                 keyval;
  GdkModifierType       modifier;

  guint                 first_modifier;

  guint                 editing : 1;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_SHORTCUT_TITLE,
  PROP_DEFAULT_ACCELERATOR,
  N_PROPS
};

enum {
  SHORTCUT_SET,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (PtyxisShortcutAccelDialog, ptyxis_shortcut_accel_dialog, ADW_TYPE_DIALOG)

static void
ptyxis_shortcut_accel_dialog_update_reset_button (PtyxisShortcutAccelDialog *self);

static gboolean
ptyxis_shortcut_accel_dialog_is_editing (PtyxisShortcutAccelDialog *self)
{
  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  return self->editing;
}

static void
ptyxis_shortcut_accel_dialog_apply_state (PtyxisShortcutAccelDialog *self)
{
  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (self->editing)
    {
      gtk_stack_set_visible_child_name (self->stack, "selection");
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", FALSE);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "display");
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", TRUE);
    }
}

static GdkModifierType
sanitize_modifier_mask (GdkModifierType mods)
{
  mods &= gtk_accelerator_get_default_mod_mask ();
  mods &= ~GDK_LOCK_MASK;

  return mods;
}

static gboolean
should_drop_shift (guint keyval_was,
                   guint keyval_is)
{
  /* Allow use of shift+arrow. See #55 */
  if (keyval_was == GDK_KEY_Left ||
      keyval_was == GDK_KEY_Right ||
      keyval_was == GDK_KEY_Up ||
      keyval_was == GDK_KEY_Down ||
      keyval_is == GDK_KEY_Left ||
      keyval_is == GDK_KEY_Right ||
      keyval_is == GDK_KEY_Up ||
      keyval_is == GDK_KEY_Down)
    return FALSE;

  if (keyval_was == keyval_is)
    return TRUE;

  return FALSE;
}

static gboolean
skip_keycode (guint keycode)
{
  /* macbook fn key */
  return keycode == 0x01D8;
}

static gboolean
ptyxis_shortcut_accel_dialog_key_pressed (GtkWidget             *widget,
                                          guint                  keyval,
                                          guint                  keycode,
                                          GdkModifierType        state,
                                          GtkEventControllerKey *controller)
{
  PtyxisShortcutAccelDialog *self = (PtyxisShortcutAccelDialog *)widget;

  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

  if (skip_keycode (keycode))
    return GDK_EVENT_PROPAGATE;

  if (ptyxis_shortcut_accel_dialog_is_editing (self))
    {
      GdkEvent *key = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
      GdkModifierType real_mask;
      guint keyval_lower;

      if (gdk_key_event_is_modifier (key))
        {
          if (self->keyval == 0 && self->modifier == 0)
            self->first_modifier = keyval;
          return GDK_EVENT_PROPAGATE;
        }

      real_mask = state & gtk_accelerator_get_default_mod_mask ();
      keyval_lower = gdk_keyval_to_lower (keyval);

      /* Normalize <Tab> */
      if (keyval_lower == GDK_KEY_ISO_Left_Tab)
        keyval_lower = GDK_KEY_Tab;

      /* Put shift back if it changed the case of the key */
      if (keyval_lower != keyval)
        real_mask |= GDK_SHIFT_MASK;

      /* We don't want to use SysRq as a keybinding but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      if (keyval_lower == GDK_KEY_Sys_Req && (real_mask & GDK_ALT_MASK) != 0)
        keyval_lower = GDK_KEY_Print;

      /* A single Escape press cancels the editing */
      if (!gdk_key_event_is_modifier (key) &&
          real_mask == 0 &&
          keyval_lower == GDK_KEY_Escape)
        {
          adw_dialog_close (ADW_DIALOG (self));
          return GDK_EVENT_STOP;
        }

      /* Backspace disables the current shortcut */
      if (real_mask == 0 && keyval_lower == GDK_KEY_BackSpace)
        {
          ptyxis_shortcut_accel_dialog_set_accelerator (self, NULL);
          gtk_widget_activate_action (GTK_WIDGET (self), "shortcut.set", NULL);
          return GDK_EVENT_STOP;
        }

      self->keyval = gdk_keyval_to_lower (keyval);
      self->modifier = sanitize_modifier_mask (state);

      if ((state & GDK_SHIFT_MASK) != 0 && should_drop_shift (self->keyval, keyval))
        self->modifier &= ~GDK_SHIFT_MASK;

      if ((state & GDK_LOCK_MASK) == 0 &&
          self->keyval != keyval)
        self->modifier |= GDK_SHIFT_MASK;

      if (self->keyval == GDK_KEY_ISO_Left_Tab && self->modifier == GDK_CONTROL_MASK)
        {
          self->keyval = GDK_KEY_Tab;
          self->modifier = GDK_CONTROL_MASK | GDK_SHIFT_MASK;
        }

      self->editing = FALSE;

      ptyxis_shortcut_accel_dialog_apply_state (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
      ptyxis_shortcut_accel_dialog_update_reset_button (self);

      gtk_widget_grab_focus (GTK_WIDGET (self->accept_button));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ptyxis_shortcut_accel_dialog_key_released (GtkWidget             *widget,
                                           guint                  keyval,
                                           guint                  keycode,
                                           GdkModifierType        state,
                                           GtkEventControllerKey *controller)
{
  PtyxisShortcutAccelDialog *self = (PtyxisShortcutAccelDialog *)widget;

  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

  if (skip_keycode (keycode))
    return;

  if (self->editing)
    {
      GdkEvent *key = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
      /*
       * If we have a chord defined and there was no modifier,
       * then any key release should be enough for us to cancel
       * our grab.
       */
      if (self->modifier == 0)
        {
          self->editing = FALSE;
          ptyxis_shortcut_accel_dialog_apply_state (self);
          return;
        }

      /*
       * If we started our sequence with a modifier, we want to
       * release our grab when that modifier has been released.
       */
      if (gdk_key_event_is_modifier (key) &&
          self->keyval != 0 &&
          self->first_modifier != 0 &&
          self->first_modifier == keyval)
        {
          self->editing = FALSE;
          self->first_modifier = 0;
          ptyxis_shortcut_accel_dialog_apply_state (self);
          return;
        }
    }
}

static void
shortcut_set_cb (PtyxisShortcutAccelDialog *self)
{
      g_signal_emit (self, signals [SHORTCUT_SET], 0,
                     ptyxis_shortcut_accel_dialog_dup_accelerator (self));

  adw_dialog_close (ADW_DIALOG (self));
}

static void
shortcut_reset_cb (PtyxisShortcutAccelDialog *self)
{
  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (self->default_accelerator != NULL)
    {
      ptyxis_shortcut_accel_dialog_set_accelerator (self, self->default_accelerator);
      g_signal_emit (self, signals [SHORTCUT_SET], 0, self->default_accelerator);
      adw_dialog_close (ADW_DIALOG (self));
    }
}

static gboolean
equal_normalized (const char *current_accel,
                  const char *default_accel)
{
  GdkModifierType current_state, default_state;
  guint current_keyval, default_keyval;

  if (current_accel == NULL && default_accel == NULL)
    return TRUE;

  if (current_accel == NULL || default_accel == NULL)
    return FALSE;

  if (!gtk_accelerator_parse (current_accel, &current_keyval, &current_state))
    return FALSE;

  if (!gtk_accelerator_parse (default_accel, &default_keyval, &default_state))
    return FALSE;

  return current_keyval == default_keyval && current_state == default_state;
}

static void
ptyxis_shortcut_accel_dialog_update_reset_button (PtyxisShortcutAccelDialog *self)
{
  g_autofree char *current_accelerator = NULL;
  gboolean visible;

  g_assert (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (self->reset_button == NULL)
    return;

  current_accelerator = ptyxis_shortcut_accel_dialog_dup_accelerator (self);
  visible = self->editing && !equal_normalized (current_accelerator, self->default_accelerator);
  gtk_widget_set_visible (GTK_WIDGET (self->reset_button), visible);
}

static void
ptyxis_shortcut_accel_dialog_constructed (GObject *object)
{
  PtyxisShortcutAccelDialog *self = (PtyxisShortcutAccelDialog *)object;

  G_OBJECT_CLASS (ptyxis_shortcut_accel_dialog_parent_class)->constructed (object);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", FALSE);
  ptyxis_shortcut_accel_dialog_update_reset_button (self);
}

static void
ptyxis_shortcut_accel_dialog_finalize (GObject *object)
{
  PtyxisShortcutAccelDialog *self = (PtyxisShortcutAccelDialog *)object;

  g_clear_pointer (&self->shortcut_title, g_free);
  g_clear_pointer (&self->default_accelerator, g_free);

  G_OBJECT_CLASS (ptyxis_shortcut_accel_dialog_parent_class)->finalize (object);
}

static void
ptyxis_shortcut_accel_dialog_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  PtyxisShortcutAccelDialog *self = PTYXIS_SHORTCUT_ACCEL_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_take_string (value, ptyxis_shortcut_accel_dialog_dup_accelerator (self));
      break;

    case PROP_SHORTCUT_TITLE:
      g_value_set_string (value, ptyxis_shortcut_accel_dialog_get_shortcut_title (self));
      break;

    case PROP_DEFAULT_ACCELERATOR:
      g_value_set_string (value, ptyxis_shortcut_accel_dialog_get_default_accelerator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_shortcut_accel_dialog_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PtyxisShortcutAccelDialog *self = PTYXIS_SHORTCUT_ACCEL_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      ptyxis_shortcut_accel_dialog_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_SHORTCUT_TITLE:
      ptyxis_shortcut_accel_dialog_set_shortcut_title (self, g_value_get_string (value));
      break;

    case PROP_DEFAULT_ACCELERATOR:
      ptyxis_shortcut_accel_dialog_set_default_accelerator (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_shortcut_accel_dialog_class_init (PtyxisShortcutAccelDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ptyxis_shortcut_accel_dialog_constructed;
  object_class->finalize = ptyxis_shortcut_accel_dialog_finalize;
  object_class->get_property = ptyxis_shortcut_accel_dialog_get_property;
  object_class->set_property = ptyxis_shortcut_accel_dialog_set_property;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORTCUT_TITLE] =
    g_param_spec_string ("shortcut-title",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEFAULT_ACCELERATOR] =
    g_param_spec_string ("default-accelerator",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  signals[SHORTCUT_SET] =
    g_signal_new ("shortcut-set",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-shortcut-accel-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, accept_button);
  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, reset_button);
  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, display_label);
  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, display_shortcut);
  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, selection_label);
  gtk_widget_class_bind_template_child (widget_class, PtyxisShortcutAccelDialog, stack);
  gtk_widget_class_bind_template_callback (widget_class, ptyxis_shortcut_accel_dialog_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, ptyxis_shortcut_accel_dialog_key_released);

  gtk_widget_class_install_action (widget_class, "shortcut.set", NULL, (GtkWidgetActionActivateFunc) shortcut_set_cb);
  gtk_widget_class_install_action (widget_class, "shortcut.reset", NULL, (GtkWidgetActionActivateFunc) shortcut_reset_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
ptyxis_shortcut_accel_dialog_init (PtyxisShortcutAccelDialog *self)
{
  self->editing = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property (self, "accelerator",
                          self->display_shortcut, "accelerator",
                          G_BINDING_SYNC_CREATE);

  ptyxis_shortcut_accel_dialog_update_reset_button (self);
}

char *
ptyxis_shortcut_accel_dialog_dup_accelerator (PtyxisShortcutAccelDialog *self)
{
  g_return_val_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self), NULL);

  if (self->keyval == 0)
    return NULL;

  return gtk_accelerator_name (self->keyval, self->modifier);
}

void
ptyxis_shortcut_accel_dialog_set_accelerator (PtyxisShortcutAccelDialog *self,
                                              const char                *accelerator)
{
  guint keyval;
  GdkModifierType state;

  g_return_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (accelerator == NULL)
    {
      if (self->keyval != 0 || self->modifier != 0)
        {
          self->keyval = 0;
          self->modifier = 0;
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
          ptyxis_shortcut_accel_dialog_update_reset_button (self);
        }
    }
  else if (gtk_accelerator_parse (accelerator, &keyval, &state))
    {
      if (keyval != self->keyval || state != self->modifier)
        {
          self->keyval = keyval;
          self->modifier = state;
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
          ptyxis_shortcut_accel_dialog_update_reset_button (self);
        }
    }
}

void
ptyxis_shortcut_accel_dialog_set_shortcut_title (PtyxisShortcutAccelDialog *self,
                                                 const char                *shortcut_title)
{
  g_autofree char *label = NULL;

  g_return_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (shortcut_title != NULL)
    {
      /* Translators: <b>%s</b> is used to show the provided text in bold */
      label = g_strdup_printf (_("Enter new shortcut to change <b>%s</b>."), shortcut_title);
    }

  if (g_set_str (&self->shortcut_title, shortcut_title))
    {
      gtk_label_set_label (self->selection_label, label);
      gtk_label_set_label (self->display_label, label);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHORTCUT_TITLE]);
    }
}

const char *
ptyxis_shortcut_accel_dialog_get_shortcut_title (PtyxisShortcutAccelDialog *self)
{
  g_return_val_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self), NULL);

  return self->shortcut_title;
}

const char *
ptyxis_shortcut_accel_dialog_get_default_accelerator (PtyxisShortcutAccelDialog *self)
{
  g_return_val_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self), NULL);

  return self->default_accelerator;
}

void
ptyxis_shortcut_accel_dialog_set_default_accelerator (PtyxisShortcutAccelDialog *self,
                                                      const char                *default_accelerator)
{
  g_return_if_fail (PTYXIS_IS_SHORTCUT_ACCEL_DIALOG (self));

  if (g_set_str (&self->default_accelerator, default_accelerator))
    {
      ptyxis_shortcut_accel_dialog_update_reset_button (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEFAULT_ACCELERATOR]);
    }
}

GtkWidget *
ptyxis_shortcut_accel_dialog_new (void)
{
  return g_object_new (PTYXIS_TYPE_SHORTCUT_ACCEL_DIALOG, NULL);
}

