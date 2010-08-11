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

/* This tool is based on a paper from SIGGRAPH '95
 * thanks to Professor D. Forsyth for prompting us to implement this tool
 */

#include <stdlib.h>
#include <math.h>
#include <values.h>
#include "appenv.h"
#include "draw_core.h"
#include "errors.h"
#include "gdisplay.h"
#include "iscissors.h"
#include "edit_selection.h"
#include "paint_funcs.h"
#include "temp_buf.h"

#include "gdisplay_ops.h"


/*  local structures  */

typedef struct _ICurve ICurve;

struct _ICurve
{
  int x1, y1;
  int x2, y2;
  link_ptr points;
};


typedef struct _iscissors Iscissors;

struct _iscissors
{
  DrawCore *      core;         /*  Core select object                      */
  
  int             x, y;         /*  upper left hand coordinate              */
  int             ix, iy;       /*  initial coordinates                     */
  int             nx, ny;       /*  new coordinates                         */

  TempBuf *       cost_buf;     /*  cost map buffer                         */
  TempBuf *       dp_buf;       /*  dynamic programming buffer              */

  ICurve *        curve1;       /*  1st curve connected to current point    */
  ICurve *        curve2;       /*  2nd curve connected to current point    */

  link_ptr        curves;       /*  the list of curves                      */

  GRegion *       region;       /*  region represented by closed iscissors  */

  int             first_point;  /*  is this the first point?                */
  int             connected;    /*  is the region closed?                   */

  int             state;        /*  state of intelligent scissor operation  */
  int             draw;         /*  items to draw on a draw request         */

  link_ptr *      scanlines;    /*  scanline array                          */
};


/**********************************************/
/*  Intelligent scissors selection apparatus  */

/*  The possible states...  */
#define  NO_ACTION         0x0
#define  SEED_PLACEMENT    0x1
#define  SEED_ADJUSTMENT   0x2
#define  WAITING           0x3

/*  The possible drawing states...  */
#define  DRAW_NOTHING      0x0
#define  DRAW_CURRENT_SEED 0x1
#define  DRAW_CURVE        0x2
#define  DRAW_ACTIVE_CURVE 0x4
#define  DRAW_ALL          (DRAW_CURRENT_SEED | DRAW_CURVE)

/*  Other defines...  */
#define  GRADIENT_SEARCH   32
#define  TARGET_HEIGHT     25
#define  TARGET_WIDTH      25
#define  POINT_WIDTH       8
#define  POINT_HALFWIDTH   4
#define  EXTEND_BY         0.2
#define  FIXED             5
#define  MIN_GRADIENT      63

#define  MAX_POINTS        2048

#define  COST_WIDTH        2  /* number of bytes for each pixel in cost map  */
#define  BLOCK_WIDTH       64
#define  BLOCK_HEIGHT      64
#define  CONV_WIDTH        (BLOCK_WIDTH + 2)
#define  CONV_HEIGHT       (BLOCK_HEIGHT + 2)

/* #define  OMEGA_Z           0.16 */
#define  OMEGA_D           0.2
#define  OMEGA_G           0.8

#define  SEED_POINT        9

/*  Functional defines  */
#define  PIXEL_COST(x)     (x >> 8)
#define  PIXEL_DIR(x)      (x & 0x000000ff)


/*  static variables  */

/*  where to move on a given link direction  */
static int move [8][2] =
{
  { 1, 0 },
  { 0, 1 },
  { -1, 1 },
  { 1, 1 },
  { -1, 0 },
  { 0, -1 },
  { 1, -1 },
  { -1, -1 },
};

/*  XPoints for drawing curves  */
static XPoint      curve_points [MAX_POINTS];

/*  cost map blocks variables  */
static TempBuf **  cost_map_blocks = NULL;
static int         horz_blocks;
static int         vert_blocks;


/*  convolution buffers --  */
static unsigned char  maxgrad_conv0 [GRADIENT_SEARCH * GRADIENT_SEARCH * 3];
static unsigned char  maxgrad_conv1 [GRADIENT_SEARCH * GRADIENT_SEARCH * 3];
static unsigned char  maxgrad_conv2 [GRADIENT_SEARCH * GRADIENT_SEARCH * 3];

/*  static unsigned char  cost_conv0 [CONV_WIDTH * CONV_HEIGHT * 3];*/
static unsigned char  cost_conv1 [CONV_WIDTH * CONV_HEIGHT * 3];
static unsigned char  cost_conv2 [CONV_WIDTH * CONV_HEIGHT * 3];

static int  horz_deriv [9] =
{
  1, 0, -1,
  2, 0, -2,
  1, 0, -1,
};

static int  vert_deriv [9] =
{
  1, 2, 1,
  0, 0, 0,
  -1, -2, -1,
};

/*
static int  laplacian [9] = 
{
  -1, -1, -1,
  -1, 8, -1,
  -1, -1, -1,
};
*/

static int blur_32 [9] = 
{
  1, 1, 1,
  1, 24, 1,
  1, 1, 1,
};

static float distance_weights [GRADIENT_SEARCH * GRADIENT_SEARCH];

static int   diagonal_weight [256];
static int   direction_value [256][4];
static int   initialized = 0;

/***********************************************************************/


/*  Local function prototypes  */
static void iscissors_draw_curve (GDisplay *, Iscissors *, ICurve *);
static void iscissors_reset (Iscissors *);
static void iscissors_free_icurves (link_ptr);
static void iscissors_free_buffers ();
static void iscissors_convert (Iscissors *, void *);
static void iscissors_convert_curve (Iscissors *, ICurve *);
static link_ptr iscissors_insert_in_list (link_ptr, int);
static int  clicked_on_vertex (Tool *);
static int  clicked_on_curve (Tool *);
static void precalculate_arrays ();
static void calculate_curve (Tool *, ICurve *);
static int  calculate_link (unsigned char *, unsigned char *, int);
static link_ptr plot_pixels (Iscissors *, TempBuf *, int, int, int, int, int, int);
static void find_optimal_path (TempBuf *, TempBuf *, int, int);
static void find_max_gradient (GImage *, int *, int *);

/*  cost map and dynamic programming buffer utility functions  */
static TempBuf *  calculate_cost_map     (GImage *, int, int, int, int);
static void       construct_cost_map     (Tool *, TempBuf *);

/*  cost map blocks utility functions  */
static void  set_cost_map_blocks         (void *, int, int, int, int);
static void  allocate_cost_map_blocks    (int, int, int, int);
static void  free_cost_map_blocks        (void);



/*  Function definitions  */

