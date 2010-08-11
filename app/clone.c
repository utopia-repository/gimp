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
#include "autodialog.h"
#include "brushes.h"
#include "errors.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "paint_core.h"
#include "clone.h"
#include "selection.h"
#include "tools.h"

#define             TARGET_HEIGHT  15
#define             TARGET_WIDTH   15


/*  forward function declarations  */
static void         clone_draw            (Tool *);
static void         clone_motion          (Tool *);
static void         clone_dialog_callback (int, int, void *, void *);

/*  local variables  */
static int          target_x = 0;             /*                         */
static int          target_y = 0;             /*  position of clone src  */
static int          align_x = 0;              /*  x offset to src        */
static int          align_y = 0;              /*  y offset to src        */
static int          first = 1;
static int          trans_tx, trans_ty;       /*  transformed target  */
static int          src_gdisp_ID;             /*  ID of source gdisplay  */

static AutoDialog   clone_dlg = NULL;
static int          clone_aligned = 1;        /*  clone tool in "aligned" mode  */
static long         aligned_value;
static int          aligned_ID;


void
clone_dialog ()
{
  int group_ID;

  if (!clone_dlg)
    {
      clone_dlg = dialog_new ("Clone Options", clone_dialog_callback, NULL);
      
      group_ID = dialog_new_item (clone_dlg, 0, GROUP_ROWS, NULL, NULL);
      aligned_value = clone_aligned;
      aligned_ID = dialog_new_item (clone_dlg, group_ID, ITEM_CHECK_BUTTON, "Aligned", NULL);
      dialog_change_item (clone_dlg, aligned_ID, &aligned_value);

      dialog_show (clone_dlg);
    }

}

void *
clone_paint_func (tool, state)
     Tool * tool;
     int state;
{
  GDisplay * gdisp;
  GDisplay * src_gdisp;
  PaintCore * paint_core;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  switch (state)
    {
    case MOTION_PAINT :
      draw_core_pause (paint_core->core, tool);
      clone_motion (tool);
      break;

    case INIT_PAINT :
      if (paint_core->state & ShiftMask)
	{
	  src_gdisp_ID = gdisp->ID;
	  target_x = paint_core->curx;
	  target_y = paint_core->cury;
	  align_x = align_y = 0;
	  first = 1;
	}
      break;

    case FINISH_PAINT :
      draw_core_stop (paint_core->core, tool);
      return NULL;
      break;

    default :
      break;
    }

  /*  Calculate the coordinates of the target  */
  src_gdisp = gdisplay_get_ID (src_gdisp_ID);
  if (!src_gdisp)
    {
      src_gdisp_ID = gdisp->ID;
      src_gdisp = gdisplay_get_ID (src_gdisp_ID);
    }      

  /*  If alignment is on, add the target to the current position  */
  if (clone_aligned)
    gdisplay_transform_coords (src_gdisp, paint_core->curx + align_x, 
			       paint_core->cury + align_y, &trans_tx, &trans_ty);
  else
    gdisplay_transform_coords (src_gdisp, target_x, target_y, &trans_tx, &trans_ty);

  if (state == INIT_PAINT)
    /*  Initialize the tool drawing core  */
    draw_core_start (paint_core->core,
		     XtWindow (src_gdisp->disp_image->canvas),
		     tool);
  else if (state == MOTION_PAINT)
    draw_core_resume (paint_core->core, tool);

  return NULL;
}


Tool *
tools_new_clone ()
{
  Tool * tool;
  PaintCore * private;

  tool = paint_core_new (CLONE);

  private = (PaintCore *) tool->private;
  private->paint_func = clone_paint_func;
  private->core->draw_func = clone_draw;

  /*  Initialize the src ID to -1  */
  src_gdisp_ID = -1;

  return tool;
}


void
tools_free_clone (tool)
     Tool * tool;
{
  paint_core_free (tool);
}


static void
clone_draw (tool)
     Tool * tool;
{
  PaintCore * paint_core;

  paint_core = (PaintCore *) tool->private;

  XDrawLine (DISPLAY, paint_core->core->win, paint_core->core->gc, 
	     trans_tx - (TARGET_WIDTH >> 1), trans_ty,
	     trans_tx + (TARGET_WIDTH >> 1), trans_ty);
  XDrawLine (DISPLAY, paint_core->core->win, paint_core->core->gc, 
	     trans_tx, trans_ty - (TARGET_HEIGHT >> 1),
	     trans_tx, trans_ty + (TARGET_HEIGHT >> 1));
}


