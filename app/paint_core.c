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
#include "brushes.h"
#include "errors.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "paint_core.h"
#include "selection.h"
#include "tools.h"
#include "undo.h"

#define    MIN_BLOCK_WIDTH  64
#define    MIN_BLOCK_HEIGHT 64

/*  local function prototypes  */
static void  reconstruct_original_image  (Tool *, void *, TempBuf *);
static void  interpolate_brush_mask      (TempBuf *, TempBuf *, int, int, int, int);


/***********************************************************************/


/*  undo blocks variables  */
static TempBuf **  undo_blocks = NULL;
static int         block_width;
static int         block_height;
static int         horz_blocks;
static int         vert_blocks;

/*  undo blocks utility functions  */
static void  set_undo_blocks             (void *, void *, int, int, int, int);
static void  allocate_undo_blocks        (int, int, int, int);
static void  free_undo_blocks            (void);


/***********************************************************************/


/*  paint buffers variables  */
static TempBuf *   orig_buf = NULL;
static TempBuf *   canvas_buf = NULL;
static TempBuf *   mask_buf = NULL;

/*  paint buffers utility functions  */
static void       free_paint_buffers     ();


/***********************************************************************/


void
paint_core_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  PaintCore * paint_core;
  GDisplay * gdisp;
  GBrushP brush;
  int w, h;
  int x, y;

  gdisp = (GDisplay *) gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  /*  Each buffer is the same size as the maximum bounds of the active brush... */
  if (!(brush = get_active_brush ()))
    {
      warning ("No brushes available for use with this tool.");
      return;
    }

  /*  If the undo blocks exist, nuke them  */
  if (undo_blocks)
    free_undo_blocks ();

  /*  Is there a floating selection?  */
  paint_core->floating = (gdisp->select->float_buf) ? True : False;

  if (paint_core->floating)
    {
      w = gdisp->select->float_buf->width;
      h = gdisp->select->float_buf->height;
    }
  else
    {
      w = gdisp->gimage->width;
      h = gdisp->gimage->height;
    }

  /*  Allocate the undo structure  */
  allocate_undo_blocks (brush->mask->width, brush->mask->height, w, h);

  /*  initialize some values  */
  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &x, &y, False);
  paint_core->startx = paint_core->lastx = paint_core->curx = x;
  paint_core->starty = paint_core->lasty = paint_core->cury = y;
  paint_core->state = bevent->state;

  /*  Get the initial undo extents  */
  paint_core->x1 = paint_core->x2 = x;
  paint_core->y1 = paint_core->y2 = y;
  paint_core->num_movements = 0;

  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;
  tool->paused_count = 0;

  /*  pause the current selection and grab the pointer  */
  selection_pause (gdisp->select);

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);
      
  /*  Let the specific painting function initialize itself  */
  (* paint_core->paint_func) (tool, INIT_PAINT);

  /*  Paint to the image  */
  (* paint_core->paint_func) (tool, MOTION_PAINT);
}


void
paint_core_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * undo_buf;
  int bx1, by1, bx2, by2;

  gdisp = (GDisplay *) gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  /*  resume the current selection and ungrab the pointer  */
  selection_resume (gdisp->select);
  XUngrabPointer (DISPLAY, bevent->time);

  /*  Let the specific painting function finish up  */
  (* paint_core->paint_func) (tool, FINISH_PAINT);

  if (paint_core->floating)
   {
      selection_find_bounds (gdisp->select, &bx1, &by1, &bx2, &by2);
      paint_core->x1 = bounds (paint_core->x1, bx1, bx2) - bx1;
      paint_core->y1 = bounds (paint_core->y1, by1, by2) - by1;
      paint_core->x2 = bounds (paint_core->x2, bx1, bx2) - bx1;
      paint_core->y2 = bounds (paint_core->y2, by1, by2) - by1;
    }

  /*  Get the undo image  */
  undo_buf = temp_buf_new ((paint_core->x2 - paint_core->x1),
			   (paint_core->y2 - paint_core->y1),
			   gdisp->gimage->bpp,
			   paint_core->x1, paint_core->y1, NULL);

  reconstruct_original_image (tool, gdisp->select->float_buf, undo_buf);

  /*  Try to push the undo--if it fails, clean up this buffer  */
  if (paint_core->floating)
    {
      if (! undo_push_mod_sel (gdisp->ID, (void *) undo_buf))
	temp_buf_free (undo_buf);
    }
  else
    {
      if (! undo_push_buffer (gdisp->gimage->ID, (void *) undo_buf))
	temp_buf_free (undo_buf);
    }
  
  /*  Set tool state to inactive -- no longer painting */
  tool->state = INACTIVE;
}


