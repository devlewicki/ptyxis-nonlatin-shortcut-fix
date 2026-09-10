/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkHSLA GdkHSLA;

struct _GdkHSLA {
  float hue;
  float saturation;
  float lightness;
  float alpha;
};

void            _gdk_hsla_init_from_rgba    (GdkHSLA          *hsla,
                                             const GdkRGBA    *rgba);
void            _gdk_rgba_init_from_hsla    (GdkRGBA          *rgba,
                                             const GdkHSLA    *hsla);
void            _gdk_hsla_shade             (GdkHSLA          *dest,
                                             const GdkHSLA    *src,
                                             float             factor);

static inline GdkRGBA
_gdk_rgba_shade (const GdkRGBA *color,
                 float          factor)
{
  GdkHSLA hsla;
  GdkRGBA color_return;

  _gdk_hsla_init_from_rgba (&hsla, color);
  _gdk_hsla_shade (&hsla, &hsla, factor);
  _gdk_rgba_init_from_hsla (&color_return, &hsla);

  return color_return;
}

G_END_DECLS
