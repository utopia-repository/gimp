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
#include <math.h>
#include "appenv.h"
#include "gdisplay.h"
#include "info_dialog.h"
#include "palette.h"
#include "transform_core.h"
#include "selection.h"
#include "tools.h"
#include "undo.h"

/* define pi */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif  /*  M_PI  */

/*  variables  */
static TranInfo    old_trans_info;
InfoDialog *       transform_info = NULL;
extern int         smoothing;

/*  forward function declarations  */
static void        info_destroy_callback   (Widget, XtPointer, XtPointer);
static void        crop_buffer             (TempBuf *, int, int, int, int);
static Selection * transform_do            (Tool *, void *);
static void        transform_paste         (Tool *, void *, void *, int);

#define ABS(x) ((x < 0) ? -x : x)
#define BILINEAR(jk,j1k,jk1,j1k1,dx,dy) \
                ((1-dy) * ((1-dx)*jk + dx*j1k) + \
		    dy  * ((1-dx)*jk1 + dx*j1k1)) 


void
transform_core_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  TransformCore * transform_core;
  GDisplay * gdisp;
  int srw, srh;
  int x, y;
  int i;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  /*  if we have already displayed the bounding box and handles,
   *  check to make sure that the display which currently owns the
   *  tool is the one which just received the button pressed event
   */
  if ((transform_core->function >= CREATING) && (gdisp_ptr == tool->gdisp_ptr) &&
      transform_core->interactive)
    {
      srw = transform_core->srw;
      srh = transform_core->srh;
      
      x = bevent->x + (srw >> 1);
      y = bevent->y + (srh >> 1);

      /*  Find which handle the cursor has been clicked in, if any...  */
      if (x == bounds (x, transform_core->sx1, transform_core->sx1 + srw) &&
	  y == bounds (y, transform_core->sy1, transform_core->sy1 + srh))
	transform_core->function = HANDLE_1;
      else if (x == bounds (x, transform_core->sx2, transform_core->sx2 + srw) &&
	       y == bounds (y, transform_core->sy2, transform_core->sy2 + srh))
	transform_core->function = HANDLE_2;
      else if (x == bounds (x, transform_core->sx3, transform_core->sx3 + srw) &&
	       y == bounds (y, transform_core->sy3, transform_core->sy3 + srh))
	transform_core->function = HANDLE_3;
      else if (x == bounds (x, transform_core->sx4, transform_core->sx4 + srw) &&
	       y == bounds (y, transform_core->sy4, transform_core->sy4 + srh))
	transform_core->function = HANDLE_4;
      
      /*  otherwise, the new function will be creating, since we want to start anew  */
      else
	transform_core->function = CREATING;

      if (transform_core->function > CREATING)
	{
	  /*  Save the current transformation info  */
	  for (i = 0; i < TRAN_INFO_SIZE; i++)
	    old_trans_info [i] = transform_core->trans_info [i];
	  
	  /*  Save the current pointer position  */
	  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &transform_core->startx,
				       &transform_core->starty, True);
	  transform_core->lastx = transform_core->startx;
	  transform_core->lasty = transform_core->starty;
	  
	  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
			Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
			GrabModeAsync, None, None, bevent->time);
	  
	  tool->state = ACTIVE;
	  
	  return;
	}
    }

  /*  if the cursor is clicked inside the current selection, show the
   *  bounding box and handles...
   */
  if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
    {
      /*  If the tool is already active, clear the current state and reset  */
      if (tool->state == ACTIVE)
	transform_core_reset (tool, gdisp_ptr);

      /*  Set the pointer to the gdisplay that owns this tool  */
      tool->gdisp_ptr = gdisp_ptr;
      tool->state = ACTIVE;

      /*  Grab the pointer if we're in non-interactive mode  */
      if (!transform_core->interactive)
	XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
                      Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
                      GrabModeAsync, None, None, bevent->time);

      /*  Initialize the transform tool  */
      (* transform_core->trans_func) (tool, gdisp_ptr, INIT);
      (* transform_core->trans_func) (tool, gdisp_ptr, RECALC);
      transform_core->gregion_id = gdisp->select->region->id;

      /*  Add a destroy dialog handler  */
      if (transform_info)
	XtAddCallback (transform_info->dialog, XmNdestroyCallback, info_destroy_callback, NULL);

      /*  start drawing the bounding box and handles...  */
      draw_core_start (transform_core->core, XtWindow (gdisp->disp_image->canvas), tool);
    }

}