void
iscissors_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  Iscissors * iscissors;
  int replace, op;
  int grab_pointer = 0;

  gdisp = (GDisplay *) gdisp_ptr;
  iscissors = (Iscissors *) tool->private;

  /*  If the tool was being used in another image...reset it  */
  if (tool->state == ACTIVE && gdisp_ptr != tool->gdisp_ptr)
    {
      iscissors->draw = DRAW_CURVE;
      draw_core_stop (iscissors->core, tool);
      
      iscissors_reset (iscissors);
    }

  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;

  /*  Determine the correct action  */
  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, 
			       &iscissors->x, &iscissors->y, False);

  switch (iscissors->state)
    {
    case NO_ACTION :
      if (!(bevent->state & ShiftMask) && !(bevent->state & ControlMask))
	if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
	  {
	    init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
	    return;
	  }
      
      /*  If the cost map blocks haven't been allocated, do so now  */
      if (!cost_map_blocks)
	  allocate_cost_map_blocks (BLOCK_WIDTH, BLOCK_HEIGHT, gdisp->gimage->width,
				    gdisp->gimage->height);

      iscissors->state = SEED_PLACEMENT;
      iscissors->draw = DRAW_CURRENT_SEED;
      grab_pointer = 1;

      if (! (bevent->state & ShiftMask))
	find_max_gradient (gdisp->gimage, &iscissors->x, &iscissors->y);
      iscissors->x = bounds (iscissors->x, 0, gdisp->gimage->width - 1);
      iscissors->y = bounds (iscissors->y, 0, gdisp->gimage->height - 1);
      
      iscissors->ix = iscissors->x;
      iscissors->iy = iscissors->y;

      /*  Initialize the selection core only on starting the tool  */
      draw_core_start (iscissors->core,
			 XtWindow (gdisp->disp_image->canvas),
			 tool);
      break;

    default :
      /*  Check if the mouse click occured on a vertex or the curve itself  */
      if (clicked_on_vertex (tool))
	{
	  iscissors->nx = iscissors->x;
	  iscissors->ny = iscissors->y;
	  iscissors->state = SEED_ADJUSTMENT;
	  iscissors->draw = DRAW_ACTIVE_CURVE;
	  draw_core_resume (iscissors->core, tool);
	  grab_pointer = 1;
	}
      /*  If the iscissors is connected, check if the click was inside  */
      else if (iscissors->connected && iscissors->region &&
	       gregion_point_inside (iscissors->region, iscissors->x, iscissors->y))
	{
	  /*  Undraw the curve  */
	  tool->state = INACTIVE;
	  iscissors->draw = DRAW_CURVE;
	  draw_core_stop (iscissors->core, tool);

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
				  op, iscissors->region);
	  
	  iscissors_reset (iscissors);
	  
	  selection_start (gdisp->select, 0, True);

	}
      /*  if we're not connected, we're adding a new point  */
      else if (!iscissors->connected)
	{
	  iscissors->state = SEED_PLACEMENT;
	  iscissors->draw = DRAW_CURRENT_SEED;
	  grab_pointer = 1;
	  
	  draw_core_resume (iscissors->core, tool);
	}
      break;

    }

  if (grab_pointer)
    XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		  Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		  GrabModeAsync, None, None, bevent->time);
      
}

void
iscissors_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  Iscissors * iscissors;
  GDisplay * gdisp;
  ICurve * curve;

  gdisp = (GDisplay *) gdisp_ptr;
  iscissors = (Iscissors *) tool->private;

  /*  Make sure X didn't skip the button release event -- as it's known to do  */
  if (iscissors->state == WAITING)
    return;

  XUngrabPointer (DISPLAY, bevent->time);

  /*  Undraw everything  */
  switch (iscissors->state)
    {
    case SEED_PLACEMENT :
      iscissors->draw = DRAW_CURVE | DRAW_CURRENT_SEED;
      break;
    case SEED_ADJUSTMENT :
      iscissors->draw = DRAW_CURVE | DRAW_ACTIVE_CURVE;
      break;
    }
  draw_core_stop (iscissors->core, tool);

  /*  First take care of the case where the user "cancels" the action  */
  if (! (bevent->state & Button3Mask))
    {
      switch (iscissors->state)
	{
	case SEED_PLACEMENT :
	  /*  Add a new icurve  */
	  if (!iscissors->first_point)
	    {
	      /*  Determine if we're connecting to the first point  */
	      if (iscissors->curves)
		{
		  curve = (ICurve *) iscissors->curves->data;
		  if (abs (iscissors->x - curve->x1) < POINT_HALFWIDTH &&
		      abs (iscissors->y - curve->y1) < POINT_HALFWIDTH)
		    {
		      iscissors->x = curve->x1;
		      iscissors->y = curve->y1;
		      iscissors->connected = 1;
		    }
		}

	      /*  Create the new curve segment  */
	      if (iscissors->ix != iscissors->x || iscissors->iy != iscissors->y)
		{
		  curve = xmalloc (sizeof (ICurve));

		  curve->x1 = iscissors->ix;
		  curve->y1 = iscissors->iy;
		  iscissors->ix = curve->x2 = iscissors->x;
		  iscissors->iy = curve->y2 = iscissors->y;
		  curve->points = NULL;
		  iscissors->curves = append_to_list (iscissors->curves, (void *) curve);

		  calculate_curve (tool, curve);
		}
	    }
	  else
	    iscissors->first_point = 0;
	  break;

	case SEED_ADJUSTMENT :
	  /*  recalculate both curves  */
	  if (iscissors->curve1)
	    {
	      iscissors->curve1->x1 = iscissors->nx;
	      iscissors->curve1->y1 = iscissors->ny;
	      calculate_curve (tool, iscissors->curve1);
	    }
	  if (iscissors->curve2)
	    {
	      iscissors->curve2->x2 = iscissors->nx;
	      iscissors->curve2->y2 = iscissors->ny;
	      calculate_curve (tool, iscissors->curve2);
	    }
	  break;

	default :
	  break;
	}
    }

  /*  Draw only the boundary  */
  iscissors->state = WAITING;
  iscissors->draw = DRAW_CURVE;
  draw_core_resume (iscissors->core, tool);

  /*  convert the curves into a region  */
  if (iscissors->connected)
    iscissors_convert (iscissors, gdisp_ptr);
}

