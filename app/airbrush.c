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
#include "brushes.h"
#include "errors.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "paint_core.h"
#include "palette.h"
#include "airbrush.h"
#include "selection.h"
#include "tools.h"


/*  The maximum amount of pressure that can be exerted  */
#define             MAX_PRESSURE  0.075

#define             OFF           0
#define             ON            1


static void         airbrush_dialog_callback (int, int, void *, void *);

/*  forward function declarations  */
static void         airbrush_motion      (Tool *);
static void         airbrush_time_out    (XtPointer, XtIntervalId *);


/*  local variables  */
static XtIntervalId timer;                 /*  timer for successive paint applications  */
static TempBuf *    scaled_brush = NULL;   /*  scaled down brush mask  */
static int          timer_state = OFF;     /*  state of airbrush tool  */
static long         rate = 10;             /*  rate of paint applications  */

static AutoDialog   airbrush_dlg = NULL;
static int          rate_scale_ID;
static int          rate_data[4] = { 0, 100, 10, 0 };
static long         rate_value;


void
airbrush_dialog ()
{
  int group_ID;
  int frame_ID;

  if (!airbrush_dlg)
    {
      airbrush_dlg = dialog_new ("Airbrush Options", airbrush_dialog_callback, NULL);
      
      group_ID = dialog_new_item (airbrush_dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (airbrush_dlg, group_ID, ITEM_FRAME, "Rate", NULL);
      rate_value = rate;
      rate_data[2] = rate_value;
      rate_scale_ID = dialog_new_item (airbrush_dlg, frame_ID, ITEM_SCALE, rate_data, NULL);

      dialog_show (airbrush_dlg);
    }
}

void *
airbrush_paint_func (tool, state)
     Tool * tool;
     int state;
{
  GDisplay * gdisp;
  GBrushP brush;
  PaintCore * paint_core;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;
  brush = get_active_brush ();

  switch (state)
    {
    case INIT_PAINT :
      timer_state = OFF;

      /*  Get a scaled down version of the brush based on the pressure  */
      scaled_brush = temp_buf_resize (scaled_brush, brush->mask->bytes, 0, 0,
				      brush->mask->width, brush->mask->height);
      scale_pixels (temp_buf_data (brush->mask), temp_buf_data (scaled_brush),
		    scaled_brush->width * scaled_brush->height,
		    (unsigned char) (255 * (MAX_PRESSURE * get_brush_opacity ())));
      break;

    case MOTION_PAINT :
      if (timer_state == ON)
	XtRemoveTimeOut (timer);
      timer_state = OFF;

      airbrush_motion (tool);

      if (rate)
	{
	  timer = XtAppAddTimeOut (app_context, (1000 / rate),
				   airbrush_time_out, (XtPointer) tool);
	  timer_state = ON;
	}
      break;

    case FINISH_PAINT :
      if (timer_state == ON)
	XtRemoveTimeOut (timer);
      timer_state = OFF;
      break;

    default :
      break;
    }

  return NULL;
}


Tool *
tools_new_airbrush ()
{
  Tool * tool;
  PaintCore * private;

  tool = paint_core_new (AIRBRUSH);

  private = (PaintCore *) tool->private;
  private->paint_func = airbrush_paint_func;

  return tool;
}


void
tools_free_airbrush (tool)
     Tool * tool;
{
  paint_core_free (tool);
  
  if (scaled_brush)
    temp_buf_free (scaled_brush);
  scaled_brush = NULL;

  if (timer_state == ON)
    XtRemoveTimeOut (timer);
  timer_state = OFF;
}


static void
airbrush_time_out (client_data, call_data)
     XtPointer client_data;
     XtIntervalId * call_data;
{
  Tool * tool;

  tool = (Tool *) client_data;

  /*  service the timer  */
  airbrush_motion (tool);

  /*  restart the timer  */
  if (rate)
    timer = XtAppAddTimeOut (app_context, (1000 / rate),
			     airbrush_time_out, (XtPointer) tool);
  else
    timer_state = OFF;
}


static void
airbrush_motion (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * new, * orig;
  int x1, y1, x2, y2;
  unsigned char col[3];

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  /*  If the shift key is down, paint in the background color...  */
  if (paint_core->state & ShiftMask)
    palette_get_background (&col[0], &col[1], &col[2]);
  else
    palette_get_foreground (&col[0], &col[1], &col[2]);

  x1 = paint_core->curx;
  y1 = paint_core->cury;
  x2 = paint_core->lastx;
  y2 = paint_core->lasty;

  /*  Get a region which can be used to paint to  */
  if (get_brush_interpolate ())
    new = paint_core_init_stroke (tool, x1, y1, x2, y2, scaled_brush);
  else
    new = paint_core_init_discrete (tool, x1, y1, scaled_brush);

  /*  Get a copy of the unblemished image  */
  orig = paint_core_get_orig_image (tool, new->x, new->y,
				    new->x + new->width, new->y + new->height);

  /*  color the pixels to the current painting color  */
  color_pixels (temp_buf_data (new), col,
		new->width * new->height, new->bytes);

  /*  apply the brush mode to the region--if applicable  */
  apply_paint_mode (temp_buf_data (new), temp_buf_data (orig), 
		    temp_buf_data (new), 
		    orig->width * orig->height, orig->bytes,
		    get_brush_paint_mode ());

  /*  paste the newly painted canvas to the gimage which is being worked on  */
  paint_core_paste_canvas (tool, new->x, new->y, new->width, new->height);

}


static void
airbrush_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (airbrush_dlg);
      airbrush_dlg = NULL;
      break;
    case CANCEL_ID:
      rate = rate_value;
      dialog_close (airbrush_dlg);
      airbrush_dlg = NULL;
      break;
    default:
      if (item_ID == rate_scale_ID)
	rate = *((long*) call_data);
      break;
    }
}


