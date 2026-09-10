/* ptyxis-shortcut-accel-dialog.h
 *
 * Copyright 2017-2023 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_SHORTCUT_ACCEL_DIALOG (ptyxis_shortcut_accel_dialog_get_type())

G_DECLARE_FINAL_TYPE (PtyxisShortcutAccelDialog, ptyxis_shortcut_accel_dialog, PTYXIS, SHORTCUT_ACCEL_DIALOG, AdwDialog)

GtkWidget  *ptyxis_shortcut_accel_dialog_new                     (void);
char       *ptyxis_shortcut_accel_dialog_dup_accelerator         (PtyxisShortcutAccelDialog *self);
void        ptyxis_shortcut_accel_dialog_set_accelerator         (PtyxisShortcutAccelDialog *self,
                                                                  const char                *accelerator);
const char *ptyxis_shortcut_accel_dialog_get_shortcut_title      (PtyxisShortcutAccelDialog *self);
void        ptyxis_shortcut_accel_dialog_set_shortcut_title      (PtyxisShortcutAccelDialog *self,
                                                                  const char                *title);
const char *ptyxis_shortcut_accel_dialog_get_default_accelerator (PtyxisShortcutAccelDialog *self);
void        ptyxis_shortcut_accel_dialog_set_default_accelerator (PtyxisShortcutAccelDialog *self,
                                                                  const char                *default_accelerator);

G_END_DECLS

