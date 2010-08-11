/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

#ifdef G_OS_WIN32
#include <process.h>
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "config/gimpbaseconfig.c"

#include "gimp.h"
#include "gimp-utils.h"


gboolean
gimp_rectangle_intersect (gint  x1,
                          gint  y1,
                          gint  width1,
                          gint  height1,
                          gint  x2,
                          gint  y2,
                          gint  width2,
                          gint  height2,
                          gint *dest_x,
                          gint *dest_y,
                          gint *dest_width,
                          gint *dest_height)
{
  gint d_x, d_y;
  gint d_w, d_h;

  d_x = MAX (x1, x2);
  d_y = MAX (y1, y2);
  d_w = MIN (x1 + width1,  x2 + width2)  - d_x;
  d_h = MIN (y1 + height1, y2 + height2) - d_y;

  if (dest_x)      *dest_x      = d_x;
  if (dest_y)      *dest_y      = d_y;
  if (dest_width)  *dest_width  = d_w;
  if (dest_height) *dest_height = d_h;

  return (d_w > 0 && d_h > 0);
}

gint64
gimp_g_object_get_memsize (GObject *object)
{
  GTypeQuery type_query;
  gint64     memsize = 0;

  g_return_val_if_fail (G_IS_OBJECT (object), 0);

  g_type_query (G_TYPE_FROM_INSTANCE (object), &type_query);

  memsize += type_query.instance_size;

  return memsize;
}

gint64
gimp_g_hash_table_get_memsize (GHashTable *hash)
{
  g_return_val_if_fail (hash != NULL, 0);

  return (2 * sizeof (gint) +
          5 * sizeof (gpointer) +
          g_hash_table_size (hash) * 3 * sizeof (gpointer));
}

gint64
gimp_g_slist_get_memsize (GSList  *slist,
                          gint64   data_size)
{
  return g_slist_length (slist) * (data_size + sizeof (GSList));
}

gint64
gimp_g_list_get_memsize (GList  *list,
                         gint64  data_size)
{
  return g_list_length (list) * (data_size + sizeof (GList));
}

gint64
gimp_g_value_get_memsize (GValue *value)
{
  gint64  memsize = sizeof (GValue);

  if (G_VALUE_HOLDS_STRING (value))
    {
      const gchar *str = g_value_get_string (value);

      if (str)
        memsize += strlen (str) + 1;
    }
  else if (G_VALUE_HOLDS_BOXED (value))
    {
      if (GIMP_VALUE_HOLDS_RGB (value))
        {
          memsize += sizeof (GimpRGB);
        }
      else if (GIMP_VALUE_HOLDS_MATRIX2 (value))
        {
          memsize += sizeof (GimpMatrix2);
        }
      else
        {
          g_printerr ("%s: unhandled boxed value type: %s\n",
                      G_STRFUNC, G_VALUE_TYPE_NAME (value));
        }
    }
  else if (G_VALUE_HOLDS_OBJECT (value))
    {
      g_printerr ("%s: unhandled object value type: %s\n",
                  G_STRFUNC, G_VALUE_TYPE_NAME (value));
    }

  return memsize;
}

/*
 *  basically copied from gtk_get_default_language()
 */
gchar *
gimp_get_default_language (const gchar *category)
{
  gchar *lang;
  gchar *p;
  gint   cat = LC_CTYPE;

  if (! category)
    category = "LC_CTYPE";

#ifdef G_OS_WIN32

  p = getenv ("LC_ALL");
  if (p != NULL)
    lang = g_strdup (p);
  else
    {
      p = getenv ("LANG");
      if (p != NULL)
        lang = g_strdup (p);
      else
        {
          p = getenv (category);
          if (p != NULL)
            lang = g_strdup (p);
          else
            lang = g_win32_getlocale ();
        }
    }

#else

  if (strcmp (category, "LC_CTYPE") == 0)
    cat = LC_CTYPE;
  else if (strcmp (category, "LC_MESSAGES") == 0)
    cat = LC_MESSAGES;
  else
    g_warning ("unsupported category used with gimp_get_default_language()");

  lang = g_strdup (setlocale (cat, NULL));

#endif

  p = strchr (lang, '.');
  if (p)
    *p = '\0';
  p = strchr (lang, '@');
  if (p)
    *p = '\0';

  return lang;
}

