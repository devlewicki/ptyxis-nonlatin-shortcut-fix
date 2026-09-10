/*
 * ptyxis-palette.c
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

#include <math.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gdkhslaprivate.h"

#include "ptyxis-palette.h"
#include "ptyxis-preferences-list-item.h"
#include "ptyxis-user-palettes.h"

typedef struct _PtyxisPaletteData
{
  const char        *id;
  const char        *name;
  PtyxisPaletteFace  faces[2];
} PtyxisPaletteData;

struct _PtyxisPalette
{
  GObject parent_instance;
  const PtyxisPaletteData *palette;
  PtyxisPaletteData *allocated;
  guint is_primary : 1;
  guint has_dark : 1;
  guint has_light : 1;
  guint use_system_accent : 1;
};

G_DEFINE_FINAL_TYPE (PtyxisPalette, ptyxis_palette, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ptyxis_palette_finalize (GObject *object)
{
  PtyxisPalette *self = PTYXIS_PALETTE (object);

  g_clear_pointer (&self->allocated, g_free);

  G_OBJECT_CLASS (ptyxis_palette_parent_class)->finalize (object);
}

static void
ptyxis_palette_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PtyxisPalette *self = PTYXIS_PALETTE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_static_string (value, self->palette->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_palette_class_init (PtyxisPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ptyxis_palette_finalize;
  object_class->get_property = ptyxis_palette_get_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ptyxis_palette_init (PtyxisPalette *self)
{
}

PtyxisPalette *
ptyxis_palette_lookup (const char *id)
{
  GListModel *model = ptyxis_palette_get_all ();
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PtyxisPalette) palette = g_list_model_get_item (model, i);

      if (g_strcmp0 (id, ptyxis_palette_get_id (palette)) == 0)
        return g_steal_pointer (&palette);
    }

  return NULL;
}

const char *
ptyxis_palette_get_id (PtyxisPalette *self)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), NULL);

  return self->palette->id;
}

const char *
ptyxis_palette_get_name (PtyxisPalette *self)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), NULL);

  return self->palette->name;
}

const PtyxisPaletteFace *
ptyxis_palette_get_face (PtyxisPalette *self,
                         gboolean       dark)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), NULL);

  return &self->palette->faces[!!dark];
}

char *
ptyxis_get_user_palettes_dir (void)
{
  return g_build_filename (g_get_user_data_dir (), APP_ID, "palettes", NULL);
}

GListModel *
ptyxis_palette_get_all (void)
{
  static GtkSortListModel *instance;

  if (instance == NULL)
    {
      g_auto(GStrv) resources = g_resources_enumerate_children ("/org/gnome/Ptyxis/palettes/", 0, NULL);
      g_autofree char *user_palettes_dir = ptyxis_get_user_palettes_dir ();
      g_autoptr(PtyxisUserPalettes) user_palettes = ptyxis_user_palettes_new (user_palettes_dir);
      GListStore *builtin = g_list_store_new (PTYXIS_TYPE_PALETTE);
      GListStore *models = g_list_store_new (G_TYPE_LIST_MODEL);
      GtkFlattenListModel *flatten = gtk_flatten_list_model_new (G_LIST_MODEL (models));

      g_list_store_append (models, builtin);

      /* If for some reason we failed to create our state directory and
       * failed to create a monitor for it, we could get NULL back for
       * the user_palettes.
       */
      if (user_palettes != NULL)
        g_list_store_append (models, user_palettes);

      for (guint i = 0; resources[i]; i++)
        {
          g_autoptr(GError) error = NULL;
          g_autofree char *path = g_strdup_printf ("/org/gnome/Ptyxis/palettes/%s", resources[i]);
          g_autoptr(PtyxisPalette) palette = ptyxis_palette_new_from_resource (path, &error);

          if (palette == NULL)
            g_critical ("Bundled palette \"%s\" is invalid: %s", resources[i], error ? error->message : "");
          else
            g_list_store_append (builtin, palette);
        }

      instance = gtk_sort_list_model_new (G_LIST_MODEL (flatten),
                                          GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (PTYXIS_TYPE_PALETTE, NULL, "name"))));
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return G_LIST_MODEL (instance);
}

static gpointer
map_func (gpointer item,
          gpointer user_data)
{
  g_autoptr(PtyxisPalette) palette = item;
  g_autoptr(GVariant) value = g_variant_take_ref (g_variant_new_string (ptyxis_palette_get_id (palette)));

  return  g_object_new (PTYXIS_TYPE_PREFERENCES_LIST_ITEM,
                        "title", ptyxis_palette_get_name (palette),
                        "value", value,
                        NULL);
}