void
iscissors_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  Iscissors * iscissors;
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;
  iscissors = (Iscissors *) tool->private;

  if (tool->state != ACTIVE || iscissors->state == NO_ACTION)
    return;

  if (iscissors->state == SEED_PLACEMENT)
    iscissors->draw = DRAW_CURRENT_SEED;
  else if (iscissors->state == SEED_ADJUSTMENT)
    iscissors->draw = DRAW_ACTIVE_CURVE;

  draw_core_pause (iscissors->core, tool);
      
  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, 
			       &iscissors->x, &iscissors->y, False);

  /*  decide what to do based on the scissors' state  */
  switch (iscissors->state)
    {
    case SEED_PLACEMENT :
      /*  Hold the shift key down to disable the auto-edge snap feature  */
      if (! (mevent->state & ShiftMask))
	find_max_gradient (gdisp->gimage, &iscissors->x, &iscissors->y);
      iscissors->x = bounds (iscissors->x, 0, gdisp->gimage->width - 1);
      iscissors->y = bounds (iscissors->y, 0, gdisp->gimage->height - 1);

      if (iscissors->first_point)
	{
	  iscissors->ix = iscissors->x;
	  iscissors->iy = iscissors->y;
	}
      break;

    case SEED_ADJUSTMENT :
      /*  Move the current seed to the location of the cursor  */
      if (! (mevent->state & ShiftMask))
	find_max_gradient (gdisp->gimage, &iscissors->x, &iscissors->y);
      iscissors->x = bounds (iscissors->x, 0, gdisp->gimage->width - 1);
      iscissors->y = bounds (iscissors->y, 0, gdisp->gimage->height - 1);

      iscissors->nx = iscissors->x;
      iscissors->ny = iscissors->y;
      break;

    default :
      break;
    }

  draw_core_resume (iscissors->core, tool);

}


void
iscissors_draw (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  Iscissors * iscissors;
  ICurve * curve;
  link_ptr list;
  int tx1, ty1, tx2, ty2;
  int txn, tyn;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  iscissors = (Iscissors *) tool->private;

  gdisplay_transform_coords (gdisp, iscissors->ix, iscissors->iy, &tx1, &ty1);

  /*  Draw the target if we're placing a seed  */
  if (iscissors->draw & DRAW_CURRENT_SEED)
    {
      gdisplay_transform_coords (gdisp, iscissors->x, iscissors->y, &tx2, &ty2);

      XDrawLine (DISPLAY, iscissors->core->win, iscissors->core->gc, 
		 tx2 - (TARGET_WIDTH >> 1), ty2,
		 tx2 + (TARGET_WIDTH >> 1), ty2);
      XDrawLine (DISPLAY, iscissors->core->win, iscissors->core->gc, 
		 tx2, ty2 - (TARGET_HEIGHT >> 1),
		 tx2, ty2 + (TARGET_HEIGHT >> 1));

      if (!iscissors->first_point)
	XDrawLine (DISPLAY, iscissors->core->win, iscissors->core->gc, 
		   tx1, ty1, tx2, ty2);
    }
  if ((iscissors->draw & DRAW_CURVE) && !iscissors->first_point)
    {
      /*  Draw a point at the init point coordinates  */
      if (!iscissors->connected)
	XFillArc (DISPLAY, iscissors->core->win, iscissors->core->gc,
		  tx1 - POINT_HALFWIDTH, ty1 - POINT_HALFWIDTH, 
		  POINT_WIDTH, POINT_WIDTH, 0, 23040);

      /*  Go through the list of icurves, and render each one...  */
      list = iscissors->curves;
      while (list)
	{
	  curve = (ICurve *) list->data;

	  /*  plot the curve  */
	  iscissors_draw_curve (gdisp, iscissors, curve);

	  gdisplay_transform_coords (gdisp, curve->x1, curve->y1, &tx1, &ty1);

	  XFillArc (DISPLAY, iscissors->core->win, iscissors->core->gc,
		    tx1 - POINT_HALFWIDTH, ty1 - POINT_HALFWIDTH, 
		    POINT_WIDTH, POINT_WIDTH, 0, 23040);

	  list = next_item (list);
	}
    }
  if (iscissors->draw & DRAW_ACTIVE_CURVE)
    {
      gdisplay_transform_coords (gdisp, iscissors->nx, 
				 iscissors->ny, &txn, &tyn);

      /*  plot both curves, and the control point between them  */
      if (iscissors->curve1)
	{
	  gdisplay_transform_coords (gdisp, iscissors->curve1->x2, 
				     iscissors->curve1->y2, &tx1, &ty1);

	  XDrawLine (DISPLAY, iscissors->core->win, iscissors->core->gc, 
		     tx1, ty1, txn, tyn);
	}
      if (iscissors->curve2)
	{
	  gdisplay_transform_coords (gdisp, iscissors->curve2->x1, 
				     iscissors->curve2->y1, &tx2, &ty2);

	  XDrawLine (DISPLAY, iscissors->core->win, iscissors->core->gc, 
		     tx2, ty2, txn, tyn);
	}

      XFillArc (DISPLAY, iscissors->core->win, iscissors->core->gc,
		txn - POINT_HALFWIDTH, tyn - POINT_HALFWIDTH, 
		POINT_WIDTH, POINT_WIDTH, 0, 23040);

    }

}


static void
iscissors_draw_curve (gdisp, iscissors, curve)
     GDisplay * gdisp;
     Iscissors * iscissors;
     ICurve * curve;
{
  link_ptr pt_list;
  int tx, ty;
  int points;
  int coords;

  points = 0;
  pt_list = curve->points;
  while (pt_list)
    {
      coords = (int) pt_list->data;
      gdisplay_transform_coords (gdisp, (coords & 0x0000ffff), 
				 (coords >> 16), &tx, &ty);
      curve_points [points].x = tx;
      curve_points [points].y = ty;
      points ++;
      pt_list = next_item (pt_list);
    }
  
  /*  draw the curve  */
  XDrawLines (DISPLAY, iscissors->core->win, iscissors->core->gc, 
	      curve_points, points, CoordModeOrigin);
}


void
iscissors_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  Iscissors * iscissors;
  int draw;

  iscissors = (Iscissors *) tool->private;
  switch (iscissors->state)
    {
    case SEED_PLACEMENT :
      draw = DRAW_CURVE | DRAW_CURRENT_SEED;
      break;
    case SEED_ADJUSTMENT :
      draw = DRAW_CURVE | DRAW_ACTIVE_CURVE;
      break;
    default :
      draw = DRAW_CURVE;
      break;
    }

  switch (action)
    {
    case PAUSE : 
      iscissors->draw = draw;
      draw_core_pause (iscissors->core, tool);
      break;
    case RESUME :
      iscissors->draw = draw;
      draw_core_resume (iscissors->core, tool);
      break;
    case HALT :
      iscissors->draw = draw;
      draw_core_stop (iscissors->core, tool);
      iscissors_reset (iscissors);
      break;
    }
}


