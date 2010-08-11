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
#include "info_dialog.h"
#include "scale_tool.h"
#include "selection.h"
#include "tools.h"
#include "transform_core.h"

#define ABS(x) ((x < 0) ? -x : x)
#define X1 0
#define Y1 1
#define X2 2
#define Y2 3 

/*  storage for information dialog fields  */
char          orig_width_buf  [MAX_INFO_BUF];
char          orig_height_buf [MAX_INFO_BUF];
char          width_buf       [MAX_INFO_BUF];
char          height_buf      [MAX_INFO_BUF];
char          x_ratio_buf     [MAX_INFO_BUF];
char          y_ratio_buf     [MAX_INFO_BUF];

/*  forward function declarations  */
static void *      scale_tool_scale       (Tool *, void *);
static void *      scale_tool_recalc      (Tool *, void *);
static void        scale_tool_motion      (Tool *, void *);
static void        scale_info_update      (Tool *);


void *
scale_tool_transform (tool, gdisp_ptr, state)
     Tool * tool;
     XtPointer gdisp_ptr;
     int state;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  switch (state)
    {
    case INIT :
      if (!transform_info)
	{
	  transform_info = info_dialog_new ("scaleInfoDialog", "Scaling Information");
	  info_dialog_add_field (transform_info, "Original Width: ", orig_width_buf);
	  info_dialog_add_field (transform_info, "Original Height: ", orig_height_buf);
	  info_dialog_add_field (transform_info, "Current Width: ", width_buf);
	  info_dialog_add_field (transform_info, "Current Height: ", height_buf);
	  info_dialog_add_field (transform_info, "X Scale Ratio: ", x_ratio_buf);
	  info_dialog_add_field (transform_info, "Y Scale Ratio: ", y_ratio_buf);
	}

      selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2);
      transform_core->trans_info [X1] = (double) x1;
      transform_core->trans_info [Y1] = (double) y1;
      transform_core->trans_info [X2] = (double) x2;
      transform_core->trans_info [Y2] = (double) y2;

      return NULL;
      break;

    case MOTION :
      scale_tool_motion (tool, gdisp_ptr);

      return (scale_tool_recalc (tool, gdisp_ptr));
      break;

    case RECALC :
      return (scale_tool_recalc (tool, gdisp_ptr));
      break;

    case FINISH :
      /*  If we're in indexed color, let the transform core take care of scaling
       *  since we can't assume that colors won't change otherwise...
       */
      if (gdisp->gimage->type == INDEXED_GIMAGE)
	return NULL;
      else
	return (scale_tool_scale (tool, gdisp_ptr));
      break;
    }

  return NULL;
}


