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
#ifndef __GREGION_H__
#define __GREGION_H__

#include "linked.h"

/* OPERATIONS */

#define ADD 0
#define SUB 1

/*  Half way point where a region is no longer visible in a selection  */
#define HALF_WAY 127

/*  The stencil operations -- Paste modes */
#define BLEND_STENCIL     0
#define COLOR_STENCIL     1
#define COPY_STENCIL      2
#define SHADE_STENCIL     3


typedef struct _gsegment GSegment;

struct _gsegment
{
  short start;
  short end;
  unsigned char value;
};

typedef struct _gregion GRegion;

struct _gregion
{
  link_ptr *  segments;
  int         extent;
  int         width;
  int         id;

  Boolean     bounds_known;    /*  do we need to recalculate the bounds  */

  int         x1, y1;          /*  coordinates for bounding box          */
  int         x2, y2;          /*  lower right hand coordinate           */
};


GRegion *     gregion_new             (int, int);
GRegion *     gregion_copy            (GRegion *, GRegion *);
void          gregion_free            (GRegion *);
Boolean       gregion_point_inside    (GRegion *, int, int);
void          gregion_find_empty_segs (GRegion *, int, int *, int, int *);
Boolean       gregion_find_bounds     (GRegion *, int *, int *, int *, int *);
int           gregion_num_segments    (GRegion *);
void          gregion_add_segment     (GRegion *, int, int, int, int);
void          gregion_sub_segment     (GRegion *, int, int, int, int);
void          gregion_combine_rect    (GRegion *, int, int, int, int, int);
void          gregion_combine_ellipse (GRegion *, int, int, int, int, int, int);
void          gregion_combine_region  (GRegion *, int, GRegion *);
void          gregion_combine_offset_region  (GRegion *, int, GRegion *, int, int);
void          gregion_invert          (GRegion *);
GRegion *     gregion_sharpen         (GRegion *);

/*  This function combines up to two source buffers 
 *  into a dest buffer based on a painting mode...
 *  The arg list for gregion_stencil is as follows:
 *
 *  1)     region
 *  2)     x offset
 *  3)     y offset
 *  4)     opacity
 *  5)     src1_ptr
 *  6)     src2_ptr
 *  7)     dest_ptr
 *  8)     color
 *  9-12)  x1, y1, x2, y2
 *  13)    mode
 *  14)    preserve_colors
 */
void          gregion_stencil         (GRegion *, int, int, double, 
				       void *, void *, void *, unsigned char *, 
				       int, int, int, int, int, int);

#endif  /*  __GREGION_H__  */
