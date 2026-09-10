/*
 * Copyright 2025 Marco Mastropaolo <marco@mastropaolo.com>
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#include "ptyxis-custom-link.h"

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

struct _PtyxisCustomLink
{
  GObject   parent_instance;
  char     *pattern;
  char     *target;
  VteRegex *compiled_regex;
  guint     compiled : 1;
};

enum {
  PROP_0,
  PROP_PATTERN,
  PROP_TARGET,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtyxisCustomLink, ptyxis_custom_link, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
ptyxis_custom_link_finalize (GObject *object)
{
  PtyxisCustomLink *self = PTYXIS_CUSTOM_LINK (object);

  g_clear_pointer (&self->pattern, g_free);
  g_clear_pointer (&self->target, g_free);
  g_clear_pointer (&self->compiled_regex, vte_regex_unref);

  G_OBJECT_CLASS (ptyxis_custom_link_parent_class)->finalize (object);
}

static void
ptyxis_custom_link_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PtyxisCustomLink *self = PTYXIS_CUSTOM_LINK (object);

  switch (property_id)
    {
    case PROP_PATTERN:
      g_value_take_string (value, ptyxis_custom_link_dup_pattern (self));
      break;

    case PROP_TARGET:
      g_value_take_string (value, ptyxis_custom_link_dup_target (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ptyxis_custom_link_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PtyxisCustomLink *self = PTYXIS_CUSTOM_LINK (object);

  switch (property_id)
    {
    case PROP_PATTERN:
      ptyxis_custom_link_set_pattern (self, g_value_get_string (value));
      break;

    case PROP_TARGET:
      ptyxis_custom_link_set_target (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ptyxis_custom_link_init (PtyxisCustomLink *self)
{
  self->pattern = g_strdup ("");
  self->target = g_strdup ("");
}

static void
ptyxis_custom_link_class_init (PtyxisCustomLinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ptyxis_custom_link_finalize;
  object_class->set_property = ptyxis_custom_link_set_property;
  object_class->get_property = ptyxis_custom_link_get_property;

  properties[PROP_PATTERN] =
    g_param_spec_string ("pattern", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TARGET] =
    g_param_spec_string ("target", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

PtyxisCustomLink *
ptyxis_custom_link_new (void)
{
  return g_object_new (PTYXIS_TYPE_CUSTOM_LINK, NULL);
}

PtyxisCustomLink *
ptyxis_custom_link_new_with_strings (const char *pattern,
                                     const char *target)
{
  return g_object_new (PTYXIS_TYPE_CUSTOM_LINK,
                       "pattern", pattern,
                       "target", target,
                       NULL);
}

char *
ptyxis_custom_link_dup_target (PtyxisCustomLink *self)
{
  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (self), NULL);

  return g_strdup (self->target);
}

void
ptyxis_custom_link_set_target (PtyxisCustomLink *self,
                               const char       *target)
{
  g_return_if_fail (PTYXIS_IS_CUSTOM_LINK (self));

  if (target == NULL)
    target = "";

  if (g_set_str (&self->target, target))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TARGET]);
}

char *
ptyxis_custom_link_dup_pattern (PtyxisCustomLink *self)
{
  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (self), NULL);

  return g_strdup (self->pattern);
}

void
ptyxis_custom_link_set_pattern (PtyxisCustomLink *self,
                                const char       *pattern)
{
  g_return_if_fail (PTYXIS_IS_CUSTOM_LINK (self));

  if (pattern == NULL)
    pattern = "";

  if (g_set_str (&self->pattern, pattern))
    {
      self->compiled = FALSE;
      g_clear_pointer (&self->compiled_regex, vte_regex_unref);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATTERN]);
    }
}

VteRegex *
ptyxis_custom_link_compile (PtyxisCustomLink *self)
{
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (self), NULL);

  if (self->compiled)
    return vte_regex_ref (self->compiled_regex);

  self->compiled = TRUE;

  self->compiled_regex = vte_regex_new_for_match (self->pattern, -1,
                                                  PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE,
                                                  &error);

  if (error != NULL)
    {
      g_warning ("Failed to compile regex: %s: Regex was: %s",
                 error->message, self->pattern);

      return NULL;
    }

  g_assert (self->compiled_regex != NULL);

  if (!vte_regex_jit (self->compiled_regex, PCRE2_JIT_COMPLETE, &error) ||
      !vte_regex_jit (self->compiled_regex, PCRE2_JIT_PARTIAL_SOFT, &error))
    g_warning ("Failed to JIT regex: %s: Regex was: %s",
               error->message, self->pattern);

  return vte_regex_ref (self->compiled_regex);
}

char *
ptyxis_custom_link_substitute (PtyxisCustomLink *self,
                               const char       *subject)
{
  g_autoptr(GError) error = NULL;
  g_autofree char* subst = NULL;

  g_return_val_if_fail (PTYXIS_IS_CUSTOM_LINK (self), NULL);
  g_return_val_if_fail (self->compiled_regex != NULL, NULL);
  g_return_val_if_fail (subject != NULL, NULL);

  subst = vte_regex_substitute (self->compiled_regex,
                                subject,
                                self->target,
                                0,
                                &error);

  if (subst == NULL)
    g_warning ("Failed to perform substitution with regex: %s: Regex was: '%s', Substitution was: '%s', Subject was: '%s'",
               (error != NULL) ? error->message : "(error null)",
               self->pattern, self->target, subject);

  return g_uri_escape_string (subst, ":/.", FALSE);
}

