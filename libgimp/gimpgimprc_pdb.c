/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpgimprc_pdb.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* NOTE: This file is autogenerated by pdbgen.pl */

#include "config.h"

#include "gimp.h"

/**
 * gimp_gimprc_query:
 * @token: The token to query for.
 *
 * Queries the gimprc file parser for information on a specified token.
 *
 * This procedure is used to locate additional information contained in
 * the gimprc file considered extraneous to the operation of the GIMP.
 * Plug-ins that need configuration information can expect it will be
 * stored in the user gimprc file and can use this procedure to
 * retrieve it. This query procedure will return the value associated
 * with the specified token. This corresponds _only_ to entries with
 * the format: (&lt;token&gt; &lt;value&gt;). The value must be a
 * string. Entries not corresponding to this format will cause warnings
 * to be issued on gimprc parsing and will not be queryable.
 *
 * Returns: The value associated with the queried token.
 */
gchar *
gimp_gimprc_query (const gchar *token)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *value = NULL;

  return_vals = gimp_run_procedure ("gimp_gimprc_query",
				    &nreturn_vals,
				    GIMP_PDB_STRING, token,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    value = g_strdup (return_vals[1].data.d_string);

  gimp_destroy_params (return_vals, nreturn_vals);

  return value;
}

/**
 * gimp_gimprc_set:
 * @token: The token to add or modify.
 * @value: The value to set the token to.
 *
 * Sets a gimprc token to a value and saves it in the gimprc.
 *
 * This procedure is used to add or change additional information in
 * the gimprc file that is considered extraneous to the operation of
 * the GIMP. Plug-ins that need configuration information can use this
 * function to store it, and gimp_gimprc_query to retrieve it. This
 * will accept _only_ string values in UTF-8 encoding.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_gimprc_set (const gchar *token,
		 const gchar *value)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_gimprc_set",
				    &nreturn_vals,
				    GIMP_PDB_STRING, token,
				    GIMP_PDB_STRING, value,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_get_default_comment:
 *
 * Get the default image comment as specified in the Preferences.
 *
 * Returns a copy of the default image comment.
 *
 * Returns: Default Image Comment.
 */
gchar *
gimp_get_default_comment (void)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *comment = NULL;

  return_vals = gimp_run_procedure ("gimp_get_default_comment",
				    &nreturn_vals,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    comment = g_strdup (return_vals[1].data.d_string);

  gimp_destroy_params (return_vals, nreturn_vals);

  return comment;
}

/**
 * gimp_get_monitor_resolution:
 * @xres: X resolution.
 * @yres: Y resolution.
 *
 * Get the monitor resolution as specified in the Preferences.
 *
 * Returns the resolution of the monitor in pixels/inch. This value is
 * taken from the Preferences (or the windowing system if this is set
 * in the Preferences) and there's no guarantee for the value to be
 * reasonable.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_get_monitor_resolution (gdouble *xres,
			     gdouble *yres)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_get_monitor_resolution",
				    &nreturn_vals,
				    GIMP_PDB_END);

  *xres = 0.0;
  *yres = 0.0;

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  if (success)
    {
      *xres = return_vals[1].data.d_float;
      *yres = return_vals[2].data.d_float;
    }

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_get_theme_dir:
 *
 * Get the directory of the current GUI theme.
 *
 * Returns a copy of the current GUI theme dir.
 *
 * Returns: The GUI theme dir.
 */
gchar *
gimp_get_theme_dir (void)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *theme_dir = NULL;

  return_vals = gimp_run_procedure ("gimp_get_theme_dir",
				    &nreturn_vals,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    theme_dir = g_strdup (return_vals[1].data.d_string);

  gimp_destroy_params (return_vals, nreturn_vals);

  return theme_dir;
}

/**
 * gimp_get_module_load_inhibit:
 *
 * Get the list of modules which should not be loaded.
 *
 * Returns a copy of the list of modules which should not be loaded.
 *
 * Returns: The list of modules.
 */
gchar *
gimp_get_module_load_inhibit (void)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *load_inhibit = NULL;

  return_vals = gimp_run_procedure ("gimp_get_module_load_inhibit",
				    &nreturn_vals,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    load_inhibit = g_strdup (return_vals[1].data.d_string);

  gimp_destroy_params (return_vals, nreturn_vals);

  return load_inhibit;
}