void
transform_core_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  Selection * new_select, * select;
  Boolean first_transform;
  int i;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  /*  if we are creating, there is nothing to be done...exit  */
  if (transform_core->function == CREATING && transform_core->interactive)
    return;

  /*  let go of the pointer lock  */
  XUngrabPointer (DISPLAY, bevent->time);

  /*  if the 3rd button isn't pressed, transform the selected region  */
  if (! (bevent->state & Button3Mask))
    {
      /*  If the select_ptr is NULL, then this is the first transformation  */
      first_transform = (transform_core->select_ptr) ? False : True;

      /*  cut the floating selection  */
      selection_cut_floating (gdisp->select, gdisp->gimage);

      /*  If we're in interactive mode, and haven't yet done any
       *  transformations, we need to copy the current selection to
       *  the transform tool's private selection pointer, so that the
       *  original source can be repeatedly modified.
       */
      if (first_transform && transform_core->interactive)
	{
	  select = selection_generic_new (gregion_copy (gdisp->select->region, NULL),
					  gdisp->select->offset_x,
					  gdisp->select->offset_y,
					  temp_buf_copy (gdisp->select->float_buf, NULL));

	  transform_core->select_ptr = (void *) select;
	}

      /*  first, send the request for the transformation to the tool...
       *  if the tool returns a NULL pointer, invoke a general transformation method.
       */
      if (! (new_select = (* transform_core->trans_func) (tool, gdisp_ptr, FINISH)))
	/*  get the transformed image via the tool's private transformation */
	new_select = transform_do (tool, gdisp_ptr);
      
      /*  Set the tool's region id to the new gdisplay region id  */
      transform_core->gregion_id = new_select->region->id;

      /*  paste the new transformed image to the gimage...also implement
       *  undo...
       */
      transform_paste (tool, gdisp_ptr, new_select, first_transform);
    }
  else
    {
      /*  stop the current tool drawing process  */
      draw_core_pause (transform_core->core, tool);

      /*  Restore the previous transformation info  */
      for (i = 0; i < TRAN_INFO_SIZE; i++)
	transform_core->trans_info [i] = old_trans_info [i];

      /*  recalculate the tool's transformation matrix  */
      (* transform_core->trans_func) (tool, gdisp_ptr, RECALC);

      /*  resume drawing the current tool  */
      draw_core_resume (transform_core->core, tool);
    }

  /*  if this tool is non-interactive, make it inactive after use  */
  if (!transform_core->interactive)
    tool->state = INACTIVE;

}


void
transform_core_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  TransformCore * transform_core;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  /*  if we are creating or this tool is non-interactive, there is 
   *  nothing to be done so exit.
   */
  if (transform_core->function == CREATING || !transform_core->interactive)
    return;

  /*  stop the current tool drawing process  */
  draw_core_pause (transform_core->core, tool);

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, &transform_core->curx,
			       &transform_core->cury, True);
  transform_core->state = mevent->state;

  /*  recalculate the tool's transformation matrix  */
  (* transform_core->trans_func) (tool, gdisp_ptr, MOTION);

  transform_core->lastx = transform_core->curx;
  transform_core->lasty = transform_core->cury;

  /*  resume drawing the current tool  */
  draw_core_resume (transform_core->core, tool);
}


void
transform_core_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  TransformCore * transform_core;

  transform_core = (TransformCore *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (transform_core->core, tool);
      break;
    case RESUME :
      /*  We need to check if the tool was paused because changes
       *  were being made to the current selection...If so, we need
       *  to reset the transform tool completely.
       */
      if (transform_core->gregion_id != ((GDisplay *) gdisp_ptr)->select->region->id)
	{
	  transform_core_reset (tool, gdisp_ptr);
	  return;
	}

      if ((* transform_core->trans_func) (tool, gdisp_ptr, RECALC))
	draw_core_resume (transform_core->core, tool);
      else
	{
	  info_dialog_popdown (transform_info);
	  tool->state = INACTIVE;
	}
      break;
    case HALT :
      draw_core_stop (transform_core->core, tool);
      info_dialog_popdown (transform_info);
      break;
    }

}


