/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * The GIMP Help plug-in
 * Copyright (C) 1999-2004 Sven Neumann <sven@gimp.org>
 *                         Michael Natterer <mitch@gimp.org>
 *                         Henrik Brix Andersen <brix@gimp.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*  This code is written so that it can also be compiled standalone.
 *  It shouldn't depend on libgimp.
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "gimphelp.h"

#ifdef DISABLE_NLS
#define _(String)  (String)
#else
#include "libgimp/stdplugins-intl.h"
#endif


/*  private variables  */

static GHashTable  *domain_hash = NULL;


/*  public functions  */

gboolean
gimp_help_init (gint    num_domain_names,
                gchar **domain_names,
                gint    num_domain_uris,
                gchar **domain_uris)
{
  const gchar *default_env_domain_uri;
  gchar       *default_domain_uri;
  gint         i;

  if (num_domain_names != num_domain_uris)
    {
      g_printerr ("help: number of names doesn't match number of URIs.\n");

      return FALSE;
    }

  /*  set default values  */
  default_env_domain_uri = g_getenv (GIMP_HELP_ENV_URI);

  if (default_env_domain_uri)
    {
      default_domain_uri = g_strdup (default_env_domain_uri);
    }
  else
    {
      gchar *help_root = g_build_filename (gimp_data_directory (),
                                           GIMP_HELP_PREFIX,
                                           NULL);

      default_domain_uri = g_filename_to_uri (help_root, NULL, NULL);

      g_free (help_root);
    }

  gimp_help_register_domain (GIMP_HELP_DEFAULT_DOMAIN,
                             default_domain_uri, NULL);

  for (i = 0; i < num_domain_names; i++)
    {
      gimp_help_register_domain (domain_names[i], domain_uris[i], NULL);
    }

  g_free (default_domain_uri);

  return TRUE;
}

void
gimp_help_exit (void)
{
  if (domain_hash)
    {
      g_hash_table_destroy (domain_hash);
      domain_hash = NULL;
    }
}

void
gimp_help_register_domain (const gchar *domain_name,
                           const gchar *domain_uri,
                           const gchar *domain_root)
{
  g_return_if_fail (domain_name != NULL);
  g_return_if_fail (domain_uri != NULL);

#ifdef GIMP_HELP_DEBUG
  g_printerr ("help: registering help domain \"%s\" with base uri \"%s\"\n",
              domain_name, domain_uri);
#endif

  if (! domain_hash)
    domain_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free,
                                         (GDestroyNotify) gimp_help_domain_free);

  g_hash_table_insert (domain_hash,
                       g_strdup (domain_name),
                       gimp_help_domain_new (domain_name,
                                             domain_uri, domain_root));
}

GimpHelpDomain *
gimp_help_lookup_domain (const gchar *domain_name)
{
  g_return_val_if_fail (domain_name, NULL);

  if (domain_hash)
    return g_hash_table_lookup (domain_hash, domain_name);

  return NULL;
}

GList *
gimp_help_parse_locales (const gchar *help_locales)
{
  GList       *locales = NULL;
  GList       *list;
  const gchar *s;
  const gchar *p;

  g_return_val_if_fail (help_locales != NULL, NULL);

  /*  split the string at colons, building a list  */
  s = help_locales;
  for (p = strchr (s, ':'); p; p = strchr (s, ':'))
    {
      gchar *new = g_strndup (s, p - s);

      locales = g_list_append (locales, new);
      s = p + 1;
    }

  if (*s)
    locales = g_list_append (locales, g_strdup (s));

  /*  if the list doesn't contain the default domain yet, append it  */
  for (list = locales; list; list = list->next)
    if (strcmp ((const gchar *) list->data, GIMP_HELP_DEFAULT_LOCALE) == 0)
      break;

  if (! list)
    locales = g_list_append (locales, g_strdup (GIMP_HELP_DEFAULT_LOCALE));

#ifdef GIMP_HELP_DEBUG
  g_printerr ("help: locales: ");
  for (list = locales; list; list = list->next)
    g_printerr ("%s ", (const gchar *) list->data);
  g_printerr ("\n");
#endif

  return locales;
}