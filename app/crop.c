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
#include <X11/keysym.h>
#include "appenv.h"
#include "actionarea.h"
#include "draw_core.h"
#include "gdisplay.h"
#include "crop.h"
#include "info_dialog.h"

typedef struct _crop Crop;

struct _crop
{
  DrawCore *      core;       /*  Core select object          */

  int             startx;     /*  starting x coord            */
  int             starty;     /*  starting y coord            */

  int             lastx;      /*  previous x coord            */
  int             lasty;      /*  previous y coord            */

  int             x1, y1;     /*  upper left hand coordinate  */
  int             x2, y2;     /*  lower right hand coords     */

  int             srw, srh;   /*  width and height of corners */

  int             tx1, ty1;   /*  transformed coords          */
  int             tx2, ty2;   /*                              */

  int             function;   /*  moving or resizing          */

};

/* possible crop functions */
#define CREATING        0
#define MOVING          1
#define RESIZING_LEFT   2
#define RESIZING_RIGHT  3
#define CROPPING        4

/* maximum information buffer size */
#define MAX_INFO_BUF    16

static InfoDialog *  crop_info = NULL;
static char          orig_x_buf [MAX_INFO_BUF];
static char          orig_y_buf [MAX_INFO_BUF];
static char          width_buf  [MAX_INFO_BUF];
static char          height_buf [MAX_INFO_BUF];


/*  Crop helper functions   */
static void crop_image              ();
static void crop_recalc             (Tool *, Crop *);
static void crop_start              (Tool *, Crop *);


/*  Crop dialog functions  */
static void crop_info_update        (Tool *);
static void crop_info_create        (Tool *);
static void crop_ok_callback        (Widget, XtPointer, XtPointer);
static void crop_selection_callback (Widget, XtPointer, XtPointer);
static void crop_close_callback     (Widget, XtPointer, XtPointer);
static void crop_destroy_callback   (Widget, XtPointer, XtPointer);


/*  Functions  */


void
crop_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  Crop * crop;
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;
  crop = (Crop *) tool->private;

  if (tool->state == INACTIVE)
    crop->function = CREATING;
  else if (gdisp_ptr != tool->gdisp_ptr)
    crop->function = CREATING;
  else
    {
      /*  If the cursor is in either the upper left or lower right boxes,
	  The new function will be to resize the current crop area        */
      if (bevent->x == bounds (bevent->x, crop->x1, crop->x1 + crop->srw) &&
	  bevent->y == bounds (bevent->y, crop->y1, crop->y1 + crop->srh))
	crop->function = RESIZING_LEFT;
      else if (bevent->x == bounds (bevent->x, crop->x2 - crop->srw, crop->x2) &&
	       bevent->y == bounds (bevent->y, crop->y2 - crop->srh, crop->y2))
	crop->function = RESIZING_RIGHT;
      
      /*  If the cursor is in either the upper right or lower left boxes,
	  The new function will be to translate the current crop area     */
      else if  ((bevent->x == bounds (bevent->x, crop->x1, crop->x1 + crop->srw) &&
		 bevent->y == bounds (bevent->y, crop->y2 - crop->srh, crop->y2)) ||
		(bevent->x == bounds (bevent->x, crop->x2 - crop->srw, crop->x2) &&
		 bevent->y == bounds (bevent->y, crop->y1, crop->y1 + crop->srh)))
	crop->function = MOVING;

      /*  If the pointer is in the rectangular region, crop it!  */
      else if (bevent->x > crop->x1 && bevent->x < crop->x2 &&
	       bevent->y > crop->y1 && bevent->y < crop->y2)
	crop->function = CROPPING;

      /*  otherwise, the new function will be creating, since we want to start anew  */
      else
	crop->function = CREATING;
    }

  if (crop->function == CREATING)
    {
      if (tool->state == ACTIVE)
	draw_core_stop (crop->core, tool);

      tool->gdisp_ptr = gdisp_ptr;

      gdisplay_untransform_coords (gdisp, bevent->x, bevent->y,
				   &crop->tx1, &crop->ty1, True);
      crop->tx2 = crop->tx1;
      crop->ty2 = crop->ty1;

      crop_start (tool, crop);
    }

  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y,
			       &crop->startx, &crop->starty, True);
  crop->lastx = crop->startx;
  crop->lasty = crop->starty;

  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);

  tool->state = ACTIVE;
}


