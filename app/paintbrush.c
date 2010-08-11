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
#include "paintbrush.h"
#include "selection.h"
#include "tools.h"

#define             PAINT_LEFT_THRESHOLD  0.05


static void         paintbrush_dialog_callback (int, int, void *, void *);

/*  forward function declarations  */
static void         paintbrush_motion      (Tool *);

/*  local variables  */
static long         fade_out = 0;             /*  time before brush needs more paint  */

static AutoDialog   paintbrush_dlg = NULL;
static int          fade_out_scale_ID;
static int          fade_out_data[4] = { 0, 500, 500, 0 };
static long         fade_out_value;


void
paintbrush_dialog ()
{
  int group_ID;
  int frame_ID;

  if (!paintbrush_dlg)
    {
      paintbrush_dlg = dialog_new ("Paintbrush Options", paintbrush_dialog_callback, NULL);
      
      group_ID = dialog_new_item (paintbrush_dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (paintbrush_dlg, group_ID, ITEM_FRAME, "Fade Out", NULL);
      fade_out_value = fade_out;
      fade_out_data[2] = fade_out_value;
      fade_out_scale_ID = dialog_new_item (paintbrush_dlg, frame_ID, ITEM_SCALE, fade_out_data, NULL);

      dialog_show (paintbrush_dlg);
    }
}

void *
paintbrush_paint_func (tool, state)
     Tool * tool;
     int state;
{
  PaintCore * paint_core;

  paint_core = (PaintCore *) tool->private;

  switch (state)
    {
    case INIT_PAINT :
      break;

    case MOTION_PAINT :
      paintbrush_motion (tool);
      break;

    case FINISH_PAINT :
      break;

    default : 
      break;
    }

  return NULL;
}


Tool *
tools_new_paintbrush ()
{
  Tool * tool;
  PaintCore * private;

  tool = paint_core_new (PAINTBRUSH);

  private = (PaintCore *) tool->private;
  private->paint_func = paintbrush_paint_func;

  return tool;
}


void
tools_free_paintbrush (tool)
     Tool * tool;
{
  paint_core_free (tool);
}


void
paintbrush_motion (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * orig;
  TempBuf * new;
  int paint_left;
  int x1, y1, x2, y2;
  unsigned char blend;
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
    new = paint_core_init_stroke (tool, x1, y1, x2, y2, NULL);
  else
    new = paint_core_init_discrete (tool, x1, y1, NULL);

  /*  Get a copy of the unblemished image  */
  orig = paint_core_get_orig_image (tool, new->x, new->y,
				    new->x + new->width, new->y + new->height);

  
  /*  determine the blend value based on the opacity  */
  /*  scale the opacity to between 0 and 255          */
  blend = (unsigned char) (255.0 * get_brush_opacity ());

  /*  factor in the fade out value  */
  if (fade_out)
    {
      paint_left = (paint_core->num_movements >= fade_out) ? 0 : 
	fade_out - paint_core->num_movements;

      /*  If paint_left is less than some threshold, stop painting altogether
       */
      if (paint_left < PAINT_LEFT_THRESHOLD * fade_out)
	paint_left = 0;

      blend *= ((float) paint_left  / (float) fade_out);
    }

  /*  If the image type is indexed, preserve colors  */
  if (gdisp->gimage->type == INDEXED_GIMAGE)
    blend = 255;

  if (blend)
    {
      /*  blend the foreground color with the original image  */
      shade_pixels (temp_buf_data (orig), temp_buf_data (new), col,
		    blend, orig->width * orig->height, orig->bytes);

      /*  apply the brush mode to the region--if applicable  */
      apply_paint_mode (temp_buf_data (new), temp_buf_data (orig), 
			temp_buf_data (new), 
			orig->width * orig->height, orig->bytes,
			get_brush_paint_mode ());

      /*  paste the newly painted canvas to the gimage which is being worked on  */
      paint_core_paste_canvas (tool, new->x, new->y, new->width, new->height);
    }

}

static void
paintbrush_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (paintbrush_dlg);
      paintbrush_dlg = NULL;
      break;
    case CANCEL_ID:
      fade_out = fade_out_value;
      dialog_close (paintbrush_dlg);
      paintbrush_dlg = NULL;
      break;
    default:
      if (item_ID == fade_out_scale_ID)
	fade_out = *((long*) call_data);
      break;
    }
}


