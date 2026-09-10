/*
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
#include <vte/vte.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_CUSTOM_LINK (ptyxis_custom_link_get_type())

G_DECLARE_FINAL_TYPE (PtyxisCustomLink, ptyxis_custom_link, PTYXIS, CUSTOM_LINK, GObject)

PtyxisCustomLink *ptyxis_custom_link_new              (void);
PtyxisCustomLink *ptyxis_custom_link_new_with_strings (const char       *pattern,
                                                       const char       *target);
void              ptyxis_custom_link_set_pattern      (PtyxisCustomLink *self,
                                                       const char       *pattern);
char             *ptyxis_custom_link_dup_pattern      (PtyxisCustomLink *self);
char             *ptyxis_custom_link_dup_target       (PtyxisCustomLink *self);
void              ptyxis_custom_link_set_target       (PtyxisCustomLink *self,
                                                       const char       *target);
VteRegex         *ptyxis_custom_link_compile          (PtyxisCustomLink *self);
char             *ptyxis_custom_link_substitute       (PtyxisCustomLink *self,
                                                       const char       *subject);

G_END_DECLS