void
paint_core_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  PaintCore * paint_core;

  gdisp = (GDisplay *) gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y,
			       &paint_core->curx, &paint_core->cury, False);
  paint_core->state = mevent->state;

  /*  increment the number of movements  */
  paint_core->num_movements++;

  (* paint_core->paint_func) (tool, MOTION_PAINT);

  paint_core->lastx = paint_core->curx;
  paint_core->lasty = paint_core->cury;
}


void
paint_core_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  PaintCore * paint_core;

  paint_core = (PaintCore *) tool->private;

  switch (action)
    {
    case PAUSE :
      draw_core_pause (paint_core->core, tool);
      break;
    case RESUME :
      draw_core_resume (paint_core->core, tool);
      break;
    case HALT :
      (* paint_core->paint_func) (tool, FINISH_PAINT);
      break;
    }

}


void
paint_core_no_draw (tool)
     Tool * tool;
{
  return;
}


Tool *
paint_core_new (type)
     int type;
{
  Tool * tool;
  PaintCore * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (PaintCore *) xmalloc (sizeof (PaintCore));

  private->core = draw_core_new (paint_core_no_draw);

  tool->type = type;
  tool->state = INACTIVE;
  tool->scroll_lock = 0;  /*  Allow scrolling  */
  tool->gdisp_ptr = NULL;
  tool->private = (void *) private;

  tool->button_press_func = paint_core_button_press;
  tool->button_release_func = paint_core_button_release;
  tool->motion_func = paint_core_motion;
  tool->arrow_keys_func = standard_arrow_keys_func;
  tool->control_func = paint_core_control;

  return tool;
}


void
paint_core_free (tool)
     Tool * tool;
{
  PaintCore * paint_core;

  paint_core = (PaintCore *) tool->private;

  /*  Make sure the selection core is not visible  */
  if (tool->state == ACTIVE)
    draw_core_stop (paint_core->core, tool);

  /*  Free the selection core  */
  draw_core_free (paint_core->core);

  /*  If the undo blocks exist, nuke them  */
  if (undo_blocks)
    free_undo_blocks ();

  /*  Free the temporary buffers if they exist  */
  free_paint_buffers ();

  /*  Finally, free the paint tool itself  */
  xfree (paint_core);
}



/************************/
/*  Painting functions  */
/************************/


TempBuf *
paint_core_init_discrete (tool, x, y, custom_bmask)
     Tool * tool;
     int x, y;
     TempBuf * custom_bmask;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * brush_mask;
  GBrushP brush;
  int bytes;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;
  brush = get_active_brush ();
  bytes = gdisp->gimage->bpp;

  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  x -= (brush->mask->width >> 1);
  y -= (brush->mask->height >> 1);
  
  /*  configure the canvas buffer  */
  canvas_buf = temp_buf_resize (canvas_buf, bytes, x, y,
				brush->mask->width, brush->mask->height);

  /*  determine which brush mask to use */
  if (custom_bmask)
    brush_mask = custom_bmask;
  else
    brush_mask = brush->mask;

  /*  set the paint tool's brush mask  */
  paint_core->brush_mask = brush_mask;

  return canvas_buf;
}


TempBuf *
paint_core_init_stroke (tool, x1, y1, x2, y2, custom_bmask)
     Tool * tool;
     int x1, y1;
     int x2, y2;
     TempBuf * custom_bmask;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * brush_mask;
  GBrushP brush;
  int sx, sy;
  int ex, ey;
  int bytes;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;
  brush = get_active_brush ();
  bytes = gdisp->gimage->bpp;

  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  x1 -= (brush->mask->width >> 1);
  y1 -= (brush->mask->height >> 1);
  x2 -= (brush->mask->width >> 1);
  y2 -= (brush->mask->height >> 1);

  sx = MINIMUM (x1, x2);
  sy = MINIMUM (y1, y2);
  ex = MAXIMUM (x1, x2);
  ey = MAXIMUM (y1, y2);

  /*  configure the canvas buffer  */
  canvas_buf = temp_buf_resize (canvas_buf, bytes, sx, sy,
				(ex - sx) + brush->mask->width, 
				(ey - sy) + brush->mask->height);

  /*  configure the brush mask buffer  */
  mask_buf = temp_buf_resize (mask_buf, brush->mask->bytes, sx, sy,
			      (ex - sx) + brush->mask->width, 
			      (ey - sy) + brush->mask->height);
  
  /*  determine which brush mask to use */
  if (custom_bmask)
    brush_mask = custom_bmask;
  else
    brush_mask = brush->mask;

  /*  calculate the brush mask buffer  */
  interpolate_brush_mask (mask_buf, brush_mask, x1 - sx, y1 - sy, x2 - sx, y2 - sy);

  /*  set the paint tool's brush mask  */
  paint_core->brush_mask = mask_buf;


  return canvas_buf;
}


