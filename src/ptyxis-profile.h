/*
 * ptyxis-profile.h
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

#include <pango/pango.h>
#include <vte/vte.h>

#include "ptyxis-palette.h"
#include "ptyxis-custom-link.h"

G_BEGIN_DECLS

#define PTYXIS_PROFILE_KEY_BACKSPACE_BINDING   "backspace-binding"
#define PTYXIS_PROFILE_KEY_BOLD_IS_BRIGHT      "bold-is-bright"
#define PTYXIS_PROFILE_KEY_CELL_HEIGHT_SCALE   "cell-height-scale"
#define PTYXIS_PROFILE_KEY_CELL_WIDTH_SCALE    "cell-width-scale"
#define PTYXIS_PROFILE_KEY_CJK_AMBIGUOUS_WIDTH "cjk-ambiguous-width"
#define PTYXIS_PROFILE_KEY_CUSTOM_COMMAND      "custom-command"
#define PTYXIS_PROFILE_KEY_DEFAULT_CONTAINER   "default-container"
#define PTYXIS_PROFILE_KEY_DELETE_BINDING      "delete-binding"
#define PTYXIS_PROFILE_KEY_EXIT_ACTION         "exit-action"
#define PTYXIS_PROFILE_KEY_LABEL               "label"
#define PTYXIS_PROFILE_KEY_LIMIT_SCROLLBACK    "limit-scrollback"
#define PTYXIS_PROFILE_KEY_LOGIN_SHELL         "login-shell"
#define PTYXIS_PROFILE_KEY_OPACITY             "opacity"
#define PTYXIS_PROFILE_KEY_PALETTE             "palette"
#define PTYXIS_PROFILE_KEY_PRESERVE_CONTAINER  "preserve-container"
#define PTYXIS_PROFILE_KEY_PRESERVE_DIRECTORY  "preserve-directory"
#define PTYXIS_PROFILE_KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define PTYXIS_PROFILE_KEY_SCROLL_ON_OUTPUT    "scroll-on-output"
#define PTYXIS_PROFILE_KEY_SCROLLBACK_LINES    "scrollback-lines"
#define PTYXIS_PROFILE_KEY_USE_PROXY           "use-proxy"
#define PTYXIS_PROFILE_KEY_USE_CUSTOM_COMMAND  "use-custom-command"
#define PTYXIS_PROFILE_KEY_CUSTOM_LINKS        "custom-links"

typedef enum _PtyxisExitAction
{
  PTYXIS_EXIT_ACTION_NONE    = 0,
  PTYXIS_EXIT_ACTION_RESTART = 1,
  PTYXIS_EXIT_ACTION_CLOSE   = 2,
} PtyxisExitAction;

typedef enum _PtyxisPreserveContainer
{
  PTYXIS_PRESERVE_CONTAINER_NEVER  = 0,
  PTYXIS_PRESERVE_CONTAINER_ALWAYS = 1,
} PtyxisPreserveContainer;

typedef enum _PtyxisPreserveDirectory
{
  PTYXIS_PRESERVE_DIRECTORY_NEVER  = 0,
  PTYXIS_PRESERVE_DIRECTORY_SAFE   = 1,
  PTYXIS_PRESERVE_DIRECTORY_ALWAYS = 2,
} PtyxisPreserveDirectory;

typedef enum _PtyxisCjkAmbiguousWidth
{
  PTYXIS_CJK_AMBIGUOUS_WIDTH_NARROW = 1,
  PTYXIS_CJK_AMBIGUOUS_WIDTH_WIDE   = 2,
} PtyxisCjkAmbiguousWidth;

#define PTYXIS_TYPE_PROFILE (ptyxis_profile_get_type())

G_DECLARE_FINAL_TYPE (PtyxisProfile, ptyxis_profile, PTYXIS, PROFILE, GObject)

PtyxisProfile           *ptyxis_profile_new                      (const char              *uuid);
PtyxisProfile           *ptyxis_profile_duplicate                (PtyxisProfile           *self);
GSettings               *ptyxis_profile_dup_settings             (PtyxisProfile           *self);
const char              *ptyxis_profile_get_uuid                 (PtyxisProfile           *self);
char                    *ptyxis_profile_dup_default_container    (PtyxisProfile           *self);
void                     ptyxis_profile_set_default_container    (PtyxisProfile           *self,
                                                                  const char              *default_container);
char                    *ptyxis_profile_dup_label                (PtyxisProfile           *self);
void                     ptyxis_profile_set_label                (PtyxisProfile           *self,
                                                                  const char              *label);
gboolean                 ptyxis_profile_get_limit_scrollback     (PtyxisProfile           *self);
void                     ptyxis_profile_set_limit_scrollback     (PtyxisProfile           *self,
                                                                  gboolean                 limit_scrollback);
int                      ptyxis_profile_get_scrollback_lines     (PtyxisProfile           *self);
void                     ptyxis_profile_set_scrollback_lines     (PtyxisProfile           *self,
                                                                  int                      scrollback_lines);
gboolean                 ptyxis_profile_get_scroll_on_keystroke  (PtyxisProfile           *self);
void                     ptyxis_profile_set_scroll_on_keystroke  (PtyxisProfile           *self,
                                                                  gboolean                 scroll_on_keystroke);
gboolean                 ptyxis_profile_get_scroll_on_output     (PtyxisProfile           *self);
void                     ptyxis_profile_set_scroll_on_output     (PtyxisProfile           *self,
                                                                  gboolean                 scroll_on_output);
gboolean                 ptyxis_profile_get_bold_is_bright       (PtyxisProfile           *self);
void                     ptyxis_profile_set_bold_is_bright       (PtyxisProfile           *self,
                                                                  gboolean                 bold_is_bright);
double                   ptyxis_profile_get_cell_height_scale    (PtyxisProfile           *self);
void                     ptyxis_profile_set_cell_height_scale    (PtyxisProfile           *self,
                                                                  double                   cell_height_scale);
double                   ptyxis_profile_get_cell_width_scale     (PtyxisProfile           *self);
void                     ptyxis_profile_set_cell_width_scale     (PtyxisProfile           *self,
                                                                  double                   cell_width_scale);
PtyxisExitAction         ptyxis_profile_get_exit_action          (PtyxisProfile           *self);
void                     ptyxis_profile_set_exit_action          (PtyxisProfile           *self,
                                                                  PtyxisExitAction         exit_action);
PtyxisPreserveContainer  ptyxis_profile_get_preserve_container   (PtyxisProfile           *self);
void                     ptyxis_profile_set_preserve_container   (PtyxisProfile           *self,
                                                                  PtyxisPreserveContainer  preserve_container);
PtyxisPreserveDirectory  ptyxis_profile_get_preserve_directory   (PtyxisProfile           *self);
void                     ptyxis_profile_set_preserve_directory   (PtyxisProfile           *self,
                                                                  PtyxisPreserveDirectory  preserve_directory);
char                    *ptyxis_profile_dup_palette_id           (PtyxisProfile           *self);
PtyxisPalette           *ptyxis_profile_dup_palette              (PtyxisProfile           *self);
void                     ptyxis_profile_set_palette              (PtyxisProfile           *self,
                                                                  PtyxisPalette           *palette);
double                   ptyxis_profile_get_opacity              (PtyxisProfile           *self);
void                     ptyxis_profile_set_opacity              (PtyxisProfile           *self,
                                                                  double                   opacity);
VteEraseBinding          ptyxis_profile_get_backspace_binding    (PtyxisProfile           *self);
void                     ptyxis_profile_set_backspace_binding    (PtyxisProfile           *self,
                                                                  VteEraseBinding          backspace_binding);
VteEraseBinding          ptyxis_profile_get_delete_binding       (PtyxisProfile           *self);
void                     ptyxis_profile_set_delete_binding       (PtyxisProfile           *self,
                                                                  VteEraseBinding          delete_binding);
PtyxisCjkAmbiguousWidth  ptyxis_profile_get_cjk_ambiguous_width  (PtyxisProfile           *self);
void                     ptyxis_profile_set_cjk_ambiguous_width  (PtyxisProfile           *self,
                                                                  PtyxisCjkAmbiguousWidth  cjk_ambiguous_width);
gboolean                 ptyxis_profile_get_login_shell          (PtyxisProfile           *self);
void                     ptyxis_profile_set_login_shell          (PtyxisProfile           *self,
                                                                  gboolean                 login_shell);
char                    *ptyxis_profile_dup_custom_command       (PtyxisProfile           *self);
void                     ptyxis_profile_set_custom_command       (PtyxisProfile           *self,
                                                                  const char              *custom_command);
gboolean                 ptyxis_profile_get_use_custom_command   (PtyxisProfile           *self);
void                     ptyxis_profile_set_use_custom_command   (PtyxisProfile           *self,
                                                                  gboolean                 use_custom_command);
gboolean                 ptyxis_profile_get_use_proxy            (PtyxisProfile           *self);
void                     ptyxis_profile_set_use_proxy            (PtyxisProfile           *self,
                                                                  gboolean                 use_proxy);
GListModel              *ptyxis_profile_list_custom_links        (PtyxisProfile           *self);
void                     ptyxis_profile_add_custom_link          (PtyxisProfile           *self,
                                                                  PtyxisCustomLink        *custom_link);
void                     ptyxis_profile_undo_remove_custom_link  (PtyxisProfile           *self,
                                                                  PtyxisCustomLink        *custom_link,
                                                                  guint                    index);
gboolean                 ptyxis_profile_remove_custom_link       (PtyxisProfile           *self,
                                                                  PtyxisCustomLink        *custom_link,
                                                                  guint                   *index);
void                     ptyxis_profile_save_custom_link_changes (PtyxisProfile           *self);

G_END_DECLS
