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
#include "draw_core.h"
#include "tools.h"
#include "edit_selection.h"
#include "gdisplay.h"
#include "undo.h"


#define EDIT_SELECT_SCROLL_LOCK 0

typedef struct _edit_selection EditSelection;

struct _edit_selection
{
  int                 origx, origy;        /*  original x and y coords            */

  int                 sx, sy;              /*  starting x and y coordinates       */
  int                 ex, ey;              /*  ending x and y coordinates         */

  int                 x1, y1;              /*  bounding box coordinates           */
  int                 x2, y2;              /*  bounding box coordinates           */

  DrawCore *          core;                /*  selection core for drawing bounds  */
  Selection *         select;              /*  Selection pointer                  */
  
  ButtonReleaseFunc   old_button_release;  /*  old button press member func       */
  MotionFunc          old_motion;          /*  old motion member function         */
  ToolCtlFunc         old_control;         /*  old control member function        */
  int                 old_scroll_lock;     /*  old value of scroll lock           */
  
};


/*  static EditSelection structure--there is ever only one present  */
static EditSelection edit_select;


void
init_edit_selection (tool, select, gdisp_ptr, x, y)
     Tool * tool;
     Selection * select;
     void * gdisp_ptr;
     int x, y;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;

  edit_select.select   =  select;
  edit_select.sx        =  x;
  edit_select.sy        =  y;
  edit_select.ex        =  x;
  edit_select.ey        =  y;

  /*  Move the (x, y) point from screen to image space  */
  gdisplay_untransform_coords (gdisp, x, y, &x, &y, False);

  edit_select.origx    =  x;
  edit_select.origy    =  y;

  edit_select.old_button_release = tool->button_release_func;
  edit_select.old_motion         = tool->motion_func;
  edit_select.old_control        = tool->control_func;
  edit_select.old_scroll_lock    = tool->scroll_lock;

  /*  reset the function pointers on the selection tool  */
  tool->button_release_func = edit_selection_button_release;
  tool->motion_func = edit_selection_motion;
  tool->control_func = edit_selection_control;
  tool->scroll_lock = EDIT_SELECT_SCROLL_LOCK;

  /*  find the bounding box  */
  selection_find_bounds (select, &edit_select.x1, &edit_select.y1,
			 &edit_select.x2, &edit_select.y2);

  /*  transform the bounding box into screen coordinates  */
  gdisplay_transform_coords (gdisp, edit_select.x1, edit_select.y1,
			     &edit_select.x1, &edit_select.y1);
  gdisplay_transform_coords (gdisp, edit_select.x2, edit_select.y2,
			     &edit_select.x2, &edit_select.y2);

  /*  pause the current selection  */
  selection_pause (gdisp->select);

  /*  Create and start the selection core  */
  edit_select.core = draw_core_new (edit_selection_draw);
  draw_core_start (edit_select.core,
		     XtWindow (gdisp->disp_image->canvas),
		     tool);
}


void
edit_selection_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  int new_x, new_y;
  int orig_offx, orig_offy;
  int x, y;
  Selection * select;
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  resume the current selection and ungrab the pointer  */
  selection_resume (gdisp->select);
  XUngrabPointer (DISPLAY, bevent->time);

  /*  Stop and free the selection core  */
  draw_core_stop (edit_select.core, tool);
  draw_core_free (edit_select.core);
  edit_select.core = NULL;
  tool->state      = INACTIVE;

  tool->button_release_func = edit_select.old_button_release;
  tool->motion_func         = edit_select.old_motion;
  tool->control_func        = edit_select.old_control;
  tool->scroll_lock         = edit_select.old_scroll_lock;

  select = edit_select.select;
  edit_select.select = NULL;

  /*  If the cancel button is down...Do nothing  */
  if (! (bevent->state & Button3Mask))
    {
      /*  Move the (x, y) point from screen to image space  */
      gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &x, &y, False);
      
      /* if there has been movement, move the selection  */
      if (edit_select.origx != x || edit_select.origy != y)
	{
	  orig_offx = select->offset_x;
	  orig_offy = select->offset_y;
	  
	  new_x = orig_offx + (x - edit_select.origx);
	  new_y = orig_offy + (y - edit_select.origy);

	  selection_translate (select, orig_offx, orig_offy, new_x, new_y, gdisp);

	  /*  Add an undo move selection to the undo stack  */
	  undo_push_move_sel (gdisp->ID, orig_offx, orig_offy, select->fresh_cut);

	  return;
	}
      /*  if no movement has occured, clear the current selection  */
      else
	selection_clear (select, gdisp_ptr);
    }

  selection_start (select, 0, True);
}


