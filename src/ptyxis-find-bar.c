/*
 * ptyxis-find-bar.c
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

#include "ptyxis-find-bar.h"
#include "ptyxis-util.h"

struct _PtyxisFindBar
{
  GtkWidget        parent_instance;

  PtyxisTerminal  *terminal;

  GtkEntry        *entry;
  GtkCheckButton  *use_regex;
  GtkCheckButton  *whole_words;
  GtkCheckButton  *match_case;
};

enum {
  PROP_0,
  PROP_TERMINAL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisFindBar, ptyxis_find_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ptyxis_find_bar_dismiss (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  PtyxisFindBar *self = (PtyxisFindBar *)widget;
  GtkWidget *revealer;

  g_assert (PTYXIS_IS_FIND_BAR (self));

  if ((revealer = gtk_widget_get_ancestor (widget, GTK_TYPE_REVEALER)))
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);

  if (self->terminal != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static gboolean
ptyxis_find_bar_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (PTYXIS_FIND_BAR (widget)->entry));
}

static char *
ptyxis_find_bar_get_search (PtyxisFindBar *self,
                            guint         *flags)
{
  g_autofree char *escaped = NULL;
  g_autofree char *boundaries = NULL;
  const char *text;

  g_assert (PTYXIS_IS_FIND_BAR (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  if (ptyxis_str_empty0 (text))
    return NULL;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->match_case)))
    *flags |= VTE_PCRE2_CASELESS;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->use_regex)))
    text = escaped = g_regex_escape_string (text, -1);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->whole_words)))
    text = boundaries = g_strdup_printf ("\\b%s\\b", text);

  return g_strdup (text);
}

static void
ptyxis_find_bar_next (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  PtyxisFindBar *self = (PtyxisFindBar *)widget;

  g_assert (PTYXIS_IS_FIND_BAR (self));

  if (self->terminal != NULL)
    vte_terminal_search_find_next (VTE_TERMINAL (self->terminal));
}

static void
ptyxis_find_bar_previous (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  PtyxisFindBar *self = (PtyxisFindBar *)widget;

  g_assert (PTYXIS_IS_FIND_BAR (self));

  if (self->terminal != NULL)
    vte_terminal_search_find_previous (VTE_TERMINAL (self->terminal));
}

static void
ptyxis_find_bar_entry_changed_cb (PtyxisFindBar *self,
                                  GtkEntry      *entry)
{
  g_autofree char *query = NULL;
  g_autoptr(VteRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  guint flags = VTE_PCRE2_MULTILINE;

  g_assert (PTYXIS_IS_FIND_BAR (self));
  g_assert (GTK_IS_ENTRY (entry));
  g_assert (!self->terminal || VTE_IS_TERMINAL (self->terminal));

  if (self->terminal == NULL)
    return;

  if ((query = ptyxis_find_bar_get_search (self, &flags)))
    regex = vte_regex_new_for_search (query, -1, flags, &error);

  vte_terminal_search_set_regex (VTE_TERMINAL (self->terminal), regex, 0);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (self->terminal), TRUE);
}

static void
ptyxis_find_bar_dispose (GObject *object)
{
  PtyxisFindBar *self = (PtyxisFindBar *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), PTYXIS_TYPE_FIND_BAR);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->terminal);

  G_OBJECT_CLASS (ptyxis_find_bar_parent_class)->dispose (object);
}

static void
ptyxis_find_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PtyxisFindBar *self = PTYXIS_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      g_value_set_object (value, self->terminal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_find_bar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PtyxisFindBar *self = PTYXIS_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      if (g_set_object (&self->terminal, g_value_get_object (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_find_bar_class_init (PtyxisFindBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ptyxis_find_bar_dispose;
  object_class->get_property = ptyxis_find_bar_get_property;
  object_class->set_property = ptyxis_find_bar_set_property;

  widget_class->grab_focus = ptyxis_find_bar_grab_focus;

  properties[PROP_TERMINAL] =
    g_param_spec_object ("terminal", NULL, NULL,
                         PTYXIS_TYPE_TERMINAL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Ptyxis/ptyxis-find-bar.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "findbar");

  gtk_widget_class_bind_template_child (widget_class, PtyxisFindBar, entry);
  gtk_widget_class_bind_template_child (widget_class, PtyxisFindBar, use_regex);
  gtk_widget_class_bind_template_child (widget_class, PtyxisFindBar, whole_words);
  gtk_widget_class_bind_template_child (widget_class, PtyxisFindBar, match_case);

  gtk_widget_class_bind_template_callback (widget_class, ptyxis_find_bar_entry_changed_cb);

  gtk_widget_class_install_action (widget_class, "search.dismiss", NULL, ptyxis_find_bar_dismiss);
  gtk_widget_class_install_action (widget_class, "search.down", NULL, ptyxis_find_bar_next);
  gtk_widget_class_install_action (widget_class, "search.up", NULL, ptyxis_find_bar_previous);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "search.dismiss", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK, "search.up", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "search.down", NULL);
}

static void
ptyxis_find_bar_init (PtyxisFindBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

PtyxisTerminal *
ptyxis_find_bar_get_terminal (PtyxisFindBar *self)
{
  g_return_val_if_fail (PTYXIS_IS_FIND_BAR (self), NULL);

  return self->terminal;
}

void
ptyxis_find_bar_set_terminal (PtyxisFindBar  *self,
                              PtyxisTerminal *terminal)
{
  g_return_if_fail (PTYXIS_IS_FIND_BAR (self));
  g_return_if_fail (!terminal || PTYXIS_IS_TERMINAL (terminal));

  if (g_set_object (&self->terminal, terminal))
    {
      gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TERMINAL]);
    }
}
