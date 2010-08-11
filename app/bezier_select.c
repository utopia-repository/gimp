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
#include "bezier_select.h"
#include "draw_core.h"
#include "edit_selection.h"
#include "errors.h"
#include "gdisplay.h"
#include "gregion.h"
#include "linked.h"

#define BEZIER_START     1
#define BEZIER_ADD       2
#define BEZIER_EDIT      4
#define BEZIER_DRAG      8

#define BEZIER_ANCHOR    1
#define BEZIER_CONTROL   2

#define BEZIER_DRAW_CURVE   1
#define BEZIER_DRAW_CURRENT 2
#define BEZIER_DRAW_HANDLES 4
#define BEZIER_DRAW_ALL     (BEZIER_DRAW_CURVE | BEZIER_DRAW_HANDLES)

#define BEZIER_WIDTH     8
#define BEZIER_HALFWIDTH 4

#define SUBDIVIDE  1000

#define IMAGE_COORDS  1
#define SCREEN_COORDS 2

/*  bezier select type definitions */

typedef struct _bezier_select BezierSelect;
typedef struct _bezier_point BezierPoint;
typedef double BezierMatrix[4][4];
typedef void (*BezierPointsFunc) (BezierSelect *, XPoint *, int);

struct _bezier_select
{
  int state;                 /* start, add, edit or drag          */
  int draw;                  /* all or part                       */
  int closed;                /* is the curve closed               */
  DrawCore *core;            /* Core drawing object               */
  BezierPoint *points;       /* the curve                         */
  BezierPoint *cur_anchor;   /* the current active anchor point   */
  BezierPoint *cur_control;  /* the current active control point  */
  BezierPoint *last_point;   /* the last point on the curve       */
  int num_points;            /* number of points in the curve     */
  GRegion *region;           /* null if the curve is open         */
  link_ptr *scanlines;       /* used in converting a curve        */
};

struct _bezier_point
{
  int type;                   /* type of point (anchor or control) */
  int x, y;                   /* location of point in image space  */
  int sx, sy;                 /* location of point in screen space */
  BezierPoint *next;          /* next point on curve               */
  BezierPoint *prev;          /* prev point on curve               */
};

/*  bezier select action functions  */

static void  bezier_select_reset           (BezierSelect *);
static void  bezier_select_button_press    (Tool *, XButtonEvent *, XtPointer);
static void  bezier_select_button_release  (Tool *, XButtonEvent *, XtPointer);
static void  bezier_select_motion          (Tool *, XMotionEvent *, XtPointer);
static void  bezier_select_control         (Tool *, int, XtPointer);
static void  bezier_select_draw            (Tool *);

static void  bezier_add_point              (BezierSelect *, int, int, int);
static void  bezier_offset_point           (BezierPoint *, int, int);
static int   bezier_check_point            (BezierPoint *, int, int);
static void  bezier_draw_curve             (BezierSelect *);
static void  bezier_draw_handles           (BezierSelect *);
static void  bezier_draw_current           (BezierSelect *);
static void  bezier_draw_point             (BezierSelect *, BezierPoint *, int);
static void  bezier_draw_line              (BezierSelect *, BezierPoint *, BezierPoint *);
static void  bezier_draw_segment           (BezierSelect *, BezierPoint *, int, int, BezierPointsFunc);
static void  bezier_draw_segment_points    (BezierSelect *, XPoint *, int);
static void  bezier_compose                (BezierMatrix, BezierMatrix, BezierMatrix);

static void  bezier_convert                (BezierSelect *, GDisplay *, int);
static void  bezier_convert_points         (BezierSelect *, XPoint *, int);
static void  bezier_convert_line           (link_ptr *, int, int, int, int);
static link_ptr  bezier_insert_in_list     (link_ptr, int);

static BezierMatrix basis = 
{
  { -1,  3, -3,  1 },
  {  3, -6,  3,  0 },
  { -3,  3,  0,  0 },
  {  1,  0,  0,  0 },
};

