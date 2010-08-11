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
#include "flip_tool.h"
#include "gdisplay.h"
#include "selection.h"
#include "transform_core.h"

/*  forward function declarations  */
static Selection *  flip_tool_flip_horz (Tool *, void *);
static Selection *  flip_tool_flip_vert (Tool *, void *);


void *
flip_tool_transform (tool, gdisp_ptr, state)
     Tool * tool;
     XtPointer gdisp_ptr;
     int state;
{
  TransformCore * transform_core;

  transform_core = (TransformCore *) tool->private;

  switch (state)
    {
    case INIT :
      transform_info = NULL;
      break;

    case MOTION :
      break;

    case RECALC :
      break;

    case FINISH :
      switch (tool->type)
	{
	case FLIP_HTOOL :
	  return (flip_tool_flip_horz (tool, gdisp_ptr));
	  break;
	case FLIP_VTOOL :
	  return (flip_tool_flip_vert (tool, gdisp_ptr));
	  break;
	}
      break;
    }

  return NULL;
}

      

Tool *
tools_new_flip_horz ()
{
  Tool * tool;
  TransformCore * private;

  tool = transform_core_new (FLIP_HTOOL, NON_INTERACTIVE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = flip_tool_transform;
  
  return tool;
}


Tool *
tools_new_flip_vert ()
{
  Tool * tool;
  TransformCore * private;

  tool = transform_core_new (FLIP_VTOOL, NON_INTERACTIVE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = flip_tool_transform;

  return tool;
}


void
tools_free_flip_tool (tool)
     Tool * tool;
{
  transform_core_free (tool);
}


static Selection *
flip_tool_flip_horz (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  Selection * new;
  GDisplay * gdisp;
  GRegion * region;
  MaskBuf * mask;
  unsigned char * msk, * m;
  unsigned char * src, * s;
  unsigned char * dest, * d;
  long srcwidth, destwidth;
  int src_col;
  int bytes, b;
  int width, height;
  int i, j;
  link_ptr list;
  GSegment * seg;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) gdisp_ptr;
  region = gdisp->select->region;

  /*  Find the selected region's bounds  */
  if (!selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2))
    return NULL;

  width = x2 - x1;
  height = y2 - y1;

  /*  Create the new selection object  */
  new = selection_generic_new (gregion_new (height, width),
			       x1, y1,
			       temp_buf_new (width, height,
					     gdisp->gimage->bpp,
					     0, 0, NULL));
  
  /*  Create a mask buffer for transforming the selected region  */
  mask = mask_buf_new (width, height);

  /*  Some calculations...  */
  bytes = gdisp->gimage->bpp;
  srcwidth = bytes * gdisp->select->float_buf->width;
  destwidth = bytes * width;

  /*  the data pointers...  */
  src  = temp_buf_data (gdisp->select->float_buf);
  dest = temp_buf_data (new->float_buf);
  msk  = mask_buf_data (mask);

  /*  set the dest to the last row of the destination edit buffer  */
  dest += destwidth - 1;
  msk  += width - 1;

  /*  vertically flip the selected region  */

  for (i = y1; i < y2; i++)
    {
      list = region->segments[i - gdisp->select->offset_y];
      if (list)
	seg = (GSegment *) list->data;
      else
	seg = NULL;

      m = msk;
      d = dest;
      s = src;

      src_col = x1 - gdisp->select->offset_x;

      j = width;
      while (j--)
	{
	  b = bytes;
	  while (b--)
	    *d-- = s[b];

	  s += bytes;

	  *m-- = (seg && src_col >= seg->start && src_col < seg->end) ? seg->value : 0;
	  src_col++;

	  if (seg && src_col >= seg->end) 
	    {
	      if ((list = next_item (list)))
		seg = (GSegment *) list->data;
	      else
		seg = NULL;
	    }

	}

      src += srcwidth;
      msk += width;
      dest += destwidth;
    }

  mask_convert_to_region (mask, new->region);

  mask_buf_free (mask);

  return new;
}


static Selection *
flip_tool_flip_vert (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  Selection * new;
  GDisplay * gdisp;
  GRegion * region;
  MaskBuf * mask;
  unsigned char * msk, * m;
  unsigned char * src;
  unsigned char * dest;
  long srcwidth, destwidth;
  int src_col;
  int bytes;
  int width, height;
  int i, j;
  link_ptr list;
  GSegment * seg;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) gdisp_ptr;
  region = gdisp->select->region;

  /*  Find the selected region's bounds  */
  if (!selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2))
    return NULL;

  width = x2 - x1;
  height = y2 - y1;

  /*  Create the new selection object  */
  new = selection_generic_new (gregion_new (height, width),
			       x1, y1,
			       temp_buf_new (width, height,
					     gdisp->gimage->bpp,
					     0, 0, NULL));

  /*  Create a mask buffer for transforming the selected region  */
  mask = mask_buf_new (width, height);

  /*  Some calculations...  */
  bytes = gdisp->gimage->bpp;
  srcwidth = bytes * gdisp->select->float_buf->width;
  destwidth = bytes * width;

  /*  the data pointers...  */
  src  = temp_buf_data (gdisp->select->float_buf);
  dest = temp_buf_data (new->float_buf);
  msk  = mask_buf_data (mask);

  /*  set the dest to the last row of the destination edit buffer  */
  dest += (height - 1) * destwidth;
  msk  += (height - 1) * width;

  /*  vertically flip the selected region  */

  for (i = y1; i < y2; i++)
    {
      memcpy (dest, src, destwidth);

      list = region->segments[i - gdisp->select->offset_y];
      if (list)
	seg = (GSegment *) list->data;
      else
	seg = NULL;

      m = msk;

      src_col = x1 - gdisp->select->offset_x;

      j = width;
      while (j--)
	{
	  *m++ = (seg && src_col >= seg->start && src_col < seg->end) ? seg->value : 0;
	  src_col++;

	  if (seg && src_col >= seg->end) 
	    {
	      if ((list = next_item (list)))
		seg = (GSegment *) list->data;
	      else
		seg = NULL;
	    }

	}

      src += srcwidth;
      msk -= width;
      dest -= destwidth;
    }

  mask_convert_to_region (mask, new->region);

  mask_buf_free (mask);

  return new;
}













