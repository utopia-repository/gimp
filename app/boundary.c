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
#include "appenv.h"
#include "errors.h"
#include "gdisplay.h"
#include "gregion.h"


/* XSegments array growth parameter */
#define MAX_SEGS_INC  2048

/*  The array of vertical segments  */
static int *      vert_segs = NULL;

/*  The array of segments  */
static XSegment * tmp_segs = NULL;
static int        num_segs = 0;
static int        max_segs = 0;

/* static empty segment arrays */
static int *      empty_segs_n = NULL;
static int        num_empty_n = 0;
static int *      empty_segs_c = NULL;
static int        num_empty_c = 0;
static int *      empty_segs_l = NULL;
static int        num_empty_l = 0;
static int        max_empty_segs = 0;

/* global state variables--improve parameter efficiency */
GRegion *    cur_region;
GDisplay *   cur_gdisp;
int          cur_offset_x;
int          cur_offset_y;


/*  local function prototypes  */
static void make_seg (int, int, int, int);
static void allocate_vert_segs ();
static void allocate_empty_segs ();
static void process_horiz_seg (int, int, int, int);
static void make_horiz_segs (int, int, int, int *, int);
static void generate_boundary ();



/*  Function definitions  */

static void
make_seg (x1, y1, x2, y2)
     int x1, y1;
     int x2, y2;
{
  if (num_segs >= max_segs) 
    {
      max_segs += MAX_SEGS_INC;

      tmp_segs = (XSegment *) xrealloc ((void *) tmp_segs, sizeof (XSegment) * max_segs);

      if (!tmp_segs)
	fatal_error ("Unable to reallocate segments array for region boundary.");
    }

  x1 += cur_offset_x;
  x2 += cur_offset_x;
  y1 += cur_offset_y;
  y2 += cur_offset_y;

  gdisplay_transform_coords (cur_gdisp, x1, y1, &x1, &y1);
  gdisplay_transform_coords (cur_gdisp, x2, y2, &x2, &y2);

  tmp_segs[num_segs].x1 = x1;
  tmp_segs[num_segs].y1 = y1;
  tmp_segs[num_segs].x2 = x2;
  tmp_segs[num_segs].y2 = y2;
  num_segs ++;

}


static void
allocate_vert_segs ()
{
  int i;

  /*  allocate and initialize the vert_segs array  */
  vert_segs = (int *) xrealloc ((void *) vert_segs, (cur_region->width + 1) * sizeof (int));

  for (i = 0; i <= cur_region->width ; i++)
    vert_segs[i] = -1;
}


static void
allocate_empty_segs ()
{
  int need_num_segs;

  /*  find the maximum possible number of empty segments given the current gregion  */
  need_num_segs = cur_region->width + 2;

  if (need_num_segs > max_empty_segs)
    {
      max_empty_segs = need_num_segs;

      empty_segs_n = (int *) xrealloc (empty_segs_n, sizeof (int) * max_empty_segs);
      empty_segs_c = (int *) xrealloc (empty_segs_c, sizeof (int) * max_empty_segs);
      empty_segs_l = (int *) xrealloc (empty_segs_l, sizeof (int) * max_empty_segs);

      if (!empty_segs_n || !empty_segs_l || !empty_segs_c)
	fatal_error ("Unable to reallocate empty segments array for region boundary.");
    }

}


static void
process_horiz_seg (x1, y1, x2, y2)
     int x1, y1;
     int x2, y2;
{
  /*  This procedure accounts for any vertical segments that must be
      drawn to close in the horizontal segments.                     */

  if (vert_segs [x1] >= 0)
    {
      make_seg (x1, vert_segs[x1], x1, y1);
      vert_segs[x1] = -1;
    }
  else
    vert_segs[x1] = y1;

  if (vert_segs [x2] >= 0)
    {
      make_seg (x2, vert_segs[x2], x2, y2);
      vert_segs[x2] = -1;
    }
  else
    vert_segs[x2] = y2;

  make_seg (x1, y1, x2, y2);
}
    

static void
make_horiz_segs (start, end, scanline, empty, num_empty)
     int start, end;
     int scanline;
     int empty[];
     int num_empty;
{
  int empty_index;
  int e_s, e_e;    /* empty segment start and end values */

  for (empty_index = 0; empty_index < num_empty; empty_index += 2)
    {
      e_s = *empty++;
      e_e = *empty++;
      if (e_s <= start && e_e >= end)
	process_horiz_seg (start, scanline, end, scanline);
      else if ((e_s > start && e_s < end) ||
	       (e_e < end && e_e > start))
	process_horiz_seg (MAXIMUM (e_s, start), scanline,
			   MINIMUM (e_e, end), scanline);
    }

}


static void
generate_boundary ()
{
  int scanline;
  int i;
  int * tmp_segs;

  /*  array for determining the vertical line segments which must be drawn  */
  allocate_vert_segs ();

  /*  make sure there is enough space for the empty segment array  */
  allocate_empty_segs ();

  num_segs = 0;

  /*  Find the empty segments for the previous and current scanlines  */
  gregion_find_empty_segs (cur_region, -1, empty_segs_l,
			   max_empty_segs, &num_empty_l);
  gregion_find_empty_segs (cur_region, 0, empty_segs_c,
			   max_empty_segs, &num_empty_c);

  for (scanline = 0 ; scanline < cur_region->extent; scanline++)
    {
      /*  find the empty segment list for the next scanline  */
      gregion_find_empty_segs (cur_region, scanline + 1, empty_segs_n,
			       max_empty_segs, &num_empty_n);

      /*  process the segments on the current scanline  */
      for (i = 1; i < num_empty_c - 1; i += 2)
	{
	  make_horiz_segs (empty_segs_c [i], empty_segs_c [i+1], scanline, empty_segs_l, num_empty_l);
	  make_horiz_segs (empty_segs_c [i], empty_segs_c [i+1], scanline+1, empty_segs_n, num_empty_n);
	}

      /*  get the next scanline of empty segments, swap others  */
      tmp_segs = empty_segs_l;
      empty_segs_l = empty_segs_c;
      num_empty_l = num_empty_c;
      empty_segs_c = empty_segs_n;
      num_empty_c = num_empty_n;
      empty_segs_n = tmp_segs;
    }

}


XSegment *
find_region_boundary (region, offset_x, offset_y, gdisp_ptr, num_elems)
     GRegion * region;
     int offset_x;
     int offset_y;
     void * gdisp_ptr;
     int * num_elems;
{
  XSegment * new_segs = NULL;

  cur_region = region;
  cur_gdisp = (GDisplay *) gdisp_ptr;
  cur_offset_x = offset_x;
  cur_offset_y = offset_y;

  /*  Calculate the boundary  */
  generate_boundary ();

  /*  Set the number of X segments  */
  *num_elems = num_segs;

  /*  Make a copy of the boundary  */
  if (num_segs)
    {
      new_segs = (XSegment *) xmalloc (sizeof (XSegment) * num_segs);
      memcpy (new_segs, tmp_segs, (sizeof (XSegment) * num_segs));
    }

  /*  Return the new boundary  */
  return new_segs;
}