Tool *
tools_new_scale_tool ()
{
  Tool * tool;
  TransformCore * private;

  tool = transform_core_new (TRANSFORM_TOOL, INTERACTIVE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = scale_tool_transform;
  private->trans_info[X1] = 0;
  private->trans_info[Y1] = 0;
  private->trans_info[X2] = 0;
  private->trans_info[Y2] = 0;

  /*  assemble the transformation matrix  */
  identity_matrix (private->transform);

  return tool;
}


void
tools_free_scale_tool (tool)
     Tool * tool;
{
  transform_core_free (tool);
}


static void
scale_info_update (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  Selection * select;
  double ratio_x, ratio_y;
  int x1, y1, x2, y2, x3, y3, x4, y4;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  if (transform_core->select_ptr)
    select = (Selection *) transform_core->select_ptr;
  else
    select = gdisp->select;

  /*  Find original sizes  */
  gregion_find_bounds (select->region, &x1, &y1, &x2, &y2);
  sprintf (orig_width_buf, "%d", x2 - x1);
  sprintf (orig_height_buf, "%d", y2 - y1);

  /*  Find current sizes  */
  x3 = (int) transform_core->trans_info [X1];
  y3 = (int) transform_core->trans_info [Y1];
  x4 = (int) transform_core->trans_info [X2];
  y4 = (int) transform_core->trans_info [Y2];

  sprintf (width_buf, "%d", x4 - x3);
  sprintf (height_buf, "%d", y4 - y3);

  ratio_x = ratio_y = 0.0;

  if (x2 - x1)
    ratio_x = (double) (x4 - x3) / (double) (x2 - x1);
  if (y2 - y1)
    ratio_y = (double) (y4 - y3) / (double) (y2 - y1);

  sprintf (x_ratio_buf, "%0.2f", ratio_x);
  sprintf (y_ratio_buf, "%0.2f", ratio_y);

  info_dialog_update (transform_info);
  info_dialog_popup (transform_info);
}


void
scale_tool_motion (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  double ratio;
  double *x1, *y1;
  double *x2, *y2;
  int w, h;
  int dir_x, dir_y;
  int diff_x, diff_y;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  diff_x = transform_core->curx - transform_core->lastx;
  diff_y = transform_core->cury - transform_core->lasty;

  switch (transform_core->function)
    {
    case HANDLE_1 :
      x1 = &transform_core->trans_info [X1];
      y1 = &transform_core->trans_info [Y1];
      x2 = &transform_core->trans_info [X2];
      y2 = &transform_core->trans_info [Y2];
      dir_x = dir_y = 1;
      break;
    case HANDLE_2 :
      x1 = &transform_core->trans_info [X2];
      y1 = &transform_core->trans_info [Y1];
      x2 = &transform_core->trans_info [X1];
      y2 = &transform_core->trans_info [Y2];
      dir_x = -1;
      dir_y = 1;
      break;
    case HANDLE_3 :
      x1 = &transform_core->trans_info [X1];
      y1 = &transform_core->trans_info [Y2];
      x2 = &transform_core->trans_info [X2];
      y2 = &transform_core->trans_info [Y1];
      dir_x = 1;
      dir_y = -1;
      break;
    case HANDLE_4 :
      x1 = &transform_core->trans_info [X2];
      y1 = &transform_core->trans_info [Y2];
      x2 = &transform_core->trans_info [X1];
      y2 = &transform_core->trans_info [Y1];
      dir_x = dir_y = -1;
      break;
    default :
      return;
    }

  /*  if just the control key is down, affect only the height  */
  if (transform_core->state & ControlMask && ! (transform_core->state & ShiftMask))
    diff_x = 0;
  /*  if just the shift key is down, affect only the width  */
  else if (transform_core->state & ShiftMask && ! (transform_core->state & ControlMask))
    diff_y = 0;

  *x1 += diff_x;
  *y1 += diff_y;

  if (dir_x > 0)
    {
      if (*x1 >= *x2) *x1 = *x2 - 1;
    }
  else
    {
      if (*x1 <= *x2) *x1 = *x2 + 1;
    }

  if (dir_y > 0)
    {
      if (*y1 >= *y2) *y1 = *y2 - 1;
    }
  else
    {
      if (*y1 <= *y2) *y1 = *y2 + 1;
    }

  /*  if both the control key & shift keys are down, keep the aspect ratio intac
t  */
  if (transform_core->state & ControlMask && transform_core->state & ShiftMask)
    {
      ratio = (double) (transform_core->x2 - transform_core->x1) / 
        (double) (transform_core->y2 - transform_core->y1);

      w = ABS ((*x2 - *x1));
      h = ABS ((*y2 - *y1));

      if (w > h * ratio)
        h = w / ratio;
      else
        w = h * ratio;

      *y1 = *y2 - dir_y * h;
      *x1 = *x2 - dir_x * w;
    }

}


static void *
scale_tool_recalc (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * select;
  GDisplay * gdisp;
  int x1, y1, x2, y2;
  int diffx, diffy;
  int cx, cy;
  double scalex, scaley;
  
  gdisp = (GDisplay *) tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  /*  find the correct selection structure  */
  if (transform_core->select_ptr)
    select = (Selection *) transform_core->select_ptr;
  else
    select = gdisp->select;


  /*  find the boundaries of the current selection  */
  if (selection_find_bounds (select, &transform_core->x1, &transform_core->y1,
			     &transform_core->x2, &transform_core->y2))
    {
      x1 = (int) transform_core->trans_info [X1];
      y1 = (int) transform_core->trans_info [Y1];
      x2 = (int) transform_core->trans_info [X2];
      y2 = (int) transform_core->trans_info [Y2];

      scalex = scaley = 1.0;
      if (transform_core->x2 - transform_core->x1)
	scalex = (double) (x2 - x1) / (double) (transform_core->x2 - transform_core->x1);
      if (transform_core->y2 - transform_core->y1)
	scaley = (double) (y2 - y1) / (double) (transform_core->y2 - transform_core->y1);

      switch (transform_core->function)
	{
	case HANDLE_1 :
	  cx = x2;  cy = y2;
	  diffx = x2 - transform_core->x2;
	  diffy = y2 - transform_core->y2;
	  break;
	case HANDLE_2 :
	  cx = x1;  cy = y2;
	  diffx = x1 - transform_core->x1;
	  diffy = y2 - transform_core->y2;
	  break;
	case HANDLE_3 :
	  cx = x2;  cy = y1;
	  diffx = x2 - transform_core->x2;
	  diffy = y1 - transform_core->y1;
	  break;
	case HANDLE_4 :
	  cx = x1;  cy = y1;
	  diffx = x1 - transform_core->x1;
	  diffy = y1 - transform_core->y1;
	  break;
	default :
	  cx = x1; cy = y1;
	  diffx = diffy = 0;
	  break;
	}

      /*  assemble the transformation matrix  */
      identity_matrix  (transform_core->transform);
      translate_matrix (transform_core->transform, (double) -cx + diffx, (double) -cy + diffy);
      scale_matrix     (transform_core->transform, scalex, scaley);
      translate_matrix (transform_core->transform, (double) cx, (double) cy);
  
      /*  transform the bounding box  */
      transform_bounding_box (tool);

      /*  update the information dialog  */
      scale_info_update (tool);

      return (void *) select;
    }

  return (void *) NULL;
}


static void *
scale_tool_scale (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * new, * select;
  GDisplay * gdisp;
  GRegion * region;
  MaskBuf * mask;
  unsigned char * msk, * m, mask_val;
  unsigned char * src, * s;
  unsigned char * dest, * d;
  double * row, * r;
  long srcwidth, destwidth;
  int src_row, src_col;
  int bytes, b;
  int width, height;
  int orig_width, orig_height;
  double x_rat, y_rat;
  double x_cum, y_cum;
  double x_last, y_last;
  double * x_frac, y_frac, tot_frac;
  int i, j;
  int frac;
  link_ptr list;
  GSegment * seg;
  int x1, y1, x2, y2;
  int x3, y3, x4, y4;
  Boolean advance_dest;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;
  select = (Selection *) transform_core->select_ptr;
  
  region = select->region;

  gregion_find_bounds (region, &x1, &y1, &x2, &y2);

  orig_width = x2 - x1;
  orig_height = y2 - y1;

  x3 = transform_core->trans_info[X1];
  y3 = transform_core->trans_info[Y1];
  x4 = transform_core->trans_info[X2];
  y4 = transform_core->trans_info[Y2];

  width = x4 - x3;
  height = y4 - y3;

  /*  Create the new selection object  */
  new = selection_generic_new (gregion_new (height, width), x3, y3,
	  temp_buf_new (width, height, select->float_buf->bytes, 0, 0, NULL));

  /*  Create a mask buffer for transforming the selected region  */
  mask = mask_buf_new (width, height);

  /*  Some calculations...  */
  bytes = select->float_buf->bytes;
  srcwidth = bytes * select->float_buf->width;
  destwidth = bytes * width;

  /*  the data pointers...  */
  src  = temp_buf_data (select->float_buf);
  dest = temp_buf_data (new->float_buf);
  msk  = mask_buf_data (mask);

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (double) orig_width / (double) width;
  y_rat = (double) orig_height / (double) height;

  /*  allocate an array to help with the calculations  */
  row    = (double *) xmalloc (sizeof (double) * width * (bytes + 1));
  x_frac = (double *) xmalloc (sizeof (double) * (width + orig_width));

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = x1;
  x_cum = (double) src_col;
  x_last = x_cum;

  for (i = 0; i < width + orig_width; i++)
    {
      if (x_cum + x_rat <= src_col + 1)
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (double) * width * (bytes + 1));

  /*  counters...  */
  src_row = y1;
  y_cum = (double) src_row;
  y_last = y_cum;

  /*  Scale the selected region  */
  i = height;
  while (i)
    {
      /* set up the mask variables  */
      if (src_row < region->extent)
	list = region->segments[src_row];
      else
	list = NULL;

      if (list)
	seg = (GSegment *) list->data;
      else
	seg = NULL;

      src_col = x1;
      x_cum = (double) src_col;

      mask_val = (seg && src_col >= seg->start && src_col < seg->end) ? seg->value : 0;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= src_row + 1)
	{
	  y_cum += y_rat;
	  y_frac = y_cum - y_last;
	  advance_dest = True;
	}
      else
	{
	  src_row ++;
	  y_frac = src_row - y_last;
	  advance_dest = False;
	}
      y_last += y_frac;

      s = src;
      r = row;

      frac = 0;
      j = width;

      while (j)
	{
	  tot_frac = x_frac[frac++] * y_frac;

	  for (b = 0; b < bytes; b++)
	    r[b] += s[b] * tot_frac;

	  /*  set the alpha (mask) channel...  */
	  r[b] += mask_val * tot_frac;

	  /*  increment the destination  */
	  if (x_cum + x_rat <= src_col + 1)
	    {
	      r += (bytes + 1);
	      x_cum += x_rat;
	      j--;
	    }

	  /* increment the source */
	  else
	    {
	      s += bytes;
	      src_col++;

	      /*  figure out the mask  */
	      if (seg && src_col >= seg->end) 
		{
		  if ((list = next_item (list)))
		    seg = (GSegment *) list->data;
		  else
		    seg = NULL;
		}

	      mask_val = (seg && src_col >= seg->start) ? seg->value : 0;
	    }

	}

      if (advance_dest)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;
	  m = msk;

	  j = width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (unsigned char) (*r++ * tot_frac);
	      *m++ = (unsigned char) (*r++ * tot_frac);
	    }

	  dest += destwidth;
	  msk += mask->width;

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (double) * width * (bytes + 1));

	  i--;
	}
      else
	src += srcwidth;
      
    }

  /*  free up temporary arrays  */
  xfree (row);
  xfree (x_frac);

  mask_convert_to_region (mask, new->region);

  mask_buf_free (mask);

  return (void *) new;
}


