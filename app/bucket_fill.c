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
#include "bucket_fill.h"
#include "fuzzy_select.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "palette.h"
#include "selection.h"
#include "tools.h"
#include "undo.h"


/*  local function prototypes  */
void  bucket_fill_button_press   (Tool *, XButtonEvent *, XtPointer);
void  bucket_fill_button_release (Tool *, XButtonEvent *, XtPointer);
void  bucket_fill_motion         (Tool *, XMotionEvent *, XtPointer);
void  bucket_fill_control        (Tool *, int, void *);
void  bucket_fill_region         (GRegion *, GImage *, int, int, int, int, int, int);

static void         bucket_fill_dialog_callback (int, int, void *, void *);

/*  local variables  */
static Boolean      inside_selection;
static int          target_x, target_y;

static long         opacity = 1000;           /*  opacity of pencil tool  */
static long         threshold = 15;

static AutoDialog   dlg = NULL;
static int          opacity_scale_ID;
static long         opacity_value;
static long         opacity_data[4] = { 0, 1000, 1000, 1 };
static int          threshold_scale_ID;
static long         threshold_value;
static long         threshold_data[4] = { 0, 255, 255, 0 };


void
bucket_fill_dialog ()
{
  int group_ID;
  int frame_ID;

  if (!dlg)
    {
      dlg = dialog_new ("Bucket Fill Options", bucket_fill_dialog_callback, NULL);
      
      group_ID = dialog_new_item (dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (dlg, group_ID, ITEM_FRAME, "Opacity", NULL);
      opacity_value = opacity;
      opacity_data[2] = opacity_value;
      opacity_scale_ID = dialog_new_item (dlg, frame_ID, ITEM_SCALE, opacity_data, NULL);

      frame_ID = dialog_new_item (dlg, group_ID, ITEM_FRAME, "Fuzzy Threshold", NULL);
      threshold_value = threshold;
      threshold_data[2] = threshold_value;
      threshold_scale_ID = dialog_new_item (dlg, frame_ID, ITEM_SCALE, threshold_data, NULL);

      dialog_show (dlg);
    }
}


void
bucket_fill_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  Fill inside the current selection?  */
  if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
    inside_selection = True;
  else
    inside_selection = False;

  /*  Keep the coordinates of the target  */
  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &target_x, &target_y, False);

  /*  Make the tool active and set the gdisplay which owns it  */
  tool->gdisp_ptr = gdisp_ptr;
  tool->state = ACTIVE;

}