Tool *
tools_new_iscissors ()
{
  Tool * tool;
  Iscissors * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (Iscissors *) xmalloc (sizeof (Iscissors));

  private->core = draw_core_new (iscissors_draw);
  private->core->line_width = 2;  /*  set line width at 2 for visibility  */
  private->curves = NULL;
  private->cost_buf = NULL;
  private->dp_buf = NULL;
  private->region = NULL;
  private->state = NO_ACTION;
  
  tool->type = ISCISSORS;
  tool->state = INACTIVE;
  tool->scroll_lock = 0;  /*  Allow scrolling  */
  tool->private = (void *) private;
  tool->button_press_func = iscissors_button_press;
  tool->button_release_func = iscissors_button_release;
  tool->motion_func = iscissors_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = iscissors_control;

  iscissors_reset (private);

  return tool;
}


void
tools_free_iscissors (tool)
     Tool * tool;
{
  Iscissors * iscissors;

  iscissors = (Iscissors *) tool->private;

  /*  Undraw curve  */
  iscissors->draw = DRAW_CURVE;

  draw_core_stop (iscissors->core, tool);

  iscissors_reset (iscissors);
  draw_core_free (iscissors->core);
  xfree (iscissors);
 
}


static void
iscissors_reset (iscissors)
     Iscissors * iscissors;
{
  /*  Free and reset the curve list  */
  if (iscissors->curves)
    {
      iscissors_free_icurves (iscissors->curves);
      free_list (iscissors->curves);
    }
  iscissors->curves = NULL;
  iscissors->curve1 = NULL;
  iscissors->curve2 = NULL;
  iscissors->first_point = 1;
  iscissors->connected = 0;
  iscissors->state = NO_ACTION;

  /*  Reset the region  */
  if (iscissors->region)
    gregion_free (iscissors->region);
  iscissors->region = NULL;
  
  /*  Reset the cost map and dp buffers  */
  iscissors_free_buffers (iscissors);

  /*  Reset the cost map blocks structure  */
  free_cost_map_blocks ();

  /*  If they haven't already been initialized, precalculate the diagonal
   *  weight and direction value arrays
   */
  if (!initialized)
    {
      precalculate_arrays ();
      initialized = 1;
    }
}


static void
iscissors_free_icurves (list)
     link_ptr list;
{
  ICurve * curve;

  while (list)
    {
      curve = (ICurve *) list->data;
      if (curve->points)
	free_list (curve->points);
      free (curve);
      list = next_item (list);
    }
}


static void
iscissors_free_buffers (iscissors)
     Iscissors * iscissors;
{
  if (iscissors->cost_buf)
    temp_buf_free (iscissors->cost_buf);
  if (iscissors->dp_buf)
    temp_buf_free (iscissors->dp_buf);

  iscissors->cost_buf = NULL;
  iscissors->dp_buf = NULL;
}


static void 
iscissors_convert (iscissors, gdisp_ptr)
     Iscissors * iscissors;
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  link_ptr list;
  ICurve * curve;
  int i, x, w;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  destroy the existing region  */
  if (iscissors->region)
    gregion_free (iscissors->region);

  /*  create a new region  */
  iscissors->region = gregion_new (gdisp->gimage->height,
				   gdisp->gimage->width);

  /* allocate room for the scanlines */
  iscissors->scanlines = xmalloc (sizeof (link_ptr) * iscissors->region->extent);

  /* zero out the scanlines */
  for (i = 0; i < iscissors->region->extent; i++)
    iscissors->scanlines[i] = NULL;

  /*  Go through the list of icurves, and scan convert each one...  */
  list = iscissors->curves;
  while (list)
    {
      curve = (ICurve *) list->data;

      /*  scan convert the curve  */
      iscissors_convert_curve (iscissors, curve);
      
      list = next_item (list);
    }

  for (i = 0; i < iscissors->region->extent; i++)
    {
      list = iscissors->scanlines[i];
      while (list)
        {
          x = (int) list->data;
          list = list->next;
          if (!list)
	    warning ("cannot properly scanline convert iscissors curve: y: %d, x: %d\n", i, x);
          else 
            {
              w = (int) list->data - x;
              gregion_add_segment (iscissors->region, x, i, w, 255);
              list = next_item (list);
            }
        }

      free_list (iscissors->scanlines[i]);
    }
  
  xfree (iscissors->scanlines);
  iscissors->scanlines = NULL;
  
}


static void iscissors_convert_curve (iscissors, curve)
     Iscissors * iscissors;
     ICurve * curve;
{
  link_ptr pt_list;
  int x, y;
  int nx, ny;
  int coords;

  if (! (pt_list = curve->points))
    return;

  coords = (int) pt_list->data;
  x = coords & 0x0000ffff;
  y = coords >> 16;

  pt_list = next_item (pt_list);

  while (pt_list)
    {
      coords = (int) pt_list->data;
      nx = coords & 0x0000ffff;
      ny = coords >> 16;

      if (ny < y)
	iscissors->scanlines [ny] = iscissors_insert_in_list (iscissors->scanlines [ny], nx);
      else if (ny > y)
	iscissors->scanlines [y] = iscissors_insert_in_list (iscissors->scanlines [y], x);

      x = nx;
      y = ny;
      pt_list = next_item (pt_list);
    }
}


static link_ptr
iscissors_insert_in_list (list, x)
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


static int
clicked_on_vertex (tool)
     Tool * tool;
{
  Iscissors * iscissors;
  link_ptr list;
  ICurve * curve;
  int curves_found = 0;

  iscissors = (Iscissors *) tool->private;

  /*  traverse through the list, returning non-zero if the current cursor
   *  position is on an existing curve vertex.  Set the curve1 and curve2
   *  variables to the two curves containing the vertex in question
   */

  iscissors->curve1 = iscissors->curve2 = NULL;

  list = iscissors->curves;

  while (list && curves_found < 2)
    {
      curve = (ICurve *) list->data;

      if (abs (curve->x1 - iscissors->x) < POINT_HALFWIDTH &&
	  abs (curve->y1 - iscissors->y) < POINT_HALFWIDTH)
	{
	  iscissors->curve1 = curve;
	  if (curves_found++)
	    return 1;
	}
      else if (abs (curve->x2 - iscissors->x) < POINT_HALFWIDTH &&
	       abs (curve->y2 - iscissors->y) < POINT_HALFWIDTH)
	{
	  iscissors->curve2 = curve;
	  if (curves_found++)
	    return 1;
	}

      list = next_item (list);
    }
  
  /*  if only one curve was found, the curves are unconnected, and
   *  the user only wants to move either the first or last point
   *  disallow this for now.
   */
  if (curves_found == 1)
    {
      return 0;
    }
    
  /*  no vertices were found at the cursor click point.  Now check whether
   *  the click occured on a curve.  If so, create a new vertex there and
   *  two curve segments to replace what used to be just one...
   */
  return clicked_on_curve (tool);
}


