/*
 * ptyxis-user-palettes.c
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

#include "ptyxis-user-palettes.h"

struct _PtyxisUserPalettes
{
  GObject       parent_instance;
  GFile        *directory;
  GFileMonitor *monitor;
  GHashTable   *file_to_palette;
  GPtrArray    *items;
};

static gpointer
ptyxis_user_palettes_get_item (GListModel *model,
                               guint       position)
{
  PtyxisUserPalettes *self = PTYXIS_USER_PALETTES (model);

  if (position < self->items->len)
    return g_object_ref (g_ptr_array_index (self->items, position));

  return NULL;
}

static guint
ptyxis_user_palettes_get_n_items (GListModel *model)
{
  PtyxisUserPalettes *self = PTYXIS_USER_PALETTES (model);

  return self->items->len;
}

static GType
ptyxis_user_palettes_get_item_type (GListModel *model)
{
  return PTYXIS_TYPE_PALETTE;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ptyxis_user_palettes_get_n_items;
  iface->get_item = ptyxis_user_palettes_get_item;
  iface->get_item_type = ptyxis_user_palettes_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtyxisUserPalettes, ptyxis_user_palettes, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ptyxis_user_palettes_load_file (PtyxisUserPalettes *self,
                                const char         *path)
{
  g_autoptr(PtyxisPalette) palette = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PTYXIS_IS_USER_PALETTES (self));
  g_assert (path != NULL);

  if ((palette = ptyxis_palette_new_from_file (path, &error)))
    {
      PtyxisPalette *previous;

      if ((previous = g_hash_table_lookup (self->file_to_palette, path)))
        {
          guint pos;

          if (g_ptr_array_find (self->items, previous, &pos))
            {
              g_object_unref (g_ptr_array_index (self->items, pos));
              g_ptr_array_index (self->items, pos) = g_object_ref (palette);
              g_hash_table_insert (self->file_to_palette,
                                   g_strdup (path),
                                   g_object_ref (palette));
              g_list_model_items_changed (G_LIST_MODEL (self), pos, 1, 1);
              return;
            }
        }

      g_hash_table_insert (self->file_to_palette,
                           g_strdup (path),
                           g_object_ref (palette));
      g_ptr_array_add (self->items, g_object_ref (palette));
      g_list_model_items_changed (G_LIST_MODEL (self), self->items->len - 1, 0, 1);
    }

  if (error)
    g_warning ("%s", error->message);
}

static void
ptyxis_user_palettes_dispose (GObject *object)
{
  PtyxisUserPalettes *self = (PtyxisUserPalettes *)object;

  if (self->monitor != NULL)
    g_file_monitor_cancel (self->monitor);

  g_clear_object (&self->directory);
  g_clear_object (&self->monitor);

  g_hash_table_remove_all (self->file_to_palette);

  if (self->items->len > 0)
    g_ptr_array_remove_range (self->items, 0, self->items->len);

  G_OBJECT_CLASS (ptyxis_user_palettes_parent_class)->dispose (object);
}

static void
ptyxis_user_palettes_finalize (GObject *object)
{
  PtyxisUserPalettes *self = (PtyxisUserPalettes *)object;

  g_clear_pointer (&self->file_to_palette, g_hash_table_unref);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (ptyxis_user_palettes_parent_class)->finalize (object);
}

static void
ptyxis_user_palettes_class_init (PtyxisUserPalettesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_user_palettes_dispose;
  object_class->finalize = ptyxis_user_palettes_finalize;
}

static void
ptyxis_user_palettes_init (PtyxisUserPalettes *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->file_to_palette = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_object_unref);
}

static void
ptyxis_user_palettes_load (PtyxisUserPalettes *self)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr;

  g_assert (PTYXIS_IS_USER_PALETTES (self));
  g_assert (G_IS_FILE (self->directory));

  enumerator = g_file_enumerate_children (self->directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          0,
                                          NULL,
                                          NULL);
  if (enumerator == NULL)
    return;

  while ((infoptr = g_file_enumerator_next_file (enumerator, NULL, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      g_autoptr(GFile) file = g_file_enumerator_get_child (enumerator, info);
      const char *path = g_file_peek_path (file);

      if (g_str_has_suffix (path, ".palette"))
        ptyxis_user_palettes_load_file (self, path);
    }
}

static void
ptyxis_user_palettes_remove (PtyxisUserPalettes *self,
                             const char         *path)
{
  PtyxisPalette *palette;

  g_assert (PTYXIS_IS_USER_PALETTES (self));
  g_assert (path != NULL);

  if ((palette = g_hash_table_lookup (self->file_to_palette, path)))
    {
      guint pos;

      if (g_ptr_array_find (self->items, palette, &pos))
        {
          g_ptr_array_remove_index (self->items, pos);
          g_list_model_items_changed (G_LIST_MODEL (self), pos, 1, 0);
        }
    }
}

static void
ptyxis_user_palettes_monitor_changed_cb (PtyxisUserPalettes *self,
                                         GFile              *file,
                                         GFile              *other_file,
                                         GFileMonitorEvent   event_type,
                                         GFileMonitor       *monitor)
{
  g_autofree char *name = NULL;

  g_assert (PTYXIS_IS_USER_PALETTES (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  name = g_file_get_basename (file);

  if (!g_str_has_suffix (name, ".palette"))
    return;

  if (event_type == G_FILE_MONITOR_EVENT_DELETED)
    ptyxis_user_palettes_remove (self, g_file_peek_path (file));
  else if (event_type == G_FILE_MONITOR_EVENT_CREATED ||
           event_type == G_FILE_MONITOR_EVENT_CHANGED)
    ptyxis_user_palettes_load_file (self, g_file_peek_path (file));
}

PtyxisUserPalettes *
ptyxis_user_palettes_new (const char *directory)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileMonitor) monitor = NULL;
  PtyxisUserPalettes *self;

  g_return_val_if_fail (directory != NULL, NULL);

  file = g_file_new_for_path (directory);
  if (!g_file_query_exists (file, NULL))
    g_file_make_directory_with_parents (file, NULL, NULL);

  monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (monitor == NULL)
    return NULL;

  self = g_object_new (PTYXIS_TYPE_USER_PALETTES, NULL);
  self->directory = g_steal_pointer (&file);
  self->monitor = g_steal_pointer (&monitor);

  g_signal_connect_object (self->monitor,
                           "changed",
                           G_CALLBACK (ptyxis_user_palettes_monitor_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ptyxis_user_palettes_load (self);

  return self;
}