TempBuf * 
paint_core_get_orig_image (tool, x1, y1, x2, y2)
     Tool * tool;
     int x1, y1;
     int x2, y2;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  int bx1, by1, bx2, by2;
  int bytes;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;
  bytes = gdisp->gimage->bpp;

  /*  configure the original image buffer  */
  if (paint_core->floating)
    {
      selection_find_bounds (gdisp->select, &bx1, &by1, &bx2, &by2);
      x1 -= bx1;
      y1 -= by1;
      x2 -= bx1;
      y2 -= by1;
    }
  orig_buf = temp_buf_resize (orig_buf, bytes, x1, y1, (x2 - x1), (y2 - y1)); 

  /*  load the orig temp buffer with the original contents of the gimage  */
  reconstruct_original_image (tool, gdisp->select->float_buf, orig_buf);
  
  return orig_buf;
}



void
paint_core_paste_canvas (tool, x, y, w, h)
     Tool * tool;
     int x, y;
     int w, h;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  GRegion * region;
  int bpp;
  int x1, y1, x2, y2;
  int nx1, ny1, nx2, ny2;
  int sm;
  Boolean no_selection = False;
  PixelRegion src1PR, src2PR, maskPR;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  bpp = gdisp->gimage->bpp;
  region = gdisp->select->region;
  paint_core = (PaintCore *) tool->private;

  /******************************************************************/
  /*  Blend the current image & the new image with the brush mask  */

  /*  configure the pixel regions  */
  /*  The canvas is source #1  */
  src1PR.bytes = canvas_buf->bytes;
  src1PR.w = w;
  src1PR.h = h;
  src1PR.rowstride = canvas_buf->width * canvas_buf->bytes;
  src1PR.data = temp_buf_data (canvas_buf);

  /*  The brush mask is the mask  */
  maskPR.bytes = paint_core->brush_mask->bytes;
  maskPR.w = paint_core->brush_mask->width;
  maskPR.h = paint_core->brush_mask->height;
  maskPR.rowstride = paint_core->brush_mask->width * paint_core->brush_mask->bytes;
  maskPR.data = temp_buf_data (paint_core->brush_mask);

  /*  If the gimage is of type INDEXED, then we need a 1 bit mask...  */
  if (gdisp->gimage->type == INDEXED_GIMAGE)
    B_W_region (&maskPR, &maskPR);
  
  /*  We need to decide where the other src is coming from...
   *  In the case of a floating selection, it's the "orig_buf".
   *  In the case of no floating selection, it's the gimage
   *
   *  We also need to determine the destination...
   *  If there is no selection at all, then the destination should be the gimage.
   *  If there is a selection, but not floating, the destination is the canvas
   *  If there is a floating selection, the destination is the float_buf
   */

  /*  Source #2 is the float_buf  */
  if (gdisp->select->float_buf)
    {
      TempBuf * float_buf = gdisp->select->float_buf;

      selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2);

      nx1 = bounds (x, x1, x2);
      ny1 = bounds (y, y1, y2);
      nx2 = bounds (x + w, x1, x2);
      ny2 = bounds (y + h, y1, y2);
  
      /******************************************************************************/
      /*  Before we paste the canvas to the float_buf, make sure to save the portions
       *  of the float buffer that are changing...
       */
      set_undo_blocks (gdisp->gimage, gdisp->select->float_buf, nx1 - x1, 
		       ny1 - y1, (nx2 - nx1), (ny2 - ny1));

      /*  offset the canvas and mask buffers valid addresses  */
      src1PR.w = (nx2 - nx1);
      src1PR.h = (ny2 - ny1);
      src1PR.data += (ny1 - canvas_buf->y) * src1PR.rowstride + 
	(nx1 - canvas_buf->x) * src1PR.bytes;

      maskPR.data += (ny1 - canvas_buf->y) * maskPR.rowstride + 
	(nx1 - canvas_buf->x) * paint_core->brush_mask->bytes;

      src2PR.rowstride = float_buf->width * float_buf->bytes;
      src2PR.data = temp_buf_data (float_buf) + 
	float_buf->bytes * ((ny1 - y1) * float_buf->width + (nx1 - x1));

      /*  Now do the composite  --  the float_buf is the destination as well as source #2  */
      composite_region (&src1PR, &src2PR, &src2PR, &maskPR);

      /*  blend the floating buffer with the gimage */
      selection_stencil (gdisp->select, gdisp->gimage, nx1 - x1, ny1 - y1,
			 nx1 + src1PR.w, ny1 + src1PR.h, BLEND_STENCIL);
      
      x1 = nx1;      y1 = ny1;
      x2 = nx2;      y2 = ny2;
    }

  /*  If there is a selection, but not floating  */
  else
    {
      /*  Find out if there is an active selected region, and what it is  */
      no_selection = ! (gdisplay_find_bounds (gdisp, &x1, &y1, &x2, &y2));

      if (no_selection)
	{
	  x1 = y1 = 0;
	  x2 = gdisp->gimage->width;
	  y2 = gdisp->gimage->height;
	}
      
      x1 = bounds (x, x1, x2);
      y1 = bounds (y, y1, y2);
      x2 = bounds (x + w, x1, x2);
      y2 = bounds (y + h, y1, y2);
       
      /***************************************************************************/
      /*  Before we paste the canvas to the gimage, make sure to save the portions
       *  of the gimage that are changing...
       */
      set_undo_blocks (gdisp->gimage, gdisp->select->float_buf, x1, y1, (x2 - x1), (y2 - y1));

      /*  offset the canvas and mask buffers valid addresses  */
      src1PR.w = (x2 - x1);
      src1PR.h = (y2 - y1);
      src1PR.data += (y1 - canvas_buf->y) * src1PR.rowstride + 
	(x1 - canvas_buf->x) * src1PR.bytes;

      maskPR.data += (y1 - canvas_buf->y) * maskPR.rowstride + 
	(x1 - canvas_buf->x) * paint_core->brush_mask->bytes;

      if ( ! no_selection )
	{
	  Selection * select;
	  
	  select = gdisp->select;
	  
	  /*  Source #2 is the gimage  */
	  src2PR.w = gdisp->gimage->width;
	  src2PR.h = gdisp->gimage->height;
	  src2PR.rowstride = gdisp->gimage->width * bpp;
	  src2PR.data = gdisp->gimage->raw_image + y1 * src2PR.rowstride + x1 * bpp;
	  
	  /*  Now do the composite  --  the canvas is the destination */
	  composite_region (&src1PR, &src2PR, &src1PR, &maskPR);

	  src2PR.data = gdisp->gimage->raw_image;
	  /*  Paste the canvas buf to the gimage based on the current selection...  */
	  gregion_stencil (select->region, select->offset_x, select->offset_y, 1.0,
			   &src1PR, NULL, &src2PR, NULL, 
			   x1 - select->offset_x, y1 - select->offset_y,
			   x2 - select->offset_x, y2 - select->offset_y,
			   COPY_STENCIL, 0);
	}
      
      /*  If there is no selection...  */
      else
	{
	  /*  Source #2 is the gimage  */
	  src2PR.rowstride = gdisp->gimage->width * bpp;
	  src2PR.data = gdisp->gimage->raw_image + y1 * src2PR.rowstride + x1 * bpp;
	  
	  /*  Now do the composite  --  the gimage is the destination */
	  composite_region (&src1PR, &src2PR, &src2PR, &maskPR);
	}
    }
  

  /*  Update undo extents  */
  if (x1 < paint_core->x1)
    paint_core->x1 = x1;
  if (y1 < paint_core->y1)
    paint_core->y1 = y1;
  if (x2 > paint_core->x2)
    paint_core->x2 = x2;
  if (y2 > paint_core->y2)
    paint_core->y2 = y2;

  /*  Update the gimage  */
  /*  Account for the fact that selection extends one pixel beyond bounds  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1) + sm, (y2 - y1) + sm, 0);
  
}


/************************************************************
 *             LOCAL FUNCTION DEFINITIONS                   *
 ************************************************************/