static int
clicked_on_curve (tool)
     Tool * tool;
{
  Iscissors * iscissors;
  link_ptr list, new_link;
  link_ptr pts;
  ICurve * curve, * new_curve;
  int coords;
  int tx, ty;

  iscissors = (Iscissors *) tool->private;

  /*  traverse through the list, returning non-zero if the current cursor
   *  position is on a curve...  If this occurs, replace the curve with two
   *  new curves, separated by the new vertex.
   */

  list = iscissors->curves;
  while (list)
    {
      curve = (ICurve *) list->data;

      pts = curve->points;

      while (pts)
	{
	  coords = (int) pts->data;
	  tx = coords & 0x0000ffff;
	  ty = coords >> 16;

	  /*  Is the specified point close enough to the curve?  */
	  if (abs (tx - iscissors->x) < POINT_HALFWIDTH &&
	      abs (ty - iscissors->y) < POINT_HALFWIDTH)
	    {
	      /*  Since we're modifying the curve, undraw the existing one  */
	      iscissors->draw = DRAW_CURVE;
	      draw_core_pause (iscissors->core, tool);

	      /*  Create the new curve  */
	      new_curve = xmalloc (sizeof (ICurve));

	      new_curve->x2 = curve->x2;
	      new_curve->y2 = curve->y2;
	      new_curve->x1 = curve->x2 = iscissors->x;
	      new_curve->y1 = curve->y2 = iscissors->y;
	      new_curve->points = NULL;

	      /*  Create the new link and supply the new curve as data  */
	      new_link = alloc_list ();
	      new_link->data = (void *) new_curve;
	      
	      /*  Insert the new link in the list  */
	      new_link->next = list->next;
	      list->next = new_link;
	  
	      iscissors->curve1 = new_curve;
	      iscissors->curve2 = curve;

	      /*  Redraw the curve  */
	      draw_core_resume (iscissors->core, tool);
	      
	      return 1;
	    }
	  
	  pts = next_item (pts);
	}


      list = next_item (list);
    }

  /*  no vertices were found at the cursor click point.  Now check whether
   *  the click occured on a curve.  If so, create a new vertex there and
   *  two curve segments to replace what used to be just one...
   */
  return 0;
}


static void
precalculate_arrays ()
{
  int i;

  for (i = 0; i < 256; i++)
    {
      /*  The diagonal weight array  */
      diagonal_weight [i] = (int) (i * M_SQRT2);

      /*  The direction value array  */
      direction_value [i][0] = (127 - abs (127 - i)) * 2;
      direction_value [i][1] = abs (127 - i) * 2;
      direction_value [i][2] = abs (191 - i) * 2;
      direction_value [i][3] = abs (63 - i) * 2;

/*
      printf ("i: %d, v0: %d, v1: %d, v2: %d, v3: %d\n", i, direction_value [i][0],
	      direction_value [i][1], direction_value [i][2], direction_value [i][3]);
*/
    }

  /*  set the 256th index of the direction_values to the hightest cost  */
  direction_value [255][0] = 255;
  direction_value [255][1] = 255;
  direction_value [255][2] = 255;
  direction_value [255][3] = 255;
}


static void
calculate_curve (tool, curve)
     Tool * tool;
     ICurve * curve;
{
  GDisplay * gdisp;
  Iscissors * iscissors;
  int x, y, dir;
  int xs, ys, xe, ye;
  int x1, y1, x2, y2;
  int width, height;
  int ewidth, eheight;

  /*  Calculate the lowest cost path from one vertex to the next as specified
   *  by the parameter "curve".
   *    Here are the steps:
   *      1)  Calculate the appropriate working area for this operation
   *      2)  Calculate any portions of the local cost map which were exposed by this area
   *      3)  Generate a temp buf representing the local cost map for the area
   *      4)  Allocate a temp buf for the dynamic programming array
   *      5)  Run the dynamic programming algorithm to find the optimal path
   *      6)  Translate the optimal path into pixels in the icurve data structure
   */

  gdisp = (GDisplay *) tool->gdisp_ptr;
  iscissors = (Iscissors *) tool->private;

  /*  Get the bounding box  */
  xs = bounds (curve->x1, 0, gdisp->gimage->width - 1);
  ys = bounds (curve->y1, 0, gdisp->gimage->height - 1);
  xe = bounds (curve->x2, 0, gdisp->gimage->width - 1);
  ye = bounds (curve->y2, 0, gdisp->gimage->height - 1);
  x1 = MINIMUM (xs, xe);
  y1 = MINIMUM (ys, ye);
  x2 = MAXIMUM (xs, xe) + 1;  /*  +1 because if xe = 199 & xs = 0, x2 - x1, width = 200  */
  y2 = MAXIMUM (ys, ye) + 1;

  /*  expand the boundaries past the ending points by 
   *  some percentage of width and height.  This serves the following purpose:
   *  It gives the algorithm more area to search so better solutions
   *  are found.  This is particularly helpful in finding "bumps" which
   *  fall outside the bounding box represented by the start and end
   *  coordinates of the "curve".
   */
  ewidth = (x2 - x1) * EXTEND_BY + FIXED;
  eheight = (y2 - y1) * EXTEND_BY + FIXED;

  if (xe >= xs)
    x2 += bounds (ewidth, 0, gdisp->gimage->width - x2);
  else
    x1 -= bounds (ewidth, 0, x1);
  if (ye >= ys)
    y2 += bounds (eheight, 0, gdisp->gimage->height - y2);
  else
    y1 -= bounds (eheight, 0, y1);


  /*  If the bounding box has width and height...  */
  if ((x2 - x1) && (y2 - y1))
    {
      width = (x2 - x1);
      height = (y2 - y1);
      /*  expose the cost map blocks corresponding to this curve's area */
      set_cost_map_blocks (gdisp->gimage, x1, y1, width, height);
      
      /*  get the cost map (for this area) in a temporary buffer  */
      iscissors->cost_buf = 
	temp_buf_resize (iscissors->cost_buf, COST_WIDTH, x1, y1, width, height);
      construct_cost_map (tool, iscissors->cost_buf);
      
      /*  allocate the dynamic programming array  */
      iscissors->dp_buf = 
	temp_buf_resize (iscissors->dp_buf, 4, x1, y1, width, height);
      
      /*  find the optimal path of pixels from (x1, y1) to (x2, y2)  */
      find_optimal_path (iscissors->cost_buf, iscissors->dp_buf, xs - x1, ys - y1);
      
      /*  get a list of the pixels in the optimal path  */
      if (curve->points)
	free_list (curve->points);
      curve->points = plot_pixels (iscissors, iscissors->dp_buf, x1, y1, xs, ys, xe, ye);
    }
  /*  If the bounding box has no width  */
  else if ((x2 - x1) == 0)
    {
      /*  plot a vertical line  */
      y = ys;  
      dir = (ys > ye) ? -1 : 1;
      curve->points = NULL;
      while (y != ye)
	{
	  curve->points = add_to_list (curve->points, (void *) ((y << 16) + xs));
	  y += dir;
	}
    }
  /*  If the bounding box has no height  */
  else if ((y2 - y1) == 0)
    {
      /*  plot a horizontal line  */
      x = xs;
      dir = (xs > xe) ? -1 : 1;
      curve->points = NULL;
      while (x != xe)
	{
	  curve->points = add_to_list (curve->points, (void *) ((ys << 16) + x));
	  x += dir;
	}
    }

}


