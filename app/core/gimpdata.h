/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdata.h
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

#ifndef __GIMP_DATA_H__
#define __GIMP_DATA_H__


#include "gimpviewable.h"


typedef enum
{
  GIMP_DATA_ERROR_OPEN,   /*  opening data file failed   */
  GIMP_DATA_ERROR_READ,   /*  reading data file failed   */
  GIMP_DATA_ERROR_WRITE,  /*  writing data file failed   */
  GIMP_DATA_ERROR_DELETE  /*  deleting data file failed  */
} GimpDataError;


#define GIMP_TYPE_DATA            (gimp_data_get_type ())
#define GIMP_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DATA, GimpData))
#define GIMP_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DATA, GimpDataClass))
#define GIMP_IS_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DATA))
#define GIMP_IS_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DATA))
#define GIMP_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DATA, GimpDataClass))


typedef struct _GimpDataClass GimpDataClass;

struct _GimpData
{
  GimpViewable  parent_instance;

  gchar        *filename;
  guint         writable  : 1;
  guint         deletable : 1;
  guint         dirty     : 1;
  guint         internal  : 1;
  gint          freeze_count;
};

struct _GimpDataClass
{
  GimpViewableClass  parent_class;

  /*  signals  */
  void       (* dirty)         (GimpData  *data);

  /*  virtual functions  */
  gboolean   (* save)          (GimpData  *data,
                                GError   **error);
  gchar    * (* get_extension) (GimpData  *data);
  GimpData * (* duplicate)     (GimpData  *data,
                                gboolean   stingy_memory_use);
};


GType         gimp_data_get_type         (void) G_GNUC_CONST;

gboolean      gimp_data_save             (GimpData     *data,
                                          GError      **error);

void          gimp_data_dirty            (GimpData     *data);
void          gimp_data_freeze           (GimpData     *data);
void          gimp_data_thaw             (GimpData     *data);

gboolean      gimp_data_delete_from_disk (GimpData     *data,
                                          GError      **error);

const gchar * gimp_data_get_extension    (GimpData     *data);

void          gimp_data_set_filename     (GimpData     *data,
                                          const gchar  *filename,
                                          gboolean      writable,
                                          gboolean      deletable);
void          gimp_data_create_filename  (GimpData     *data,
					  const gchar  *dest_dir);

GimpData    * gimp_data_duplicate        (GimpData     *data,
                                          gboolean      stingy_memory_use);

void          gimp_data_make_internal    (GimpData     *data);

gint          gimp_data_name_compare     (GimpData     *data1,
                                          GimpData     *data2);

#define GIMP_DATA_ERROR (gimp_data_error_quark ())

GQuark        gimp_data_error_quark      (void) G_GNUC_CONST;


#endif /* __GIMP_DATA_H__ */