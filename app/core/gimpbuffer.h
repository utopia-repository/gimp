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

#ifndef __GIMP_BUFFER_H__
#define __GIMP_BUFFER_H__


#include "gimpviewable.h"


#define GIMP_TYPE_BUFFER            (gimp_buffer_get_type ())
#define GIMP_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_BUFFER, GimpBuffer))
#define GIMP_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_BUFFER, GimpBufferClass))
#define GIMP_IS_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_BUFFER))
#define GIMP_IS_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_BUFFER))
#define GIMP_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_BUFFER, GimpBufferClass))


typedef struct _GimpBufferClass GimpBufferClass;

struct _GimpBuffer
{
  GimpViewable  parent_instance;

  TileManager  *tiles;
};

struct _GimpBufferClass
{
  GimpViewableClass  parent_class;
};


GType        gimp_buffer_get_type   (void) G_GNUC_CONST;

GimpBuffer * gimp_buffer_new        (TileManager      *tiles,
                                     const gchar      *name,
                                     gboolean          copy_pixels);

gint         gimp_buffer_get_width  (const GimpBuffer *buffer);
gint         gimp_buffer_get_height (const GimpBuffer *buffer);


#endif /* __GIMP_BUFFER_H__ */
