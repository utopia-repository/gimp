/* gimpparasite.c: Copyright 1998 Jay Cox <jaycox@gimp.org>
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

#include "config.h"

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimp.h"
#include "gimp-parasites.h"
#include "gimpparasitelist.h"


void
gimp_parasite_attach (Gimp         *gimp,
		      GimpParasite *parasite)
{
  gimp_parasite_list_add (gimp->parasites, parasite);
}

void
gimp_parasite_detach (Gimp        *gimp,
		      const gchar *name)
{
  gimp_parasite_list_remove (gimp->parasites, name);
}

GimpParasite *
gimp_parasite_find (Gimp        *gimp,
		    const gchar *name)
{
  return gimp_parasite_list_find (gimp->parasites, name);
}

static void
list_func (const gchar    *key,
	   GimpParasite   *parasite,
	   gchar        ***current)
{
  *(*current)++ = g_strdup (key);
}

gchar **
gimp_parasite_list (Gimp *gimp,
		    gint *count)
{
  gchar **list;
  gchar **current;

  *count = gimp_parasite_list_length (gimp->parasites);

  list = current = g_new (gchar *, *count);

  gimp_parasite_list_foreach (gimp->parasites, (GHFunc) list_func, &current);

  return list;
}


/*  FIXME: this doesn't belong here  */

void
gimp_parasite_shift_parent (GimpParasite *parasite)
{
  if (parasite == NULL)
    return;

  parasite->flags = (parasite->flags >> 8);
}


/*  parasiterc functions  */

void
gimp_parasiterc_load (Gimp *gimp)
{
  gchar  *filename;
  GError *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  filename = gimp_personal_rc_file ("parasiterc");

  if (gimp->be_verbose)
    g_print ("Parsing '%s'\n", gimp_filename_to_utf8 (filename));

  if (! gimp_config_deserialize_file (GIMP_CONFIG (gimp->parasites),
				      filename, NULL, &error))
    {
      if (error->code != GIMP_CONFIG_ERROR_OPEN_ENOENT)
        g_message (error->message);
      g_error_free (error);
    }

  g_free (filename);
}

void
gimp_parasiterc_save (Gimp *gimp)
{
  const gchar *header =
    "GIMP parasiterc\n"
    "\n"
    "This file will be entirely rewritten every time you quit the gimp.";
  const gchar *footer =
    "end of parasiterc";

  gchar  *filename;
  GError *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_PARASITE_LIST (gimp->parasites));

  filename = gimp_personal_rc_file ("parasiterc");

  if (gimp->be_verbose)
    g_print ("Writing '%s'\n", gimp_filename_to_utf8 (filename));

  if (! gimp_config_serialize_to_file (GIMP_CONFIG (gimp->parasites),
				       filename,
				       header, footer, NULL,
				       &error))
    {
      g_message (error->message);
      g_error_free (error);
    }

  g_free (filename);
}
