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

#ifndef __GIMP_THUMB_BOX_H__
#define __GIMP_THUMB_BOX_H__


#define GIMP_TYPE_THUMB_BOX            (gimp_thumb_box_get_type ())
#define GIMP_THUMB_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_THUMB_BOX, GimpThumbBox))
#define GIMP_THUMB_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_THUMB_BOX, GimpThumbBoxClass))
#define GIMP_IS_THUMB_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_THUMB_BOX))
#define GIMP_IS_THUMB_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_THUMB_BOX))
#define GIMP_THUMB_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_THUMB_BOX, GimpThumbBoxClass))


typedef struct _GimpThumbBoxClass GimpThumbBoxClass;

struct _GimpThumbBox
{
  GtkFrame       parent_instance;

  GimpImagefile *imagefile;
  GSList        *uris;

  GtkWidget     *preview;
  GtkWidget     *filename;
  GtkWidget     *info;
  GtkWidget     *thumb_progress;

  gboolean       progress_active;
  GtkWidget     *progress;

  guint          idle_id;
};

struct _GimpThumbBoxClass
{
  GtkFrameClass  parent_class;
};


GType       gimp_thumb_box_get_type  (void) G_GNUC_CONST;

GtkWidget * gimp_thumb_box_new       (Gimp         *gimp);

void        gimp_thumb_box_set_uri   (GimpThumbBox *box,
                                      const gchar  *uri);
void        gimp_thumb_box_take_uris (GimpThumbBox *box,
                                      GSList       *uris);


#endif  /*  __GIMP_THUMB_BOX_H__  */