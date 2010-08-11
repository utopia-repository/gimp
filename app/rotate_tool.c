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
#include "rotate_tool.h"
#include "selection.h"
#include "tools.h"
#include "transform_core.h"

/*  index into trans_info array  */
#define ANGLE          0
#define EPSILON        0.018  /*  ~ 1 degree  */

/*  variables local to this file  */
char          angle_buf  [MAX_INFO_BUF];

/*  forward function declarations  */
static void *      rotate_tool_rotate      (Tool *, void *);
static void *      rotate_tool_recalc      (Tool *, void *);
static void        rotate_tool_motion      (Tool *, void *);
static void        rotate_info_update      (Tool *);

void *
rotate_tool_transform (tool, gdisp_ptr, state)
     Tool * tool;
     XtPointer gdisp_ptr;
     int state;
{
  TransformCore * transform_core;

  transform_core = (TransformCore *) tool->private;

  switch (state)
    {
    case INIT :
      if (!transform_info)
	{
	  transform_info = info_dialog_new ("rotateInfoDialog", "Rotation Information");
	  info_dialog_add_field (transform_info, "Angle: ", angle_buf);
	}

      transform_core->trans_info[ANGLE] = 0.0;

      return NULL;
      break;

    case MOTION :
      rotate_tool_motion (tool, gdisp_ptr);

      return (rotate_tool_recalc (tool, gdisp_ptr));
      break;

    case RECALC :
      return (rotate_tool_recalc (tool, gdisp_ptr));
      break;

    case FINISH :
      return (rotate_tool_rotate (tool, gdisp_ptr));
      break;
    }

  return NULL;
}


