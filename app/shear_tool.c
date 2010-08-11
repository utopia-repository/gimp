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
#include "shear_tool.h"
#include "selection.h"
#include "tools.h"
#include "transform_core.h"

/*  index into trans_info array  */
#define HORZ_OR_VERT   0
#define XSHEAR         1
#define YSHEAR         2

/*  types of shearing  */
#define HORZ           1
#define VERT           2

/*  the minimum movement before direction of shear can be determined (pixels) */
#define MIN_MOVE       5

/*  variables local to this file  */
static int         direction_unknown;
static char        xshear_buf  [MAX_INFO_BUF];
static char        yshear_buf  [MAX_INFO_BUF];

/*  forward function declarations  */
static void *      shear_tool_recalc      (Tool *, void *);
static void        shear_tool_motion      (Tool *, void *);
static void        shear_info_update      (Tool *);

void *
shear_tool_transform (tool, gdisp_ptr, state)
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
	  transform_info = info_dialog_new ("shearInfoDialog", "Shear Information");
	  info_dialog_add_field (transform_info, "X Shear Magnitude: ", xshear_buf);
	  info_dialog_add_field (transform_info, "Y Shear Magnitude: ", yshear_buf);
	}
      direction_unknown = 1;
      transform_core->trans_info[HORZ_OR_VERT] = HORZ;
      transform_core->trans_info[XSHEAR] = 0.0;
      transform_core->trans_info[YSHEAR] = 0.0;

      return NULL;
      break;

    case MOTION :
      shear_tool_motion (tool, gdisp_ptr);

      return (shear_tool_recalc (tool, gdisp_ptr));
      break;

    case RECALC :
      return (shear_tool_recalc (tool, gdisp_ptr));
      break;

    case FINISH :
      direction_unknown = 1;
      break;
    }

  return NULL;
}


Tool *
tools_new_shear_tool ()
{
  Tool * tool;
  TransformCore * private;

  tool = transform_core_new (TRANSFORM_TOOL, INTERACTIVE);

  private = tool->private;

  /*  set the rotation specific transformation attributes  */
  private->trans_func = shear_tool_transform;

  /*  assemble the transformation matrix  */
  identity_matrix (private->transform);

  return tool;
}


void
tools_free_shear_tool (tool)
     Tool * tool;
{
  transform_core_free (tool);
}

static void
shear_info_update (tool)
     Tool * tool;
{
  TransformCore * transform_core;

  transform_core = (TransformCore *) tool->private;

  sprintf (xshear_buf, "%0.2f", transform_core->trans_info[XSHEAR]);
  sprintf (yshear_buf, "%0.2f", transform_core->trans_info[YSHEAR]);

  info_dialog_update (transform_info);
  info_dialog_popup (transform_info);
}


static void
shear_tool_motion (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  int diffx, diffy;
  int dir;

  transform_core = (TransformCore *) tool->private;

  diffx = transform_core->curx - transform_core->lastx;
  diffy = transform_core->cury - transform_core->lasty;

  /*  If we haven't yet decided on which way to control shearing
   *  decide using the maximum differential
   */

  if (direction_unknown)
    {
      if (abs(diffx) > MIN_MOVE || abs(diffy) > MIN_MOVE)
	{
	  if (abs(diffx) > abs(diffy))
	    {
	      transform_core->trans_info[HORZ_OR_VERT] = HORZ;
	      transform_core->trans_info[VERT] = 0.0;
	    }
	  else
	    {
	      transform_core->trans_info[HORZ_OR_VERT] = VERT;
	      transform_core->trans_info[HORZ] = 0.0;
	    }

	  direction_unknown = 0;
	}
      /*  set the current coords to the last ones  */
      else
	{
	  transform_core->curx = transform_core->lastx;
	  transform_core->cury = transform_core->lasty;
	}
    }

  /*  if the direction is known, keep track of the magnitude  */
  if (!direction_unknown)
    {
      dir = transform_core->trans_info[HORZ_OR_VERT];
      switch (transform_core->function)
	{
	case HANDLE_1 :
	  if (dir == HORZ)
	    transform_core->trans_info[XSHEAR] -= diffx;
	  else
	    transform_core->trans_info[YSHEAR] -= diffy;
	  break;
	case HANDLE_2 :
	  if (dir == HORZ)
	    transform_core->trans_info[XSHEAR] -= diffx;
	  else
	    transform_core->trans_info[YSHEAR] += diffy;
	  break;
	case HANDLE_3 :
	  if (dir == HORZ)
	    transform_core->trans_info[XSHEAR] += diffx;
	  else
	    transform_core->trans_info[YSHEAR] -= diffy;
	  break;
	case HANDLE_4 :
	  if (dir == HORZ)
	    transform_core->trans_info[XSHEAR] += diffx;
	  else
	    transform_core->trans_info[YSHEAR] += diffy;
	  break;
	default :
	  return;
	}
    }
  
}



static void *
shear_tool_recalc (tool, gdisp_ptr)
     Tool * tool;
     void * gdisp_ptr;
{
  TransformCore * transform_core;
  Selection * select;
  GDisplay * gdisp;
  float width, height;
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

      width = transform_core->x2 - transform_core->x1;
      height = transform_core->y2 - transform_core->y1;

      if (width == 0)
	width = 1;
      if (height == 0)
	height = 1;

      /*  assemble the transformation matrix  */
      identity_matrix  (transform_core->transform);
      translate_matrix (transform_core->transform, -cx, -cy);

      /*  shear matrix  */
      if (transform_core->trans_info[HORZ_OR_VERT] == HORZ)
	xshear_matrix (transform_core->transform, 
		       (float) transform_core->trans_info [XSHEAR] / height);
      else
	yshear_matrix (transform_core->transform, 
		       (float) transform_core->trans_info [YSHEAR] / width);

      translate_matrix (transform_core->transform, +cx, +cy);
  
      /*  transform the bounding box  */
      transform_bounding_box (tool);

      /*  update the information dialog  */
      shear_info_update (tool);

      return (void *) select;
    }

  return (void *) NULL;
}

