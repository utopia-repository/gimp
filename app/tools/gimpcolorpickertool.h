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

#ifndef  __GIMP_COLOR_PICKER_TOOL_H__
#define  __GIMP_COLOR_PICKER_TOOL_H__


#include "gimpcolortool.h"


#define GIMP_TYPE_COLOR_PICKER_TOOL            (gimp_color_picker_tool_get_type ())
#define GIMP_COLOR_PICKER_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_COLOR_PICKER_TOOL, GimpColorPickerTool))
#define GIMP_COLOR_PICKER_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_COLOR_PICKER_TOOL, GimpColorPickerToolClass))
#define GIMP_IS_COLOR_PICKER_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_COLOR_PICKER_TOOL))
#define GIMP_IS_COLOR_PICKER_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_COLOR_PICKER_TOOL))
#define GIMP_COLOR_PICKER_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_COLOR_PICKER_TOOL, GimpColorPickerToolClass))


typedef struct _GimpColorPickerTool      GimpColorPickerTool;
typedef struct _GimpColorPickerToolClass GimpColorPickerToolClass;

struct _GimpColorPickerTool
{
  GimpColorTool  parent_instance;

  GtkWidget     *dialog;
  GtkWidget     *color_area;
  GtkWidget     *color_frame1;
  GtkWidget     *color_frame2;
};

struct _GimpColorPickerToolClass
{
  GimpColorToolClass  parent_class;
};


void    gimp_color_picker_tool_register (GimpToolRegisterCallback  callback,
                                         gpointer                  data);

GType   gimp_color_picker_tool_get_type (void) G_GNUC_CONST;


#endif  /*  __GIMP_COLOR_PICKER_TOOL_H__  */