Tool*
tools_new_bezier_select ()
{
  Tool * tool;
  BezierSelect * bezier_sel;

  tool = xmalloc (sizeof (Tool));

  bezier_sel = xmalloc (sizeof (BezierSelect));

  bezier_sel->num_points = 0;
  bezier_sel->region = NULL;
  bezier_sel->core = draw_core_new (bezier_select_draw);
  bezier_select_reset (bezier_sel);
  
  tool->type = BEZIER_SELECT;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;   /*  Do not allow scrolling  */
  tool->private = (void *) bezier_sel;
  tool->button_press_func = bezier_select_button_press;
  tool->button_release_func = bezier_select_button_release;
  tool->motion_func = bezier_select_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = bezier_select_control;

  return tool;
}

void
tools_free_bezier_select (tool)
     Tool * tool;
{
  BezierSelect * bezier_sel;

  bezier_sel = tool->private;

  if (tool->state == ACTIVE)
    draw_core_stop (bezier_sel->core, tool);
  draw_core_free (bezier_sel->core);

  bezier_select_reset (bezier_sel);

  xfree (bezier_sel);
}

static void
bezier_select_reset (bezier_sel)
     BezierSelect * bezier_sel;
{
  BezierPoint * points;
  BezierPoint * start_pt;
  BezierPoint * temp_pt;
  
  if (bezier_sel->num_points > 0)
    {
      points = bezier_sel->points;
      start_pt = (bezier_sel->closed) ? (bezier_sel->points) : (NULL);
      
      do {
	temp_pt = points;
	points = points->next;

	xfree (temp_pt);
      } while (points != start_pt);
    }

  if (bezier_sel->region)
    gregion_free (bezier_sel->region);

  bezier_sel->state = BEZIER_START;    /* we are starting the curve */
  bezier_sel->draw = BEZIER_DRAW_ALL;  /* draw everything by default */
  bezier_sel->closed = 0;              /* the curve is initally open */
  bezier_sel->points = NULL;           /* initially there are no points */
  bezier_sel->cur_anchor = NULL;
  bezier_sel->cur_control = NULL;
  bezier_sel->last_point = NULL;
  bezier_sel->num_points = 0;          /* intially there are no points */
  bezier_sel->region = NULL;           /* empty region */
  bezier_sel->scanlines = NULL;
}