void
crop_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  Crop * crop;

  crop = (Crop *) tool->private;

  XUngrabPointer (DISPLAY, bevent->time);

  if (! (bevent->state & Button3Mask))
    {
      if (crop->function == CROPPING)
	crop_image ();
      else
	{
	  /*  if the crop information dialog doesn't yet exist, create the bugger  */
	  if (! crop_info)
	    crop_info_create (tool);

	  crop_info_update (tool);
	}
    }
  else
    {
      draw_core_stop (((Crop *) tool->private)->core, tool);
      info_dialog_popdown (crop_info);
      tool->state = INACTIVE;
    }
}


void
crop_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  Crop * crop;
  GDisplay * gdisp;
  int x1, y1, x2, y2;
  int curx, cury;
  int inc_x, inc_y;

  crop = (Crop *) tool->private;
  gdisp = (GDisplay *) gdisp_ptr;

  /*  This is the only case when the motion events should be ignored--
      we're just waiting for the button release event to crop the image  */
  if (crop->function == CROPPING)
    return;

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, &curx, &cury, True);
  x1 = crop->startx;
  y1 = crop->starty;
  x2 = curx;
  y2 = cury;

  inc_x = (x2 - x1);
  inc_y = (y2 - y1);

  /*  If there have been no changes... return  */
  if (crop->lastx == curx && crop->lasty == cury)
    return;

  draw_core_pause (crop->core, tool);

  switch (crop->function)
    {
    case CREATING :
      x1 = bounds (x1, 0, gdisp->gimage->width);
      y1 = bounds (y1, 0, gdisp->gimage->height);
      x2 = bounds (x2, 0, gdisp->gimage->width);
      y2 = bounds (y2, 0, gdisp->gimage->height);
      break;

    case RESIZING_LEFT :
      x1 = bounds (crop->tx1 + inc_x, 0, gdisp->gimage->width);
      y1 = bounds (crop->ty1 + inc_y, 0, gdisp->gimage->height);
      x2 = MAXIMUM (x1, crop->tx2);
      y2 = MAXIMUM (y1, crop->ty2);
      crop->startx = curx;
      crop->starty = cury;
      break;

    case RESIZING_RIGHT :
      x2 = bounds (crop->tx2 + inc_x, 0, gdisp->gimage->width);
      y2 = bounds (crop->ty2 + inc_y, 0, gdisp->gimage->height);
      x1 = MINIMUM (crop->tx1, x2);
      y1 = MINIMUM (crop->ty1, y2);
      crop->startx = curx;
      crop->starty = cury;
      break;

    case MOVING :
      inc_x = bounds (inc_x, -crop->tx1, gdisp->gimage->width - crop->tx2);
      inc_y = bounds (inc_y, -crop->ty1, gdisp->gimage->height - crop->ty2);
      x1 = crop->tx1 + inc_x;
      x2 = crop->tx2 + inc_x;
      y1 = crop->ty1 + inc_y;
      y2 = crop->ty2 + inc_y;
      crop->startx = curx;
      crop->starty = cury;
      break;
    }

  /*  make sure that the coords are in bounds  */
  crop->tx1 = MINIMUM (x1, x2);
  crop->ty1 = MINIMUM (y1, y2);
  crop->tx2 = MAXIMUM (x1, x2);
  crop->ty2 = MAXIMUM (y1, y2);

  crop->lastx = curx;
  crop->lasty = cury;

  /*  recalculate the coordinates for crop_draw based on the new values  */
  crop_recalc (tool, crop);
  draw_core_resume (crop->core, tool);
}


void
crop_arrow_keys_func (tool, kevent, gdisp_ptr)
     Tool * tool;
     XKeyEvent * kevent;
     void * gdisp_ptr;
{
  int inc_x, inc_y;
  GDisplay * gdisp;
  Crop * crop;
  KeySym key;

  gdisp = (GDisplay *) gdisp_ptr;
  crop = (Crop *) tool->private;

  inc_x = inc_y = 0;

  if (kevent->type != KeyPress)
    return;
  
  XLookupString (kevent, NULL, 0, &key, NULL);

  switch (key)
    {
    case XK_Up    : inc_y = -1; break;
    case XK_Left  : inc_x = -1; break;
    case XK_Right : inc_x =  1; break;
    case XK_Down  : inc_y =  1; break;
    }

  /*  If the shift key is down, move by an accelerated increment  */
  if (kevent->state & ShiftMask)
    {
      inc_y *= app_data.arrow_accel;
      inc_x *= app_data.arrow_accel;
    }

  draw_core_pause (crop->core, tool);

  if (kevent->state & ControlMask)  /* RESIZING */
    {
      crop->tx2 = bounds (crop->tx2 + inc_x, 0, gdisp->gimage->width);
      crop->ty2 = bounds (crop->ty2 + inc_y, 0, gdisp->gimage->height);
      crop->tx1 = MINIMUM (crop->tx1, crop->tx2);
      crop->ty1 = MINIMUM (crop->ty1, crop->ty2);
    }
  else
    {
      inc_x = bounds (inc_x, -crop->tx1, gdisp->gimage->width - crop->tx2);
      inc_y = bounds (inc_y, -crop->ty1, gdisp->gimage->height - crop->ty2);
      crop->tx1 += inc_x;
      crop->tx2 += inc_x;
      crop->ty1 += inc_y;
      crop->ty2 += inc_y;
    }

  crop_recalc (tool, crop);
  draw_core_resume (crop->core, tool);
}


