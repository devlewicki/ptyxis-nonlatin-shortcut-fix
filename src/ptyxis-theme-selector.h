/* ptyxis-theme-selector-private.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_THEME_SELECTOR (ptyxis_theme_selector_get_type())

G_DECLARE_FINAL_TYPE (PtyxisThemeSelector, ptyxis_theme_selector, PTYXIS, THEME_SELECTOR, GtkWidget)

GtkWidget  *ptyxis_theme_selector_new             (void);
const char *ptyxis_theme_selector_get_action_name (PtyxisThemeSelector *self);
void        ptyxis_theme_selector_set_action_name (PtyxisThemeSelector *self,
                                                   const char          *action_name);

G_END_DECLS