static void
bezier_select_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay *gdisp;
  BezierSelect *bezier_sel;
  BezierPoint *points;
  BezierPoint *start_pt;
  int grab_pointer;
  int op, replace;
  int x, y;
  
  gdisp = (GDisplay *) gdisp_ptr;
  bezier_sel = tool->private;
  grab_pointer = 0;

  /*  If the tool was being used in another image...reset it  */
  if (tool->state == ACTIVE && gdisp_ptr != tool->gdisp_ptr)
    bezier_select_reset (bezier_sel);

  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &x, &y, True);

  switch (bezier_sel->state)
    {
    case BEZIER_START:
      grab_pointer = 1;
      tool->state = ACTIVE;
      tool->gdisp_ptr = gdisp_ptr;
      
      if (!(bevent->state & ShiftMask) && !(bevent->state & ControlMask))
	if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
	  {
	    init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
	    break;
	  }
      
      bezier_sel->state = BEZIER_ADD;
      bezier_sel->draw = BEZIER_DRAW_CURRENT | BEZIER_DRAW_HANDLES;

      bezier_add_point (bezier_sel, BEZIER_ANCHOR, x, y);
      bezier_add_point (bezier_sel, BEZIER_CONTROL, x, y);
      
      draw_core_start (bezier_sel->core, XtWindow (gdisp->disp_image->canvas), tool);
      break;
    case BEZIER_ADD:
      grab_pointer = 1;
      
      if (bezier_sel->cur_anchor && 
	  bezier_check_point (bezier_sel->cur_anchor, x, y))
	{
	  break;
	}

      if (bezier_sel->cur_anchor->next &&
	  bezier_check_point (bezier_sel->cur_anchor->next, x, y))
	{
	  bezier_sel->cur_control = bezier_sel->cur_anchor->next;
	  break;
	}
      
      if (bezier_sel->cur_anchor->prev &&
	  bezier_check_point (bezier_sel->cur_anchor->prev, x, y))
	{
	  bezier_sel->cur_control = bezier_sel->cur_anchor->prev;
	  break;
	}
      
      if (bezier_check_point (bezier_sel->points, x, y))
	{
	  bezier_sel->draw = BEZIER_DRAW_ALL;
	  draw_core_pause (bezier_sel->core, tool);

	  bezier_add_point (bezier_sel, BEZIER_CONTROL, x, y);
	  bezier_sel->last_point->next = bezier_sel->points;
	  bezier_sel->points->prev = bezier_sel->last_point;
	  bezier_sel->cur_anchor = bezier_sel->points;
	  bezier_sel->cur_control = bezier_sel->points->next;
	  
	  bezier_sel->closed = 1;
	  bezier_sel->state = BEZIER_EDIT;
	  bezier_sel->draw = BEZIER_DRAW_ALL;

	  draw_core_resume (bezier_sel->core, tool);
	}
      else
	{
	  bezier_sel->draw = BEZIER_DRAW_HANDLES;
	  draw_core_pause (bezier_sel->core, tool);

	  bezier_add_point (bezier_sel, BEZIER_CONTROL, x, y);
	  bezier_add_point (bezier_sel, BEZIER_ANCHOR, x, y);
	  bezier_add_point (bezier_sel, BEZIER_CONTROL, x, y);

	  bezier_sel->draw = BEZIER_DRAW_CURRENT | BEZIER_DRAW_HANDLES;
	  draw_core_resume (bezier_sel->core, tool);
	}
      break;
    case BEZIER_EDIT:
      if (!bezier_sel->closed)
	fatal_error ("tried to edit on open bezier curve");

      /* erase the handles */
      bezier_sel->draw = BEZIER_DRAW_HANDLES;
      draw_core_pause (bezier_sel->core, tool);

      /* unset the current anchor and control */
      bezier_sel->cur_anchor = NULL;
      bezier_sel->cur_control = NULL;
	      
      points = bezier_sel->points;
      start_pt = bezier_sel->points;
      
      /* find if the button press occurred on a point */
      do {
	  if (bezier_check_point (points, x, y))
	    {
	      /* set the current anchor and control points */
	      switch (points->type)
		{
		case BEZIER_ANCHOR:
		  bezier_sel->cur_anchor = points;
		  bezier_sel->cur_control = bezier_sel->cur_anchor->next;
		  break;
		case BEZIER_CONTROL:
		  bezier_sel->cur_control = points;
		  if (bezier_sel->cur_control->next->type == BEZIER_ANCHOR)
		    bezier_sel->cur_anchor = bezier_sel->cur_control->next;
		  else
		    bezier_sel->cur_anchor = bezier_sel->cur_control->prev;
		  break;
		}
	      grab_pointer = 1;
	      break;
	    }
	  
	  points = points->next;
	} while (points != start_pt);

      if (!grab_pointer && gregion_point_inside (bezier_sel->region, x, y))
	{
	  tool->state = INACTIVE;
	  bezier_sel->draw = BEZIER_DRAW_CURVE;
	  draw_core_resume (bezier_sel->core, tool);
	  
	  bezier_sel->draw = 0;
	  draw_core_stop (bezier_sel->core, tool);
	  
	  replace = 0;
	  if (bevent->state & ShiftMask)
	    op = ADD;
	  else if (bevent->state & ControlMask)
	    op = SUB;
	  else
	    {
	      op = ADD;
	      replace = 1;
	    }
	  
	  if (replace)
	    selection_clear (gdisp->select, gdisp_ptr);
	  else
	    selection_anchor (gdisp->select, gdisp_ptr);
	  
	  gregion_combine_region (gdisp->select->region,
				  op, bezier_sel->region);
	  
	  bezier_select_reset (bezier_sel);
	  
	  selection_start (gdisp->select, 0, True);
	}
      else
	{
	  /* draw the handles */
	  bezier_sel->draw = BEZIER_DRAW_HANDLES;
	  draw_core_resume (bezier_sel->core, tool);
	}
      break;
    }

  if (grab_pointer)
    XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		  Button1MotionMask | ButtonReleaseMask, GrabModeAsync, 
		  GrabModeAsync, None, None, bevent->time);
}