void
bucket_fill_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  Selection * select;
  GRegion * region = NULL;
  int x1, y1, x2, y2;
  int offset_x, offset_y;
  unsigned char col [3];

  palette_get_foreground (&col[0], &col[1], &col[2]);

  gdisp = (GDisplay *) gdisp_ptr;
  select = gdisp->select;

  /*  if the 3rd button isn't pressed, fill the selected region  */
  if (! (bevent->state & Button3Mask))
    {
      /*  If there is a floating buffer and we've clicked inside of it,
       *  we need to actually shade the pixels in the floating temp buffer
       */
      if (inside_selection && gdisp->select->float_buf)
	{
	  TempBuf * new_buf;

	  /*  save the current floating buffer  */
	  new_buf = temp_buf_copy_area (select->float_buf, NULL, 0, 0,
					select->float_buf->width,
					select->float_buf->height);

	  /*  push the new buffer to the undo stack...  */
	  undo_push_mod_sel (gdisp->ID, new_buf);

	  /*  shade the floating buffer  */
	  shade_pixels (temp_buf_data (select->float_buf),
			temp_buf_data (select->float_buf),
			col, (unsigned char) (255.0 * ((float) opacity / 1000.0)),
			select->float_buf->width * select->float_buf->height,
			select->float_buf->bytes);

	  /*  find the bounds for updating  */
	  selection_find_bounds (select, &x1, &y1, &x2, &y2);

	  /*  paste the floating buffer down on the gimage */
	  selection_stencil (select, (void *) gdisp->gimage,
			     0, 0, select->float_buf->width,
			     select->float_buf->height, BLEND_STENCIL);
	}

      /*  If there is not a floating buffer, then we need to blend the
       *  foreground color with the gimage
       */
      else
	{
	  /*  find the extents of the changed portion of the image  */
	  if (inside_selection)
	    {
	      region = gdisp->select->region;
	      gdisplay_find_bounds (gdisp, &x1, &y1, &x2, &y2);
	      offset_x = gdisp->select->offset_x;
	      offset_y = gdisp->select->offset_y;
	    }
	  else
	    {
	      region = gregion_new (gdisp->gimage->height, gdisp->gimage->width);
	      
	      find_contiguous_region (region, gdisp->gimage,
				      gdisp->gimage->bpp,
				      threshold, target_x, target_y, NULL);

	      /*  now, subtract the gdisp's current selection's region from the calculated one
	       *  (assuming there is one)
	       */
	      gregion_combine_offset_region (region, SUB, gdisp->select->region,
					     gdisp->select->offset_x, gdisp->select->offset_y);

	      gregion_find_bounds (region, &x1, &y1, &x2, &y2);
	      offset_x = offset_y = 0;
      	    }
	  /*  push the image to the undo stack... */
	  undo_push_image (gdisp->gimage->ID, x1, y1, x2, y2);
	      
	  /*  fill the selected region of the gimage...  */
	  bucket_fill_region (region, gdisp->gimage, x1, y1, x2, y2, offset_x, offset_y);
	}

      /*  update the region  */
      gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1), (y2 - y1), 0);

      /*  Free the region if we created a new one  */
      if (!inside_selection && region)
	gregion_free (region);
	
    }

  tool->state = INACTIVE;
}


void
bucket_fill_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
}


void
bucket_fill_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
}


Tool *
tools_new_bucket_fill ()
{
  Tool * tool;

  tool = (Tool *) xmalloc (sizeof (Tool));

  tool->type = BUCKET_FILL;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;  /*  Disallow scrolling  */
  tool->private = NULL;
  tool->button_press_func = bucket_fill_button_press;
  tool->button_release_func = bucket_fill_button_release;
  tool->motion_func = bucket_fill_motion;
  tool->arrow_keys_func = standard_arrow_keys_func;
  tool->control_func = bucket_fill_control;

  return tool;
}


void
tools_free_bucket_fill (tool)
     Tool * tool;
{

}


void
bucket_fill_region (region, gimage, x1, y1, x2, y2, offset_x, offset_y)
     GRegion * region;
     GImage * gimage;
     int x1, y1;
     int x2, y2;
     int offset_x, offset_y;
{
  PixelRegion srcPR, destPR;
  unsigned char col [3];

  palette_get_foreground (&col[0], &col[1], &col[2]);

  srcPR.bytes = gimage->bpp;
  srcPR.w = (x2 - x1);
  srcPR.h = (y2 - y1);
  srcPR.rowstride = gimage->width * gimage->bpp;
  srcPR.data = gimage->raw_image + y1 * srcPR.rowstride + x1 * srcPR.bytes;

  destPR.w = gimage->width;
  destPR.h = gimage->height;
  destPR.rowstride = gimage->width * gimage->bpp;
  destPR.data = gimage->raw_image;

  gregion_stencil (region, offset_x, offset_y, (float) opacity / 1000.0, 
		   &srcPR, NULL, &destPR, col, x1, y1, x2, y2,
		   SHADE_STENCIL, (gimage->type == INDEXED_GIMAGE) ? 1 : 0);
}


static void
bucket_fill_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (dlg);
      dlg = NULL;
      break;
    case CANCEL_ID:
      opacity = opacity_value;
      threshold = threshold_value;
      dialog_close (dlg);
      dlg = NULL;
      break;
    default:
      if (item_ID == opacity_scale_ID)
	opacity = *((long*) call_data);
      if (item_ID == threshold_scale_ID)
	threshold = *((long*) call_data);
      break;
    }
}
