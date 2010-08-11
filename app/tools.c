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
#include "airbrush.h"
#include "bezier_select.h"
#include "blend.h"
#include "bucket_fill.h"
#include "clone.h"
#include "color_picker.h"
#include "convolve.h"
#include "crop.h"
#include "cursorutil.h"
#include "gdisplay.h"
#include "tools.h"
#include "ellipse_select.h"
#include "flip_tool.h"
#include "free_select.h"
#include "fuzzy_select.h"
#include "iscissors.h"
#include "paintbrush.h"
#include "rect_select.h"
#include "text_tool.h"
#include "tools.h"
#include "transform_tool.h"
#include "widget.h"

/* Global Data */

Tool * active_tool = NULL;


unsigned short tool_cursors[] =
{
  XC_tcross,
  XC_tcross,
  XC_tcross,
  XC_tcross,
  XC_tcross,
  XC_tcross,
  XC_cross,
  XC_tcross,
  XC_sb_h_double_arrow,
  XC_sb_v_double_arrow,
  XC_tcross,
  XC_tcross,
  XC_pencil,
  XC_pencil,
  XC_pencil,
  XC_pencil,
  XC_tcross,
  XC_xterm,
};


/* Function definitions */

static void
tools_free (tool)
     Tool * tool;
{
  switch (tool->type)
    {
    case RECT_SELECT :
      tools_free_rect_select (tool);
      break;
    case ELLIPSE_SELECT :
      tools_free_ellipse_select (tool);
      break;
    case FREE_SELECT :
      tools_free_free_select (tool);
      break;
    case FUZZY_SELECT :
      tools_free_fuzzy_select (tool);
      break;
    case BEZIER_SELECT :
      tools_free_bezier_select (tool);
      break;
    case ISCISSORS :
      tools_free_iscissors (tool);
      break;
    case CROP :
      tools_free_crop (tool);
      break;
    case TRANSFORM_TOOL :
      tools_free_transform_tool (tool);
      break;
    case FLIP_HTOOL :
      tools_free_flip_tool (tool);
      break;
    case FLIP_VTOOL :
      tools_free_flip_tool (tool);
      break;
    case COLOR_PICKER :
      tools_free_color_picker (tool);
      break;
    case BUCKET_FILL :
      tools_free_bucket_fill (tool);
      break;
    case PAINTBRUSH :
      tools_free_paintbrush (tool);
      break;
    case AIRBRUSH :
      tools_free_airbrush (tool);
      break;
    case CLONE :
      tools_free_clone (tool);
      break;
    case CONVOLVE :
      tools_free_convolve (tool);
      break;
    case BLEND :
      tools_free_blend (tool);
      break;
    case TEXT :
      tools_free_text (tool);
      break;
    }

  gdisplay_remove_tool_cursor ();

  xfree (tool);
}


void
tools_select (type)
     int type;
{
  if (active_tool)
    {
      tools_toggle_activation (tool_widgets [active_tool->type]);
      tools_free (active_tool);
    }

  switch (type)
    {
    case RECT_SELECT :
      active_tool = tools_new_rect_select ();
      break;
    case ELLIPSE_SELECT :
      active_tool = tools_new_ellipse_select ();
      break;
    case FREE_SELECT :
      active_tool = tools_new_free_select ();
      break;
    case FUZZY_SELECT :
      active_tool = tools_new_fuzzy_select ();
      break;
    case BEZIER_SELECT :
      active_tool = tools_new_bezier_select ();
      break;
    case ISCISSORS :
      active_tool = tools_new_iscissors ();
      break;
    case CROP :
      active_tool = tools_new_crop ();
      break;
    case TRANSFORM_TOOL :
      active_tool = tools_new_transform_tool ();
      break;
    case FLIP_HTOOL :
      active_tool = tools_new_flip_horz ();
      break;
    case FLIP_VTOOL :
      active_tool = tools_new_flip_vert ();
      break;
    case COLOR_PICKER :
      active_tool = tools_new_color_picker ();
      break;
    case BUCKET_FILL :
      active_tool = tools_new_bucket_fill ();
      break;
    case PAINTBRUSH :
      active_tool = tools_new_paintbrush ();
      break;
    case AIRBRUSH :
      active_tool = tools_new_airbrush ();
      break;
    case CLONE :
      active_tool = tools_new_clone ();
      break;
    case CONVOLVE :
      active_tool = tools_new_convolve ();
      break;
    case BLEND :
      active_tool = tools_new_blend ();
      break;
    case TEXT :
      active_tool = tools_new_text ();
      break;
    }

  /*  Set the paused count variable to 0  */
  active_tool->paused_count = 0;

  tools_toggle_activation (tool_widgets[type]);

  gdisplay_install_tool_cursor (tool_cursors [type]);
}


void
tools_options (type)
     int type;
{
  switch (type)
    {
    case RECT_SELECT :
      break;
    case ELLIPSE_SELECT :
      ellipse_dialog ();
      break;
    case FREE_SELECT :
      break;
    case FUZZY_SELECT :
      break;
    case BEZIER_SELECT :
      break;
    case ISCISSORS :
      break;
    case CROP :
      break;
    case TRANSFORM_TOOL :
      transform_tool_dialog ();
      break;
    case FLIP_HTOOL :
      break;
    case FLIP_VTOOL :
      break;
    case COLOR_PICKER :
      break;
    case BUCKET_FILL :
      bucket_fill_dialog ();
      break;
    case PAINTBRUSH :
      paintbrush_dialog ();
      break;
    case AIRBRUSH :
      airbrush_dialog ();
      break;
    case CLONE :
      clone_dialog ();
      break;
    case CONVOLVE :
      convolve_dialog ();
      break;
    case BLEND :
      blend_dialog ();
      break;
    case TEXT :
      break;
    }
}


void
active_tool_control (action, gdisp_ptr)
     int action;
     void * gdisp_ptr;
{
  if (active_tool)
    {
      if (active_tool->gdisp_ptr == gdisp_ptr)
	{
	  switch (action)
	    {
	    case PAUSE :
	      if (active_tool->state == ACTIVE)
		{
		  if (! active_tool->paused_count)
		    {
		      active_tool->state = PAUSED;
		      (* active_tool->control_func) (active_tool, action, gdisp_ptr);
		    }
		}
	      active_tool->paused_count++;

	      break;
	    case RESUME :
	      active_tool->paused_count--;
	      if (active_tool->state == PAUSED)
		{
		  if (! active_tool->paused_count)
		    {
		      active_tool->state = ACTIVE;
		      (* active_tool->control_func) (active_tool, action, gdisp_ptr);
		    }
		}
	      break;
	    case HALT :
	      active_tool->state = INACTIVE;
	      (* active_tool->control_func) (active_tool, action, gdisp_ptr);
	      break;
	    }
	}
    }
}


void
tools_toggle_activation (button)
     Widget button;
{
  Pixmap pixmap1, pixmap2;

  XtVaGetValues (button,
		 XmNlabelPixmap, &pixmap1,
		 XmNuserData, &pixmap2,
		 NULL);

  XtVaSetValues (button,
		 XmNlabelPixmap, pixmap2,
		 XmNuserData, pixmap1,
		 NULL);
}


void
standard_arrow_keys_func (tool, kevent, gdisp_ptr)
     Tool * tool;
     XKeyEvent * kevent;
     XtPointer gdisp_ptr;
{
}