static void
bezier_select_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  BezierSelect *bezier_sel;

  gdisp = tool->gdisp_ptr;
  bezier_sel = tool->private;
  bezier_sel->state &= ~(BEZIER_DRAG);

  XUngrabPointer (DISPLAY, bevent->time);

  if (bezier_sel->closed)
    bezier_convert (bezier_sel, tool->gdisp_ptr, SUBDIVIDE);
}

static void
bezier_select_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  static int lastx, lasty;
  
  GDisplay * gdisp;
  BezierSelect * bezier_sel;
  BezierPoint * anchor;
  BezierPoint * opposite_control;
  int offsetx;
  int offsety;
  int x, y;

  if (tool->state != ACTIVE)
    return;

  gdisp = gdisp_ptr;
  bezier_sel = tool->private;

  if (!bezier_sel->cur_anchor || !bezier_sel->cur_control)
    return;

  bezier_sel->draw = BEZIER_DRAW_CURRENT | BEZIER_DRAW_HANDLES;
  draw_core_pause (bezier_sel->core, tool);

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, &x, &y, True);

  /* If this is the first point then change the state and "remember" the point.
   */
  if (!(bezier_sel->state & BEZIER_DRAG))
    {
      bezier_sel->state |= BEZIER_DRAG;
      lastx = x;
      lasty = y;
    }

  if (mevent->state & ControlMask)
    {
      /* the control key is down ... move the current anchor point */
      /* we must also move the neighboring control points appropriately */

      offsetx = x - lastx;
      offsety = y - lasty;
      
      bezier_offset_point (bezier_sel->cur_anchor, offsetx, offsety);
      bezier_offset_point (bezier_sel->cur_anchor->next, offsetx, offsety);
      bezier_offset_point (bezier_sel->cur_anchor->prev, offsetx, offsety);
    }
  else
    {
      /* the control key is not down ... we move the current control point */
      
      offsetx = x - bezier_sel->cur_control->x;
      offsety = y - bezier_sel->cur_control->y;

      bezier_offset_point (bezier_sel->cur_control, offsetx, offsety);

      /* if the shift key is not down then we align the opposite control */
      /* point...ie the opposite control point acts like a mirror of the */
      /* current control point */
      
      if (!(mevent->state & ShiftMask))
	{
	  anchor = NULL;
	  opposite_control = NULL;
	  
	  if (bezier_sel->cur_control->next)
	    {
	      if (bezier_sel->cur_control->next->type == BEZIER_ANCHOR)
		{
		  anchor = bezier_sel->cur_control->next;
		  opposite_control = anchor->next;
		}
	    }
	  if (bezier_sel->cur_control->prev)
	    {
	      if (bezier_sel->cur_control->prev->type == BEZIER_ANCHOR)
		{
		  anchor = bezier_sel->cur_control->prev;
		  opposite_control = anchor->prev;
		}
	    }

	  if (!anchor)
	    fatal_error ("Encountered orphaned bezier control point");

	  if (opposite_control)
	    {
	      offsetx = bezier_sel->cur_control->x - anchor->x;
	      offsety = bezier_sel->cur_control->y - anchor->y;
	      
	      opposite_control->x = anchor->x - offsetx;
	      opposite_control->y = anchor->y - offsety;
	    }
	}
    }
  
  bezier_sel->draw = BEZIER_DRAW_CURRENT | BEZIER_DRAW_HANDLES;
  draw_core_resume (bezier_sel->core, tool);

  lastx = x;
  lasty = y;
}

static void
bezier_select_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     XtPointer gdisp_ptr;
{
  BezierSelect * bezier_sel;

  bezier_sel = tool->private;
  
  switch (action)
    {
    case PAUSE :
      draw_core_pause (bezier_sel->core, tool);
      break;
    case RESUME :
      draw_core_resume (bezier_sel->core, tool);
      break;
    case HALT :
      draw_core_stop (bezier_sel->core, tool);
      bezier_select_reset (bezier_sel);
      break;
    }
}

