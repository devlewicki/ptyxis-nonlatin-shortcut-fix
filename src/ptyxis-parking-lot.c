/*
 * ptyxis-parking-lot.c
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

#include "ptyxis-parking-lot.h"

#define DEFAULT_TIMEOUT_SECONDS 5

typedef struct _PtyxisParkedTab
{
  GList             link;
  PtyxisParkingLot *lot;
  PtyxisTab        *tab;
  guint             source_id;
} PtyxisParkedTab;

struct _PtyxisParkingLot
{
  GObject parent_instance;
  GQueue  tabs;
  guint   timeout;
};

enum {
  PROP_0,
  PROP_TIMEOUT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisParkingLot, ptyxis_parking_lot, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ptyxis_parking_lot_remove (PtyxisParkingLot *self,
                           PtyxisParkedTab  *parked,
                           gboolean          force_quit)
{
  g_autoptr(PtyxisTab) tab = NULL;

  g_assert (PTYXIS_IS_PARKING_LOT (self));
  g_assert (parked != NULL);

  tab = g_steal_pointer (&parked->tab);

  g_clear_handle_id (&parked->source_id, g_source_remove);
  g_queue_unlink (&self->tabs, &parked->link);
  parked->lot = NULL;
  g_clear_pointer (&parked, g_free);

  if (tab != NULL)
    {
      g_autofree char *title = ptyxis_tab_dup_title (tab);
      g_debug ("Removing tab \"%s\" from parking lot", title);

      if (force_quit)
        ptyxis_tab_force_quit (tab);
    }
}

static void
ptyxis_parking_lot_dispose (GObject *object)
{
  PtyxisParkingLot *self = (PtyxisParkingLot *)object;

  while (self->tabs.head != NULL)
    ptyxis_parking_lot_remove (self, self->tabs.head->data, TRUE);

  G_OBJECT_CLASS (ptyxis_parking_lot_parent_class)->dispose (object);
}

static void
ptyxis_parking_lot_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PtyxisParkingLot *self = PTYXIS_PARKING_LOT (object);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      g_value_set_uint (value, self->timeout);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_parking_lot_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PtyxisParkingLot *self = PTYXIS_PARKING_LOT (object);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      ptyxis_parking_lot_set_timeout (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_parking_lot_class_init (PtyxisParkingLotClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_parking_lot_dispose;
  object_class->get_property = ptyxis_parking_lot_get_property;
  object_class->set_property = ptyxis_parking_lot_set_property;

  properties[PROP_TIMEOUT] =
    g_param_spec_uint ("timeout", NULL, NULL,
                       0, G_MAXUINT, DEFAULT_TIMEOUT_SECONDS,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ptyxis_parking_lot_init (PtyxisParkingLot *self)
{
  self->timeout = DEFAULT_TIMEOUT_SECONDS;
}

PtyxisParkingLot *
ptyxis_parking_lot_new (void)
{
  return g_object_new (PTYXIS_TYPE_PARKING_LOT, NULL);
}

guint
ptyxis_parking_lot_get_timeout (PtyxisParkingLot *self)
{
  g_return_val_if_fail (PTYXIS_IS_PARKING_LOT (self), 0);

  return self->timeout;
}

void
ptyxis_parking_lot_set_timeout (PtyxisParkingLot *self,
                                guint             timeout)
{
  g_return_if_fail (PTYXIS_IS_PARKING_LOT (self));

  if (timeout != self->timeout)
    {
      self->timeout = timeout;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEOUT]);
    }
}

static gboolean
ptyxis_parking_lot_source_func (gpointer data)
{
  PtyxisParkedTab *parked = data;

  g_assert (parked != NULL);
  g_assert (PTYXIS_IS_PARKING_LOT (parked->lot));
  g_assert (PTYXIS_IS_TAB (parked->tab));
  g_assert (parked->source_id != 0);

  ptyxis_parking_lot_remove (parked->lot, parked, TRUE);

  return G_SOURCE_REMOVE;
}

void
ptyxis_parking_lot_push (PtyxisParkingLot *self,
                         PtyxisTab        *tab)
{
  PtyxisParkedTab *parked;
  g_autofree char *title = NULL;

  g_return_if_fail (PTYXIS_IS_PARKING_LOT (self));
  g_return_if_fail (PTYXIS_IS_TAB (tab));

  title = ptyxis_tab_dup_title (tab);
  g_debug ("Adding tab \"%s\" to parking lot", title);

  parked = g_new0 (PtyxisParkedTab, 1);
  parked->link.data = parked;
  parked->tab = g_object_ref (tab);
  parked->lot = self;
  parked->source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                  self->timeout,
                                                  ptyxis_parking_lot_source_func,
                                                  parked, NULL);

  g_queue_push_tail_link (&self->tabs, &parked->link);
}

/**
 * ptyxis_parking_lot_pop:
 * @self: a #PtyxisParkingLot
 *
 * Returns: (transfer full) (nullable): a #PtyxisTab or %NULL
 */
PtyxisTab *
ptyxis_parking_lot_pop (PtyxisParkingLot *self)
{
  PtyxisParkedTab *parked;
  PtyxisTab *ret = NULL;

  g_return_val_if_fail (PTYXIS_IS_PARKING_LOT (self), NULL);

  g_debug ("Request to pop tab from parking lot of %u tabs",
           self->tabs.length);

  if ((parked = g_queue_peek_head (&self->tabs)))
    {
      ret = g_steal_pointer (&parked->tab);
      ptyxis_parking_lot_remove (self, parked, FALSE);
    }

  return ret;
}
