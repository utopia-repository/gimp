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
#include "color_picker.h"
#include "gdisplay.h"
#include "info_dialog.h"
#include "palette.h"
#include "tools.h"


/*  local function prototypes  */
void  color_picker_button_press     (Tool *, XButtonEvent *, XtPointer);
void  color_picker_button_release   (Tool *, XButtonEvent *, XtPointer);
void  color_picker_motion           (Tool *, XMotionEvent *, XtPointer);
void  color_picker_control          (Tool *, int, void *);
void  color_picker_destroy_callback (Widget, XtPointer, XtPointer);

static void          get_color (Tool *, void *, int, int, int);
static void          color_picker_info_update (Tool *);

/* maximum information buffer size */
#define MAX_INFO_BUF    4

/*  local variables  */
static unsigned char  col_value [4] = { 0, 0, 0, 0 };

static InfoDialog *   color_picker_info = NULL;
static char           red_buf   [MAX_INFO_BUF];
static char           green_buf [MAX_INFO_BUF];
static char           blue_buf  [MAX_INFO_BUF];
static char           index_buf [MAX_INFO_BUF];
static char           grey_buf  [MAX_INFO_BUF];


void
color_picker_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  If this is the first invocation of the tool, or a different gdisplay,
   *  create (or recreate) the info dialog...
   */
  if (tool->state == INACTIVE || gdisp_ptr != tool->gdisp_ptr)
    {
      /*  if the dialog exists, free it  */
      if (color_picker_info)
	{
	  XtRemoveCallback (color_picker_info->dialog, XmNdestroyCallback, 
			    color_picker_destroy_callback, NULL);
	  info_dialog_free (color_picker_info);
	}
      
      color_picker_info = info_dialog_new ("colorPickerInfoDialog", "Color Picker");
      XtAddCallback (color_picker_info->dialog, XmNdestroyCallback, 
		     color_picker_destroy_callback, NULL);

      /*  if the gdisplay is for a color image, the dialog must have RGB  */
      switch (gdisp->gimage->type)
	{
	case RGB_GIMAGE :
	  info_dialog_add_field (color_picker_info, "Red", red_buf);
	  info_dialog_add_field (color_picker_info, "Green", green_buf);
	  info_dialog_add_field (color_picker_info, "Blue", blue_buf);
	  break;

	case INDEXED_GIMAGE :
	  info_dialog_add_field (color_picker_info, "Red", red_buf);
	  info_dialog_add_field (color_picker_info, "Green", green_buf);
	  info_dialog_add_field (color_picker_info, "Blue", blue_buf);
	  info_dialog_add_field (color_picker_info, "Index", index_buf);
	  break;

	case GREY_GIMAGE :
	  info_dialog_add_field (color_picker_info, "Intensity", grey_buf);
	  break;
	  
	default :
	  break;
	}
    }

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);
      
  /*  Make the tool active and set the gdisplay which owns it  */
  tool->gdisp_ptr = gdisp_ptr;
  tool->state = ACTIVE;

  /*  if the shift key is down, create a new color.
   *  otherwise, modify the current color.
   */
  if (bevent->state & ShiftMask)
    get_color (tool, gdisp_ptr, bevent->x, bevent->y, COLOR_NEW);
  else
    get_color (tool, gdisp_ptr, bevent->x, bevent->y, COLOR_UPDATE);
}


void
color_picker_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  XUngrabPointer (DISPLAY, bevent->time);
  get_color (tool, gdisp_ptr, bevent->x, bevent->y, COLOR_FINISH);
}


void
color_picker_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  get_color (tool, gdisp_ptr, mevent->x, mevent->y, COLOR_UPDATE);
}


void
color_picker_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
}


Tool *
tools_new_color_picker ()
{
  Tool * tool;

  tool = (Tool *) xmalloc (sizeof (Tool));

  tool->type = COLOR_PICKER;
  tool->state = INACTIVE;
  tool->scroll_lock = 0;  /*  Allow scrolling  */
  tool->private = NULL;
  tool->button_press_func = color_picker_button_press;
  tool->button_release_func = color_picker_button_release;
  tool->motion_func = color_picker_motion;
  tool->arrow_keys_func = standard_arrow_keys_func;
  tool->control_func = color_picker_control;

  return tool;
}


void
tools_free_color_picker (tool)
     Tool * tool;
{
  if (color_picker_info)
    {
      info_dialog_free (color_picker_info);
      color_picker_info = NULL;
    }
}


static void
get_color (tool, gdisp_ptr, x, y, final)
     Tool * tool;
     void * gdisp_ptr;
     int x, y;
     int final;
{
  GDisplay * gdisp;
  unsigned char * src;
  int index;
  
  gdisp = (GDisplay *) gdisp_ptr;

  /*  First, transform the coordinates to gimp image space  */
  gdisplay_untransform_coords (gdisp, x, y, &x, &y, False);

  x = bounds (x, 0, gdisp->gimage->width);
  y = bounds (y, 0, gdisp->gimage->height);

  /*  If the image is color, get RGB  */
  switch (gdisp->gimage->type)
    {
    case RGB_GIMAGE :
      src = gdisp->gimage->raw_image + 3 * (gdisp->gimage->width * y + x);
      col_value [0] = src [0];
      col_value [1] = src [1];
      col_value [2] = src [2];
      break;

    case INDEXED_GIMAGE :
      src = gdisp->gimage->raw_image + (gdisp->gimage->width * y + x);
      col_value [3] = src [0];
      index = src [0] * 3;
      col_value [0] = gdisp->gimage->cmap [index];
      col_value [1] = gdisp->gimage->cmap [index + 1];
      col_value [2] = gdisp->gimage->cmap [index + 2];
      break;

    case GREY_GIMAGE :
      src = gdisp->gimage->raw_image + (gdisp->gimage->width * y + x);
      col_value [0] = src [0];
      col_value [1] = src [0];
      col_value [2] = src [0];
      break;

    default :
      break;
    }

  palette_set_active_color (col_value [0], col_value [1], col_value [2], final);
  color_picker_info_update (tool);
}


static void
color_picker_info_update (tool)
     Tool * tool;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) tool->gdisp_ptr;

  switch (gdisp->gimage->type)
    {
    case RGB_GIMAGE :
      sprintf (red_buf, "%d", col_value [0]);
      sprintf (green_buf, "%d", col_value [1]);
      sprintf (blue_buf, "%d", col_value [2]);
      break;

    case INDEXED_GIMAGE :
      sprintf (red_buf, "%d", col_value [0]);
      sprintf (green_buf, "%d", col_value [1]);
      sprintf (blue_buf, "%d", col_value [2]);
      sprintf (index_buf, "%d", col_value [3]);
      break;

    case GREY_GIMAGE :
      sprintf (grey_buf, "%d", col_value [0]);
      break;
    }

  info_dialog_update (color_picker_info);
  info_dialog_popup (color_picker_info);

}


void
color_picker_destroy_callback (w, client_data, call_data)
  Widget w;
  XtPointer client_data;
  XtPointer call_data;
{
  Tool * tool;
  
  tool = active_tool;

  tool->state = INACTIVE;
  color_picker_info = NULL;
}



