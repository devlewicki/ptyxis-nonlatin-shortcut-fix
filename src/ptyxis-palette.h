/*
 * ptyxis-palette.h
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

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_PALETTE (ptyxis_palette_get_type())

typedef struct _PtyxisPaletteScarf
{
  GdkRGBA foreground;
  GdkRGBA background;
} PtyxisPaletteScarf;

#define PTYXIS_PALETTE_SCARF_VISUAL_BELL 0
#define PTYXIS_PALETTE_SCARF_SUPERUSER   1
#define PTYXIS_PALETTE_SCARF_REMOTE      2
#define PTYXIS_PALETTE_N_SCARVES         3

typedef struct _PtyxisPaletteFace
{
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA titlebar_background;
  GdkRGBA titlebar_foreground;
  GdkRGBA cursor_bg;
  GdkRGBA cursor_fg;
  GdkRGBA indexed[16];
  union {
    PtyxisPaletteScarf scarves[PTYXIS_PALETTE_N_SCARVES];
    struct {
      PtyxisPaletteScarf visual_bell;
      PtyxisPaletteScarf superuser;
      PtyxisPaletteScarf remote;
    };
  };
} PtyxisPaletteFace;

G_DECLARE_FINAL_TYPE (PtyxisPalette, ptyxis_palette, PTYXIS, PALETTE, GObject)

GListModel              *ptyxis_palette_get_all                (void);
GListModel              *ptyxis_palette_list_model_get_default (void);
PtyxisPalette           *ptyxis_palette_lookup                 (const char     *name);
PtyxisPalette           *ptyxis_palette_new_from_file          (const char     *file,
                                                                GError        **error);
PtyxisPalette           *ptyxis_palette_new_from_resource      (const char     *file,
                                                                GError        **error);
const char              *ptyxis_palette_get_id                 (PtyxisPalette  *self);
const char              *ptyxis_palette_get_name               (PtyxisPalette  *self);
const PtyxisPaletteFace *ptyxis_palette_get_face               (PtyxisPalette  *self,
                                                                gboolean        dark);
gboolean                 ptyxis_palette_use_system_accent      (PtyxisPalette  *self);
gboolean                 ptyxis_palette_is_primary             (PtyxisPalette  *self);
gboolean                 ptyxis_palette_has_dark               (PtyxisPalette  *self);
gboolean                 ptyxis_palette_has_light              (PtyxisPalette  *self);
char                    *ptyxis_get_user_palettes_dir          (void);

G_END_DECLS