static void
bezier_select_draw (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  BezierSelect * bezier_sel;
  BezierPoint * points;
  int num_points;
  int draw_curve;
  int draw_handles;
  int draw_current;

  gdisp = tool->gdisp_ptr;
  bezier_sel = tool->private;

  if (!bezier_sel->draw)
    return;

  draw_curve = bezier_sel->draw & BEZIER_DRAW_CURVE;
  draw_current = bezier_sel->draw & BEZIER_DRAW_CURRENT;
  draw_handles = bezier_sel->draw & BEZIER_DRAW_HANDLES;

  /* reset to the default drawing state of drawing the curve and handles */
  bezier_sel->draw = BEZIER_DRAW_ALL;

  /* transform the points from image space to screen space */
  points = bezier_sel->points;
  num_points = bezier_sel->num_points;

  while (points && num_points)
   { 
      gdisplay_transform_coords (gdisp, points->x, points->y, &points->sx, &points->sy);
      points = points->next;
      num_points--;
    }

  if (draw_curve)
    bezier_draw_curve (bezier_sel);
  if (draw_handles)
    bezier_draw_handles (bezier_sel);
  if (draw_current)
    bezier_draw_current (bezier_sel);
}

static void
bezier_add_point (bezier_sel, type, x, y)
     BezierSelect * bezier_sel;
     int type;
     int x, y;
{
  BezierPoint *newpt;

  newpt = xmalloc (sizeof (BezierPoint));

  newpt->type = type;
  newpt->x = x;
  newpt->y = y;
  newpt->next = NULL;
  newpt->prev = NULL;

  if (bezier_sel->last_point)
    {
      bezier_sel->last_point->next = newpt;
      newpt->prev = bezier_sel->last_point;
      bezier_sel->last_point = newpt;
    }
  else
    {
      bezier_sel->points = newpt;
      bezier_sel->last_point = newpt;
    }

  switch (type)
    {
    case BEZIER_ANCHOR:
      bezier_sel->cur_anchor = newpt;
      break;
    case BEZIER_CONTROL:
      bezier_sel->cur_control = newpt;
      break;
    }

  bezier_sel->num_points += 1;
}

static void
bezier_offset_point (pt, x, y)
     BezierPoint * pt;
     int x, y;
{
  if (pt)
    {
      pt->x += x;
      pt->y += y;
    }
}

static int
bezier_check_point (pt, x, y)
     BezierPoint * pt;
     int x, y;
{
  int l, r, t, b;
  
  if (pt)
    {
      l = pt->x - BEZIER_HALFWIDTH;
      r = pt->x + BEZIER_HALFWIDTH;
      t = pt->y - BEZIER_HALFWIDTH;
      b = pt->y + BEZIER_HALFWIDTH;

      return ((x >= l) && (x <= r) && (y >= t) && (y <= b));
    }

  return 0;
}

static void
bezier_draw_curve (bezier_sel)
     BezierSelect * bezier_sel;
{
  BezierPoint * points;
  BezierPoint * start_pt;
  int num_points;

  points = bezier_sel->points;

  if (bezier_sel->closed)
    {
      start_pt = bezier_sel->points;
      
      do {
	bezier_draw_segment (bezier_sel, points, 
			     SUBDIVIDE, SCREEN_COORDS,
			     bezier_draw_segment_points);
	
	points = points->next;
	points = points->next;
	points = points->next;
      } while (points != start_pt);
    }
  else
    {
      num_points = bezier_sel->num_points;
      
      while (num_points >= 4)
	{
	  bezier_draw_segment (bezier_sel, points, 
			       SUBDIVIDE, SCREEN_COORDS,
			       bezier_draw_segment_points);
	  
	  points = points->next;
	  points = points->next;
	  points = points->next;
	  num_points -= 3;
	}
    }
}

static void
bezier_draw_handles (bezier_sel)
     BezierSelect * bezier_sel;
{
  BezierPoint * points;
  int num_points;

  points = bezier_sel->points;
  num_points = bezier_sel->num_points;
  if (num_points <= 0)
    return;
  
  do {
    if (points == bezier_sel->cur_anchor)
      {
	bezier_draw_point (bezier_sel, points, 0);
	bezier_draw_point (bezier_sel, points->next, 0);
	bezier_draw_point (bezier_sel, points->prev, 0);
	bezier_draw_line (bezier_sel, points, points->next);
	bezier_draw_line (bezier_sel, points, points->prev);
      }
    else
      {
	bezier_draw_point (bezier_sel, points, 1);
      }
    
    if (points) points = points->next;
    if (points) points = points->next;
    if (points) points = points->next;
    num_points -= 3;
  } while (num_points > 0);
}