void
clone_motion (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  GDisplay * src_gdisp = NULL;
  PaintCore * paint_core;
  TempBuf * orig;
  TempBuf * new;
  int x1, y1, x2, y2;
  int offset_x, offset_y;
  unsigned char blend;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  x1 = paint_core->curx;
  y1 = paint_core->cury;
  x2 = paint_core->lastx;
  y2 = paint_core->lasty;

  /*  If the shift key is down, move the src target and return */
  if (paint_core->state & ShiftMask)
    {
      target_x = x1;
      target_y = y1;
      align_x = align_y = 0;
      first = 1;
      return;
    }
  /*  otherwise, update the target  */
  else
    {
      if (clone_aligned)
	{
	  if (first)
	    {
	      align_x = target_x - x1;
	      align_y = target_y - y1;
	      first = 0;
	    }
	  
	  /*  find offset to src area  */
	  offset_x = align_x;
	  offset_y = align_y;
	}
      else
	{
	  target_x += (x1 - x2);
	  target_y += (y1 - y2);

	  /*  find offset to src area  */
	  offset_x = target_x - x1;
	  offset_y = target_y - y1;
	}
    }

  /*  Find the source gdisplay */
  src_gdisp = gdisplay_get_ID (src_gdisp_ID);

  /*  If the src_gdisp no longer exists, do nothing  */
  if (!src_gdisp)
    return;

  /*  Check to make sure the two gdisps are of the same type  */
  if (src_gdisp->gimage->type != gdisp->gimage->type)
    return;

  /*  Get a region which can be used to paint to  */
  if (get_brush_interpolate ())
    new = paint_core_init_stroke (tool, x1, y1, x2, y2, NULL);
  else
    new = paint_core_init_discrete (tool, x1, y1, NULL);

  /*  If the source gimage is different from the destination,
   *  then we should copy straight from the destination image
   *  to the canvas.
   *  Otherwise, we need a call to get_orig_image to make sure
   *  we get a copy of the unblemished (offset) image
   */
  if (src_gdisp->gimage->ID != gdisp->gimage->ID)
    {
      x1 = new->x;  y1 = new->y;
      temp_buf_load (new, src_gdisp->gimage, new->x + offset_x, new->y + offset_y, 
		     new->width, new->height);
      new->x = x1;  new->y = y1;
    }
  else
    {
      /*  get the original image  */
      orig = paint_core_get_orig_image (tool,
					new->x + offset_x, new->y + offset_y,
					new->x + new->width + offset_x, 
					new->y + new->height + offset_y);

      /*  copy the original (offset) image to the canvas  */
      memcpy (temp_buf_data (new), temp_buf_data (orig),
	      orig->width * orig->height * orig->bytes);
    }

  /*  Get a copy of the unblemished image  */
  orig = paint_core_get_orig_image (tool,
				    new->x, new->y,
				    new->x + new->width, 
				    new->y + new->height);

  /*  determine the blend value based on the opacity  */
  /*  scale the opacity to between 0 and 255          */
  blend = (unsigned char) (255.0 * get_brush_opacity());

  /*  If the image type is indexed, preserve colors  */
  if (gdisp->gimage->type == INDEXED_GIMAGE)
    blend = 255;

  /*  blend the offset image with the original image  */
  blend_pixels (temp_buf_data (new), temp_buf_data (orig), temp_buf_data (new),
		blend, orig->width * orig->height, orig->bytes);

  /*  Make sure the clone src was in bounds  */
  {
    /*  If the source == dest and there was no floating buffer (or)
     *  the source != dest, we need to check against the src_gdisp bounds
     */
    if (!paint_core->floating)
      {
	/*  if the operation is in the normal gimage  */
	x1 = bounds (new->x + offset_x, 0, src_gdisp->gimage->width);
	y1 = bounds (new->y + offset_y, 0, src_gdisp->gimage->height);
	x2 = bounds (new->x + new->width + offset_x, 0, src_gdisp->gimage->width);
	y2 = bounds (new->y + new->height + offset_y, 0, src_gdisp->gimage->height);
      }
    else
      {
	/*  if the operation is contained in a floating buffer...  */
	int fx1, fy1, fx2, fy2;

	selection_find_bounds (gdisp->select, &fx1, &fy1, &fx2, &fy2);
      
	x1 = bounds (new->x + offset_x, fx1, fx2);
	y1 = bounds (new->y + offset_y, fy1, fy2);
	x2 = bounds (new->x + new->width + offset_x, fx1, fx2);
	y2 = bounds (new->y + new->height + offset_y, fy1, fy2);
      }
  }

  /*  paste the newly painted canvas to the gimage which is being worked on  */
  paint_core_paste_canvas (tool, x1 - offset_x, y1 - offset_y,
			   (x2 - x1), (y2 - y1));

}

static void
clone_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (clone_dlg);
      clone_dlg = NULL;
      break;
    case CANCEL_ID:
      dialog_close (clone_dlg);
      clone_dlg = NULL;
      clone_aligned = (int) aligned_value;
      break;
    default:
      if (item_ID == aligned_ID)
	clone_aligned = *((long*) call_data);
      break;
    }
}


