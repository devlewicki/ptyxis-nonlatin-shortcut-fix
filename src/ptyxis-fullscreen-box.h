/* ptyxis-fullscreen-box.h
 *
 * Copyright © 2021 Purism SPC
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

G_BEGIN_DECLS

#define PTYXIS_TYPE_FULLSCREEN_BOX (ptyxis_fullscreen_box_get_type())

G_DECLARE_FINAL_TYPE (PtyxisFullscreenBox, ptyxis_fullscreen_box, PTYXIS, FULLSCREEN_BOX, GtkWidget)

PtyxisFullscreenBox *ptyxis_fullscreen_box_new            (void);
gboolean             ptyxis_fullscreen_box_get_fullscreen (PtyxisFullscreenBox *self);
void                 ptyxis_fullscreen_box_set_fullscreen (PtyxisFullscreenBox *self,
                                                           gboolean             fullscreen);
gboolean             ptyxis_fullscreen_box_get_autohide   (PtyxisFullscreenBox *self);
void                 ptyxis_fullscreen_box_set_autohide   (PtyxisFullscreenBox *self,
                                                           gboolean             autohide);
GtkWidget           *ptyxis_fullscreen_box_get_content    (PtyxisFullscreenBox *self);
void                 ptyxis_fullscreen_box_set_content    (PtyxisFullscreenBox *self,
                                                           GtkWidget           *content);
void                 ptyxis_fullscreen_box_add_top_bar    (PtyxisFullscreenBox *self,
                                                           GtkWidget           *child);
void                 ptyxis_fullscreen_box_add_bottom_bar (PtyxisFullscreenBox *self,
                                                           GtkWidget           *child);
void                 ptyxis_fullscreen_box_reveal         (PtyxisFullscreenBox *self);
void                 ptyxis_fullscreen_box_unreveal       (PtyxisFullscreenBox *self);

G_END_DECLS
