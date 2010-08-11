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
#include <X11/keysym.h>
#include "appenv.h"
#include "scale.h"
#include "scroll.h"
#include "general.h"
#include "disp-callbacks.h"
#include "selection.h"
#include "tools.h"
#include "gdisplay.h"

static void
redraw (gdisp, drawing_a, x, y, w, h)
     GDisplay *gdisp;
     Widget drawing_a;
     int x, y;
     int w, h;
{
  long x1, y1, x2, y2;    /*  coordinate of rectangle corners  */

  x1 = x;
  y1 = y;
  x2 = (x+w);
  y2 = (y+h);
      
  x1 = bounds (x1, 0, gdisp->disp_image->width);
  y1 = bounds (y1, 0, gdisp->disp_image->height);
  x2 = bounds (x2, 0, gdisp->disp_image->width);
  y2 = bounds (y2, 0, gdisp->disp_image->height);

  if ((x2 - x1) && (y2 - y1))
    gdisplay_put_image (gdisp, x1, y1, (x2 - x1), (y2 - y1));

}

void
expose_resize (drawing_a, client_data, call_data)
     Widget drawing_a;
     XtPointer client_data;
     XtPointer call_data;
{
  XExposeEvent *xexpose;
  GDisplay *gdisp;
  XmDrawingAreaCallbackStruct *cbs =
  (XmDrawingAreaCallbackStruct *) call_data;

  gdisp = gdisplay_active (drawing_a);

  if (cbs->reason == XmCR_RESIZE) 
    {
      resize_display (gdisp);
    }
  else if (cbs->reason == XmCR_EXPOSE)
    {
      xexpose = (XExposeEvent *) cbs->event;

      redraw (gdisp, drawing_a,
	      xexpose->x, xexpose->y,
	      xexpose->width, xexpose->height);
    }
}

void
input_callback (drawing_a, client_data, call_data)
     Widget drawing_a;
     XtPointer client_data;
     XtPointer call_data;
{
  XmDrawingAreaCallbackStruct *cbs = 
    (XmDrawingAreaCallbackStruct *) call_data;
  XEvent *event = cbs->event;
  XKeyEvent *kevent;
  GDisplay *gdisp;

  gdisp = gdisplay_active (drawing_a);

  if (event->xany.type == KeyPress || event->xany.type == KeyRelease)
    {
      KeySym key;
      
      kevent = (XKeyPressedEvent *) cbs->event;
      XLookupString (kevent, NULL, 0, &key, NULL);
      switch (key)
	{
	case XK_Left : case XK_Right :
	case XK_Up   : case XK_Down  :
	  if (active_tool)
	    (* active_tool->arrow_keys_func) (active_tool, kevent, gdisp);
	  break;
	}
    }

}


void
button_pressed (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonPressedEvent *bevent;
  GDisplay *gdisp;

  gdisp = (GDisplay *) client_data;
  bevent = (XButtonPressedEvent *) event;

  if (bevent->button == 1)
    {
      if (active_tool)
	(* active_tool->button_press_func) (active_tool, bevent, gdisp);
    }
  else if (bevent->button == 2)
    start_grab_and_scroll (gdisp, bevent);
}


void
button_released (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonReleasedEvent *bevent;
  GDisplay *gdisp;

  gdisp = (GDisplay *) client_data;
  bevent = (XButtonReleasedEvent *) event;
  
  if (bevent->button == 1)
    {
      if (active_tool)
	if (active_tool->state == ACTIVE)
	  (* active_tool->button_release_func) (active_tool, bevent, gdisp);
    }
  else if (bevent->button == 2)
    end_grab_and_scroll (gdisp, bevent);
}


void
tool_motion (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XMotionEvent *mevent;
  GDisplay *gdisp;

  gdisp = (GDisplay *) client_data;
  mevent = (XMotionEvent *) event;

  if (active_tool)
    if (active_tool->state == ACTIVE)
      {
	/*  if the first mouse button is down, check for automatic
	 *  scrolling...
	 */
	if ((mevent->state & Button1Mask) && !active_tool->scroll_lock)
	  {
	    if (mevent->x < 0 || mevent->y < 0 ||
		mevent->x > gdisp->disp_image->width ||
		mevent->y > gdisp->disp_image->height)
	      scroll_to_pointer_position (gdisp, mevent);
	  }

	(* active_tool->motion_func) (active_tool, mevent, gdisp);
      }
}


void
destroy_window (drawing_a, client_data, call_data)
     Widget drawing_a;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;

  gdisp = gdisplay_active (drawing_a);
  if (gdisp)
    gdisplay_remove_and_delete (gdisp);
}