void
transform_core_no_draw (tool)
     Tool * tool;
{
  return;
}


void
transform_core_draw (tool)
     Tool * tool;
{
  int x1, y1, x2, y2, x3, y3, x4, y4;
  TransformCore * transform_core;
  GDisplay * gdisp;
  int srw, srh;

  gdisp = tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  #define SRW 10
  #define SRH 10

  gdisplay_transform_coords (gdisp, transform_core->tx1, transform_core->ty1,
			     &transform_core->sx1, &transform_core->sy1);
  gdisplay_transform_coords (gdisp, transform_core->tx2, transform_core->ty2,
			     &transform_core->sx2, &transform_core->sy2);
  gdisplay_transform_coords (gdisp, transform_core->tx3, transform_core->ty3,
			     &transform_core->sx3, &transform_core->sy3);
  gdisplay_transform_coords (gdisp, transform_core->tx4, transform_core->ty4,
			     &transform_core->sx4, &transform_core->sy4);

  x1 = transform_core->sx1;  y1 = transform_core->sy1;
  x2 = transform_core->sx2;  y2 = transform_core->sy2;
  x3 = transform_core->sx3;  y3 = transform_core->sy3;
  x4 = transform_core->sx4;  y4 = transform_core->sy4;

  /*  find the handles' width and height  */
  transform_core->srw = srw = SRW;
  transform_core->srh = srh = SRH;

  /*  draw the bounding box  */
  XDrawLine (DISPLAY, transform_core->core->win, transform_core->core->gc,
	     x1, y1, x2, y2);
  XDrawLine (DISPLAY, transform_core->core->win, transform_core->core->gc,
	     x2, y2, x4, y4);
  XDrawLine (DISPLAY, transform_core->core->win, transform_core->core->gc,
	     x3, y3, x4, y4);
  XDrawLine (DISPLAY, transform_core->core->win, transform_core->core->gc,
	     x3, y3, x1, y1);

  /*  draw the tool handles  */
  XFillRectangle (DISPLAY, transform_core->core->win, transform_core->core->gc,
		  x1 - (srw >> 1), y1 - (srh >> 1), srw, srh);
  XFillRectangle (DISPLAY, transform_core->core->win, transform_core->core->gc,
		  x2 - (srw >> 1), y2 - (srh >> 1), srw, srh);
  XFillRectangle (DISPLAY, transform_core->core->win, transform_core->core->gc,
		  x3 - (srw >> 1), y3 - (srh >> 1), srw, srh);
  XFillRectangle (DISPLAY, transform_core->core->win, transform_core->core->gc,
		  x4 - (srw >> 1), y4 - (srh >> 1), srw, srh);
}


Tool *
transform_core_new (type, interactive)
     int type;
     int interactive;
{
  Tool * tool;
  TransformCore * private;
  int i;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (TransformCore *) xmalloc (sizeof (TransformCore));

  private->interactive = interactive;

  if (interactive)
    private->core = draw_core_new (transform_core_draw);
  else
    private->core = draw_core_new (transform_core_no_draw);

  private->function = CREATING;
  private->select_ptr = NULL;
  private->gregion_id = -1;

  for (i = 0; i < TRAN_INFO_SIZE; i++)
    private->trans_info[i] = 0;

  tool->type = type;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;    /*  Do not allow scrolling  */
  tool->gdisp_ptr = NULL;
  tool->private = (void *) private;

  tool->button_press_func = transform_core_button_press;
  tool->button_release_func = transform_core_button_release;
  tool->motion_func = transform_core_motion;
  tool->arrow_keys_func = standard_arrow_keys_func;
  tool->control_func = transform_core_control;

  return tool;
}


void
transform_core_free (tool)
     Tool * tool;
{
  TransformCore * transform_core;
  Selection * select;

  transform_core = (TransformCore *) tool->private;

  /*  Make sure the selection core is not visible  */
  if (tool->state == ACTIVE)
    draw_core_stop (transform_core->core, tool);

  /*  Free the selection core  */
  draw_core_free (transform_core->core);

  /*  Free up the original selection if it exists  */
  select = (Selection *) transform_core->select_ptr;
  selection_generic_free (select);

  /*  If there is an information dialog, free it up  */
  if (transform_info)
    info_dialog_free (transform_info);
  transform_info = NULL;

  /*  Finally, free the transform tool itself  */
  xfree (transform_core);
}


