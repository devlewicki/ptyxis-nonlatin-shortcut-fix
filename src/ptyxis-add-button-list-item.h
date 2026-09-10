/* ptyxis-add-button-list-item.h
 *
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_ADD_BUTTON_LIST_ITEM (ptyxis_add_button_list_item_get_type())

G_DECLARE_FINAL_TYPE (PtyxisAddButtonListItem, ptyxis_add_button_list_item, PTYXIS, ADD_BUTTON_LIST_ITEM, GObject)

PtyxisAddButtonListItem *ptyxis_add_button_list_item_new              (GObject                 *item);
gpointer                 ptyxis_add_button_list_item_get_item         (PtyxisAddButtonListItem *self);
gboolean                 ptyxis_add_button_list_item_is_add_button    (PtyxisAddButtonListItem *self);

G_END_DECLS