static int
calculate_link (p, q, link)
     unsigned char * p;
     unsigned char * q;
     int link;
{
  int value = 0;

  /*  calculate the contribution of the laplacian  */
/*  if (*((char *) q) > 0 || *((char *) p) <= 0)
    value += 255 * OMEGA_Z;
*/

  /*  calculate the contribution of the gradient magnitude  */

  if (link > 1)
    value += diagonal_weight [q[0]] * OMEGA_G;
  else
    value += q[0] * OMEGA_G;

  /*  calculate the contribution of the gradient direction  */
  value += (direction_value [q [1]][link] + direction_value [p [1]][link]) * OMEGA_D;

  return value;
}


static link_ptr
plot_pixels (iscissors, dp_buf, x1, y1, xs, ys, xe, ye)
     Iscissors * iscissors;
     TempBuf * dp_buf;
     int x1, y1;
     int xs, ys;
     int xe, ye;
{
  int x, y;
  int coords;
  int link;
  int width;
  unsigned int * data;
  link_ptr list = NULL;

  width = dp_buf->width;

  /*  Start the data pointer at the correct location  */
  data = (unsigned int *) temp_buf_data (dp_buf) + (ye - y1) * width + (xe - x1);

  x = xe;
  y = ye;

  while (1)
    {
      coords = (y << 16) + x;
      list = add_to_list (list, (void *) coords);

      link = PIXEL_DIR (*data);
      if (link == SEED_POINT)
	return list;

      x += move [link][0];
      y += move [link][1];
      data += move [link][1] * width + move [link][0];
    }

  /*  won't get here  */
  return NULL;
}


static void
find_optimal_path (cost_buf, dp_buf, xs, ys)
     TempBuf * cost_buf;
     TempBuf * dp_buf;
     int xs, ys;
{
  int i, j, k;
  int x, y;
  int link;
  int linkdir;
  int dirx, diry;
  int min_cost;
  int new_cost;
  int cum_cost [8];
  int link_cost [8];
  int pixel_cost [8];
  int pixel [8];
  unsigned int * data, *d;
  unsigned char * cost, *c;

  /*  initialize the dynamic programming buffer  */
  data = (unsigned int *) temp_buf_data (dp_buf);
  for (i = 0; i < dp_buf->height; i++)
    for (j = 0; j < dp_buf->width; j++)
      *data++ = 0;  /*  0 cumulative cost, 0 direction  */

  /*  what directions are we filling the array in according to?  */
  dirx = (xs == 0) ? 1 : -1;
  diry = (ys == 0) ? 1 : -1;
  linkdir = (dirx * diry);

  y = ys;

  /*  Start the data pointer at the correct location  */
  data = (unsigned int *) temp_buf_data (dp_buf);
  cost = (unsigned char *) temp_buf_data (cost_buf);
  
  for (i = 0; i < dp_buf->height; i++)
    {
      x = xs;

      d = data + y * dp_buf->width + x;
      c = cost + COST_WIDTH * (y * cost_buf->width + x);
 
      for (j = 0; j < dp_buf->width; j++)
	{
	  min_cost = MAXINT;

	  for (k = 0; k < 8; k++)
	    pixel [k] = 0;

	  /*  Find the valid neighboring pixels  */
	  /*  the previous pixel  */
	  if (j)
	    pixel [((dirx == 1) ? 4 : 0)] = -dirx;

	  /*  the previous row of pixels  */
	  if (i)
	    {
	      pixel [((diry == 1) ? 5 : 1)] = -diry * dp_buf->width;

	      link = (linkdir == 1) ? 3 : 2;
	      if (j)
		pixel [((diry == 1) ? (link + 4) : link)] = -diry * dp_buf->width - dirx;

	      link = (linkdir == 1) ? 2 : 3;
	      if (j != dp_buf->width - 1)
		pixel [((diry == 1) ? (link + 4) : link)] = -diry * dp_buf->width + dirx;
	    }

	  /*  find the minimum cost of going through each neighbor to reach the
	   *  seed point...
	   */
	  link = -1;
	  for (k = 0; k < 8; k ++)
	    if (pixel [k])
	      {
		link_cost [k] = calculate_link (c, c + (COST_WIDTH * pixel [k]), ((k > 3) ? k - 4 : k));
		pixel_cost [k] = PIXEL_COST (d [pixel [k]]);
		cum_cost [k] = pixel_cost [k] + link_cost [k];
		if (cum_cost [k] < min_cost)
		  {
		    min_cost = cum_cost [k];
		    link = k;
		  }
	      }

	  /*  If anything can be done...  */
	  if (link >= 0)
	    {
	      /*  set the cumulative cost of this pixel and the new direction  */
	      *d = (cum_cost [link] << 8) + link;

	      /*  possibly change the links from the other pixels to this pixel...
	       *  these changes occur if a neighboring pixel will receive a lower
	       *  cumulative cost by going through this pixel.  
	       */
	      for (k = 0; k < 8; k ++)
		if (pixel [k] && k != link)
		  {
		    /*  if the cumulative cost at the neighbor is greater than
		     *  the cost through the link to the current pixel, change the
		     *  neighbor's link to point to the current pixel.
		     */
		    new_cost = link_cost [k] + cum_cost [link];
		    if (pixel_cost [k] > new_cost)
		      /*  reverse the link direction   /-----------------------\ */
		      d[pixel [k]] = (new_cost << 8) + ((k > 3) ? k - 4 : k + 4);
		  }
	    } 
	  /*  Set the seed point  */
	  else if (!i && !j)
	    *d = SEED_POINT;
	    
	  /*  increment the data pointer and the x counter  */
	  c += COST_WIDTH*dirx;
	  d += dirx;
	  x += dirx;
	}

      /*  increment the y counter  */
      y += diry;
    }
}