void
transform_bounding_box (tool)
     Tool * tool;
{
  TransformCore * transform_core;

  transform_core = (TransformCore *) tool->private;

  transform_point (transform_core->transform,
		   transform_core->x1, transform_core->y1,
		   &transform_core->tx1, &transform_core->ty1);
  transform_point (transform_core->transform,
		   transform_core->x2, transform_core->y1,
		   &transform_core->tx2, &transform_core->ty2);
  transform_point (transform_core->transform,
		   transform_core->x1, transform_core->y2,
		   &transform_core->tx3, &transform_core->ty3);
  transform_point (transform_core->transform,
		   transform_core->x2, transform_core->y2,
		   &transform_core->tx4, &transform_core->ty4);
}


void
transform_point (m, x, y, nx, ny)
     Matrix m;
     double x, y;
     double *nx, *ny;
{
  double xx, yy, zz;

  xx = m[0][0] * x + m[0][1] * y + m[0][2];
  yy = m[1][0] * x + m[1][1] * y + m[1][2];
  zz = m[2][0] * x + m[2][1] * y + m[2][2];
  
  if (!zz)
    zz = 1.0;

  *nx = xx / zz;
  *ny = yy / zz;
}


void
mult_matrix (m1, m2)
     Matrix m1, m2;
{
  Matrix result;
  int i, j, k;

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      {
	result [i][j] = 0.0;
	for (k = 0; k < 3; k++)
	  result [i][j] += m1 [i][k] * m2[k][j];
      }

  /*  copy the result into matrix 2  */
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      m2 [i][j] = result [i][j];
}


void
identity_matrix (m)
     Matrix m;
{
  int i, j;

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      m[i][j] = (i == j) ? 1 : 0;

}


void
translate_matrix (m, x, y)
     Matrix m;
     double x, y;
{
  Matrix trans;

  identity_matrix (trans);
  trans[0][2] = x;
  trans[1][2] = y;
  mult_matrix (trans, m);
}


void
scale_matrix (m, x, y)
     Matrix m;
     double x, y;
{
  Matrix scale;

  identity_matrix (scale);
  scale[0][0] = x;
  scale[1][1] = y;
  mult_matrix (scale, m);
}


void 
rotate_matrix (m, theta)
     Matrix m;
     double theta;
{
  Matrix rotate;
  double cos_theta, sin_theta;

  cos_theta = cos (theta);
  sin_theta = sin (theta);

  identity_matrix (rotate);
  rotate[0][0] = cos_theta;
  rotate[0][1] = -sin_theta;
  rotate[1][0] = sin_theta;
  rotate[1][1] = cos_theta;
  mult_matrix (rotate, m);
}


void
xshear_matrix (m, shear)
     Matrix m;
     double shear;
{
  Matrix shear_m;

  identity_matrix (shear_m);
  shear_m[0][1] = shear;
  mult_matrix (shear_m, m);
}


void
yshear_matrix (m, shear)
     Matrix m;
     double shear;
{
  Matrix shear_m;

  identity_matrix (shear_m);
  shear_m[1][0] = shear;
  mult_matrix (shear_m, m);
}


/*  find the determinate for a 3x3 matrix  */
double
determinate (m)
     Matrix m;
{
  int i;
  double det = 0;

  for (i = 0; i < 3; i ++)
    {
      det += m[0][i] * m[1][(i+1)%3] * m[2][(i+2)%3];
      det -= m[2][i] * m[1][(i+1)%3] * m[0][(i+2)%3];
    }

  return det;
}