GListModel *
ptyxis_palette_list_model_get_default (void)
{
  static GtkMapListModel *instance;

  if (instance == NULL)
    {
      instance = gtk_map_list_model_new (g_object_ref (ptyxis_palette_get_all ()),
                                         map_func, NULL, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return G_LIST_MODEL (instance);
}

static gboolean
ptyxis_palette_load_color (const char  *path,
                           GdkRGBA     *color,
                           GKeyFile    *key_file,
                           const char  *scheme,
                           const char  *key,
                           GError     **error)
{
  g_autofree char *str = NULL;

  memset (color, 0, sizeof *color);

  if (!(str = g_key_file_get_string (key_file, scheme, key, NULL)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing %s key in %s section",
                   path, key, scheme);
      return FALSE;
    }

  if (!gdk_rgba_parse (color, str))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is not a valid color for %s in section %s of \"%s\"",
                   str, key, scheme, path);
      return FALSE;
    }

  return TRUE;
}

static gboolean
is_dark (const GdkRGBA *color)
{
  /* http://alienryderflex.com/hsp.html */
  double r = color->red * 255.0;
  double g = color->green * 255.0;
  double b = color->blue * 255.0;
  double hsp = sqrt (0.299 * (r * r) +
                     0.587 * (g * g) +
                     0.114 * (b * b));

  return hsp <= 127.5;
}

static inline GdkRGBA
mix (const GdkRGBA *fg,
     const GdkRGBA *bg,
     double alpha)
{
  GdkRGBA dest;

  dest.red = ((1 - alpha) * bg->red) + (alpha * fg->red);
  dest.green = ((1 - alpha) * bg->green) + (alpha * fg->green);
  dest.blue = ((1 - alpha) * bg->blue) + (alpha * fg->blue);
  dest.alpha = 1.0;

  return dest;
}

static gboolean
ptyxis_palette_load_face (const char         *path,
                          PtyxisPaletteFace  *face,
                          GKeyFile           *key_file,
                          const char         *scheme,
                          GError            **error)
{
  gboolean dark;

  if (!g_key_file_has_group (key_file, scheme))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing %s section",
                   path, scheme);
      return FALSE;
    }

  if (!ptyxis_palette_load_color (path, &face->cursor_bg, key_file, scheme, "CursorBackground", NULL))
    ptyxis_palette_load_color (path, &face->cursor_bg, key_file, scheme, "Cursor", NULL);

  ptyxis_palette_load_color (path, &face->cursor_fg, key_file, scheme, "CursorForeground", NULL);

  if (FALSE ||
      !ptyxis_palette_load_color (path, &face->foreground, key_file, scheme, "Foreground", error) ||
      !ptyxis_palette_load_color (path, &face->background, key_file, scheme, "Background", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[0], key_file, scheme, "Color0", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[1], key_file, scheme, "Color1", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[2], key_file, scheme, "Color2", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[3], key_file, scheme, "Color3", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[4], key_file, scheme, "Color4", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[5], key_file, scheme, "Color5", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[6], key_file, scheme, "Color6", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[7], key_file, scheme, "Color7", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[8], key_file, scheme, "Color8", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[9], key_file, scheme, "Color9", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[10], key_file, scheme, "Color10", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[11], key_file, scheme, "Color11", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[12], key_file, scheme, "Color12", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[13], key_file, scheme, "Color13", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[14], key_file, scheme, "Color14", error) ||
      !ptyxis_palette_load_color (path, &face->indexed[15], key_file, scheme, "Color15", error))
    return FALSE;

  dark = is_dark (&face->background);

  if (!ptyxis_palette_load_color (path, &face->titlebar_foreground, key_file, scheme, "TitlebarForeground", NULL))
    face->titlebar_foreground = _gdk_rgba_shade (&face->foreground, dark ? 1.25 : .95);
  if (!ptyxis_palette_load_color (path, &face->titlebar_background, key_file, scheme, "TitlebarBackground", NULL))
    face->titlebar_background = dark ? _gdk_rgba_shade (&face->background, 1.25) : face->background;

  if (!ptyxis_palette_load_color (path, &face->visual_bell.foreground, key_file, scheme, "BellForeground", NULL))
    face->visual_bell.foreground = face->titlebar_foreground;
  if (!ptyxis_palette_load_color (path, &face->visual_bell.background, key_file, scheme, "BellBackground", NULL))
    face->visual_bell.background = mix (&face->indexed[11], &face->titlebar_background, .25);

  if (!ptyxis_palette_load_color (path, &face->superuser.foreground, key_file, scheme, "SuperuserForeground", NULL))
    face->superuser.foreground = _gdk_rgba_shade (&face->titlebar_foreground, dark ? 1 : .8);
  if (!ptyxis_palette_load_color (path, &face->superuser.background, key_file, scheme, "SuperuserBackground", NULL))
    face->superuser.background = mix (&face->indexed[1], &face->background, dark ? .33 : .5);

  if (!ptyxis_palette_load_color (path, &face->remote.foreground, key_file, scheme, "RemoteForeground", NULL))
    face->remote.foreground = _gdk_rgba_shade (&face->titlebar_foreground, dark ? 1 : .8);
  if (!ptyxis_palette_load_color (path, &face->remote.background, key_file, scheme, "RemoteBackground", NULL))
    face->remote.background = mix (&face->indexed[12], &face->background, dark ? .33 : .5);

  return TRUE;
}

