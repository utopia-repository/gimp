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
#include <values.h>
#include <math.h>
#include "appenv.h"
#include "errors.h"
#include "gdisplay.h"
#include "gregion.h"
#include "linked.h"
#include "memutils.h"
#include "paint_funcs.h"
#include "temp_buf.h"

#define MINIMUM(x,y) ((x < y) ? x : y)
#define MAXIMUM(x,y) ((x > y) ? x : y)
#define ROUND(x) ((int) (x + 0.5))

static void* segment_alloc (void);
static void  segment_free  (void *);

/* id's for gregions... */
static int gregion_id = 0;

static MemChunk segment_mem_chunk = NULL;

static void*
segment_alloc ()
{
  if (!segment_mem_chunk)
    segment_mem_chunk = mem_chunk_create (sizeof (GSegment), ALLOC_AND_FREE);
  
  return mem_chunk_alloc (segment_mem_chunk);
}

static void
segment_free (mem)
     void *mem;
{
  mem_chunk_free (segment_mem_chunk, mem);
}

static GSegment *
gsegment_new (start, end, value)
     int start, end;
     unsigned char value;
{
  GSegment *seg;

  seg = segment_alloc ();

  seg->start = start;
  seg->end = end;
  seg->value = value;

  return seg;
}


static link_ptr
remove_obscured_segments (list, x, width)
     link_ptr list;
     int x;
     int width;
{
  link_ptr lptr, first_half, end_half, last;
  GSegment * seg;

  lptr = list;
  first_half = NULL;
  end_half = NULL;

  while (lptr)
    {
      seg = (GSegment *) lptr->data;
      /* if segment is obscured, ignore and free segment  */
      if (seg->start >= x && seg->end <= (x + width))
	{
	  last = lptr;
	  segment_free (seg);
	  lptr = next_item (lptr);
	  last->next = NULL;
	  free_list (last);
	}
      else if (seg->start < x)
	{
	  first_half = lptr;
	  lptr = next_item (lptr);
	}
      else
	{
	  end_half = lptr;
	  lptr = NULL;
	}
    }

  if (!first_half)
    return end_half;
  else
    {
      first_half->next = end_half;
      return list;
    }
}


GRegion *
gregion_new (size, width)
     int size;
     int width;
{
  int i;
  GRegion *new;

  new = (GRegion *) xmalloc (sizeof (GRegion));
  new->extent = size;
  new->width = width;
  new->segments = (link_ptr *) xmalloc (sizeof (link_ptr) * size);
  new->bounds_known = False;
  new->id = gregion_id++;

  for (i = 0; i < size; i++)
    new->segments[i] = NULL;

  return new;

}


GRegion *
gregion_copy (src, dest)
     GRegion * src;
     GRegion * dest;
{
  GRegion * new;
  link_ptr list;
  GSegment * seg;
  int i;

  /*  If the dest is NULL, create a new structure to return  */
  if (!dest)
    new = gregion_new (src->extent, src->width);
  /*  Otherwise, add the src region to the dest region  */
  else
    {
      new = dest;
      if (dest->extent != src->extent)
	warning ("Copying may fail due to unequal extents between src and dest.");
    }

  for (i = 0; i < src->extent; i++)
    {
      list = src->segments[i];
      while (list)
	{
	  seg = (GSegment *) list->data;
	  gregion_add_segment (new, seg->start, i, (seg->end - seg->start), seg->value);
	  list = next_item(list);
	}
    }

  return new;
}


void
gregion_free (region)
     GRegion * region;
{
  link_ptr list;
  GSegment * seg;
  int i;

  for (i = 0; i < region->extent; i++)
    {
      list = region->segments[i];
      while (list)
	{
	  seg = (GSegment *) list->data;
	  segment_free (seg);
	  list = next_item(list);
	}
      free_list (region->segments[i]);
    }
  xfree (region->segments);
  xfree (region);
}


Boolean
gregion_point_inside (region, x, y)
     GRegion * region;
     int x, y;
{
  link_ptr list;
  GSegment * seg;

  if (x < 0 || x >= region->width || y < 0 || y >= region->extent)
    return False;

  list = region->segments[y];
  while (list)
    {
      seg = (GSegment *) list->data;
      if (x >= seg->start && x < seg->end)
	return True;
      list = next_item (list);
    }

  return False;
}


