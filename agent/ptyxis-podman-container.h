/* ptyxis-podman-container.h
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

#include <json-glib/json-glib.h>

#include "ptyxis-agent-ipc.h"
#include "ptyxis-run-context.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_PODMAN_CONTAINER (ptyxis_podman_container_get_type())

G_DECLARE_DERIVABLE_TYPE (PtyxisPodmanContainer, ptyxis_podman_container, PTYXIS, PODMAN_CONTAINER, PtyxisIpcContainerSkeleton)

struct _PtyxisPodmanContainerClass
{
  PtyxisIpcContainerSkeletonClass parent_class;

  gboolean (*deserialize)         (PtyxisPodmanContainer  *self,
                                   JsonObject             *object,
                                   GError                **error);
  void     (*prepare_run_context) (PtyxisPodmanContainer  *self,
                                   PtyxisRunContext       *run_context);
};

gboolean    ptyxis_podman_container_deserialize  (PtyxisPodmanContainer  *self,
                                                  JsonObject             *object,
                                                  GError                **error);
const char *ptyxis_podman_container_lookup_label (PtyxisPodmanContainer  *self,
                                                  const char             *key);

G_END_DECLS
