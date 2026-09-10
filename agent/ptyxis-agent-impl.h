/*
 * ptyxis-agent-impl.h
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

#include "ptyxis-agent-ipc.h"
#include "ptyxis-container-provider.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_AGENT_IMPL (ptyxis_agent_impl_get_type())

G_DECLARE_FINAL_TYPE (PtyxisAgentImpl, ptyxis_agent_impl, PTYXIS, AGENT_IMPL, PtyxisIpcAgentSkeleton)

PtyxisAgentImpl *ptyxis_agent_impl_get_default   (void);
PtyxisAgentImpl *ptyxis_agent_impl_new           (GError                  **error);
void             ptyxis_agent_impl_add_container (PtyxisAgentImpl          *self,
                                                  PtyxisIpcContainer       *container);
void             ptyxis_agent_impl_add_provider  (PtyxisAgentImpl          *self,
                                                  PtyxisContainerProvider  *provider);


G_END_DECLS