void
gregion_find_empty_segs (region, scanline, empty_segs, max_empty, num_empty)
     GRegion * region;
     int scanline;
     int empty_segs[];
     int max_empty;
     int *num_empty;
{
  GSegment *seg;
  link_ptr seg_list;
  int index1, index2;
  int s, e;
  

  *num_empty = 0;

  if (scanline < 0 || scanline >= region->extent)
    seg_list = NULL;
  else
    seg_list = region->segments[scanline];

  empty_segs[(*num_empty)++] = 0;

  while (seg_list)
    {
      if (*num_empty >= max_empty-1)
	{
	  warning ("Maximum segment limit reached in gregion_find_empty_segs");
	  return;
	}

      seg = (GSegment *) seg_list->data;
      if (seg->value < HALF_WAY)
	seg = NULL;

      if (seg)
	{
	  empty_segs[(*num_empty)++] = seg->start;
	  empty_segs[(*num_empty)++] = seg->end;
	}

      seg_list = next_item (seg_list);
    }

  empty_segs[(*num_empty)++] = MAXINT;

  /*  Now, go through the empty segment list and remove duplicated entries  */
  index1 = index2 = s = 0;
  e = 1;
  for (index1 = 0; index1 < *num_empty - 2; index1 += 2)
    {
      /*  if this segment is flush against the next, reduce  */
      if (empty_segs [index1 + 1] == empty_segs [index1 + 2])
	e += 2;
      /*  else, move to the next segment  */
      else
	{
	  empty_segs [index2] = empty_segs [s];
	  empty_segs [index2 + 1] = empty_segs [e];
	  index2 += 2;
	  s += 2;
	  e += 2;
	}
    }
}


Boolean
gregion_find_bounds (region, x1, y1, x2, y2)
     GRegion * region;
     int * x1, * y1;
     int * x2, * y2;
{
  GSegment * seg;
  link_ptr list;
  int i;

  *x1 = region->width;
  *y1 = 0;
  *x2 = 0;
  *y2 = region->extent;

  while ((*y1 < region->extent) && (!region->segments[*y1]))
    (*y1)++;

  while ((*y2) && (!region->segments[(*y2) - 1]))
    (*y2)--;

  /*  if there are no segments, make sure that the  */
  /*  bounding box encompasses the entire image.    */
  if (*y1 == region->extent)
    {
      *x1 = *y1 = *x2 = *y2 = 0;
      return False;
    }

  /*  if the region's bounds have already been reliably calculated...don't bother */
  if (region->bounds_known)
    {
      *x1 = region->x1;
      *y1 = region->y1;
      *x2 = region->x2;
      *y2 = region->y2;
      return True;
    }

  for (i = *y1; i < *y2; i++)
    {
      list = region->segments[i];
      
      if (list)
	{
	  seg = (GSegment *) list->data;
	  if (seg->start < *x1)
	    *x1 = seg->start;
	  list = next_item (list);

	  while (list)
	    {
	      seg = (GSegment *) list->data;
	      list = next_item(list);
	    }
	  if (seg->end > *x2)
	    *x2 = seg->end;
	}
    }

  region->x1 = *x1;
  region->y1 = *y1;
  region->x2 = *x2;
  region->y2 = *y2;
  region->bounds_known = True;

  return True;
}


int
gregion_num_segments (region)
     GRegion * region;
{
  link_ptr list;
  int i;
  int num_segs = 0;

  for (i = 0; i < region->extent; i++)
    {
      list = region->segments[i];
      while (list)
	{
	  num_segs ++;
	  list = next_item(list);
	}
    }

  return num_segs;
}


void
gregion_add_segment (region, x, y, width, value)
     GRegion * region;
     int x, y;
     int width;
     int value;
{
  GSegment *cur, *next;
  link_ptr rest;
  link_ptr list;
  int x2 = x + width;

  /*  check horizontal extents...  */
  if (x2 < 0) x2 = 0;
  if (x2 > region->width) x2 = region->width;
  if (x < 0) x = 0;
  if (x > region->width) x = region->width;
  width = x2 - x;
  if (!width) return;

  region->bounds_known = False;

  region->segments[y] = remove_obscured_segments (region->segments[y], x, width);

  list = region->segments[y];

  while (list)
    {
      cur = (GSegment *) list->data;
      /* here are the cases under which a new segment may fall */
      /* 1)  It occurs before the current segment              */
      if (x + width <= cur->start)
	{
	  /* do some shuffling */
	  list->data = gsegment_new (x, x + width, value);
	  rest = next_item (list);
	  rest = add_to_list (rest, cur);
	  list->next = rest;
	  return;
	}
      /* 2)  segment intersects another segment                */
      else if (x < cur->end)
	{
	  cur->start = MINIMUM (x, cur->start);
	  cur->end = MAXIMUM ((x + width), cur->end);
	  rest = next_item (list);
	  if (rest)
	    {
	      next = (GSegment *) rest->data;
	      /*  if this segment extends into the next as well...  */
	      if (cur->end >= next->start)
		{
		  cur->end = next->end;
		  list->next = rest->next;
		  
		  segment_free (next);
		  rest->next = NULL;
		  free_list (rest);
		}
	    }
	  return;
	}
      list = next_item (list);
    }
  /* 3)  Segment occurs separate and after everything else  */
  region->segments[y] =
    append_to_list (region->segments[y], gsegment_new (x, x + width, value));
}


