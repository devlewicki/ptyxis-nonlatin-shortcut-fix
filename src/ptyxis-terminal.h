/*
 * ptyxis-terminal.h
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

#include <vte/vte.h>

#include "ptyxis-palette.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_TERMINAL (ptyxis_terminal_get_type())

G_DECLARE_FINAL_TYPE (PtyxisTerminal, ptyxis_terminal, PTYXIS, TERMINAL, VteTerminal)

PtyxisPalette *ptyxis_terminal_get_palette                   (PtyxisTerminal *self);
void           ptyxis_terminal_set_palette                   (PtyxisTerminal *self,
                                                              PtyxisPalette  *palette);
const char    *ptyxis_terminal_get_current_container_name    (PtyxisTerminal *self);
const char    *ptyxis_terminal_get_current_container_runtime (PtyxisTerminal *self);
char          *ptyxis_terminal_dup_current_directory_uri     (PtyxisTerminal *self);
char          *ptyxis_terminal_dup_current_file_uri          (PtyxisTerminal *self);
gboolean       ptyxis_terminal_can_paste                     (PtyxisTerminal *self);
void           ptyxis_terminal_paste                         (PtyxisTerminal *self);
void           ptyxis_terminal_reset_for_size                (PtyxisTerminal *self);
void           ptyxis_terminal_update_custom_links_list      (PtyxisTerminal *self,
                                                              GListModel     *custom_links);

G_END_DECLS
