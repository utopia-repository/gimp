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
#include "gdisplay.h"
#include "edit_selection.h"
#include "rect_select.h"
#include "rect_selectP.h"

extern long antialiasing;  /*  ellipse select antialiasing  */

/*************************************/
/*  Rectangular selection apparatus  */


/*  rect select action functions  */

void
rect_select_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  RectSelect * rect_sel;
  int x, y;

  gdisp = (GDisplay *) gdisp_ptr;
  rect_sel = (RectSelect *) tool->private;

  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &x, &y, True);

  rect_sel->x = x;
  rect_sel->y = y;
  rect_sel->w = 0;
  rect_sel->h = 0;
  rect_sel->replace = 0;

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);
      
  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;

  if (bevent->state & ShiftMask)
    rect_sel->op = ADD;
  else if (bevent->state & ControlMask)
    rect_sel->op = SUB;
  else
    {
      if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
	{
	  init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
	  return;
	}
      rect_sel->op = ADD;
      rect_sel->replace = 1;
    }

  draw_core_start (rect_sel->core,
		     XtWindow (gdisp->disp_image->canvas),
		     tool);

}

void
rect_select_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  RectSelect * rect_sel;
  GDisplay * gdisp;
  int x1, y1, x2, y2, w, h;

  gdisp = (GDisplay *) gdisp_ptr;
  rect_sel = (RectSelect *) tool->private;

  XUngrabPointer (DISPLAY, bevent->time);

  draw_core_stop (rect_sel->core, tool);
  tool->state = INACTIVE;

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & Button3Mask))
    {
      /*  if applicable, replace the current selection  */
      /*  or insure that a floating selection is anchored down...  */
      if (rect_sel->replace)
	selection_clear (gdisp->select, gdisp_ptr);
      else
	selection_anchor (gdisp->select, gdisp_ptr);

      x1 = (rect_sel->w < 0) ? rect_sel->x + rect_sel->w : rect_sel->x;
      y1 = (rect_sel->h < 0) ? rect_sel->y + rect_sel->h : rect_sel->y;
      w = (rect_sel->w < 0) ? -rect_sel->w : rect_sel->w;
      h = (rect_sel->h < 0) ? -rect_sel->h : rect_sel->h;
      x2 = x1 + w;
      y2 = y1 + h;

      switch (tool->type)
	{
	case RECT_SELECT :
	  gregion_combine_rect (gdisp->select->region, rect_sel->op,
				x1, y1,
				(x2 - x1), (y2 - y1));
	  break;
	case ELLIPSE_SELECT :
	  gregion_combine_ellipse (gdisp->select->region, rect_sel->op,
				   x1, y1,
				   (x2 - x1), (y2 - y1), antialiasing);
	  break;
	}

      selection_start (gdisp->select, 0, True);
	    
    }
}

void
rect_select_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  RectSelect * rect_sel;
  GDisplay * gdisp;
  int x, y;

  if (tool->state != ACTIVE)
    return;

  gdisp = (GDisplay *) gdisp_ptr;
  rect_sel = (RectSelect *) tool->private;

  draw_core_pause (rect_sel->core, tool);
      
  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, &x, &y, True);
  rect_sel->w = (x - rect_sel->x);
  rect_sel->h = (y - rect_sel->y);

  /*  If both the control and shift keys are down,
      then make the rectangle square (or ellipse circular) */
  if (mevent->state & ShiftMask && mevent->state & ControlMask)
    rect_sel->w = rect_sel->h = MAXIMUM (rect_sel->w, rect_sel->h);

  draw_core_resume (rect_sel->core, tool);

}


void
rect_select_draw (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  RectSelect * rect_sel;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  rect_sel = (RectSelect *) tool->private;

  x1 = MINIMUM (rect_sel->x, rect_sel->x + rect_sel->w);
  y1 = MINIMUM (rect_sel->y, rect_sel->y + rect_sel->h);
  x2 = MAXIMUM (rect_sel->x, rect_sel->x + rect_sel->w);
  y2 = MAXIMUM (rect_sel->y, rect_sel->y + rect_sel->h);

  gdisplay_transform_coords (gdisp, x1, y1, &x1, &y1);
  gdisplay_transform_coords (gdisp, x2, y2, &x2, &y2);

  XDrawRectangle (DISPLAY, rect_sel->core->win,
		  rect_sel->core->gc, x1, y1, (x2 - x1), (y2 - y1));
}


void
rect_select_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  RectSelect * rect_sel;

  rect_sel = (RectSelect *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (rect_sel->core, tool);
      break;
    case RESUME :
      draw_core_resume (rect_sel->core, tool);
      break;
    case HALT :
      draw_core_stop (rect_sel->core, tool);
      break;
    }
}


Tool *
tools_new_rect_select ()
{
  Tool * tool;
  RectSelect * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (RectSelect *) xmalloc (sizeof (RectSelect));

  private->core = draw_core_new (rect_select_draw);
  private->x = private->y = 0;
  private->w = private->h = 0;

  tool->type = RECT_SELECT;
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
tools_free_rect_select (tool)
     Tool * tool;
{
  RectSelect * rect_sel;

  rect_sel = (RectSelect *) tool->private;

  draw_core_free (rect_sel->core);
  xfree (rect_sel);
 
}