void
crop_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  Crop * crop;

  crop = (Crop *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (crop->core, tool);
      break;
    case RESUME :
      crop_recalc (tool, crop);
      draw_core_resume (crop->core, tool);
      break;
    case HALT :
      draw_core_stop (crop->core, tool);
      info_dialog_popdown (crop_info);
      break;
    }
}


void
crop_draw (tool)
     Tool * tool;
{
  Crop * crop;
  GDisplay * gdisp;

  #define SRW 10
  #define SRH 10

  gdisp = (GDisplay *) tool->gdisp_ptr;
  crop = (Crop *) tool->private;

  XDrawLine (DISPLAY, crop->core->win, crop->core->gc,
	     crop->x1, crop->y1, gdisp->disp_image->width, crop->y1);
  XDrawLine (DISPLAY, crop->core->win, crop->core->gc,
	     crop->x1, crop->y1, crop->x1, gdisp->disp_image->height);
  XDrawLine (DISPLAY, crop->core->win, crop->core->gc,
	     crop->x2, crop->y2, 0, crop->y2);
  XDrawLine (DISPLAY, crop->core->win, crop->core->gc,
	     crop->x2, crop->y2, crop->x2, 0);

  crop->srw = ((crop->x2 - crop->x1) < SRW) ? (crop->x2 - crop->x1) : SRW;
  crop->srh = ((crop->y2 - crop->y1) < SRH) ? (crop->y2 - crop->y1) : SRH;

  XFillRectangle (DISPLAY, crop->core->win, crop->core->gc,
		  crop->x1, crop->y1, crop->srw, crop->srh);
  XFillRectangle (DISPLAY, crop->core->win, crop->core->gc,
		  crop->x2 - crop->srw, crop->y2-crop->srh, crop->srw, crop->srh);
  XFillRectangle (DISPLAY, crop->core->win, crop->core->gc,
		  crop->x2 - crop->srw, crop->y1, crop->srw, crop->srh);
  XFillRectangle (DISPLAY, crop->core->win, crop->core->gc,
		  crop->x1, crop->y2-crop->srh, crop->srw, crop->srh);

  crop_info_update (tool);
}


Tool *
tools_new_crop ()
{
  Tool * tool;
  Crop * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (Crop *) xmalloc (sizeof (Crop));

  private->core = draw_core_new (crop_draw);
  private->startx = private->starty = 0;
  private->function = CREATING;

  tool->type = CROP;
  tool->state = INACTIVE;
  tool->scroll_lock = 0;  /*  Allow scrolling  */
  tool->private = (void *) private;
  tool->gdisp_ptr = NULL;

  tool->button_press_func = crop_button_press;
  tool->button_release_func = crop_button_release;
  tool->motion_func = crop_motion;
  tool->arrow_keys_func = crop_arrow_keys_func;
  tool->control_func = crop_control;

  return tool;
}


void
tools_free_crop (tool)
     Tool * tool;
{
  Crop * crop;

  crop = (Crop *) tool->private;

  if (tool->state == ACTIVE)
    draw_core_stop (crop->core, tool);

  draw_core_free (crop->core);

  if (crop_info)
    {
      info_dialog_popdown (crop_info);
      crop_info = NULL;
    }

  xfree (crop);
}