/*  find the cofactor matrix of a matrix  */
void
cofactor (m, m_cof)
     Matrix m;
     Matrix m_cof;
{
  int i, j;
  int x1, y1;
  int x2, y2;
  
  for (i = 0; i < 3; i++)
    {
      switch (i)
	{
	case 0 : y1 = 1; y2 = 2; break;
	case 1 : y1 = 0; y2 = 2; break;
	case 2 : y1 = 0; y2 = 1; break;
	}
      for (j = 0; j < 3; j++)
	{
	  switch (j)
	    {
	    case 0 : x1 = 1; x2 = 2; break;
	    case 1 : x1 = 0; x2 = 2; break;
	    case 2 : x1 = 0; x2 = 1; break;
	    }
	  m_cof[i][j] = (m[x1][y1] * m[x2][y2] - m[x1][y2] * m[x2][y1]) * 
	    (((i+j) % 2) ? -1 : 1);
	}
    }
}


/*  find the inverse of a 3x3 matrix  */
void
invert (m, m_inv)
     Matrix m;
     Matrix m_inv;
{
  double det = determinate (m);
  int i, j;

  if (det == 0.0)
    return;

  /*  Find the cofactor matrix of m, store it in m_inv  */
  cofactor (m, m_inv);

  /*  divide by the determinate  */
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      m_inv[i][j] = m_inv[i][j] / det;
	
}

void
transform_core_reset(tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * select;
  GDisplay * gdisp;

  transform_core = (TransformCore *) tool->private;
  gdisp = (GDisplay *) gdisp_ptr;

  select = (Selection *) transform_core->select_ptr;
  selection_generic_free (select); 
  transform_core->select_ptr = NULL;

  /*  inactivate the tool  */
  transform_core->function = CREATING;
  draw_core_stop (transform_core->core, tool);
  info_dialog_popdown (transform_info);
  tool->state = INACTIVE;

}


static void
info_destroy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Tool * tool;
  
  tool = active_tool;

  if (transform_info)
    {
      draw_core_stop (((TransformCore *) tool->private)->core, tool);
      tool->state = INACTIVE;
      transform_info = NULL;
    }
}


/*  Crop a temp buffer -- place the results back into the original  */

static void
crop_buffer (buf, x1, y1, x2, y2)
     TempBuf * buf;
     int x1, y1;
     int x2, y2;
{
  unsigned char * src;
  unsigned char * dest;
  int rowstride_src, rowstride_dest;
  int new_width;
  int new_height;
  int i;

  /*  create the new buffer based on this size  */
  new_width = (x2 - x1);
  new_height = (y2 - y1);

  rowstride_src = buf->bytes * buf->width;
  rowstride_dest = buf->bytes * new_width;

  /*  crop the src, but store the result at the origin of src...  */
  src = temp_buf_data (buf) + rowstride_src * y1 + buf->bytes * x1;
  dest = temp_buf_data (buf);

  for (i = y1; i < y2; i++)
    {
      memcpy (dest, src, rowstride_dest);
      
      src += rowstride_src;
      dest += rowstride_dest;
    }

  /*  realloc the temp buf data...
   *  Remember to change this if the temp_buf is restructured for
   *  application specific memory management routines...
   */
  buf->data = xrealloc (buf->data, new_width * new_height * buf->bytes);
  buf->width = new_width;
  buf->height = new_height;
  
}


