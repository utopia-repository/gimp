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
#include "boundary.h"
#include "draw_core.h"
#include "edit_selection.h"
#include "fuzzy_select.h"
#include "gdisplay.h"
#include "tools.h"

typedef struct _fuzzy_select FuzzySelect;

struct _fuzzy_select
{
  DrawCore *     core;         /*  Core select object                      */
  
  int            x, y;         /*  Point from which to execute seed fill  */
  int            last_x;       /*                                         */
  int            last_y;       /*  variables to keep track of sensitivity */
  int            threshold;    /*  threshold value for soft seed fill     */

  int            op;           /*  selection operation (ADD, SUB, etc)     */
  int            replace;      /*  replace current selection?              */
};


#define DIFF(x,y) ((x < y) ? y - x : x - y)
#define ABS(x)    ((x < 0) ? -x : x)


/*  XSegments which make up the fuzzy selection boundary  */
static XSegment *  segs = NULL;
static int         num_segs = 0;
static GRegion *   fuzzy_region = NULL;


/*************************************/
/*  Fuzzy selection apparatus  */


static Boolean
is_pixel_sufficiently_different (col1, col2, threshold, bytes)
     unsigned char * col1;
     unsigned char * col2;
     int threshold;
     int bytes;
{
  unsigned char c1, c2;

  while (bytes--)
    {
      c1 = *col1++; c2 = *col2++;
      if (DIFF (c1, c2) > threshold)
	return False;
    }

  return True;
}


static Boolean
find_contiguous_segment (col, line, width, bytes, threshold, initial, start, end)
     unsigned char * col;
     unsigned char * line;
     int width;
     int bytes;
     int threshold;
     int initial;
     int * start;
     int * end;
{
  unsigned char * col2;
  Boolean status;

  /* check the starting pixel */
  if ( !is_pixel_sufficiently_different (col, line, threshold, bytes))
    return False;

  *start = initial;
  *end = initial + 1;

  status = True;
  col2 = line - bytes;

  while ( status && (*start > 0) )
    {
      if ((status = is_pixel_sufficiently_different (col, col2, threshold, bytes)))
	(*start)--;
      col2 -= bytes;
    }

  status = True;
  col2 = line + bytes;

  while ( status && (*end < width) )
    {
      if ((status = is_pixel_sufficiently_different (col, col2, threshold, bytes)))
	(*end)++;
      col2 += bytes;
    }

  return True;
}


void
find_contiguous_region (region, image, bytes, threshold, x, y, col)
     GRegion * region;
     GImage * image;
     int bytes;
     int threshold;
     int x, y;
     unsigned char * col;
{
  int start, end, i;
  unsigned char * line;

  if (y >= image->height || y < 0) return;
  if (gregion_point_inside (region, x, y)) return;

  line = image->raw_image + bytes * (image->width * y + x);
  if (!col) col = line;

  if ( ! find_contiguous_segment (col, line, image->width, 
				  bytes, threshold, x, &start, &end))
    return;

  gregion_add_segment (region, start, y, (end - start), 255);
  
  for (i = start; i < end; i++)
    {
      find_contiguous_region (region, image, bytes, threshold, i, y - 1, col);
      find_contiguous_region (region, image, bytes, threshold, i, y + 1, col);
    }

}



/*  fuzzy select action functions  */

void
fuzzy_select_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  FuzzySelect * fuzzy_sel;

  gdisp = (GDisplay *) gdisp_ptr;
  fuzzy_sel = (FuzzySelect *) tool->private;

  fuzzy_sel->x = bevent->x;
  fuzzy_sel->y = bevent->y;
  fuzzy_sel->last_x = fuzzy_sel->x;
  fuzzy_sel->last_y = fuzzy_sel->y;
  fuzzy_sel->threshold = app_data.threshold;
  fuzzy_sel->replace = 0;

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);
      
  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;

  if (bevent->state & ShiftMask)
    fuzzy_sel->op = ADD;
  else if (bevent->state & ControlMask)
    fuzzy_sel->op = SUB;
  else
    {
      if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
	{
	  init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
	  return;
	}
      fuzzy_sel->op = ADD;
      fuzzy_sel->replace = 1;
    }

  /*  calculate the region boundary  */
  segs = fuzzy_select_calculate (tool, gdisp_ptr, &num_segs);

  draw_core_start (fuzzy_sel->core,
		     XtWindow (gdisp->disp_image->canvas),
		     tool);
}