static void
find_max_gradient (gimage, x, y)
     GImage * gimage;
     int * x, * y;
{
  static int first_gradient = 1;

  PixelRegion srcPR, destPR;
  int radius;
  int i, j, b;
  int sx, sy, cx, cy;
  int x1, y1, x2, y2;
  float gradient, max_gradient;
  unsigned char * dh, * dv, hmax, vmax;

  radius = GRADIENT_SEARCH >> 1;
  /*  calculate the extent of the search  */
  cx = bounds (*x, 0, gimage->width);
  cy = bounds (*y, 0, gimage->height);
  sx = cx - radius;
  sy = cy - radius;
  x1 = bounds (cx - radius, 0, gimage->width);
  y1 = bounds (cy - radius, 0, gimage->height);
  x2 = bounds (cx + radius, 0, gimage->width);
  y2 = bounds (cy + radius, 0, gimage->height);
  /*  calculate the factor to multiply the distance from the cursor by  */

  if (first_gradient)
    {
      /*  compute the distance weights  */
      for (i = 0; i < GRADIENT_SEARCH; i++)
	for (j = 0; j < GRADIENT_SEARCH; j++)
	  distance_weights [i * GRADIENT_SEARCH + j] =
	    1.0 / (1 + sqrt ((i - radius) * (i - radius) + (j - radius) * (j - radius)));
      first_gradient = 0;
    }

  /*  Set the gimage up as the source pixel region  */
  srcPR.bytes = gimage->bpp;
  srcPR.w = (x2 - x1) + 1;
  srcPR.h = (y2 - y1) + 1;
  srcPR.rowstride = gimage->width * gimage->bpp;
  srcPR.data = gimage->raw_image + y1 * srcPR.rowstride + x1 * srcPR.bytes;

  /*  Blur the source to get rid of noise  */
  destPR.rowstride = GRADIENT_SEARCH * 3;
  destPR.data = maxgrad_conv0;
  convolve_region (&srcPR, &destPR, blur_32, 3, 32, NORMAL);

  /*  Set the "src" temp buf up as the new source Pixel Region  */
  srcPR.rowstride = destPR.rowstride;
  srcPR.data = destPR.data;

  /*  Get the horizontal derivative  */
  destPR.data = maxgrad_conv1;
  convolve_region (&srcPR, &destPR, horz_deriv, 3, 1, ABSOLUTE);

  /*  Get the vertical derivative  */
  destPR.data = maxgrad_conv2;
  convolve_region (&srcPR, &destPR, vert_deriv, 3, 1, ABSOLUTE);

  /*  Get the pointers to the derivatives  */

  max_gradient = 0;
  *x = cx;
  *y = cy;
  x1 = radius - (cx - x1);
  x2 = GRADIENT_SEARCH + (x2 - (cx + radius));
  y1 = radius - (cy - y1);
  y2 = GRADIENT_SEARCH + (y2 - (cy + radius));

  /*  Find the point of max gradient
   *    skip the border pixels since they were not calculated
   *    correctly during the convolutions
   */
  for (i = y1 + 1; i < y2 - 1; i++)
    {
      dh = maxgrad_conv1 + srcPR.rowstride*(i-y1) + srcPR.bytes;
      dv = maxgrad_conv2 + srcPR.rowstride*(i-y1) + srcPR.bytes;

      for (j = x1 + 1; j < x2 - 1; j++)
	{	  

	  hmax = dh[0];
	  vmax = dv[0];
	  for (b = 1; b < gimage->bpp; b++)
	    {
	      if (dh[b] > hmax) hmax = dh[b];
	      if (dv[b] > vmax) vmax = dv[b];
	    }

	  gradient = sqrt (hmax * hmax + vmax * vmax);
	  gradient *= distance_weights [i * GRADIENT_SEARCH +j];

	  if (gradient > max_gradient)
	    {
	      max_gradient = gradient;
	      *x = sx + j;
	      *y = sy + i;
	    }
	  dh += srcPR.bytes;
	  dv += srcPR.bytes;
	}
    }
}


static TempBuf *
calculate_cost_map (gimage, x, y, w, h)
     GImage * gimage;
     int x, y;
     int w, h;
{
  TempBuf * cost_map;
  PixelRegion srcPR, destPR;
  int width, height;
  int offx, offy;
  int i, j, b;
  int x1, y1, x2, y2;
  double gradient, direction;
  unsigned char * dh, * dv, * cm;
  int hmax, vmax;

  /*  allocate the new cost map  */
  cost_map = temp_buf_new (w, h, COST_WIDTH, x, y, NULL);

  /*  calculate the extent of the search make a 1 pixel border */
  x1 = bounds (x - 1, 0, gimage->width);
  y1 = bounds (y - 1, 0, gimage->height);
  x2 = bounds (x + w + 1, 0, gimage->width);
  y2 = bounds (y + h + 1, 0, gimage->height);

  width = x2 - x1;
  height = y2 - y1;
  offx = (x - x1);
  offy = (y - y1);

  /*  Set the gimage up as the source pixel region  */
  srcPR.bytes = gimage->bpp;
  srcPR.w = width;
  srcPR.h = height;
  srcPR.rowstride = gimage->width * gimage->bpp;
  srcPR.data = gimage->raw_image + y1 * srcPR.rowstride + x1 * srcPR.bytes;

  /*  Get the laplacian  */
/*
  destPR.data = cost_conv0 + 3 * (CONV_WIDTH * (1 - offy) + (1 - offx));
  destPR.rowstride = CONV_WIDTH * 3;
  convolve_region (&srcPR, &destPR, laplacian, 3, 1, NEGATIVE);
*/

  /*  Get the horizontal derivative  */
  destPR.data = cost_conv1 + 3 * (CONV_WIDTH * (1 - offy) + (1 - offx));
  destPR.rowstride = CONV_WIDTH * 3;
  convolve_region (&srcPR, &destPR, horz_deriv, 3, 1, NEGATIVE);

  /*  Get the vertical derivative  */
  destPR.data = cost_conv2 + 3 * (CONV_WIDTH * (1 - offy) + (1 - offx));
  convolve_region (&srcPR, &destPR, vert_deriv, 3, 1, NEGATIVE);

  cm = temp_buf_data (cost_map);

  /*  fill in the cost map  */

  for (i = 0; i < h; i++)
    {
/*      l =  cost_conv0 + destPR.rowstride*(i + offy) + srcPR.bytes * offx;*/
      dh = cost_conv1 + destPR.rowstride*(i + offy) + srcPR.bytes * offx;
      dv = cost_conv2 + destPR.rowstride*(i + offy) + srcPR.bytes * offx;

      for (j = 0; j < w; j++)
	{	  
	  hmax = dh[0] - 128;
	  vmax = dv[0] - 128;
/*	  lmax = l[0] - 128;*/
	  for (b = 1; b < gimage->bpp; b++)
	    {
	      if (abs (dh[b] - 128) > abs (hmax)) hmax = dh[b] - 128;
	      if (abs (dv[b] - 128) > abs (vmax)) vmax = dv[b] - 128;
/*	      if (abs (l[b] - 128) > abs (lmax)) lmax = l[b] - 128;*/
	    }

	  /*  store the information in the cost map  */
	  /*  first, the laplacian  */
/*	  *cm++ = (unsigned char) lmax;*/
	  
	  /*  Find the gradient  */
	  gradient = sqrt (hmax * hmax + vmax * vmax);
	  *cm++ = 255 - (unsigned char) bounds (gradient, 0, 255);

	  /*  Find the direction, if the gradient is significant*/
	  if (gradient > MIN_GRADIENT)
	    {
	      if (!hmax)
		direction = (vmax > 0) ? M_PI_2 : -M_PI_2;
	      else
		direction = atan ((double) vmax / (double) hmax);
	      /*  Scale the direction from between 0 and 254, corresponding to -PI/2, PI/2 
	       *  255 is reserved for directionless pixels
	       */
	      *cm++ = (unsigned char) (254 * (direction + M_PI_2) / M_PI);
	    }
	  else
	    *cm++ = 255;  /* reserved for weak gradient pixels  */

	  dh += srcPR.bytes;
	  dv += srcPR.bytes;
/*	  l += srcPR.bytes;*/

	}

    }

  return cost_map;
}


