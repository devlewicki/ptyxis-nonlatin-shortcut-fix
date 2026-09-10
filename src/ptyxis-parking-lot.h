/*
 * ptyxis-parking-lot.h
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

#include "ptyxis-tab.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_PARKING_LOT (ptyxis_parking_lot_get_type())

G_DECLARE_FINAL_TYPE (PtyxisParkingLot, ptyxis_parking_lot, PTYXIS, PARKING_LOT, GObject)

PtyxisParkingLot *ptyxis_parking_lot_new         (void);
guint             ptyxis_parking_lot_get_timeout (PtyxisParkingLot *self);
void              ptyxis_parking_lot_set_timeout (PtyxisParkingLot *self,
                                                  guint             timeout);
void              ptyxis_parking_lot_push        (PtyxisParkingLot *self,
                                                  PtyxisTab        *tab);
PtyxisTab        *ptyxis_parking_lot_pop         (PtyxisParkingLot *self);

G_END_DECLS
