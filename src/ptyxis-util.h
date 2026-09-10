/* ptyxis-util.h
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

#include <vte/vte.h>

G_BEGIN_DECLS

#define VTE_PCRE2_UCP 0x00020000u
#define VTE_PCRE2_MULTILINE 0x00000400u
#define VTE_PCRE2_CASELESS 0x00000008u

#define VTE_VERSION_NUMERIC ((vte_get_major_version()) * 10000 + (vte_get_minor_version()) * 100 + (vte_get_micro_version()))

#define _GDK_RGBA_DECODE(c) ((unsigned)(((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : \
                                        ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                                        ((c) >= '0' && (c) <= '9') ? ((c)-'0') : \
                                        -1))
#define _GDK_RGBA_SELECT_COLOR(_str, index3, index6) (sizeof(_str) <= 4 ? _GDK_RGBA_DECODE ((_str)[index3]) : _GDK_RGBA_DECODE ((_str)[index6]))
#define GDK_RGBA(str) ((GdkRGBA) {\
    ((_GDK_RGBA_SELECT_COLOR(str, 0, 0) << 4) | _GDK_RGBA_SELECT_COLOR(str, 0, 1)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 1, 2) << 4) | _GDK_RGBA_SELECT_COLOR(str, 1, 3)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 2, 4) << 4) | _GDK_RGBA_SELECT_COLOR(str, 2, 5)) / 255., \
    ((sizeof(str) % 4 == 1) ? ((_GDK_RGBA_SELECT_COLOR(str, 3, 6) << 4) | _GDK_RGBA_SELECT_COLOR(str, 3, 7)) : 0xFF) / 255. })

typedef enum
{
  PTYXIS_PROCESS_KIND_HOST    = 0,
  PTYXIS_PROCESS_KIND_FLATPAK = 1,
} PtyxisProcessKind;

PtyxisProcessKind   ptyxis_get_process_kind      (void) G_GNUC_CONST;
const char * const *ptyxis_host_environ          (void) G_GNUC_CONST;
char               *ptyxis_path_expand           (const char *path);
char               *ptyxis_path_collapse         (const char *path);
gboolean            ptyxis_shell_supports_dash_l (const char *shell);
gboolean            ptyxis_is_shell              (const char *arg0);
GListModel         *ptyxis_parse_shells          (const char *etc_shells);
const char         *ptyxis_app_name              (void) G_GNUC_CONST;
GVariant           *ptyxis_variant_new_toast     (const char *title,
                                                  guint       timeout);
gboolean            ptyxis_is_default            (void);
gboolean            ptyxis_make_default          (void);
char               *ptyxis_escape_underline      (const char *str);

static inline void
ptyxis_take_str (char **out_str,
                 char  *new_str)
{
  g_free (*out_str);
  *out_str = new_str;
}

static inline gboolean
ptyxis_str_empty0 (const char *s)
{
  return !s || !*s;
}

G_END_DECLS