/*  Actually carry out a transformation  */
static Selection *
transform_do (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  Selection * new, * select;
  GRegion * region;
  TempBuf * mask_buf, * new_mask;
  TempBuf * buf;
  Matrix m;
  int interpolation;
  int itx, ity;
  int offset;
  int x1, y1, x2, y2;
  int offx, offy;
  int tx1, ty1, tx2, ty2;
  int width, height, bytes, b;
  int rowstride;
  int x, y;
  double xinc, yinc;
  double tx, ty;
  double dx, dy;
  unsigned char * data_src;
  unsigned char * data_dest;
  unsigned char * mask_data_src;
  unsigned char * mask_data_dest;
  unsigned char * p1, * p2, * p3, * p4;
  unsigned char black = 0;
  unsigned char bg_col[3];

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;
  select = (Selection *) transform_core->select_ptr;

  /*  determine if interpolation is turned on...  */
  interpolation = smoothing;

  /*  If the gimage is indexed color, ignore smoothing value  */
  if (gdisp->gimage->type == INDEXED_GIMAGE)
    interpolation = 0;

  /*  Get the background color  */
  palette_get_background (&bg_col[0], &bg_col[1], &bg_col[2]);

  /*  Find the inverse of the transformation matrix  */
  invert (transform_core->transform, m);

  /*  The original bounding box  */
  x1 = transform_core->x1;
  y1 = transform_core->y1;
  x2 = transform_core->x2;
  y2 = transform_core->y2;

  /*  Find the bounding coordinates  */
  tx1 = MINIMUM (transform_core->tx1, transform_core->tx2);
  tx1 = MINIMUM (tx1, transform_core->tx3);
  tx1 = MINIMUM (tx1, transform_core->tx4);
  ty1 = MINIMUM (transform_core->ty1, transform_core->ty2);
  ty1 = MINIMUM (ty1, transform_core->ty3);
  ty1 = MINIMUM (ty1, transform_core->ty4);
  tx2 = MAXIMUM (transform_core->tx1, transform_core->tx2);
  tx2 = MAXIMUM (tx2, transform_core->tx3);
  tx2 = MAXIMUM (tx2, transform_core->tx4);
  ty2 = MAXIMUM (transform_core->ty1, transform_core->ty2);
  ty2 = MAXIMUM (ty2, transform_core->ty3);
  ty2 = MAXIMUM (ty2, transform_core->ty4);

  /*  Get a mask buffer representing the mask of the selection  */
  mask_buf = mask_convert_from_region (select->region, TRUE);

  /*  Get the new temporary buffer for the transformed result  */
  buf = temp_buf_new ((tx2 - tx1), (ty2 - ty1), gdisp->gimage->bpp, 0, 0, NULL);
  new_mask = mask_buf_new ((tx2 - tx1), (ty2 - ty1));

  width = select->float_buf->width;
  height = select->float_buf->height;
  bytes = select->float_buf->bytes;
  rowstride = width * bytes;
  data_src = temp_buf_data (select->float_buf);
  data_dest = temp_buf_data (buf);
  mask_data_src = temp_buf_data (mask_buf);
  mask_data_dest = temp_buf_data (new_mask);

  xinc = m[0][0];
  yinc = m[1][0];
  /*  z_inc = m[2][0];  Ignore the z until it becomes an issue  */

  /*  If we're interpolating, set x1 and y1 back one pixel so that
   *  we can correctly interpolate pixels on the leftmost and topmost borders
   */
  offx = x1;
  offy = y1;
  if (interpolation)
    {
      x1 --;  y1 --;
    }

  for (y = ty1; y < ty2; y++)
    {
      /*  When we calculate the inverse transformation, we should transform
       *  the center of each destination pixel...
       */
      tx = xinc * (tx1 + 0.5) + m[0][1] * (y + 0.5) + m[0][2];
      ty = yinc * (tx1 + 0.5) + m[1][1] * (y + 0.5) + m[1][2];
      /*  tz = zinc * x1 + m[2][1] * y + m[2][2];  */
      for (x = tx1; x < tx2; x++)
	{
	  /*  tx & ty are the coordinates of the point in the original
	   *  selection's floating buffer.  Make sure they're within bounds
	   */
	  if (tx < 0)
	    itx = (int) (tx - 0.999999);
	  else
	    itx = (int) tx;

	  if (ty < 0)
	    ity = (int) (ty - 0.999999);
	  else
	    ity = (int) ty;

	  /*  if interpolation is on, get the fractional error  */
	  if (interpolation)
	    {
	      dx = tx - itx;
	      dy = ty - ity;
	    }

	  if (itx >= x1 && itx < x2 && ity >= y1 && ity < y2)
	    {
	      itx -= offx;
	      ity -= offy;

	      offset = itx + ity * width;
	      /*  Set the destination pixels  */
	      if (interpolation)
		{
		  /*  calculate the offsets to the neighboring pixels  */
		  p1 = p3 = mask_data_src + offset;
		  p4 = p2 = p1 + 1;
		  p3 += width;
		  p4 += width;
		  if (itx + 1 >= width)
		    p4 = p2 = &black;
		  else if (itx < 0)
		    p1 = p3 = &black;
		  if (ity + 1 >= height)
		    p3 = p4 = &black;
		  else if (ity < 0)
		    p1 = p2 = &black;

		  *mask_data_dest++ = BILINEAR (*p1, *p2, *p3, *p4, dx, dy);

		  /*  no do the same for the image buffer  */
		  p1 = p3 = data_src + offset*bytes;
		  p4 = p2 = p1 + bytes;
		  p3 += rowstride;
		  p4 += rowstride;
		  if (itx + 1 >= width)
		    {
		      p2 = p1;
		      p4 = p3;
		    }
		  else if (itx < 0)
		    {
		      p1 = p2;
		      p3 = p4;
		    }
		  if (ity + 1 >= height)
		    {
		      p3 = p1;
		      p4 = p2;
		    }
		  else if (ity < 0)
		    {
		      p1 = p3;
		      p2 = p4;
		    }

		  for (b = 0; b < bytes; b++)
		    *data_dest++ = BILINEAR (*p1++, *p2++, *p3++, *p4++, dx, dy);
		}
	      else
		{
		  *mask_data_dest++ = mask_data_src [offset];
		  
		  offset *= bytes;
		  for (b = 0; b < bytes; b++)
		    *data_dest++ = data_src [offset++];
		}
	    }
	  else
	    {
	      /*  increment the destination pointers  */
	      mask_data_dest ++;
	      for (b = 0; b < bytes; b++)
		*data_dest ++ = bg_col[b];
	    }
	  /*  increment the transformed coordinates  */
	  tx += xinc;
	  ty += yinc;
	  /*  tz += zinc;  */
	}
    }

  /*  Make the new floating selection  */
  region = gregion_new (new_mask->height, new_mask->width);
  mask_convert_to_region (new_mask, region);
  gregion_find_bounds (region, &x1, &y1, &x2, &y2);
  crop_buffer (buf, x1, y1, x2, y2);   /*  crop the buffer  */
  new = selection_generic_new (region, tx1, ty1, buf);

  /*  Free mask buffers  */
  mask_buf_free (mask_buf);
  mask_buf_free (new_mask);
  
  return new;
}