static void 
reconstruct_original_image (tool, float_buf_ptr, buf)
     Tool * tool;
     void * float_buf_ptr;
     TempBuf * buf;
{
  PaintCore * paint_core;
  GDisplay * gdisp;
  TempBuf * block;
  TempBuf * float_buf;
  int index;
  int x, y;
  int endx, endy;
  int row, col;
  int offx, offy;
  int x2, y2;
  int width, height;
  PixelRegion srcPR, destPR;

  paint_core = (PaintCore *) tool->private;

  if (paint_core->floating)
    {
      float_buf = (TempBuf *) float_buf_ptr;
      width = float_buf->width;
      height = float_buf->height;
      /*  load the buffer from the floating buffer  */
      temp_buf_copy_area (float_buf, buf, buf->x, buf->y, buf->width, buf->height);
    }
  else
    {
      gdisp = (GDisplay *) tool->gdisp_ptr;
      /*  load the buffer from the current gimage  */
      temp_buf_load (buf, gdisp->gimage, buf->x, buf->y, buf->width, buf->height);
      width = gdisp->gimage->width;
      height = gdisp->gimage->height;
    }

  /*  init some variables  */
  srcPR.bytes = buf->bytes;
  destPR.rowstride = buf->bytes * buf->width;

  y = bounds (buf->y, 0, height);
  endx = bounds (buf->x + buf->width, buf->x, width);
  endy = bounds (buf->y + buf->height, buf->y, height);

  row = (y / block_height);
  /*  patch the buffer with the saved portions of the image  */
  while (y < endy)
    {
      x = bounds (buf->x, 0, width);
      col = (x / block_width);

      /*  calculate y offset into this row of blocks  */
      offy = (y - row * block_height);
      y2 = (row + 1) * block_height;
      if (y2 > endy) y2 = endy;
      srcPR.h = y2 - y;

      while (x < endx)
      {
	index = row * horz_blocks + col;
	block = undo_blocks [index];
	/*  If the block exists, patch it into buf  */
	if (block)
	  {
	    /* calculate x offset into the block  */
	    offx = (x - col * block_width);
	    x2 = (col + 1) * block_width;
	    if (x2 > endx) x2 = endx;
	    srcPR.w = x2 - x;
	    
	    /*  Calculate the offsets into each buffer  */
	    srcPR.rowstride = srcPR.bytes * block->width;
	    srcPR.data = temp_buf_data (block) + offy * srcPR.rowstride + offx * srcPR.bytes;
	    destPR.data = temp_buf_data (buf) + 
	      (y - buf->y) * destPR.rowstride + (x - buf->x) * buf->bytes;

	    copy_region (&srcPR, &destPR);
	  }

	col ++;
	x = col * block_width;
      }

      row ++;
      y = row * block_height;
    }
}