static void
bezier_draw_current (bezier_sel)
     BezierSelect * bezier_sel;
{
  BezierPoint * points;

  points = bezier_sel->cur_anchor;

  if (points) points = points->prev;
  if (points) points = points->prev;
  if (points) points = points->prev;

  if (points)
    bezier_draw_segment (bezier_sel, points, 
			 SUBDIVIDE, SCREEN_COORDS,
			 bezier_draw_segment_points);

  if (points != bezier_sel->cur_anchor)
    {
      points = bezier_sel->cur_anchor;
      
      if (points) points = points->next;
      if (points) points = points->next;
      if (points) points = points->next;
      
      if (points)
	bezier_draw_segment (bezier_sel, bezier_sel->cur_anchor, 
			     SUBDIVIDE, SCREEN_COORDS,
			     bezier_draw_segment_points);
    }
}

static void
bezier_draw_point (bezier_sel, pt, fill)
     BezierSelect * bezier_sel;
     BezierPoint * pt;
     int fill;
{
  if (pt)
    {
      switch (pt->type)
	{
	case BEZIER_ANCHOR:
	  if (fill)
	    {
	      XFillArc (DISPLAY, bezier_sel->core->win, bezier_sel->core->gc,
			pt->sx - BEZIER_HALFWIDTH, pt->sy - BEZIER_HALFWIDTH, 
			BEZIER_WIDTH, BEZIER_WIDTH, 0, 23040);
	    }
	  else
	    {
	      XDrawArc (DISPLAY, bezier_sel->core->win, bezier_sel->core->gc,
			pt->sx - BEZIER_HALFWIDTH, pt->sy - BEZIER_HALFWIDTH, 
			BEZIER_WIDTH, BEZIER_WIDTH, 0, 23040);
	    }
	  break;
	case BEZIER_CONTROL:
	  if (fill)
	    {
	      XFillRectangle (DISPLAY, bezier_sel->core->win, bezier_sel->core->gc,
			      pt->sx - BEZIER_HALFWIDTH, pt->sy - BEZIER_HALFWIDTH, 
			      BEZIER_WIDTH, BEZIER_WIDTH);
	    }
	  else
	    {
	      XDrawRectangle (DISPLAY, bezier_sel->core->win, bezier_sel->core->gc,
			      pt->sx - BEZIER_HALFWIDTH, pt->sy - BEZIER_HALFWIDTH, 
			      BEZIER_WIDTH, BEZIER_WIDTH);
	    }
	  break;
	}
    }
}

static void
bezier_draw_line (bezier_sel, pt1, pt2)
     BezierSelect * bezier_sel;
     BezierPoint * pt1;
     BezierPoint * pt2;
{
  if (pt1 && pt2)
    {
      XDrawLine (DISPLAY, bezier_sel->core->win, 
		 bezier_sel->core->gc,
		 pt1->sx, pt1->sy, pt2->sx, pt2->sy);
    }
}