/*  Paste a transform to the gdisplay  */

void
transform_paste (tool, gdisp_ptr, select_ptr, first)
     Tool * tool;
     void * gdisp_ptr;
     void * select_ptr;
     int first;
{
  TransformCore * transform_core;
  TransformUndo * tu;
  GDisplay * gdisp;
  Selection * select;
  int x1, y1, x2, y2;
  int x3, y3, x4, y4;
  int sm;   /*  selection margin  */
  int i;

  select = (Selection *) select_ptr;

  if (!select)
    return;

  transform_core = (TransformCore *) tool->private;
  gdisp = (GDisplay *) gdisp_ptr;

  /*  Find the bounding box for the new selection  */
  gregion_find_bounds (select->region, &x1, &y1, &x2, &y2);

  /*  Find the bounding box for the original selection  */
  selection_find_bounds (gdisp->select, &x3, &y3, &x4, &y4);

  /*  Swap the contents of the current selection and the new one  */
  selection_swap (gdisp->select, select);

  /*  paste the selection's floating buf to the gimage  */
  selection_paste_floating (gdisp->select, (void *) gdisp->gimage, BLEND_STENCIL);

  /*  create and initialize the transform_undo structure  */
  {
    tu = (TransformUndo *) xmalloc (sizeof (TransformUndo));
    tu->old_select = select;
    tu->tool_type = tool->type;
    /*  Was the selection that we're transforming just cut?  */
    tu->initial_move = gdisp->select->fresh_cut;
    /*  Was this the first adjustment?  */
    tu->first = first;
    for (i = 0; i < TRAN_INFO_SIZE; i++)
      tu->trans_info[i] = old_trans_info[i];
  }

  /*  If we can't push this paste operation onto the undo stack,
   *  we must delete the old gdisplay select structure
   */
  if (! undo_push_transform (gdisp->ID, (void *) tu))
    {
      selection_generic_free (select);
      xfree (tu);
    }

  /*  Find the bounding box for the new selection  */
  selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2);

  x1 = MINIMUM (x1, x3);
  y1 = MINIMUM (y1, y3);
  x2 = MAXIMUM (x2, x4);
  y2 = MAXIMUM (y2, y4);

  /* update the gdisplays  */
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);
  
}