void
gregion_sub_segment (region, x, y, width, value)
     GRegion * region;
     int x, y;
     int width;
     int value;
{
  GSegment *cur;
  link_ptr rest;
  link_ptr list;
  int x2 = x + width;

  /*  check horizontal extents...  */
  if (x2 < 0) x2 = 0;
  if (x2 > region->width) x2 = region->width;
  if (x < 0) x = 0;
  if (x > region->width) x = region->width;
  width = x2 - x;
  if (!width) return;

  region->bounds_known = False;

  region->segments[y] = remove_obscured_segments (region->segments[y], x, width);

  list = region->segments[y];

  while (list)
    {
      cur = (GSegment *) list->data;
      /* here are the cases under which a new segment may fall */
      /* 1)  It occurs before the current segment              */
      if (x + width < cur->start)
	{
	  /*  Just ignore it */
	  return;
	}
      /* 2)  segment intersects another segment                */
      else if (x < cur->end)
	{
	  /*  takes a bite off of the left  */
	  if (x <= cur->start && (x + width) < cur->end)
	      cur->start = (x + width);

	  /*  takes a bite off of the right */
	  else if (x > cur->start && (x + width) >= cur->end)
	    {
	      x2 = cur->end;
	      cur->end = x;
	      /*  repass the reduced segment into this function...  */
	      gregion_sub_segment (region, x2, y, (x + width - x2), 255);
	    }

	  /*  takes a bite out of the middle */
	  else
	    {
	      /* create a new segment which is the right half  */
	      rest = next_item (list);
	      rest = add_to_list (rest, gsegment_new ((x+width), cur->end, cur->value));
	      list->next = rest;

	      /* cut the current segment down to the size of the left half */
	      cur->end = x;
	    }

	  return;
	}

      list = next_item (list);
    }
  /* 3)  Segment occurs separate and after everything else  */
  /* Simply ignore the segment--it has no bearing  */
}


void
gregion_combine_rect (region, op, x, y, w, h)
     GRegion * region;
     int op;
     int x, y;
     int w, h;
{
  int i;

  for (i = y; i < y + h; i++)
    {
      if (i >= 0 && i < region->extent)
	switch (op)
	  {
	  case ADD :
	    gregion_add_segment (region, x, i, w, 255);
	    break;
	  case SUB :
	    gregion_sub_segment (region, x, i, w, 255);
	    break;
	  }
    }
}


void
gregion_combine_ellipse (region, op, x, y, w, h, aa)
     GRegion * region;
     int op;
     int x, y;
     int w, h;
     int aa;    /*  antialias selection?  */
{
  int i, j;
  int x0, x1, x2;
  int val, last;
  float a_sqr, b_sqr, aob_sqr;
  float w_sqr, h_sqr;
  float y_sqr;
  float t0, t1;
  float r;
  float cx, cy;
  float rad;
  float dist;
  

  if (!w || !h)
    return;

  a_sqr = (w * w / 4.0);
  b_sqr = (h * h / 4.0);
  aob_sqr = a_sqr / b_sqr;

  cx = x + w / 2.0;
  cy = y + h / 2.0;

  for (i = y; i < (y + h); i++)
    {
      if (i >= 0 && i < region->extent)
	{
	  /*  Non-antialiased code  */
	  if (!aa)
	    {
	      y_sqr = (i + 0.5 - cy) * (i + 0.5 - cy);
	      rad = sqrt (a_sqr - a_sqr * y_sqr / (double) b_sqr);
	      x1 = ROUND (cx - rad);
	      x2 = ROUND (cx + rad);
	  
	      switch (op)
		{
		case ADD :
		  gregion_add_segment (region, x1, i, (x2 - x1), 255);
		  break;
		case SUB :
		  gregion_sub_segment (region, x1, i, (x2 - x1), 255);
		  break;
		}
	    }
	  /*  antialiasing  */
	  else
	    {
	      x0 = x;
	      last = 0;
	      h_sqr = (i + 0.5 - cy) * (i + 0.5 - cy);
	      for (j = x; j < (x + w); j++)
		{
		  w_sqr = (j + 0.5 - cx) * (j + 0.5 - cx);

		  if (h_sqr != 0)
		    {
		      t0 = w_sqr / h_sqr;
		      t1 = a_sqr / (t0 + aob_sqr);
		      r = sqrt (t1 + t0 * t1);
		      rad = sqrt (w_sqr + h_sqr);
		      dist = rad - r;
		    }
		  else 
		    dist = -1.0;

		  if (dist < -0.5)
		    val = 255;
		  else if (dist < 0.5)
		    val = (int) (255 * (1 - (dist + 0.5)));
		  else
		    val = 0;

		  if (last != val && last)
		    {
		      switch (op)
			{
			case ADD :
			  gregion_add_segment (region, x0, i, j - x0, last);
			  break;
			case SUB :
			  gregion_sub_segment (region, x0, i, j - x0, last);
			  break;
			}
		    }

		  if (last != val)
		    {
		      x0 = j;
		      last = val;
		    }
		}

	      if (last)
		{
		  if (op == ADD)
		    gregion_add_segment (region, x0, i, j - x0, last);
		  else if (op == SUB)
		    gregion_sub_segment (region, x0, i, j - x0, last);
		}
	    }

	}
    }      
}


