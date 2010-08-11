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

#ifndef  __GIMP_RECTANGLE_TOOL_H__
#define  __GIMP_RECTANGLE_TOOL_H__

#include "gimptool.h"


typedef enum
{
  GIMP_RECTANGLE_TOOL_PROP_0,
  GIMP_RECTANGLE_TOOL_PROP_CONTROLS,
  GIMP_RECTANGLE_TOOL_PROP_DIMENSIONS_ENTRY,
  GIMP_RECTANGLE_TOOL_PROP_PRESSX,
  GIMP_RECTANGLE_TOOL_PROP_PRESSY,
  GIMP_RECTANGLE_TOOL_PROP_X1,
  GIMP_RECTANGLE_TOOL_PROP_Y1,
  GIMP_RECTANGLE_TOOL_PROP_X2,
  GIMP_RECTANGLE_TOOL_PROP_Y2,
  GIMP_RECTANGLE_TOOL_PROP_FUNCTION,
  GIMP_RECTANGLE_TOOL_PROP_CONSTRAIN,
  GIMP_RECTANGLE_TOOL_PROP_LAST = GIMP_RECTANGLE_TOOL_PROP_CONSTRAIN
} GimpRectangleToolProp;


#define GIMP_TYPE_RECTANGLE_TOOL               (gimp_rectangle_tool_interface_get_type ())
#define GIMP_IS_RECTANGLE_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_RECTANGLE_TOOL))
#define GIMP_RECTANGLE_TOOL(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_RECTANGLE_TOOL, GimpRectangleTool))
#define GIMP_RECTANGLE_TOOL_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GIMP_TYPE_RECTANGLE_TOOL, GimpRectangleToolInterface))


/*  possible functions  */
enum
{
  RECT_INACTIVE,
  RECT_CREATING,
  RECT_MOVING,
  RECT_RESIZING_UPPER_LEFT,
  RECT_RESIZING_UPPER_RIGHT,
  RECT_RESIZING_LOWER_LEFT,
  RECT_RESIZING_LOWER_RIGHT,
  RECT_RESIZING_LEFT,
  RECT_RESIZING_RIGHT,
  RECT_RESIZING_TOP,
  RECT_RESIZING_BOTTOM,
  RECT_EXECUTING
};


typedef struct _GimpRectangleTool          GimpRectangleTool;
typedef struct _GimpRectangleToolInterface GimpRectangleToolInterface;

struct _GimpRectangleToolInterface
{
  GTypeInterface base_iface;

  gboolean (* execute)           (GimpRectangleTool *rect_tool,
                                  gint               x,
                                  gint               y,
                                  gint               w,
                                  gint               h);
  void     (* cancel)            (GimpRectangleTool *rect_tool);

  gboolean (* rectangle_changed) (GimpRectangleTool *rect_tool);
};


GType       gimp_rectangle_tool_interface_get_type  (void) G_GNUC_CONST;


void        gimp_rectangle_tool_constructor         (GObject           *object);
void        gimp_rectangle_tool_dispose             (GObject           *object);
gboolean    gimp_rectangle_tool_initialize          (GimpTool          *tool,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_control             (GimpTool          *tool,
                                                     GimpToolAction     action,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_button_press        (GimpTool          *tool,
                                                     GimpCoords        *coords,
                                                     guint32            time,
                                                     GdkModifierType    state,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_button_release      (GimpTool          *tool,
                                                     GimpCoords        *coords,
                                                     guint32            time,
                                                     GdkModifierType    state,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_motion              (GimpTool          *tool,
                                                     GimpCoords        *coords,
                                                     guint32            time,
                                                     GdkModifierType    state,
                                                     GimpDisplay       *display);
gboolean    gimp_rectangle_tool_key_press           (GimpTool          *tool,
                                                     GdkEventKey       *kevent,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_oper_update         (GimpTool          *tool,
                                                     GimpCoords        *coords,
                                                     GdkModifierType    state,
                                                     gboolean           proximity,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_cursor_update       (GimpTool          *tool,
                                                     GimpCoords        *coords,
                                                     GdkModifierType    state,
                                                     GimpDisplay       *display);
void        gimp_rectangle_tool_draw                (GimpDrawTool      *draw);
gboolean    gimp_rectangle_tool_execute             (GimpRectangleTool *rect_tool);
void        gimp_rectangle_tool_cancel              (GimpRectangleTool *rect_tool);
void        gimp_rectangle_tool_halt                (GimpRectangleTool *rectangle);
void        gimp_rectangle_tool_configure           (GimpRectangleTool *rectangle);
void        gimp_rectangle_tool_set_constrain       (GimpRectangleTool *rectangle,
                                                     gboolean           constrain);
gboolean    gimp_rectangle_tool_no_movement         (GimpRectangleTool *rectangle);

/*  convenience functions  */

void        gimp_rectangle_tool_install_properties  (GObjectClass *klass);
void        gimp_rectangle_tool_set_property        (GObject      *object,
                                                     guint         property_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec);
void        gimp_rectangle_tool_get_property        (GObject      *object,
                                                     guint         property_id,
                                                     GValue       *value,
                                                     GParamSpec   *pspec);


#endif  /*  __GIMP_RECTANGLE_TOOL_H__  */
