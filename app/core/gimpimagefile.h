/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpimagefile.h
 *
 * Thumbnail handling according to the Thumbnail Managing Standard.
 * http://triq.net/~pearl/thumbnail-spec/
 *
 * Copyright (C) 2001-2002  Sven Neumann <sven@gimp.org>
 *                          Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_IMAGEFILE_H__
#define __GIMP_IMAGEFILE_H__


#include "gimpviewable.h"


#define GIMP_TYPE_IMAGEFILE            (gimp_imagefile_get_type ())
#define GIMP_IMAGEFILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_IMAGEFILE, GimpImagefile))
#define GIMP_IMAGEFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_IMAGEFILE, GimpImagefileClass))
#define GIMP_IS_IMAGEFILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_IMAGEFILE))
#define GIMP_IS_IMAGEFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_IMAGEFILE))
#define GIMP_IMAGEFILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_IMAGEFILE, GimpImagefileClass))


typedef struct _GimpImagefileClass GimpImagefileClass;

struct _GimpImagefile
{
  GimpViewable        parent_instance;

  Gimp               *gimp;
  GimpThumbnail      *thumbnail;
  gchar              *mime_type;
  gchar              *description;
  gboolean            static_desc;
};

struct _GimpImagefileClass
{
  GimpViewableClass   parent_class;

  void (* info_changed) (GimpImagefile *imagefile);
};


GType           gimp_imagefile_get_type              (void) G_GNUC_CONST;

GimpImagefile * gimp_imagefile_new                   (Gimp          *gimp,
                                                      const gchar   *uri);
void            gimp_imagefile_update                (GimpImagefile *imagefile);
void            gimp_imagefile_create_thumbnail      (GimpImagefile *imagefile,
                                                      GimpContext   *context,
                                                      GimpProgress  *progress,
                                                      gint           size,
                                                      gboolean       replace);
void            gimp_imagefile_create_thumbnail_weak (GimpImagefile *imagefile,
                                                      GimpContext   *context,
                                                      GimpProgress  *progress,
                                                      gint           size,
                                                      gboolean       replace);
gboolean        gimp_imagefile_check_thumbnail       (GimpImagefile *imagefile);
gboolean        gimp_imagefile_save_thumbnail        (GimpImagefile *imagefile,
                                                      const gchar   *mime_type,
                                                      GimpImage     *gimage);
const gchar   * gimp_imagefile_get_desc_string       (GimpImagefile *imagefile);


#endif /* __GIMP_IMAGEFILE_H__ */