Tool *
tools_new_rotate_tool ()
{
  Tool * tool;
  TransformCore * private;

  tool = transform_core_new (TRANSFORM_TOOL, INTERACTIVE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = rotate_tool_transform;
  private->trans_info[ANGLE] = 0.0;

  /*  assemble the transformation matrix  */
  identity_matrix (private->transform);

  return tool;
}


void
tools_free_rotate_tool (tool)
     Tool * tool;
{
  transform_core_free (tool);
}

static void
rotate_info_update (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  TransformCore * transform_core;
  float angle;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  angle = (transform_core->trans_info[ANGLE] * 180.0) / M_PI;

  sprintf (angle_buf, "%0.2f", angle);

  info_dialog_update (transform_info);
  info_dialog_popup (transform_info);
}


static void
rotate_tool_motion (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  float angle1, angle2, angle;
  float cx, cy;
  float x1, y1, x2, y2;

  transform_core = (TransformCore *) tool->private;

  cx = (transform_core->x1 + transform_core->x2) / 2.0;
  cy = (transform_core->y1 + transform_core->y2) / 2.0;

  x1 = transform_core->curx - cx;
  x2 = transform_core->lastx - cx;
  y1 = cy - transform_core->cury;
  y2 = cy - transform_core->lasty;
  
  /*  find the first angle  */
  angle1 = atan2 (y1, x1);
  
  /*  find the angle  */
  angle2 = atan2 (y2, x2);
  
  angle = angle2 - angle1;
  
  if (angle > M_PI || angle < -M_PI)
    angle = angle2 - ((angle1 < 0) ? 2*M_PI + angle1 : angle1 - 2*M_PI);

  /*  increment the transform tool's angle  */
  transform_core->trans_info[ANGLE] += angle;

  /*  limit the angle to between 0 and 360 degrees  */
  if (transform_core->trans_info[ANGLE] < - M_PI)
    transform_core->trans_info[ANGLE] = 2 * M_PI - transform_core->trans_info[ANGLE];
  else if (transform_core->trans_info[ANGLE] > M_PI)
    transform_core->trans_info[ANGLE] = transform_core->trans_info[ANGLE] - 2 * M_PI;
  
}



static void *
rotate_tool_recalc (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * select;
  GDisplay * gdisp;
  float cx, cy;
  
  gdisp = (GDisplay *) tool->gdisp_ptr;
  transform_core = (TransformCore *) tool->private;

  /*  find the correct selection structure  */
  if (transform_core->select_ptr)
    select = (Selection *) transform_core->select_ptr;
  else
    select = gdisp->select;


  /*  find the boundaries  */
  if (selection_find_bounds (select, &transform_core->x1, &transform_core->y1,
			     &transform_core->x2, &transform_core->y2))
    {
      cx = (transform_core->x1 + transform_core->x2) / 2.0;
      cy = (transform_core->y1 + transform_core->y2) / 2.0;

      /*  assemble the transformation matrix  */
      identity_matrix  (transform_core->transform);
      translate_matrix (transform_core->transform, -cx, -cy);
      rotate_matrix    (transform_core->transform, transform_core->trans_info[ANGLE]);
      translate_matrix (transform_core->transform, +cx, +cy);
  
      /*  transform the bounding box  */
      transform_bounding_box (tool);

      /*  update the information dialog  */
      rotate_info_update (tool);

      return (void *) select;
    }

  return (void *) NULL;
}


static TempBuf *
rotate_image_90_180_270 (image, degrees)
     TempBuf * image;
     float * degrees;
{
  TempBuf * new;
  unsigned char * src, * s;
  unsigned char * dest, * d;
  int image_width, image_height;
  int new_width, new_height;
  int bytes, b;
  int rowstride_src, rowstride_dest;
  
  int x, y;

  /*
   *  This procedure rotates the input image by either 90, 180, or 270 degrees.
   *  This process is necessary because the standard 3 step raster rotation
   *  works best for rotations between -45 and 45 degrees.
   */

  image_width = image->width;
  image_height = image->height;
  bytes = image->bytes;

  if (*degrees >= (-0.25 * M_PI) && *degrees <= (0.25 * M_PI))
    {
      new_width = image_width;
      new_height = image_height;
      new = temp_buf_new (new_width, new_height, bytes, 0, 0, NULL);
      rowstride_src = image_width * bytes;
      rowstride_dest = new_width * bytes;

      src = temp_buf_data (image);
      dest = temp_buf_data (new);

      for (y = 0; y < new_height; y++)
	{
	  s = src;
	  d = dest;

	  for (x = 0; x < new_width; x++)
	    {
	      b = bytes;
	      while (b--)
		*d++ = *s++;
	    }

	  src += rowstride_src;
	  dest += rowstride_dest;
	}
    }
  else if (*degrees > (0.25 * M_PI) && *degrees < (0.75 * M_PI))
    {
      /*  for 90 degrees, x = y & y = x  */
      new_width = image_height;
      new_height = image_width;
      new = temp_buf_new (new_width, new_height, bytes, 0, 0, NULL);
      rowstride_src = image_width * bytes;
      rowstride_dest = new_width * bytes;

      src = temp_buf_data (image) + rowstride_src * (image_height - 1);
      dest = temp_buf_data (new);

      for (y = 0; y < new_height; y++)
	{
	  s = src;
	  d = dest;

	  for (x = 0; x < new_width; x++)
	    {
	      for (b = 0; b < bytes; b++)
		*d++ = s[b];
	      s -= rowstride_src;
	    }

	  src += bytes;
	  dest += rowstride_dest;
	}

      *degrees -= 0.5 * M_PI;
      
    }
  else if (*degrees >= (0.75 * M_PI) || *degrees <= (-0.75 * M_PI))
    {
      /*  for 180 degrees, x = -x & y = -y  */
      new_width = image_width;
      new_height = image_height;
      new = temp_buf_new (new_width, new_height, bytes, 0, 0, NULL);
      rowstride_src = image_width * bytes;
      rowstride_dest = new_width * bytes;

      src = temp_buf_data (image) + rowstride_src * image_height - bytes;
      dest = temp_buf_data (new);

      for (y = 0; y < new_height; y++)
	{
	  s = src;
	  d = dest;

	  for (x = 0; x < new_width; x++)
	    {
	      for (b = 0; b < bytes; b++)
		*d++ = s[b];

	      s -= bytes;
	    }

	  src -= rowstride_src;
	  dest += rowstride_dest;
	}
      if (*degrees < 0)
	*degrees += M_PI;
      else
	*degrees -= M_PI;
    }
  else if (*degrees > (-0.75 * M_PI) && *degrees < (-0.25 * M_PI))
    {
      /*  for 270 degrees, x = y & y = -x */
      new_width = image_height;
      new_height = image_width;
      new = temp_buf_new (new_width, new_height, bytes, 0, 0, NULL);
      rowstride_src = image_width * bytes;
      rowstride_dest = new_width * bytes;

      src = temp_buf_data (image) + rowstride_src - bytes;
      dest = temp_buf_data (new);

      for (y = 0; y < new_height; y++)
	{
	  s = src;
	  d = dest;

	  for (x = 0; x < new_width; x++)
	    {
	      for (b = 0; b < bytes; b++)
		*d++ = s[b];
	      s += rowstride_src;
	    }

	  src -= bytes;
	  dest += rowstride_dest;
	}

      *degrees += 0.5 * M_PI;
    }

  else
    return NULL;

  return new;
}

/*  This procedure returns a valid pointer to a new selection if the
 *  requested angle is a multiple of 90 degrees...
 */

static void *
rotate_tool_rotate (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * new, * select;
  GDisplay * gdisp;
  GRegion * region;
  MaskBuf * mask, * new_mask;
  TempBuf * new_image;
  float angle;
  int xo, yo;
  int num_incs;

  gdisp = (GDisplay *) gdisp_ptr;
  transform_core = (TransformCore *) tool->private;
  select = (Selection *) transform_core->select_ptr;

  /*  make a copy of the angle measurement so that rotate_image_90_180_270
   *  can modify it to reflect the new angle after some rotation
   */
  num_incs = (transform_core->trans_info[ANGLE] + EPSILON) / (M_PI / 2.0);
  angle = num_incs * (M_PI / 2.0);
  if (fabs (angle - transform_core->trans_info[ANGLE]) > EPSILON)
    return NULL;

  /*  generate a mask from the transform_core's region  */
  mask = mask_convert_from_region (select->region, TRUE);

  /*  Based on the angle, rotate by an integral multiple of 90 degrees  */
  angle = transform_core->trans_info[ANGLE];
  new_image = rotate_image_90_180_270 (select->float_buf, &angle);

  angle = transform_core->trans_info[ANGLE];
  new_mask = rotate_image_90_180_270 ((TempBuf *) mask, &angle);

  region = gregion_new (new_mask->height, new_mask->width);
  mask_convert_to_region (new_mask, region);

  /*  Find the origin of the bounding box  */
  xo = MINIMUM (transform_core->tx1, transform_core->tx2);
  xo = MINIMUM (xo, transform_core->tx3);
  xo = MINIMUM (xo, transform_core->tx4);
  yo = MINIMUM (transform_core->ty1, transform_core->ty2);
  yo = MINIMUM (yo, transform_core->ty3);
  yo = MINIMUM (yo, transform_core->ty4);

  /*  Create the new selection object  */
  new = selection_generic_new (region, xo, yo, new_image);

  /*  Free mask buffers...  */
  mask_buf_free (mask);
  mask_buf_free (new_mask);

  return (void *) new;
}