static void
interpolate_brush_mask (buf, mask, x1, y1, x2, y2)
     TempBuf * buf;
     TempBuf * mask;
     int x1, y1;
     int x2, y2;
{
  PixelRegion maskPR, srcPR;
  int dx, dy;
  int incx;
  int error;
  int rowstride;
  int count;
  int size;

  /*  first, set the buffer to black  */
  size = buf->width * buf->height * buf->bytes;
  memset (temp_buf_data (buf), 0, size);

  /*  setup the pixel region structures  */
  maskPR.bytes = mask->bytes;
  maskPR.w = mask->width;
  maskPR.h = mask->height;
  maskPR.rowstride = maskPR.w * maskPR.bytes;
  maskPR.data = temp_buf_data (mask);

  srcPR.bytes = buf->bytes;
  srcPR.w = mask->width;
  srcPR.h = mask->height;
  srcPR.rowstride = buf->width * srcPR.bytes;
  srcPR.data = temp_buf_data (buf);

  dx = x2 - x1;
  dy = y2 - y1;

  /*  If there has been no length to this stroke, just put the mask
   *  down once and return...otherwise, interpolate the mask buffer
   */
  if (!dx && !dy)
    add_region (&srcPR, &maskPR, &srcPR);
  else 
    {
      if (dy < 0)
	{
	  dy = -dy;
	  rowstride = -srcPR.rowstride;
	}
      else
	rowstride = srcPR.rowstride;
      
      if (dx < 0)
	{
	  dx = -dx;
	  incx = -1;
	}
      else
	incx = 1;

      if (dx > dy)
	{
	  count = dx;
	  error = -dx / 2;
	  
	  srcPR.data += y1 * srcPR.rowstride + x1;
	  
	  while (count--)
	    {
	      add_region (&srcPR, &maskPR, &srcPR);
	      srcPR.data += incx;
	      
	      error += dy;
	      if (error > 0)
		{
		  srcPR.data += rowstride;
		  error -= dx;
		}
	    }
	}
      else
	{
	  count = dy;
	  error = -dy / 2;
	  
	  srcPR.data += y1 * buf->width + x1;
	  
	  while (count--)
	    {
	      add_region (&srcPR, &maskPR, &srcPR);
	      srcPR.data += rowstride;
	      
	      error += dx;
	      if (error > 0)
		{
		  srcPR.data += incx;
		  error -= dy;
		}
	    }
	}
    }
}


