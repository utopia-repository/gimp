/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpdrawablecombobox.h
 * Copyright (C) 2004 Sven Neumann <sven@gimp.org>
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

#ifndef __GIMP_DRAWABLE_COMBO_BOX_H__
#define __GIMP_DRAWABLE_COMBO_BOX_H__


G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


typedef gboolean (* GimpDrawableConstraintFunc) (gint32   image_id,
                                                 gint32   drawable_id,
                                                 gpointer data);


GtkWidget * gimp_drawable_combo_box_new (GimpDrawableConstraintFunc constraint,
                                         gpointer                   data);
GtkWidget * gimp_channel_combo_box_new  (GimpDrawableConstraintFunc constraint,
                                         gpointer                   data);
GtkWidget * gimp_layer_combo_box_new    (GimpDrawableConstraintFunc constraint,
                                         gpointer                   data);


G_END_DECLS

#endif /* __GIMP_DRAWABLE_COMBO_BOX_H__ */