void
edit_selection_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;

  if (tool->state != ACTIVE)
    return;

  gdisp = (GDisplay *) gdisp_ptr;

  if (mevent->x != edit_select.ex || mevent->y != edit_select.ey)
    {
      draw_core_pause (edit_select.core, tool);

      edit_select.ex = mevent->x;
      edit_select.ey = mevent->y;

      draw_core_resume (edit_select.core, tool);
    }

}


void
edit_selection_draw (tool)
     Tool * tool;
{
  int i;
  int diff_x, diff_y;
  GDisplay * gdisp;
  XSegment * seg;
  Selection * select;

  gdisp = (GDisplay *) tool->gdisp_ptr;

  diff_x = edit_select.ex - edit_select.sx;
  diff_y = edit_select.ey - edit_select.sy;

  /*  Draw only the selection's bounding box if App resource "useBBox" is True  */
  if (app_data.use_bbox)
    {
      XDrawRectangle (DISPLAY, edit_select.core->win, edit_select.core->gc,
		      edit_select.x1 + diff_x, edit_select.y1 + diff_y,
		      (edit_select.x2 - edit_select.x1),
		      (edit_select.y2 - edit_select.y1));
    }

  /*  Otherwise, draw the entire outline  */
  else
    {
      select = edit_select.select;

      /*  offset the current selection  */
      seg = select->xelems;
      for (i = 0; i < select->num_xelems; i++)
	{
	  seg->x1 += diff_x;
	  seg->x2 += diff_x;
	  seg->y1 += diff_y;
	  seg->y2 += diff_y;
	  seg++;
	}
      
      XDrawSegments (DISPLAY, edit_select.core->win, edit_select.core->gc,
		     select->xelems, select->num_xelems);
      
      /*  reset the the current selection  */
      seg = select->xelems;
      for (i = 0; i < select->num_xelems; i++)
	{
	  seg->x1 -= diff_x;
	  seg->x2 -= diff_x;
	  seg->y1 -= diff_y;
	  seg->y2 -= diff_y;
	  seg++;
	}
    }

}


void
edit_selection_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  switch (action)
    {
    case PAUSE : 
      draw_core_pause (edit_select.core, tool);
      break;
    case RESUME :
      draw_core_resume (edit_select.core, tool);
      break;
    case HALT :
      draw_core_stop (edit_select.core, tool);
      draw_core_free (edit_select.core);
      break;
    }
}


void
edit_sel_arrow_keys_func (tool, kevent, gdisp_ptr)
     Tool * tool;
     XKeyEvent * kevent;
     void * gdisp_ptr;
{
  int inc_x, inc_y;
  int orig_x, orig_y;
  GDisplay * gdisp;
  KeySym key;

  gdisp = (GDisplay *) gdisp_ptr;

  inc_x = inc_y = 0;

  if (kevent->type != KeyPress || tool->state == ACTIVE)
    return;

  XLookupString (kevent, NULL, 0, &key, NULL);

  switch (key)
    {
    case XK_Up    : inc_y = -1; break;
    case XK_Left  : inc_x = -1; break;
    case XK_Right : inc_x =  1; break;
    case XK_Down  : inc_y =  1; break;
    }

  /*  If the shift key is down, move by an accelerated increment  */
  if (kevent->state & ShiftMask)
    {
      inc_y *= app_data.arrow_accel;
      inc_x *= app_data.arrow_accel;
    }

  orig_x = gdisp->select->offset_x;
  orig_y = gdisp->select->offset_y;

  selection_translate (gdisp->select,
		       orig_x, orig_y,
		       gdisp->select->offset_x + inc_x,
		       gdisp->select->offset_y + inc_y,
		       (void *) gdisp);

  /*  Add an undo move selection to the undo stack  */
  undo_push_move_sel (gdisp->ID, orig_x, orig_y, gdisp->select->fresh_cut);
  
}



