/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
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

#include <string.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimp.h"
#include "gimp-templates.h"
#include "gimplist.h"
#include "gimptemplate.h"


/* functions to load and save the gimp templates files */

void
gimp_templates_load (Gimp *gimp)
{
  gchar  *filename;
  GError *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_LIST (gimp->templates));

  filename = gimp_personal_rc_file ("templaterc");

  if (gimp->be_verbose)
    g_print ("Parsing '%s'\n", gimp_filename_to_utf8 (filename));

  if (!gimp_config_deserialize_file (GIMP_CONFIG (gimp->templates),
				     filename, NULL, &error))
    {
      if (error->code == GIMP_CONFIG_ERROR_OPEN_ENOENT)
        {
          g_clear_error (&error);
          g_free (filename);

          filename = g_build_filename (gimp_sysconf_directory (),
                                       "templaterc", NULL);

          if (!gimp_config_deserialize_file (GIMP_CONFIG (gimp->templates),
                                             filename, NULL, &error))
            {
              g_message (error->message);
            }
        }
      else
        {
          g_message (error->message);
        }

      g_clear_error (&error);
    }

  gimp_list_reverse (GIMP_LIST (gimp->templates));

  g_free (filename);
}

void
gimp_templates_save (Gimp *gimp)
{
  const gchar *header =
    "GIMP templaterc\n"
    "\n"
    "This file will be entirely rewritten every time you quit the gimp.";
  const gchar *footer =
    "end of templaterc";

  gchar  *filename;
  GError *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_LIST (gimp->templates));

  filename = gimp_personal_rc_file ("templaterc");

  if (gimp->be_verbose)
    g_print ("Writing '%s'\n", gimp_filename_to_utf8 (filename));

  if (! gimp_config_serialize_to_file (GIMP_CONFIG (gimp->templates),
				       filename,
				       header, footer, NULL,
				       &error))
    {
      g_message (error->message);
      g_error_free (error);
    }

  g_free (filename);
}


/*  just like gimp_list_get_child_by_name() but matches case-insensitive  */
static GimpObject *
gimp_templates_migrate_get_child_by_name (const GimpContainer *container,
                                          const gchar         *name)
{
  GimpList *list = GIMP_LIST (container);
  GList    *glist;

  for (glist = list->list; glist; glist = g_list_next (glist))
    {
      GimpObject *object = glist->data;

      if (! g_ascii_strcasecmp (object->name, name))
        return object;
    }

  return NULL;
}

/**
 * gimp_templates_migrate:
 * @olddir: the old user directory
 *
 * Migrating the templaterc from GIMP 2.0 to GIMP 2.2 needs this special
 * hack since we changed the way that units are handled. This function
 * merges the user's templaterc with the systemwide templaterc. The goal
 * is to replace the unit for a couple of default templates with "pixels".
 **/
void
gimp_templates_migrate (const gchar *olddir)
{
  GimpContainer *templates = gimp_list_new (GIMP_TYPE_TEMPLATE, TRUE);
  gchar         *filename  = gimp_personal_rc_file ("templaterc");

  if (gimp_config_deserialize_file (GIMP_CONFIG (templates), filename,
                                    NULL, NULL))
    {
      gchar *tmp = g_build_filename (gimp_sysconf_directory (),
                                     "templaterc", NULL);

      if (olddir && strstr (olddir, "2.0"))
        {
          /* We changed the spelling of a couple of template names
           * from upper to lower case between 2.0 and 2.2.
           */
          GimpContainerClass *class = GIMP_CONTAINER_GET_CLASS (templates);
          gpointer            func  = class->get_child_by_name;

          class->get_child_by_name = gimp_templates_migrate_get_child_by_name;

          gimp_config_deserialize_file (GIMP_CONFIG (templates),
                                        tmp, NULL, NULL);

          class->get_child_by_name = func;
        }
      else
        {
          gimp_config_deserialize_file (GIMP_CONFIG (templates),
                                        tmp, NULL, NULL);
        }

      g_free (tmp);

      gimp_list_reverse (GIMP_LIST (templates));

      gimp_config_serialize_to_file (GIMP_CONFIG (templates), filename,
                                     NULL, NULL, NULL, NULL);
    }

  g_free (filename);
}
