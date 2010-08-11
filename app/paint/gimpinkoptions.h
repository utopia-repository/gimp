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

#ifndef  __GIMP_INK_OPTIONS_H__
#define  __GIMP_INK_OPTIONS_H__


#include "gimppaintoptions.h"


#define GIMP_TYPE_INK_OPTIONS            (gimp_ink_options_get_type ())
#define GIMP_INK_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_INK_OPTIONS, GimpInkOptions))
#define GIMP_INK_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_INK_OPTIONS, GimpInkOptionsClass))
#define GIMP_IS_INK_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_INK_OPTIONS))
#define GIMP_IS_INK_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_INK_OPTIONS))
#define GIMP_INK_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_INK_OPTIONS, GimpInkOptionsClass))


typedef struct _GimpInkOptions        GimpInkOptions;
typedef struct _GimpPaintOptionsClass GimpInkOptionsClass;

struct _GimpInkOptions
{
  GimpPaintOptions  paint_options;

  gdouble           size;
  gdouble           tilt_angle;

  gdouble           size_sensitivity;
  gdouble           vel_sensitivity;
  gdouble           tilt_sensitivity;

  GimpInkBlobType   blob_type;
  gdouble           blob_aspect;
  gdouble           blob_angle;
};


GType   gimp_ink_options_get_type (void) G_GNUC_CONST;


#endif  /*  __GIMP_INK_OPTIONS_H__  */
