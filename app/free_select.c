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
#include "draw_core.h"
#include "edit_selection.h"
#include "errors.h"
#include "free_select.h"
#include "gdisplay.h"
#include "linked.h"

typedef struct _free_select FreeSelect;

struct _free_select
{
  DrawCore *      core;       /*  Core select object                      */
  
  int             num_pts;    /*  Number of points in the polygon         */

  int             op;         /*  selection operation (ADD, SUB, etc)     */
  int             replace;    /*  replace current selection?              */
};


#define DEFAULT_MAX_INC  1024
#define ROUND(x) ((int) (x + 0.5))

/*  The global array of XPoints for drawing the polygon...  */
static XPoint *  pts = NULL;
static int       max_pts = 0;


int
add_point (num_pts, x, y)
     int num_pts;
     int x, y;
{
  if (num_pts >= max_pts)
    {
      max_pts += DEFAULT_MAX_INC;

      pts = (XPoint *) xrealloc ((void *) pts, sizeof (XPoint) * max_pts);

      if (!pts)
	fatal_error ("Unable to reallocate points array in free_select.");
    }

  pts[num_pts].x = x;
  pts[num_pts].y = y;

  return 1;
}


/*  Routines to scan convert the polygon  */

static link_ptr
insert_in_list (list, x)
     link_ptr list;
     int x;
{
  link_ptr orig = list;
  link_ptr rest;

  if (!list)
    return add_to_list (list, (void *) x);

  while (list)
    {
      rest = next_item (list);
      if (x < (int) list->data)
	{
	  rest = add_to_list (rest, list->data);
	  list->next = rest;
	  list->data = (void *) x;
	  return orig;
	}
      else if (!rest)
	{
	  append_to_list (list, (void *) x);
	  return orig;
	}
      list = next_item (list);
    }

  return orig;
}


static void
convert_segment (scanlines, extent, width, x1, y1, x2, y2)
     link_ptr * scanlines;
     int extent;
     int width;
     int x1, y1;
     int x2, y2;
{
  int ydiff, y, tmp;
  float xinc, xstart;

  if (y1 > y2)
    { tmp = y2; y2 = y1; y1 = tmp; 
      tmp = x2; x2 = x1; x1 = tmp; }
  ydiff = (y2 - y1);

  if ( ydiff )
    {
      xinc = (float) (x2 - x1) / (float) ydiff;
      xstart = x1 + 0.5 * xinc;
      for (y = y1 ; y < y2; y++)
	{
	  if (y >= 0 && y < extent)
	    scanlines[y] = insert_in_list (scanlines[y], ROUND (xstart));
	  xstart += xinc;
	}
    }

}


static GRegion *
scan_convert (num_pts, extent, width, gdisp)
     int num_pts;
     int extent;
     int width;
     GDisplay * gdisp;
{
  GRegion * region;
  link_ptr * scanlines;
  link_ptr list;
  int x1, y1, x2, y2;
  int x, w;
  int i;

  if (num_pts < 3) 
    return NULL;

  region = gregion_new (extent, width);

  scanlines = (link_ptr *) xmalloc (sizeof (link_ptr) * extent);
  for (i = 0; i < extent; i++)
    scanlines[i] = NULL;

  for (i = 0; i < num_pts - 1; i++)
    {
      gdisplay_untransform_coords (gdisp, pts[i].x, pts[i].y, &x1, &y1, True);
      gdisplay_untransform_coords (gdisp, pts[i+1].x, pts[i+1].y, &x2, &y2, True);
      convert_segment (scanlines, extent, width, x1, y1, x2, y2);
    }

  gdisplay_untransform_coords (gdisp, pts[i].x, pts[i].y, &x1, &y1, True);
  gdisplay_untransform_coords (gdisp, pts[0].x, pts[0].y, &x2, &y2, True);
  convert_segment (scanlines, extent, width, x1, y1, x2, y2);

  for (i = 0; i < extent; i++)
    {
      list = scanlines[i];
      while (list)
	{
	  x = (int) list->data;
	  list = next_item(list);
	  if (!list)
	      warning ("Cannot properly scanline convert polygon!\n");
	  else 
	    {
	      w = (int) list->data - x;
	      gregion_add_segment (region, x, i, w, 255);
	      list = next_item (list);
	    }
	}

      free_list (scanlines[i]);
    }
  xfree (scanlines);
  
  return region;
}