static void
set_undo_blocks (gimage_ptr, float_buf_ptr, x, y, w, h)
     void * gimage_ptr;
     void * float_buf_ptr;
     int x, y;
     int w, h;
{
  GImage * gimage;
  TempBuf * float_buf;
  int endx, endy;
  int startx;
  int index;
  int x1, y1;
  int x2, y2;
  int row, col;
  int width, height;

  /*  We need to initialize variables based on whether this is a floating selection  */
  if (float_buf_ptr)
    {
      float_buf = (TempBuf *) float_buf_ptr;
      width = float_buf->width;
      height = float_buf->height;
    }
  else
    {
      gimage = (GImage *) gimage_ptr;
      width = gimage->width;
      height = gimage->height;
    }

  startx = x;
  endx = x + w;
  endy = y + h;

  row = y / block_width;
  while (y < endy)
    {
      col = x / block_width;
      while (x < endx)
	{
	  index = row * horz_blocks + col;
	  
	  /*  If the block doesn't exist, create and initialize it  */
	  if (! undo_blocks [index])
	    {
	      /*  determine memory efficient width and height of block  */
	      x1 = col * block_width;
	      x2 = bounds (x1 + block_width, 0, width);
	      w = (x2 - x1);
	      y1 = row * block_height;
	      y2 = bounds (y1 + block_height, 0, height);
	      h = (y2 - y1);

	      if (float_buf_ptr)
		/*  Get the specified portion of the float buf  */
		undo_blocks [index] = temp_buf_copy_area (float_buf, NULL, x1, y1, w, h);
	      else
		/*  get the specified portion of the gimage  */
		undo_blocks [index] = temp_buf_load (NULL, gimage, x1, y1, w, h);
	    }
	  col++;
	  x = col * block_width;
	}

      row ++;
      y = row * block_height;
      x = startx;
    }
}


static void
allocate_undo_blocks (brush_width, brush_height, image_width, image_height)
     int brush_width, brush_height;
     int image_width, image_height;
{
  int num_blocks;
  int i;

  /*  calculate the width and height of each undo block  */
  block_width = (brush_width*2 < MIN_BLOCK_WIDTH) ? MIN_BLOCK_WIDTH : brush_width*2;
  block_height = (brush_height*2 < MIN_BLOCK_HEIGHT) ? MIN_BLOCK_HEIGHT : brush_height*2;

  /*  calculate the number of rows and cols in the undo block grid  */
  horz_blocks = (image_width + block_width - 1) / block_width;
  vert_blocks = (image_height + block_height - 1) / block_height;

  /*  Allocate the array  */
  num_blocks = horz_blocks * vert_blocks;
  undo_blocks = (TempBuf **) xmalloc (sizeof (TempBuf *) * num_blocks);

  /*  Initialize the array  */
  for (i = 0; i < num_blocks; i++)
    undo_blocks [i] = NULL;
}


static void
free_undo_blocks ()
{
  int i;
  int num_blocks;

  num_blocks = vert_blocks * horz_blocks;

  for (i = 0; i < num_blocks; i++)
    if (undo_blocks [i])
      temp_buf_free (undo_blocks [i]);

  xfree (undo_blocks);

  undo_blocks = NULL;
}

/*****************************************************/
/*  Paint buffers utility functions                  */
/*****************************************************/


static void
free_paint_buffers ()
{
  if (orig_buf)
    temp_buf_free (orig_buf);

  orig_buf = NULL;

  if (canvas_buf)
    temp_buf_free (canvas_buf);

  canvas_buf = NULL;

  if (mask_buf)
    temp_buf_free (mask_buf);
  
  mask_buf = NULL;
}
