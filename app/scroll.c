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
#include "scale.h"
#include "scroll.h"
#include "cursorutil.h"


/*  This is the delay before dithering begins
 *  for example, after an operation such as scrolling
 */
#define DITHER_DELAY 250  /*  milliseconds  */


/*  STATIC variables  */
/*  These are the values of the initial pointer grab   */
static int startx, starty;


void
vert_scrolled (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;
  XmScrollBarCallbackStruct *cbs =
    (XmScrollBarCallbackStruct *) call_data;

  gdisp = (GDisplay *) client_data;

  if (cbs->value != gdisp->offset_y)
    {
      gdisp->offset_y = cbs->value;

      gdisplay_disp_region (gdisp, 0, 0, gdisp->disp_image->width, 
			    gdisp->disp_image->height,
			    DITHER_DELAY);
    }
}


void
horz_scrolled (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;
  XmScrollBarCallbackStruct *cbs =
    (XmScrollBarCallbackStruct *) call_data;

  gdisp = (GDisplay *) client_data;

  if (cbs->value != gdisp->offset_x)
    {
      gdisp->offset_x = cbs->value;
  
      gdisplay_disp_region (gdisp, 0, 0, gdisp->disp_image->width, 
			    gdisp->disp_image->height,
			    DITHER_DELAY);
    }

}

void start_grab_and_scroll (gdisp, bevent)
     GDisplay *gdisp;
     XButtonPressedEvent *bevent;
{
  Cursor cursor;

  cursor = XCreateFontCursor (DISPLAY, XC_fleur);

  startx = bevent->x + gdisp->offset_x;
  starty = bevent->y + gdisp->offset_y;

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button2MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, cursor, bevent->time);

  XFreeCursor (DISPLAY, cursor);
}


void end_grab_and_scroll (gdisp, bevent)
     GDisplay *gdisp;
     XButtonReleasedEvent *bevent;
{
  XUngrabPointer (DISPLAY, bevent->time);
}


void grab_and_scroll (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XMotionEvent *mevent;
  GDisplay *gdisp;
  int old_x, old_y;

  gdisp = (GDisplay *) client_data;
  mevent = (XMotionEvent *) event;

  old_x = gdisp->offset_x;
  old_y = gdisp->offset_y;

  gdisp->offset_x = startx - mevent->x;
  gdisp->offset_y = starty - mevent->y;

  bounds_checking (gdisp);

  if (gdisp->offset_x != old_x || gdisp->offset_y != old_y)
    {
      setup_scale (gdisp);

      gdisplay_disp_region (gdisp, 0, 0, gdisp->disp_image->width, 
			    gdisp->disp_image->height,
			    DITHER_DELAY);
    }
}


void scroll_to_pointer_position (gdisp, mevent)
     GDisplay * gdisp;
     XMotionEvent * mevent;
{
  int old_x, old_y;

  Window root, child;
  int root_x, root_y;
  int child_x, child_y;
  unsigned int mask;
  
  old_x = gdisp->offset_x;
  old_y = gdisp->offset_y;

  /*  The cases for scrolling  */
  if (mevent->x < 0)
    gdisp->offset_x += mevent->x;
  else if (mevent->x > gdisp->disp_image->width)
    gdisp->offset_x += (mevent->x - gdisp->disp_image->width);
  if (mevent->y < 0)
    gdisp->offset_y += mevent->y;
  else if (mevent->y > gdisp->disp_image->height)
    gdisp->offset_y += (mevent->y - gdisp->disp_image->height);

  bounds_checking (gdisp);

  if (gdisp->offset_x != old_x || gdisp->offset_y != old_y)
    {
      setup_scale (gdisp);

      gdisplay_disp_region (gdisp, 0, 0, gdisp->disp_image->width, 
			    gdisp->disp_image->height,
			    DITHER_DELAY);

      XQueryPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas),
		     &root, &child,
		     &root_x, &root_y,
		     &child_x, &child_y,
		     &mask);

      if (child_x == mevent->x && child_y == mevent->y)
	/*  Put this event back on the queue -- so it keeps scrolling */
	XPutBackEvent (DISPLAY, (XEvent *) mevent);
    }
}


