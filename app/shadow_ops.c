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
#include "paint_funcs.h"
#include "selection.h"
#include "shadow_ops.h"
#include "undo.h"


void
gimage_merge_shadow (gimage, select_ptr)
     GImage * gimage;
     void * select_ptr;
{
  Selection * select;
  int bytes;
  int x1, y1, x2, y2;
  Boolean area;
  PixelRegion srcPR, destPR;

  select = (Selection *) select_ptr;
  area = gregion_find_bounds (select->region, &x1, &y1, &x2, &y2);
  bytes = gimage->bpp;

  /*  take care of the case where there is no selection  */
  if (! area)
    {
      /*  put the untouched image onto the undo stack  */
      undo_push_image (gimage->ID, 0, 0, gimage->width, gimage->height);
      /*  now, make the changes  */
      memcpy (gimage->raw_image, gimage->shadow_buf, 
	      gimage->width * gimage->height * bytes);
    }
  /*  Now the case where we must only affect what's inside the selected region  */
  else
    {
      /*  We need to go through the process of finding the correct area
	  to slap onto the undo stack...  */
      if (!select->float_buf)
	{
	  /*  put the untouched image onto the undo stack  */
	  undo_push_image (gimage->ID, x1, y1, x2, y2);
	  
	  /*  now, make the proposed changes only the selected region  */
	  srcPR.bytes = bytes;
	  srcPR.w = (x2 - x1);
	  srcPR.h = (y2 - y1);
	  srcPR.rowstride = gimage->width * bytes;
	  srcPR.data = gimage->shadow_buf + y1 * srcPR.rowstride + x1 * srcPR.bytes;

	  destPR.w = gimage->width;
	  destPR.h = gimage->height;
	  destPR.rowstride = srcPR.rowstride;
	  destPR.data = gimage->raw_image;

	  gregion_stencil (select->region, select->offset_x, select->offset_y, 1.0,
			   &srcPR, &destPR, &destPR, NULL, x1, y1, x2, y2, BLEND_STENCIL, 0);
	}
      else
	{
	  TempBuf * new_buf;

	  /*  find the selection's boundaries  */
	  selection_find_bounds (select, &x1, &y1, &x2, &y2);

	  /*  Save the current buffer's state to push on the undo stack  */
	  new_buf = temp_buf_copy_area (select->float_buf, NULL, 0, 0, 
					select->float_buf->width, select->float_buf->height);

	  /*  push the new buf to the undo stack  */
	  undo_push_mod_sel (((GDisplay *) select->gdisp)->ID, new_buf);

	  /*  get the new floating buffer from the shadow gimage  */
	  {
	    int tx1, ty1, tx2, ty2;
	    int w, h;

	    gdisplay_find_bounds ((GDisplay *) select->gdisp, &tx1, &ty1, &tx2, &ty2);

	    w  = (tx2 - tx1);
	    h  = (ty2 - ty1);

	    /*  ignore degenerate case  */
	    if (w && h)
	      {
		srcPR.bytes = gimage->bpp;
		srcPR.w = w;
		srcPR.h = h;
		srcPR.rowstride = gimage->width * gimage->bpp;
		srcPR.data = gimage->shadow_buf + srcPR.rowstride * ty1 + srcPR.bytes * tx1;
	    
		destPR.rowstride = select->float_buf->width * select->float_buf->bytes;
		destPR.data = temp_buf_data (select->float_buf) + 
		  srcPR.bytes * ((ty1 - y1) * select->float_buf->width + (tx1 - x1));
	    
		copy_region (&srcPR, &destPR);
	      }
	  }

	  /*  First, restore the original gimage  */
	  temp_buf_paste (select->orig_buf, gimage, x1, y1, (x2 - x1), (y2 - y1));

	  /*  blend the floating buffer with the gimage */
	  selection_stencil (select, gimage, 0, 0, select->float_buf->width,
			     select->float_buf->height, BLEND_STENCIL);
	}
    }
  
}


void
gimage_put_shadow (gimage, x, y, w, h)
     GImage * gimage;
     int x, y;
     int w, h;
{
  unsigned char * shadow, * raw;
  long width, next_line;
  int i, bytes;
  int x2, y2;

  shadow = gimage_get_shadow_addr (gimage);
  raw = gimage->raw_image;
  
  x2 = x + w;  y2 = y + h;
  /*  some bounds checking  */
  x  = bounds (x, 0, gimage->width);
  y  = bounds (y, 0, gimage->height);
  x2 = bounds (x2, 0, gimage->width);
  y2 = bounds (y2, 0, gimage->height);

  w = (x2 - x);
  h = (y2 - y);

  bytes = gimage->bpp;
  shadow += bytes * (gimage->width * y + x);
  raw += bytes * (gimage->width * y + x);
  next_line = gimage->width * bytes;
  width = w * bytes;

  i = h;
  while (i--)
    {
      memcpy (raw, shadow, width);
      raw += next_line;
      shadow += next_line;
    }

}


void
gimage_cast_shadow  (gimage)
     GImage * gimage;
{
  long size;
  unsigned char * shadow;

  size = gimage->width * gimage->height * gimage->bpp;
  shadow = gimage_get_shadow_addr (gimage);
  memcpy (shadow, gimage->raw_image, size);
}


