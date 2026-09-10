/* ptyxis-podman-provider.h
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

#include "ptyxis-container-provider.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_PODMAN_PROVIDER (ptyxis_podman_provider_get_type())

G_DECLARE_FINAL_TYPE (PtyxisPodmanProvider, ptyxis_podman_provider, PTYXIS, PODMAN_PROVIDER, PtyxisContainerProvider)

PtyxisContainerProvider *ptyxis_podman_provider_new                (void);
const char              *ptyxis_podman_provider_get_version        (void);
gboolean                 ptyxis_podman_provider_check_version      (guint                  major,
                                                                    guint                  minor,
                                                                    guint                  micro);
void                     ptyxis_podman_provider_queue_update       (PtyxisPodmanProvider  *self);
gboolean                 ptyxis_podman_provider_update_sync        (PtyxisPodmanProvider  *self,
                                                                    GCancellable          *cancellable,
                                                                    GError               **error);
void                     ptyxis_podman_provider_set_type_for_label (PtyxisPodmanProvider  *self,
                                                                    const char            *key,
                                                                    const char            *value,
                                                                    GType                  container_type);

G_END_DECLS