static gboolean
ptyxis_palette_load_info (const char         *path,
                          PtyxisPaletteData  *data,
                          GKeyFile           *key_file,
                          GError            **error)
{
  g_autofree char *name = NULL;
  g_autofree char *base = NULL;
  g_autofree char *id = NULL;

  if (!g_key_file_has_group (key_file, "Palette"))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing Palette section",
                   path);
      return FALSE;
    }

  if (!(name = g_key_file_get_string (key_file, "Palette", "Name", NULL)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" is missing Name key of Palette section",
                   path);
      return FALSE;
    }

  data->name = g_intern_string (name);

  base = g_path_get_basename (path);
  if (!g_str_has_suffix (base, ".palette"))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"%s\" does not have suffix .palette",
                   path);
      return FALSE;
    }

  id = g_strndup (base, strlen (base) - strlen (".palette"));

  data->id = g_intern_string (id);

  return TRUE;
}

PtyxisPalette *
ptyxis_palette_new_from_file (const char  *path,
                              GError     **error)
{
  g_autoptr(GKeyFile) key_file = NULL;
  PtyxisPaletteData data;
  PtyxisPalette *self;
  gboolean has_light;
  gboolean has_dark;

  g_return_val_if_fail (path != NULL, NULL);

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, path, 0, error))
    return NULL;

  memset (&data, 0, sizeof data);

  if (!ptyxis_palette_load_info (path, &data, key_file, error))
    return NULL;

  has_light = g_key_file_has_group (key_file, "Light");
  has_dark = g_key_file_has_group (key_file, "Dark");

  /* If not using Light/Dark breakdown, then assume the values
   * are directly in "[Palette]". Otherwise we expect both.
   */
  if (!has_light && !has_dark)
    {
      if (!ptyxis_palette_load_face (path, &data.faces[0], key_file, "Palette", error))
        return NULL;
      data.faces[1] = data.faces[0];
      has_dark = is_dark (&data.faces[0].background);
      has_light = !has_dark;
    }
  else
    {
      if (!ptyxis_palette_load_face (path, &data.faces[0], key_file, "Light", error))
        return NULL;

      if (!ptyxis_palette_load_face (path, &data.faces[1], key_file, "Dark", error))
        return NULL;
    }

  self = g_object_new (PTYXIS_TYPE_PALETTE, NULL);
  self->allocated = g_memdup2 (&data, sizeof data);
  self->palette = self->allocated;
  self->is_primary = g_key_file_get_boolean (key_file, "Palette", "Primary", NULL);
  self->use_system_accent = g_key_file_get_boolean (key_file, "Palette", "UseSystemAccent", NULL);
  self->has_dark = has_dark;
  self->has_light = has_light;

  return self;
}

PtyxisPalette *
ptyxis_palette_new_from_resource (const char  *path,
                                  GError     **error)
{
  g_autoptr(GKeyFile) key_file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  PtyxisPaletteData data;
  PtyxisPalette *self;
  gboolean has_light;
  gboolean has_dark;

  g_return_val_if_fail (path != NULL, NULL);

  if (!(bytes = g_resources_lookup_data (path, 0, error)))
    return NULL;

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_bytes (key_file, bytes, 0, error))
    return NULL;

  memset (&data, 0, sizeof data);

  if (!ptyxis_palette_load_info (path, &data, key_file, error))
    return NULL;

  has_light = g_key_file_has_group (key_file, "Light");
  has_dark = g_key_file_has_group (key_file, "Dark");

  if (!has_light && !has_dark)
    {
      if (!ptyxis_palette_load_face (path, &data.faces[0], key_file, "Palette", error))
        return NULL;
      data.faces[1] = data.faces[0];
      has_dark = is_dark (&data.faces[0].background);
      has_light = !has_dark;
    }
  else
    {
      if (!ptyxis_palette_load_face (path, &data.faces[0], key_file, "Light", error))
        return NULL;

      if (!ptyxis_palette_load_face (path, &data.faces[1], key_file, "Dark", error))
        return NULL;
    }

  self = g_object_new (PTYXIS_TYPE_PALETTE, NULL);
  self->allocated = g_memdup2 (&data, sizeof data);
  self->palette = self->allocated;
  self->is_primary = g_key_file_get_boolean (key_file, "Palette", "Primary", NULL);
  self->use_system_accent = g_key_file_get_boolean (key_file, "Palette", "UseSystemAccent", NULL);
  self->has_dark = has_dark;
  self->has_light = has_light;

  return self;
}

gboolean
ptyxis_palette_use_system_accent (PtyxisPalette *self)
{
  return self->use_system_accent;
}

gboolean
ptyxis_palette_is_primary (PtyxisPalette *self)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), FALSE);

  return self->is_primary;
}

gboolean
ptyxis_palette_has_dark (PtyxisPalette *self)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), FALSE);

  return self->has_dark;
}

gboolean
ptyxis_palette_has_light (PtyxisPalette *self)
{
  g_return_val_if_fail (PTYXIS_IS_PALETTE (self), FALSE);

  return self->has_light;
}
