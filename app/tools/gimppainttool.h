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

#ifndef __GIMP_PAINT_TOOL_H__
#define __GIMP_PAINT_TOOL_H__


#include "gimpcolortool.h"


#define GIMP_TYPE_PAINT_TOOL            (gimp_paint_tool_get_type ())
#define GIMP_PAINT_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PAINT_TOOL, GimpPaintTool))
#define GIMP_PAINT_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PAINT_TOOL, GimpPaintToolClass))
#define GIMP_IS_PAINT_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PAINT_TOOL))
#define GIMP_IS_PAINT_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PAINT_TOOL))
#define GIMP_PAINT_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PAINT_TOOL, GimpPaintToolClass))


typedef struct _GimpPaintToolClass GimpPaintToolClass;

struct _GimpPaintTool
{
  GimpColorTool    parent_instance;

  gboolean         pick_colors;  /*  pick color if ctrl is pressed   */
  gboolean         draw_line;

  gboolean         show_cursor;
  gboolean         draw_brush;
  gdouble          brush_x;
  gdouble          brush_y;

  GimpPaintCore   *core;
};

struct _GimpPaintToolClass
{
  GimpColorToolClass  parent_class;
};


GType   gimp_paint_tool_get_type            (void) G_GNUC_CONST;

void    gimp_paint_tool_enable_color_picker (GimpPaintTool     *tool,
                                             GimpColorPickMode  mode);


#endif  /*  __GIMP_PAINT_TOOL_H__  */