static void
bezier_draw_segment (bezier_sel, points, subdivisions, space, points_func)
     BezierSelect * bezier_sel;
     BezierPoint * points;
     int subdivisions;
     int space;
     BezierPointsFunc points_func;
{
#define ROUND(x)  ((int) ((x) + 0.5))

  static XPoint xpoints[256];
  static int nxpoints = 256;

  BezierMatrix geometry;
  BezierMatrix tmp1, tmp2;
  BezierMatrix deltas;
  double x, dx, dx2, dx3;
  double y, dy, dy2, dy3;
  double d, d2, d3;
  int lastx, lasty;
  int newx, newy;
  int index;
  int i;

  /* construct the geometry matrix from the segment */
  /* assumes that a valid segment containing 4 points is passed in */

  for (i = 0; i < 4; i++)
    {
      if (!points)
	fatal_error ("bad bezier segment");
      
      switch (space)
	{
	case IMAGE_COORDS:
	  geometry[i][0] = points->x;
	  geometry[i][1] = points->y;
	  break;
	case SCREEN_COORDS:
	  geometry[i][0] = points->sx;
	  geometry[i][1] = points->sy;
	  break;
	default:
	  fatal_error ("unknown coordinate space: %d", space);
	  break;
	}

      geometry[i][2] = 0;
      geometry[i][3] = 0;
      
      points = points->next;
    }

  /* subdivide the curve n times */
  /* n can be adjusted to give a finer or coarser curve */
  
  d = 1.0 / subdivisions;
  d2 = d * d;
  d3 = d * d * d;

  /* construct a temporary matrix for determining the forward diffencing deltas */
  
  tmp2[0][0] = 0;     tmp2[0][1] = 0;     tmp2[0][2] = 0;    tmp2[0][3] = 1;
  tmp2[1][0] = d3;    tmp2[1][1] = d2;    tmp2[1][2] = d;    tmp2[1][3] = 0;
  tmp2[2][0] = 6*d3;  tmp2[2][1] = 2*d2;  tmp2[2][2] = 0;    tmp2[2][3] = 0;
  tmp2[3][0] = 6*d3;  tmp2[3][1] = 0;     tmp2[3][2] = 0;    tmp2[3][3] = 0;

  /* compose the basis and geometry matrices */
  bezier_compose (basis, geometry, tmp1);

  /* compose the above results to get the deltas matrix */
  bezier_compose (tmp2, tmp1, deltas);

  /* extract the x deltas */
  x = deltas[0][0];
  dx = deltas[1][0];
  dx2 = deltas[2][0];
  dx3 = deltas[3][0];

  /* extract the y deltas */
  y = deltas[0][1];
  dy = deltas[1][1];
  dy2 = deltas[2][1];
  dy3 = deltas[3][1];

  lastx = x;
  lasty = y;

  xpoints[0].x = lastx;
  xpoints[0].y = lasty;
  index = 1;
  
  /* loop over the curve */
  for (i = 0; i < subdivisions; i++)
    {
      /* increment the x values */
      x += dx;
      dx += dx2;
      dx2 += dx3;

      /* increment the y values */
      y += dy;
      dy += dy2;
      dy2 += dy3;
      
      newx = ROUND (x);
      newy = ROUND (y);

      /* if this point is different than the last one...then draw it */
      if ((lastx != newx) || (lasty != newy))
	{
	  /* add the point to the point buffer */
	  xpoints[index].x = newx;
	  xpoints[index].y = newy;
	  index++;
	  
	  /* if the point buffer is full put it to the screen and zero it out */
	  if (index >= nxpoints)
	    {
	      (* points_func) (bezier_sel, xpoints, index);
	      index = 0;
	    }
	}

      lastx = newx;
      lasty = newy;
    }

  /* if there are points in the buffer, then put them on the screen */
  if (index)
    (* points_func) (bezier_sel, xpoints, index);
}

static void
bezier_draw_segment_points (bezier_sel, points, npoints)
     BezierSelect * bezier_sel;
     XPoint * points;
     int npoints;
{
  XDrawPoints (DISPLAY, bezier_sel->core->win,
	       bezier_sel->core->gc, points,
	       npoints, CoordModeOrigin);
}

static void
bezier_compose (a, b, ab)
     BezierMatrix a;
     BezierMatrix b;
     BezierMatrix ab;
{
  int i, j;

  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
        {
          ab[i][j] = (a[i][0] * b[0][j] +
                      a[i][1] * b[1][j] +
                      a[i][2] * b[2][j] +
                      a[i][3] * b[3][j]);
        }
    }
}

static int start_convert;
static int extent, width;
static int lastx;
static int lasty;

