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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include "appenv.h"
#include "edit_selection.h"
#include "ellipse_select.h"
#include "gdisplay.h"
#include "rect_select.h"
/*  private header file for rect_select data structure  */
#include "rect_selectP.h"

#define NO  0
#define YES 1

/*************************************/
/*  Ellipsoidal selection apparatus  */

/*  ellipse select action functions  */

static void  ellipse_dialog_callback (int, int, void *, void *);

long                antialiasing = NO;   /*  rect_select accesses this variable  */
static long         antialiasing_value;
static AutoDialog   ellipse_dlg = NULL;
static int          antialiasing_ID;


void
ellipse_dialog ()
{
  int group_ID;

  if (!ellipse_dlg)
    {
      ellipse_dlg = dialog_new ("Ellipse Select Options", ellipse_dialog_callback, NULL);
      
      group_ID = dialog_new_item (ellipse_dlg, 0, GROUP_ROWS, NULL, NULL);

      antialiasing_value = antialiasing;
      antialiasing_ID = dialog_new_item (ellipse_dlg, group_ID, ITEM_CHECK_BUTTON, 
					 "Anti-Aliasing", NULL);
      dialog_change_item (ellipse_dlg, antialiasing_ID, &antialiasing_value);

      dialog_show (ellipse_dlg);
    }
}


void
ellipse_select_draw (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  EllipseSelect * ellipse_sel;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  ellipse_sel = (EllipseSelect *) tool->private;

  x1 = MINIMUM (ellipse_sel->x, ellipse_sel->x + ellipse_sel->w);
  y1 = MINIMUM (ellipse_sel->y, ellipse_sel->y + ellipse_sel->h);
  x2 = MAXIMUM (ellipse_sel->x, ellipse_sel->x + ellipse_sel->w);
  y2 = MAXIMUM (ellipse_sel->y, ellipse_sel->y + ellipse_sel->h);

  gdisplay_transform_coords (gdisp, x1, y1, &x1, &y1);
  gdisplay_transform_coords (gdisp, x2, y2, &x2, &y2);

  XDrawArc (DISPLAY, ellipse_sel->core->win,
	    ellipse_sel->core->gc,
	    x1, y1,
	    (x2 - x1), (y2 - y1), 0, 23040);
}


Tool *
tools_new_ellipse_select ()
{
  Tool * tool;
  EllipseSelect * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (EllipseSelect *) xmalloc (sizeof (EllipseSelect));

  private->core = draw_core_new (ellipse_select_draw);
  /*  Make the selection static, not blinking  */
  private->x = private->y = 0;
  private->w = private->h = 0;

  tool->type = ELLIPSE_SELECT;
  tool->state = INACTIVE;
  tool->scroll_lock = 0;  /*  Allow scrolling  */
  tool->private = (void *) private;
  tool->button_press_func = rect_select_button_press;
  tool->button_release_func = rect_select_button_release;
  tool->motion_func = rect_select_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = rect_select_control;

  return tool;
}


void
tools_free_ellipse_select (tool)
     Tool * tool;
{
  EllipseSelect * ellipse_sel;

  ellipse_sel = (EllipseSelect *) tool->private;

  draw_core_free (ellipse_sel->core);
  xfree (ellipse_sel);
 
}


/*  ellipse dialog callback function  */

static void
ellipse_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (ellipse_dlg);
      ellipse_dlg = NULL;
      break;
    case CANCEL_ID:
      dialog_close (ellipse_dlg);
      ellipse_dlg = NULL;
      antialiasing = (int) antialiasing_value;
      break;
    default:
      if (item_ID == antialiasing_ID)
	antialiasing = *((long*) call_data);
      break;
    }
}



