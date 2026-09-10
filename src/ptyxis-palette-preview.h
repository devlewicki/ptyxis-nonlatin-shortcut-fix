/* ptyxis-palette-preview.h
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

#include "ptyxis-palette.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_PALETTE_PREVIEW (ptyxis_palette_preview_get_type())

G_DECLARE_FINAL_TYPE (PtyxisPalettePreview, ptyxis_palette_preview, PTYXIS, PALETTE_PREVIEW, GtkWidget)

GtkWidget                  *ptyxis_palette_preview_new           (PtyxisPalette              *palette);
PtyxisPalette              *ptyxis_palette_preview_get_palette   (PtyxisPalettePreview       *self);
gboolean                    ptyxis_palette_preview_get_dark      (PtyxisPalettePreview       *self);
void                        ptyxis_palette_preview_set_dark      (PtyxisPalettePreview       *self,
                                                                  gboolean                    dark);
const PangoFontDescription *ptyxis_palette_preview_get_font_desc (PtyxisPalettePreview       *self);
void                        ptyxis_palette_preview_set_font_desc (PtyxisPalettePreview       *self,
                                                                  const PangoFontDescription *font_desc);

G_END_DECLS