void
gregion_combine_region (region, op, add_on)
     GRegion * region;
     int op;
     GRegion * add_on;
{
  GSegment *seg;
  link_ptr list;
  int i;

  for (i = 0; i < add_on->extent; i++)
    {
      list = add_on->segments[i];
      while (list)
	{
	  seg = (GSegment *) list->data;

	  switch (op)
	    {
	    case ADD :
	      gregion_add_segment (region, seg->start, i, (seg->end - seg->start), seg->value);
	      break;
	    case SUB :
	      gregion_sub_segment (region, seg->start, i, (seg->end - seg->start), 255);
	      break;
	    }

	  list = next_item (list);
	}
    }

}


void
gregion_combine_offset_region (region, op, add_on, off_x, off_y)
     GRegion * region;
     int op;
     GRegion * add_on;
     int off_x, off_y;
{
  GSegment *seg;
  link_ptr list;
  int i;

  for (i = 0; i < add_on->extent; i++)
    {
      list = add_on->segments[i];
      while (list)
	{
	  seg = (GSegment *) list->data;

	  switch (op)
	    {
	    case ADD :
	      gregion_add_segment (region, seg->start + off_x, i + off_y, (seg->end - seg->start), seg->value);
	      break;
	    case SUB :
	      gregion_sub_segment (region, seg->start + off_x, i + off_y, (seg->end - seg->start), 255);
	      break;
	    }

	  list = next_item (list);
	}
    }

}


void
gregion_invert (region)
     GRegion * region;
{
  GSegment *seg;
  link_ptr list, newlist;
  int i, start;

  region->bounds_known = False;

  for (i = 0; i < region->extent; i++)
    {
      list = region->segments[i];
      newlist = NULL;
      start = 0;

      while (list)
	{
	  seg = (GSegment *) list->data;

	  /*  here's the case where we add a new segment  */
	  if (seg->start > start)
	    newlist = append_to_list (newlist, (void *) gsegment_new (start, seg->start, 255));

	  start = seg->end;

	  /*  here's the case where we take the inverse of an existing segment  */
	  if (seg->value != 255)
	    {
	      seg->value = 255 - seg->value;
	      newlist = append_to_list (newlist, (void *) seg);
	    }
	  else
	    segment_free (seg);

	  list = next_item (list);
	}

      if (start < region->width)
	newlist = append_to_list (newlist, (void *) gsegment_new (start, region->width, 255));

      free_list (region->segments[i]);
      region->segments[i] = newlist;
    }
  
}


GRegion *
gregion_sharpen (region)
     GRegion * region;
{
  GSegment * seg;
  GRegion * new_region;
  link_ptr list, newlist;
  int i, start, next_start;

  new_region = gregion_new (region->extent, region->width);

  for (i = 0; i < region->extent; i++)
    {
      list = region->segments[i];
      newlist = NULL;
      
      if (list)
	start = ((GSegment *) list->data)->start;

      while (list)
	{
	  seg = (GSegment *) list->data;

	  if (next_item (list))
	    next_start = ((GSegment *) (next_item (list))->data)->start;
	  else
	    next_start = MAXINT;

	  if (next_start != seg->end)
	    {
	      newlist = append_to_list (newlist, (void *) gsegment_new (start, seg->end, 255));
	      start = next_start;
	    }

	  list = next_item (list);
	}

      new_region->segments[i] = newlist;
    }

  return new_region;
}


