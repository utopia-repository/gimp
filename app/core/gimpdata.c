/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdata.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#ifdef G_OS_WIN32
#include "libgimpbase/gimpwin32-io.h"
#endif

#include "core-types.h"

#include "gimpdata.h"
#include "gimpmarshal.h"

#include "gimp-intl.h"

/* Need this test somewhere, might as well do it here */
#ifdef G_OS_WIN32
#if GLIB_CHECK_VERSION (2,6,0)
#error GIMP 2.2 for Win32 must be compiled against GLib 2.4 
#endif
#endif

enum
{
  DIRTY,
  LAST_SIGNAL
};


static void    gimp_data_class_init   (GimpDataClass *klass);
static void    gimp_data_init         (GimpData      *data,
                                       GimpDataClass *data_class);

static void    gimp_data_finalize     (GObject       *object);

static void    gimp_data_name_changed (GimpObject    *object);
static gint64  gimp_data_get_memsize  (GimpObject    *object,
                                       gint64        *gui_size);

static void    gimp_data_real_dirty   (GimpData      *data);


static guint data_signals[LAST_SIGNAL] = { 0 };

static GimpViewableClass *parent_class = NULL;


GType
gimp_data_get_type (void)
{
  static GType data_type = 0;

  if (! data_type)
    {
      static const GTypeInfo data_info =
      {
        sizeof (GimpDataClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_data_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpData),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_data_init,
      };

      data_type = g_type_register_static (GIMP_TYPE_VIEWABLE,
                                          "GimpData",
                                          &data_info, 0);
  }

  return data_type;
}

static void
gimp_data_class_init (GimpDataClass *klass)
{
  GObjectClass    *object_class      = G_OBJECT_CLASS (klass);
  GimpObjectClass *gimp_object_class = GIMP_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  data_signals[DIRTY] =
    g_signal_new ("dirty",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDataClass, dirty),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  object_class->finalize          = gimp_data_finalize;

  gimp_object_class->name_changed = gimp_data_name_changed;
  gimp_object_class->get_memsize  = gimp_data_get_memsize;

  klass->dirty                    = gimp_data_real_dirty;
  klass->save                     = NULL;
  klass->get_extension            = NULL;
  klass->duplicate                = NULL;
}

static void
gimp_data_init (GimpData      *data,
                GimpDataClass *data_class)
{
  data->filename     = NULL;
  data->writable     = TRUE;
  data->deletable    = TRUE;
  data->dirty        = TRUE;
  data->internal     = FALSE;
  data->freeze_count = 0;

  /*  look at the passed class pointer, not at GIMP_DATA_GET_CLASS(data)
   *  here, because the latter is always GimpDataClass itself
   */
  if (! data_class->save)
    data->writable = FALSE;
}

static void
gimp_data_finalize (GObject *object)
{
  GimpData *data = GIMP_DATA (object);

  if (data->filename)
    {
      g_free (data->filename);
      data->filename = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_data_name_changed (GimpObject *object)
{
  if (GIMP_OBJECT_CLASS (parent_class)->name_changed)
    GIMP_OBJECT_CLASS (parent_class)->name_changed (object);

  gimp_data_dirty (GIMP_DATA (object));
}

static gint64
gimp_data_get_memsize (GimpObject *object,
                       gint64     *gui_size)
{
  GimpData *data    = GIMP_DATA (object);
  gint64    memsize = 0;

  if (data->filename)
    memsize += strlen (data->filename) + 1;

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object,
                                                                  gui_size);
}

static void
gimp_data_real_dirty (GimpData *data)
{
  data->dirty = TRUE;

  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (data));
}

/**
 * gimp_data_save:
 * @data:  object whose contents are to be saved.
 * @error: return location for errors or %NULL
 *
 * Save the object.  If the object is marked as "internal", nothing happens.
 * Otherwise, it is saved to disk, using the file name set by
 * gimp_data_set_filename().  If the save is successful, the
 * object is marked as not dirty.  If not, an error message is returned
 * using the @error argument.
 *
 * Returns: %TRUE if the object is internal or the save is successful.
 **/