GimpUnit
gimp_get_default_unit (void)
{
#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
  const gchar *measurement = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);

  switch (*((guchar *) measurement))
    {
    case 1: /* metric   */
      return GIMP_UNIT_MM;

    case 2: /* imperial */
      return GIMP_UNIT_INCH;
    }
#endif

  return GIMP_UNIT_INCH;
}

gboolean
gimp_boolean_handled_accum (GSignalInvocationHint *ihint,
                            GValue                *return_accu,
                            const GValue          *handler_return,
                            gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = ! signal_handled;

  return continue_emission;
}

GParameter *
gimp_parameters_append (GType       object_type,
                        GParameter *params,
                        gint       *n_params,
                        ...)
{
  va_list args;

  g_return_val_if_fail (g_type_is_a (object_type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (n_params != NULL, NULL);
  g_return_val_if_fail (params != NULL || *n_params == 0, NULL);

  va_start (args, n_params);
  params = gimp_parameters_append_valist (object_type, params, n_params, args);
  va_end (args);

  return params;
}

GParameter *
gimp_parameters_append_valist (GType       object_type,
                               GParameter *params,
                               gint       *n_params,
                               va_list     args)
{
  GObjectClass *object_class;
  gchar        *param_name;

  g_return_val_if_fail (g_type_is_a (object_type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (n_params != NULL, NULL);
  g_return_val_if_fail (params != NULL || *n_params == 0, NULL);

  object_class = g_type_class_ref (object_type);

  param_name = va_arg (args, gchar *);

  while (param_name)
    {
      gchar      *error = NULL;
      GParamSpec *pspec = g_object_class_find_property (object_class,
                                                        param_name);

      if (! pspec)
        {
          g_warning ("%s: object class `%s' has no property named `%s'",
                     G_STRFUNC, g_type_name (object_type), param_name);
          break;
        }

      params = g_renew (GParameter, params, *n_params + 1);

      params[*n_params].name         = param_name;
      params[*n_params].value.g_type = 0;

      g_value_init (&params[*n_params].value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      G_VALUE_COLLECT (&params[*n_params].value, args, 0, &error);

      if (error)
        {
          g_warning ("%s: %s", G_STRFUNC, error);
          g_free (error);
          g_value_unset (&params[*n_params].value);
          break;
        }

      *n_params = *n_params + 1;

      param_name = va_arg (args, gchar *);
    }

  g_type_class_unref (object_class);

  return params;
}

void
gimp_parameters_free (GParameter *params,
                      gint        n_params)
{
  g_return_if_fail (params != NULL || n_params == 0);

  if (params)
    {
      gint i;

      for (i = 0; i < n_params; i++)
        g_value_unset (&params[i].value);

      g_free (params);
    }
}

void
gimp_value_array_truncate (GValueArray  *args,
                           gint          n_values)
{
  gint i;

  g_return_if_fail (args != NULL);
  g_return_if_fail (n_values > 0 && n_values <= args->n_values);

  for (i = args->n_values; i > n_values; i--)
    g_value_array_remove (args, i - 1);
}

gchar *
gimp_get_temp_filename (Gimp        *gimp,
                        const gchar *extension)
{
  static gint  id = 0;
  static gint  pid;
  gchar       *filename;
  gchar       *basename;
  gchar       *path;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (extension != NULL, NULL);

  if (id == 0)
    pid = getpid ();

  basename = g_strdup_printf ("gimp-temp-%d%d.%s", pid, id++, extension);

  path = gimp_config_path_expand (GIMP_BASE_CONFIG (gimp->config)->temp_path,
                                  TRUE, NULL);

  filename = g_build_filename (path, basename, NULL);

  g_free (path);
  g_free (basename);

  return filename;
}