static void
construct_cost_map (tool, cost_buf)
     Tool * tool;
     TempBuf * cost_buf;
{
  TempBuf * block;
  int index;
  int x, y;
  int endx, endy;
  int row, col;
  int offx, offy;
  int x2, y2;
  PixelRegion srcPR, destPR;

  /*  init some variables  */
  srcPR.bytes = cost_buf->bytes;
  destPR.rowstride = cost_buf->bytes * cost_buf->width;

  y = cost_buf->y;
  endx = cost_buf->x + cost_buf->width;
  endy = cost_buf->y + cost_buf->height;

  row = (y / BLOCK_HEIGHT);
  /*  patch the buffer with the saved portions of the image  */
  while (y < endy)
    {
      x = cost_buf->x;
      col = (x / BLOCK_WIDTH);

      /*  calculate y offset into this row of blocks  */
      offy = (y - row * BLOCK_HEIGHT);
      y2 = (row + 1) * BLOCK_HEIGHT;
      if (y2 > endy) y2 = endy;
      srcPR.h = y2 - y;

      while (x < endx)
      {
	index = row * horz_blocks + col;
	block = cost_map_blocks [index];
	/*  If the block exists, patch it into buf  */
	if (block)
	  {
	    /* calculate x offset into the block  */
	    offx = (x - col * BLOCK_WIDTH);
	    x2 = (col + 1) * BLOCK_WIDTH;
	    if (x2 > endx) x2 = endx;
	    srcPR.w = x2 - x;
	    
	    /*  Calculate the offsets into each buffer  */
	    srcPR.rowstride = srcPR.bytes * block->width;
	    srcPR.data = temp_buf_data (block) + offy * srcPR.rowstride + offx * srcPR.bytes;
	    destPR.data = temp_buf_data (cost_buf) + 
	      (y - cost_buf->y) * destPR.rowstride + (x - cost_buf->x) * cost_buf->bytes;

	    copy_region (&srcPR, &destPR);
	  }

	col ++;
	x = col * BLOCK_WIDTH;
      }

      row ++;
      y = row * BLOCK_HEIGHT;
    }
/*
  {
    TempBuf * new;

    new = temp_buf_copy (cost_buf, NULL);

    temp_buf_to_gdisplay (new);
  }
*/
}


/*  cost map blocks utility functions  */

static void
set_cost_map_blocks (gimage_ptr, x, y, w, h)
     void * gimage_ptr;
     int x, y;
     int w, h;
{
  GImage * gimage;
  int endx, endy;
  int startx;
  int index;
  int x1, y1;
  int x2, y2;
  int row, col;
  int width, height;

  gimage = (GImage *) gimage_ptr;
  width = gimage->width;
  height = gimage->height;

  startx = x;
  endx = x + w;
  endy = y + h;

  row = y / BLOCK_HEIGHT;
  while (y < endy)
    {
      col = x / BLOCK_WIDTH;
      while (x < endx)
	{
	  index = row * horz_blocks + col;
	  
	  /*  If the block doesn't exist, create and initialize it  */
	  if (! cost_map_blocks [index])
	    {
	      /*  determine memory efficient width and height of block  */
	      x1 = col * BLOCK_WIDTH;
	      x2 = bounds (x1 + BLOCK_WIDTH, 0, width);
	      w = (x2 - x1);
	      y1 = row * BLOCK_HEIGHT;
	      y2 = bounds (y1 + BLOCK_HEIGHT, 0, height);
	      h = (y2 - y1);

	      /*  calculate a cost map for the specified portion of the gimage  */
	      cost_map_blocks [index] = calculate_cost_map (gimage, x1, y1, w, h);
	    }
	  col++;
	  x = col * BLOCK_WIDTH;
	}

      row ++;
      y = row * BLOCK_HEIGHT;
      x = startx;
    }
}


static void
allocate_cost_map_blocks (block_width, block_height, image_width, image_height)
     int block_width, block_height;
     int image_width, image_height;
{
  int num_blocks;
  int i;

  /*  calculate the number of rows and cols in the cost map block grid  */
  horz_blocks = (image_width + block_width - 1) / block_width;
  vert_blocks = (image_height + block_height - 1) / block_height;

  /*  Allocate the array  */
  num_blocks = horz_blocks * vert_blocks;
  cost_map_blocks = (TempBuf **) xmalloc (sizeof (TempBuf *) * num_blocks);

  /*  Initialize the array  */
  for (i = 0; i < num_blocks; i++)
    cost_map_blocks [i] = NULL;
}


static void
free_cost_map_blocks ()
{
  int i;
  int num_blocks;

  if (!cost_map_blocks)
    return;

  num_blocks = vert_blocks * horz_blocks;

  for (i = 0; i < num_blocks; i++)
    if (cost_map_blocks [i])
      temp_buf_free (cost_map_blocks [i]);

  xfree (cost_map_blocks);

  cost_map_blocks = NULL;
}