static void
bezier_convert (bezier_sel, gdisp, subdivisions)
     BezierSelect * bezier_sel;
     GDisplay * gdisp;
     int subdivisions;
{
  BezierPoint * points;
  BezierPoint * start_pt;
  link_ptr list;
  int x, w;
  int i;
  
  if (!bezier_sel->closed)
    fatal_error ("tried to convert an open bezier curve");

  /* destroy on previos region */
  if (bezier_sel->region)
    {
      gregion_free (bezier_sel->region);
      bezier_sel->region = NULL;
    }

  /* get the new regions maximum extents */
  extent = gdisp->select->region->extent;
  width = gdisp->select->region->width;

  /* create a new region */
  bezier_sel->region = gregion_new (extent, width);

  /* allocate room for the scanlines */
  bezier_sel->scanlines = xmalloc (sizeof (link_ptr) * extent);

  /* zero out the scanlines */
  for (i = 0; i < extent; i++)
    bezier_sel->scanlines[i] = NULL;

  /* scan convert the curve */
  points = bezier_sel->points;
  start_pt = bezier_sel->points;
  start_convert = 1;
  
  do {
    bezier_draw_segment (bezier_sel, points, 
			 subdivisions, IMAGE_COORDS, 
			 bezier_convert_points);
    
    points = points->next;
    points = points->next;
    points = points->next;
  } while (points != start_pt);
  
  bezier_convert_line (bezier_sel->scanlines, lastx, lasty,
		       bezier_sel->points->x, bezier_sel->points->y);

  for (i = 0; i < extent; i++)
    {
      list = bezier_sel->scanlines[i];
      while (list)
        {
          x = (int) list->data;
          list = list->next;
          if (!list)
	    warning ("cannot properly scanline convert bezier curve: %d", x);
          else 
            {
              w = (int) list->data - x;
              gregion_add_segment (bezier_sel->region, x, i, w, 255);
              list = next_item (list);
            }
        }

      free_list (bezier_sel->scanlines[i]);
    }
  
  xfree (bezier_sel->scanlines);
  bezier_sel->scanlines = NULL;
}

static void
bezier_convert_points (bezier_sel, points, npoints)
     BezierSelect * bezier_sel;
     XPoint * points;
     int npoints;
{
  int i;

  if (start_convert)
    start_convert = 0;
  else
    bezier_convert_line (bezier_sel->scanlines, lastx, lasty, points[0].x, points[0].y);

  for (i = 0; i < (npoints - 1); i++)
    {
      bezier_convert_line (bezier_sel->scanlines,
			   points[i].x, points[i].y,
			   points[i+1].x, points[i+1].y);
    }
  
  lastx = points[npoints-1].x;
  lasty = points[npoints-1].y;
}

static void
bezier_convert_line (scanlines, x1, y1, x2, y2)
  link_ptr * scanlines;
  int x1, y1, x2, y2;
{
  int dx, dy;
  int error, inc;
  int tmp;
  float slope;

  if (y1 == y2)
    return;

  if (y1 > y2)
    {
      tmp = y2; y2 = y1; y1 = tmp; 
      tmp = x2; x2 = x1; x1 = tmp; 
    }
  
  if (y1 < 0)
    {
      if (y2 < 0)
	return;
      
      if (x2 == x1)
	{
	  y1 = 0;
	}
      else
	{
	  slope = (float) (y2 - y1) / (float) (x2 - x1);
	  x1 = x2 + (0 - y2) / slope;
	  y1 = 0;
	}
    }

  if (y2 >= extent)
    {
      if (y1 >= extent)
	return;

      if (x2 == x1)
	{
	  y2 = extent;
	}
      else
	{
	  slope = (float) (y2 - y1) / (float) (x2 - x1);
	  x2 = x1 + (extent - y1) / slope;
	  y2 = extent;
	}
    }

  if (y1 == y2)
    return;

  dx = x2 - x1;
  dy = y2 - y1;
  
  scanlines = &scanlines[y1];

  if (((dx < 0) ? -dx : dx) > ((dy < 0) ? -dy : dy))
    {
      if (dx < 0)
        {
          inc = -1;
          dx = -dx;
        }
      else
        {
          inc = 1;
        }
      
      error = -dx /2;
      while (x1 != x2)
        {
          error += dy;
          if (error > 0)
            {
              error -= dx;
	      *scanlines = bezier_insert_in_list (*scanlines, x1);
	      scanlines++;
            }
          
          x1 += inc;
        }
    }
  else
    {
      error = -dy /2;
      if (dx < 0)
        {
          dx = -dx;
          inc = -1;
        }
      else
        {
          inc = 1;
        }
      
      while (y1++ < y2)
        {
	  *scanlines = bezier_insert_in_list (*scanlines, x1);
	  scanlines++;
          
          error += dx;
          if (error > 0)
            {
              error -= dy;
              x1 += inc;
            }
        }
    }
}

static link_ptr
bezier_insert_in_list (list, x)
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