static void
crop_image ()
{
  Tool * tool;
  Crop * crop;
  int width, height;
  
  tool = active_tool;
  crop = (Crop *) tool->private;

  width = crop->tx2 - crop->tx1;
  height = crop->ty2 - crop->ty1;

  /*  This function creates the new gdisplay  */
  if (width && height)
  {
    GImage * new_gimage;
    GDisplay * gdisp, * new_gdisp;
    unsigned char * src, * dest;
    long srcwidth, destwidth;
    int bytes;
    int i;
    
    gdisp = (GDisplay *) tool->gdisp_ptr;
    bytes = gdisp->gimage->bpp;
    new_gimage = gimage_new (width, height, gdisp->gimage->type, gdisp->gimage->bpp);
    
    /*  if this is an INDEXED_GIMAGE, copy the colormap  */
    if (new_gimage->type == INDEXED_GIMAGE)
      {
	memcpy (new_gimage->cmap, gdisp->gimage->cmap, COLORMAP_SIZE);
	new_gimage->num_cols = gdisp->gimage->num_cols;
      }

    /*  copy the src image to the dest image  */
    srcwidth = bytes * gdisp->gimage->width;
    destwidth = bytes * width;
    src = gdisp->gimage->raw_image + srcwidth * crop->ty1 + bytes * crop->tx1;
    dest = new_gimage->raw_image;

    for (i = crop->ty1; i < crop->ty2; i++)
      {
	memcpy (dest, src, destwidth);
	src += srcwidth;
	dest += destwidth;
      }

    /*  create the new gdisplay  */
    new_gdisp = gdisplay_gimage (new_gimage, gdisp->scale);
    /*  paint the new gdisplay  */
    gdisplay_paint (new_gdisp);
  }
}


static void
crop_recalc (tool, crop)
     Tool * tool;
     Crop * crop;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) tool->gdisp_ptr;

  gdisplay_transform_coords (gdisp, crop->tx1, crop->ty1, 
			     &crop->x1, &crop->y1);
  gdisplay_transform_coords (gdisp, crop->tx2, crop->ty2,
			     &crop->x2, &crop->y2);
}


static void
crop_start (tool, crop)
     Tool * tool;
     Crop * crop;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) tool->gdisp_ptr;

  crop_recalc (tool, crop);
  draw_core_start (crop->core, XtWindow (gdisp->disp_image->canvas), tool);
}


/*******************************************************/
/*  Crop dialog functions                              */
/*******************************************************/


static ActionAreaItem action_items[3] = 
{
  { "Crop", crop_ok_callback, NULL, NULL },
  { "Selection", crop_selection_callback, NULL, NULL }, 
  { "Close", crop_close_callback, NULL, NULL },
};


static void
crop_info_create (tool)
     Tool * tool;
{
  Widget action_area;
  
  /*  create the info dialog  */
  crop_info = info_dialog_new ("cropInfoDialog", "Crop Information");
  XtAddCallback (crop_info->dialog, XmNdestroyCallback, crop_destroy_callback, NULL);

  /*  add the information fields  */
  info_dialog_add_field (crop_info, "X Origin: ", orig_x_buf);
  info_dialog_add_field (crop_info, "Y Origin: ", orig_y_buf);
  info_dialog_add_field (crop_info, "Width: ", width_buf);
  info_dialog_add_field (crop_info, "Height: ", height_buf);

  /* Create the action area  */
  action_area = build_action_area (crop_info->rowcol, action_items, 3);

  XtManageChild (action_area);
}


static void
crop_info_update (tool)
  Tool * tool;
{
  Crop * crop;

  crop = (Crop *) tool->private;

  sprintf (orig_x_buf, "%d", crop->tx1);
  sprintf (orig_y_buf, "%d", crop->ty1);
  sprintf (width_buf, "%d", (crop->tx2 - crop->tx1));
  sprintf (height_buf, "%d", (crop->ty2 - crop->ty1));

  info_dialog_update (crop_info);
  info_dialog_popup (crop_info);
}


static void
crop_ok_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  crop_image ();
}


static void
crop_selection_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Tool * tool;
  Crop * crop;
  GDisplay * gdisp;

  tool = active_tool;
  crop = (Crop *) tool->private;
  gdisp = (GDisplay *) tool->gdisp_ptr;
  
  draw_core_pause (crop->core, tool);
  if (! gdisplay_find_bounds (gdisp, &crop->tx1, &crop->ty1, &crop->tx2, &crop->ty2))
    {
      crop->tx1 = crop->ty1 = 0;
      crop->tx2 = gdisp->gimage->width;
      crop->ty2 = gdisp->gimage->height;
    }

  crop_recalc (tool, crop);
  draw_core_resume (crop->core, tool);
}


static void
crop_close_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Tool * tool;
  
  tool = active_tool;

  draw_core_stop (((Crop *) tool->private)->core, tool);
  info_dialog_popdown (crop_info);
  tool->state = INACTIVE;
}


static void
crop_destroy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Tool * tool;
  
  tool = active_tool;

  if (crop_info)
    {
      draw_core_stop (((Crop *) tool->private)->core, tool);
      tool->state = INACTIVE;
      crop_info = NULL;
    }
}