void
fuzzy_select_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  FuzzySelect * fuzzy_sel;
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;
  fuzzy_sel = (FuzzySelect *) tool->private;

  XUngrabPointer (DISPLAY, bevent->time);

  draw_core_stop (fuzzy_sel->core, tool);
  tool->state = INACTIVE;

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & Button3Mask))
    {
      /*  if applicable, replace the current selection  */
      /*  or insure that a floating selection is anchored down...  */
      if (fuzzy_sel->replace)
	selection_clear (gdisp->select, gdisp_ptr);
      else
	selection_anchor (gdisp->select, gdisp_ptr);

      gregion_combine_region (gdisp->select->region, fuzzy_sel->op,
			      fuzzy_region);

      selection_start (gdisp->select, 0, True);

      /*  adapt the threshold based on the final value of this use  */
      app_data.threshold = fuzzy_sel->threshold;
    }

  /*  free the fuzzy region struct  */
  gregion_free (fuzzy_region);
  fuzzy_region = NULL;

  /*  If the segment array is allocated, free it  */
  if (segs)
    xfree (segs);
  segs = NULL;

}

void
fuzzy_select_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  FuzzySelect * fuzzy_sel;
  XSegment * new_segs;
  int num_new_segs;
  int diff, diff_x, diff_y;

  if (tool->state != ACTIVE)
    return;

  fuzzy_sel = (FuzzySelect *) tool->private;

  diff_x = mevent->x - fuzzy_sel->last_x;
  diff_y = mevent->y - fuzzy_sel->last_y;

  diff = ((ABS (diff_x) > ABS (diff_y)) ? diff_x : diff_y) / 2;

  fuzzy_sel->last_x = mevent->x;
  fuzzy_sel->last_y = mevent->y;

  fuzzy_sel->threshold += diff;
  fuzzy_sel->threshold = bounds (fuzzy_sel->threshold, 0, 255);

  /*  calculate the new fuzzy boundary  */
  new_segs = fuzzy_select_calculate (tool, gdisp_ptr, &num_new_segs);

  /*  stop the current boundary  */
  draw_core_pause (fuzzy_sel->core, tool);

  /*  make sure the XSegment array is freed before we assign the new one  */
  if (segs)
    xfree (segs);
  segs = new_segs;
  num_segs = num_new_segs;

  /*  start the new boundary  */
  draw_core_resume (fuzzy_sel->core, tool);

}


XSegment *
fuzzy_select_calculate (tool, gdisp_ptr, nsegs)
     Tool * tool;
     void * gdisp_ptr;
     int * nsegs;
{
  FuzzySelect * fuzzy_sel;
  GDisplay * gdisp;
  GRegion * new;
  int x, y;

  fuzzy_sel = (FuzzySelect *) tool->private;
  gdisp = (GDisplay *) gdisp_ptr;

  new = gregion_new (gdisp->gimage->height, gdisp->gimage->width);
  if (fuzzy_region)
    gregion_free (fuzzy_region);
  fuzzy_region = new;
  
  gdisplay_untransform_coords (gdisp, fuzzy_sel->x,
			       fuzzy_sel->y, &x, &y, False);
  
  find_contiguous_region (fuzzy_region, gdisp->gimage,
			  gdisp->gimage->bpp,
			  fuzzy_sel->threshold, x, y, NULL);
  
  /*  calculate and allocate a new XSegment array which represents the boundary
   *  of our calculated region
   */
  return find_region_boundary (fuzzy_region, 0, 0, gdisp_ptr, nsegs);
}


void
fuzzy_select_draw (tool)
     Tool * tool;
{
  FuzzySelect * fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;

  if (segs)
    XDrawSegments (DISPLAY, fuzzy_sel->core->win,
		   fuzzy_sel->core->gc, segs, num_segs);
}


void
fuzzy_select_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  FuzzySelect * fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (fuzzy_sel->core, tool);
      break;
    case RESUME :
      draw_core_resume (fuzzy_sel->core, tool);
      break;
    case HALT :
      draw_core_stop (fuzzy_sel->core, tool);
      break;
    }
}


Tool *
tools_new_fuzzy_select ()
{
  Tool * tool;
  FuzzySelect * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (FuzzySelect *) xmalloc (sizeof (FuzzySelect));

  private->core = draw_core_new (fuzzy_select_draw);

  tool->type = FUZZY_SELECT;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;  /*  Disallow scrolling  */
  tool->private = (void *) private;
  tool->button_press_func = fuzzy_select_button_press;
  tool->button_release_func = fuzzy_select_button_release;
  tool->motion_func = fuzzy_select_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = fuzzy_select_control;

  return tool;
}


void
tools_free_fuzzy_select (tool)
     Tool * tool;
{
  FuzzySelect * fuzzy_sel;

  fuzzy_sel = (FuzzySelect *) tool->private;
  draw_core_free (fuzzy_sel->core);
  xfree (fuzzy_sel);
}


