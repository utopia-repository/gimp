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

#ifndef __GIMP_CROP_OPTIONS_H__
#define __GIMP_CROP_OPTIONS_H__


#include "core/gimptooloptions.h"


#define GIMP_TYPE_CROP_OPTIONS            (gimp_crop_options_get_type ())
#define GIMP_CROP_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CROP_OPTIONS, GimpCropOptions))
#define GIMP_CROP_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CROP_OPTIONS, GimpCropOptionsClass))
#define GIMP_IS_CROP_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CROP_OPTIONS))
#define GIMP_IS_CROP_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CROP_OPTIONS))
#define GIMP_CROP_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CROP_OPTIONS, GimpCropOptionsClass))


typedef struct _GimpCropOptions      GimpCropOptions;
typedef struct _GimpToolOptionsClass GimpCropOptionsClass;

struct _GimpCropOptions
{
  GimpToolOptions  parent_instence;

  gboolean         layer_only;
  gboolean         allow_enlarge;
  gboolean         keep_aspect;
  GimpCropMode     crop_mode;
};


GType       gimp_crop_options_get_type (void) G_GNUC_CONST;

GtkWidget * gimp_crop_options_gui      (GimpToolOptions *tool_options);


#endif /* __GIMP_CROP_OPTIONS_H__ */