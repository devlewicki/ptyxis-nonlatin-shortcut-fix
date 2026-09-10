/*
 * ptyxis-find-bar.h
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

#pragma once

#include <gtk/gtk.h>

#include "ptyxis-terminal.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_FIND_BAR (ptyxis_find_bar_get_type())

G_DECLARE_FINAL_TYPE (PtyxisFindBar, ptyxis_find_bar, PTYXIS, FIND_BAR, GtkWidget)

PtyxisTerminal *ptyxis_find_bar_get_terminal (PtyxisFindBar  *self);
void            ptyxis_find_bar_set_terminal (PtyxisFindBar  *self,
                                              PtyxisTerminal *terminal);

G_END_DECLS