/*************************************/
/*  Polygonal selection apparatus  */


/*  free select action functions  */

void
free_select_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  FreeSelect * free_sel;

  gdisp = (GDisplay *) gdisp_ptr;
  free_sel = (FreeSelect *) tool->private;

  free_sel->num_pts = 0;

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);
      
  free_sel->replace = 0;

  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;

  if (bevent->state & ShiftMask)
    free_sel->op = ADD;
  else if (bevent->state & ControlMask)
    free_sel->op = SUB;
  else
    {
      if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
	{
	  init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
	  return;
	}
      free_sel->op = ADD;
      free_sel->replace = 1;
    }

  draw_core_start (free_sel->core,
		     XtWindow (gdisp->disp_image->canvas),
		     tool);

}

void
free_select_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  FreeSelect * free_sel;
  GDisplay * gdisp;
  GRegion * region;

  gdisp = (GDisplay *) gdisp_ptr;
  free_sel = (FreeSelect *) tool->private;

  XUngrabPointer (DISPLAY, bevent->time);

  draw_core_stop (free_sel->core, tool);

  tool->state = INACTIVE;

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & Button3Mask))
    {
      /*  if applicable, replace the current selection  */
      /*  or insure that a floating selection is anchored down...  */
      if (free_sel->replace)
	selection_clear (gdisp->select, gdisp_ptr);
      else
	selection_anchor (gdisp->select, gdisp_ptr);

      region = scan_convert (free_sel->num_pts,
			     gdisp->select->region->extent,
			     gdisp->select->region->width,
			     gdisp);

      if (region)
	{
	  gregion_combine_region (gdisp->select->region,
				  free_sel->op, region);
	  gregion_free (region);
	}

      selection_start (gdisp->select, 0, True);
	    
    }
}

void
free_select_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  FreeSelect * free_sel;
  GDisplay * gdisp;

  if (tool->state != ACTIVE)
    return;

  gdisp = (GDisplay *) gdisp_ptr;
  free_sel = (FreeSelect *) tool->private;

  if (add_point (free_sel->num_pts, mevent->x, mevent->y))
    free_sel->num_pts ++;

  if (free_sel->num_pts > 1)
    {
      XDrawLines (DISPLAY, free_sel->core->win, free_sel->core->gc,
		  pts + (free_sel->num_pts - 2), 2, CoordModeOrigin);
      /*  becase this is being drawn XOR'd, it is necessary to draw the
       *  endpoint twice so that it doesn't disappear when the next
       *  segment is added...
       */
/*      XDrawPoint (DISPLAY, free_sel->core->win, free_sel->core->gc,
		  pts [free_sel->num_pts - 1].x, pts [free_sel->num_pts - 1].y);
*/
    }
      

}


void
free_select_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  FreeSelect * free_sel;

  free_sel = (FreeSelect *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (free_sel->core, tool);
      break;
    case RESUME :
      draw_core_resume (free_sel->core, tool);
      break;
    case HALT :
      draw_core_stop (free_sel->core, tool);
      break;
    }
}


void
free_select_draw (tool)
     Tool * tool;
{
  FreeSelect * free_sel;

  free_sel = (FreeSelect *) tool->private;

  XDrawLines (DISPLAY, free_sel->core->win, free_sel->core->gc,
	      pts, free_sel->num_pts, CoordModeOrigin);
}


Tool *
tools_new_free_select ()
{
  Tool * tool;
  FreeSelect * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (FreeSelect *) xmalloc (sizeof (FreeSelect));

  private->core = draw_core_new (free_select_draw);
  /*  Make the selection static, not blinking  */

  private->num_pts = 0;

  tool->type = FREE_SELECT;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;   /*  Do not allow scrolling  */
  tool->private = (void *) private;
  tool->button_press_func = free_select_button_press;
  tool->button_release_func = free_select_button_release;
  tool->motion_func = free_select_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = free_select_control;

  return tool;
}


void
tools_free_free_select (tool)
     Tool * tool;
{
  FreeSelect * free_sel;

  free_sel = (FreeSelect *) tool->private;

  draw_core_free (free_sel->core);
  xfree (free_sel);
 
}