gboolean
gimp_data_save (GimpData  *data,
                GError   **error)
{
  gboolean success = FALSE;

  g_return_val_if_fail (GIMP_IS_DATA (data), FALSE);
  g_return_val_if_fail (data->writable == TRUE, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (data->internal)
    {
      data->dirty = FALSE;
      return TRUE;
    }

  g_return_val_if_fail (data->filename != NULL, FALSE);

  if (GIMP_DATA_GET_CLASS (data)->save)
    success = GIMP_DATA_GET_CLASS (data)->save (data, error);

  if (success)
    data->dirty = FALSE;

  return success;
}

/**
 * gimp_data_dirty:
 * @data: a #GimpData object.
 *
 * Marks @data as dirty.  Unless the object is frozen, this causes
 * its preview to be invalidated, and emits a "dirty" signal.  If the
 * object is frozen, the function has no effect.
 **/
void
gimp_data_dirty (GimpData *data)
{
  g_return_if_fail (GIMP_IS_DATA (data));

  if (data->freeze_count == 0)
    g_signal_emit (data, data_signals[DIRTY], 0);
}

/**
 * gimp_data_freeze:
 * @data: a #GimpData object.
 *
 * Increments the freeze count for the object.  A positive freeze count
 * prevents the object from being treated as dirty.  Any call to this
 * function must be followed eventually by a call to gimp_data_thaw().
 **/
void
gimp_data_freeze (GimpData *data)
{
  g_return_if_fail (GIMP_IS_DATA (data));

  data->freeze_count++;
}

/**
 * gimp_data_thaw:
 * @data: a #GimpData object.
 *
 * Decrements the freeze count for the object.  If the freeze count
 * drops to zero, the object is marked as dirty, and the "dirty"
 * signal is emitted.  It is an error to call this function without
 * having previously called gimp_data_freeze().
 **/
void
gimp_data_thaw (GimpData *data)
{
  g_return_if_fail (GIMP_IS_DATA (data));
  g_return_if_fail (data->freeze_count > 0);

  data->freeze_count--;

  if (data->freeze_count == 0)
    gimp_data_dirty (data);
}

/**
 * gimp_data_delete_from_disk:
 * @data:  a #GimpData object.
 * @error: return location for errors or %NULL
 *
 * Deletes the object from disk.  If the object is marked as "internal",
 * nothing happens.  Otherwise, if the file exists whose name has been
 * set by gimp_data_set_filename(), it is deleted.  Obviously this is
 * a potentially dangerous function, which should be used with care.
 *
 * Returns: %TRUE if the object is internal to Gimp, or the deletion is
 *          successful.
 **/
gboolean
gimp_data_delete_from_disk (GimpData  *data,
                            GError   **error)
{
  g_return_val_if_fail (GIMP_IS_DATA (data), FALSE);
  g_return_val_if_fail (data->filename != NULL, FALSE);
  g_return_val_if_fail (data->deletable == TRUE, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (data->internal)
    return TRUE;

  if (unlink (data->filename) == -1)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_DELETE,
                   _("Could not delete '%s': %s"),
                   gimp_filename_to_utf8 (data->filename), g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

const gchar *
gimp_data_get_extension (GimpData *data)
{
  g_return_val_if_fail (GIMP_IS_DATA (data), NULL);

  if (GIMP_DATA_GET_CLASS (data)->get_extension)
    return GIMP_DATA_GET_CLASS (data)->get_extension (data);

  return NULL;
}

/**
 * gimp_data_set_filename:
 * @data:     A #GimpData object
 * @filename: File name to assign to @data.
 * @writable: %TRUE if we want to be able to write to this file.
 * @deletable: %TRUE if we want to be able to delete this file.
 *
 * This function assigns a file name to @data, and sets some flags
 * according to the properties of the file.  If @writable is %TRUE,
 * and the user has permission to write or overwrite the requested file
 * name, and a "save" method exists for @data's object type, then
 * @data is marked as writable.
 **/
void
gimp_data_set_filename (GimpData    *data,
                        const gchar *filename,
                        gboolean     writable,
                        gboolean     deletable)
{
  g_return_if_fail (GIMP_IS_DATA (data));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (g_path_is_absolute (filename));

  if (data->internal)
    return;

  if (data->filename)
    g_free (data->filename);

  data->filename  = g_strdup (filename);
  data->writable  = FALSE;
  data->deletable = FALSE;

  /*  if the data is supposed to be writable or deletable,
   *  still check if it really is
   */
  if (writable || deletable)
    {
      gchar *dirname = g_path_get_dirname (filename);

      if ((access (filename, F_OK) == 0 &&  /* check if the file exists    */
           access (filename, W_OK) == 0) || /* and is writable             */
          (access (filename, F_OK) != 0 &&  /* OR doesn't exist            */
           access (dirname,  W_OK) == 0))   /* and we can write to its dir */
        {
          data->writable  = writable  ? TRUE : FALSE;
          data->deletable = deletable ? TRUE : FALSE;
        }

      g_free (dirname);

      /*  if we can't save, we are not writable  */
      if (! GIMP_DATA_GET_CLASS (data)->save)
        data->writable = FALSE;
    }
}

/**
 * gimp_data_create_filename:
 * @data:     a #Gimpdata object.
 * @dest_dir: directory in which to create a file name.
 *
 * This function creates a unique file name to be used for saving
 * a representation of @data in the directory @dest_dir.  If the
 * user does not have write permission in @dest_dir, then @data
 * is marked as "not writable", so you should check on this before
 * assuming that @data can be saved.
 **/
void
gimp_data_create_filename (GimpData    *data,
                           const gchar *dest_dir)
{
  gchar  *safename;
  gchar  *filename;
  gchar  *fullpath;
  gint    i;
  gint    unum  = 1;
  GError *error = NULL;

  g_return_if_fail (GIMP_IS_DATA (data));
  g_return_if_fail (dest_dir != NULL);
  g_return_if_fail (g_path_is_absolute (dest_dir));

  if (data->internal)
    return;

#ifdef G_OS_WIN32
  {
    const gchar *charset;

    filename = g_strdup (gimp_object_get_name (GIMP_OBJECT (data)));

    /* Map illegal characters to '-' while the name is still in UTF-8 */
    for (i = 0; filename[i]; i++)
      if (strchr ("<>:\"/\\|", filename[i]) ||
	  g_ascii_iscntrl (filename[i]) ||
	  g_ascii_isspace (filename[i]))
	filename[i] = '-';

    /* Map also trailing periods to '-' */
    for (i = strlen (filename) - 1; i >= 0; i--)
      if (filename[i] == '.')
	filename[i] = '-';
      else
	break;

    /* Next convert to the filename charset. Note that this branch of
     * GIMP must be compiled against GLib 2.4 and we thus use system
     * codepage for file names in the GLib API.
     */
    g_get_charset (&charset);
    safename = g_convert_with_fallback (filename,
					-1, charset, "UTF-8",
					"-", NULL, NULL, &error);
    if (! safename)
      {
	g_warning ("gimp_data_create_filename:\n"
		   "g_convert_with_fallback() failed for '%s': %s",
		   filename, error->message);
	g_free (filename);
	g_error_free (error);
	return;
      }
    g_free (filename);
  }
#else
  safename = g_filename_from_utf8 (gimp_object_get_name (GIMP_OBJECT (data)),
                                   -1, NULL, NULL, &error);
  if (! safename)
    {
      g_warning ("gimp_data_create_filename:\n"
                 "g_filename_from_utf8() failed for '%s': %s",
                 gimp_object_get_name (GIMP_OBJECT (data)), error->message);
      g_error_free (error);
      return;
    }

  if (safename[0] == '.')
    safename[0] = '-';

  for (i = 0; safename[i]; i++)
    if (safename[i] == G_DIR_SEPARATOR || g_ascii_isspace (safename[i]))
      safename[i] = '-';
#endif

  filename = g_strconcat (safename, gimp_data_get_extension (data), NULL);

  fullpath = g_build_filename (dest_dir, filename, NULL);

  g_free (filename);

  while (g_file_test (fullpath, G_FILE_TEST_EXISTS))
    {
      g_free (fullpath);

      filename = g_strdup_printf ("%s-%d%s",
                                  safename,
                                  unum++,
                                  gimp_data_get_extension (data));

      fullpath = g_build_filename (dest_dir, filename, NULL);

      g_free (filename);
    }

  g_free (safename);

  gimp_data_set_filename (data, fullpath, TRUE, TRUE);

  g_free (fullpath);
}

/**
 * gimp_data_duplicate:
 * @data:              a #GimpData object
 * @stingy_memory_use: if %TRUE, use the disk rather than RAM
 *                     where possible.
 *
 * Creates a copy of @data, if possible.  Only the object data is
 * copied:  the newly created object is not automatically given an
 * object name, file name, preview, etc.
 *
 * Returns: the newly created copy, or %NULL if @data cannot be copied.
 **/
GimpData *
gimp_data_duplicate (GimpData *data,
                     gboolean  stingy_memory_use)
{
  g_return_val_if_fail (GIMP_IS_DATA (data), NULL);

  if (GIMP_DATA_GET_CLASS (data)->duplicate)
    return GIMP_DATA_GET_CLASS (data)->duplicate (data, stingy_memory_use);

  return NULL;
}

/**
 * gimp_data_make_internal:
 * @data: a #GimpData object.
 *
 * Mark @data as "internal" to Gimp, which means that it will not be
 * saved to disk.  Note that if you do this, later calls to
 * gimp_data_save() and gimp_data_delete_from_disk() will
 * automatically return successfully without giving any warning.
 **/
void
gimp_data_make_internal (GimpData *data)
{
  g_return_if_fail (GIMP_IS_DATA (data));

  if (data->filename)
    {
      g_free (data->filename);
      data->filename = NULL;
    }

  data->internal  = TRUE;
  data->writable  = FALSE;
  data->deletable = FALSE;
}

/**
 * gimp_data_name_compare:
 * @data1: a #GimpData object.
 * @data2: another #GimpData object.
 *
 * Compares the names of the two objects for use in sorting; see
 * gimp_object_name_collate() for the method.  Objects marked as
 * "internal" are considered to come before any objects that are not.
 *
 * Return value: -1 if @data1 compares before @data2,
 *                0 if they compare equal,
 *                1 if @data1 compares after @data2.
 **/
gint
gimp_data_name_compare (GimpData *data1,
                        GimpData *data2)
{
  /*  move the internal objects (like the FG -> BG) gradient) to the top  */
  if (data1->internal != data2->internal)
    return data1->internal ? -1 : 1;

  return gimp_object_name_collate ((GimpObject *) data1,
                                   (GimpObject *) data2);
}

/**
 * gimp_data_error_quark:
 *
 * This function is used to implement the GIMP_DATA_ERROR macro. It
 * shouldn't be called directly.
 *
 * Return value: the #GQuark to identify error in the GimpData error domain.
 **/
GQuark
gimp_data_error_quark (void)
{
  static GQuark quark = 0;

  if (! quark)
    quark = g_quark_from_static_string ("gimp-data-error-quark");

  return quark;
}