void
gregion_stencil (region, off_x, off_y, opacity, src1_ptr, src2_ptr, dest_ptr, 
		 col, x1, y1, x2, y2, mode, preserve_colors)
     GRegion * region;
     int off_x, off_y;
     double opacity;
     void * src1_ptr;
     void * src2_ptr;
     void * dest_ptr;
     unsigned char * col;
     int x1, y1;
     int x2, y2;
     int mode;
     int preserve_colors;
{
  link_ptr seg_list;
  GSegment * seg;
  PixelRegion * src1PR, * src2PR, * destPR;
  int scanline;
  int bytes;
  int nx1, nx2, ny1, ny2;
  int x_s, x_e;
  int y;
  long length;
  unsigned char * src1, *src2;
  unsigned char * s1, *s2;
  unsigned char * dest, *d;
  unsigned char blend;

  src1PR = (PixelRegion *) src1_ptr;
  src2PR = (PixelRegion *) src2_ptr;
  destPR = (PixelRegion *) dest_ptr;

  /*  get the number of bytes  --  assume they're the same for src1, src2, & dest  */
  bytes = src1PR->bytes;
    
  /*  some bounds checking  */
  nx1 = bounds (x1 + off_x, 0, destPR->w);
  ny1 = bounds (y1 + off_y, 0, destPR->h);
  nx2 = bounds (x2 + off_x, 0, destPR->w);  
  ny2 = bounds (y2 + off_y, 0, destPR->h);  

  /*  The data pointers...  */
  dest = destPR->data + destPR->rowstride * ny1;

  src1 = src1PR->data + (ny1 - y1 - off_y) * src1PR->rowstride + (nx1 - x1 - off_x) * bytes;
  if (src2PR)
    src2 = src2PR->data + (ny1 - y1 - off_y) * src2PR->rowstride + (nx1 - x1 - off_x) * bytes;
  else src2 = NULL;

  /*  stencil the source(s) into the destination  */
  for (scanline = ny1 ; scanline < ny2; scanline++)
    {
      y = scanline - off_y;
      if (y >= 0 && y < region->extent)
	seg_list = region->segments[y];
      else
	seg_list = NULL;

      while (seg_list)
	{
	  seg = (GSegment *) seg_list->data;
	  
	  x_s = bounds ((seg->start + off_x), nx1, nx2);
	  x_e = bounds ((seg->end + off_x), nx1, nx2);
	  
	  /*  calculate the length  */
	  length = x_e - x_s;

	  if (length)
	    {
	      /*  calculate the pointers  */
	      s1 = src1 + (x_s - nx1) * bytes;
	      s2 = src2 + (x_s - nx1) * bytes;
	      d = dest + x_s * bytes;
	      
	      /***********************************************************/
	      /*  EFFECTS list                                           */
	      /*  For each effect, the degenerate case is provided       */
	      /*  for extra speeeeeeed!!!!!!                             */
	      /***********************************************************/

	      if (preserve_colors)
		blend = 255;
	      else
		blend = (unsigned char) (seg->value * opacity);

	      switch (mode)
		{
		case BLEND_STENCIL :
		  /*  if blend is 0 or 255, just copy either s1 or s2 to d  */
		  if (!blend)
		    memcpy (d, s2, length * bytes);
		  else if (blend == 255)
		    memcpy (d, s1, length * bytes);
		  else
		    /*  blend source 1 and source 2 into the destination if
		     *  the blend value will make a difference...
		     */
		    blend_pixels (s1, s2, d, blend, length, bytes);
		  break;
		  
		case SHADE_STENCIL :
		  /*  if blend is 0 or 255, then either copy s1 to d or color d with col  */
		  if (!blend)
		    memcpy (d, s1, length * bytes);
		  else if (blend == 255)
		    color_pixels (d, col, length, bytes);
		  else
		  /*  shade by the opacity and region's alpha value  */
		  shade_pixels (s1, d, col, blend, length, bytes);
		  break;
		  
		case COPY_STENCIL :
		  /*  copy the segment directly, no special FX  */
		  memcpy (d, s1, length * bytes);
		  break;
		  
		case COLOR_STENCIL :
		  /*  color the segment with the background color  */
		  color_pixels (d, col, length, bytes);
		  break;
		}
	    }

	  seg_list = next_item (seg_list);
	}

      /*  increment source and dest pointers  */
      src1 += src1PR->rowstride;
      if (src2PR)
	src2 += src2PR->rowstride;
      dest += destPR->rowstride;
    }


}
