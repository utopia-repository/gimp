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
#include "convolve.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "paint_core.h"
#include "selection.h"
#include "tools.h"

#define BLUR          0
#define SHARPEN       1
#define CUSTOM        2

#define FIELD_COLS    4

#define MIN_BLUR      64         /*  (8/9 original pixel)   */
#define MAX_BLUR      0.25       /*  (1/33 original pixel)  */
#define MIN_SHARPEN   -512
#define MAX_SHARPEN   -64

static void         convolve_dialog_callback (int, int, void *, void *);
static void         custom_matrix_callback   (int, int, void *, void *);
static void         custom_matrix_dlg_popup  ();

/*  forward function declarations  */
static void         convolve_motion      (Tool *);
static void         calculate_matrix     ();
static void         integer_matrix       (float *, int *, int);
static void         copy_matrix          (float *, float *, int);
static int          sum_matrix           (int *, int);

/*  local variables  */
static long         convolve_type = BLUR;
static long         pressure = 500;

static AutoDialog   dlg = NULL;
static long         type_value;
static int          blur_ID;
static int          sharpen_ID;
static int          custom_ID;
static int          pressure_ID;
static long         pressure_value;
static long         pressure_data[4] = { 1, 1000, 1000, 1 };

static AutoDialog   custom_matrix_dlg = NULL;
static int          matrix_entry_IDs [25];

static int          matrix [25];
static int          matrix_size;
static int          matrix_divisor;

static float        custom_matrix [25] =
{
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 1, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
};

static float        blur_matrix [25] = 
{
  0, 0, 0, 0, 0,
  0, 1, 1, 1, 0,
  0, 1, MIN_BLUR, 1, 0, 
  0, 1, 1, 1, 0,
  0, 0 ,0, 0, 0,
};

static float        sharpen_matrix [25] = 
{
  0, 0, 0, 0, 0,
  0, 1, 1, 1, 0,
  0, 1, MIN_SHARPEN, 1, 0,
  0, 1, 1, 1, 0,
  0, 0, 0, 0, 0,
};


void
convolve_dialog ()
{
  int radio_ID;
  int group_ID;
  int frame_ID;
  long button_set [3];

  if (!dlg)
    {
      dlg = dialog_new ("Convolve Options", convolve_dialog_callback, NULL);
      
      group_ID = dialog_new_item (dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (dlg, group_ID, ITEM_FRAME, "Convolution Type", NULL);

      radio_ID = dialog_new_item (dlg, frame_ID, (GROUP_ROWS | GROUP_RADIO), NULL, NULL);

      type_value = convolve_type;
      button_set [0] = (type_value == BLUR);
      button_set [1] = (type_value == SHARPEN);
      button_set [2] = (type_value == CUSTOM);

      blur_ID = dialog_new_item (dlg, radio_ID, ITEM_RADIO_BUTTON, "Blur", NULL);
      dialog_change_item (dlg, blur_ID, button_set);
      sharpen_ID = dialog_new_item (dlg, radio_ID, ITEM_RADIO_BUTTON, "Sharpen", NULL);
      dialog_change_item (dlg, sharpen_ID, button_set + 1);
      custom_ID = dialog_new_item (dlg, radio_ID, ITEM_RADIO_BUTTON, "Custom", NULL);
      dialog_change_item (dlg, custom_ID, button_set + 2);

      frame_ID = dialog_new_item (dlg, group_ID, ITEM_FRAME, "Pressure", NULL);
      pressure_value = pressure;
      pressure_data[2] = pressure_value;
      pressure_ID = dialog_new_item (dlg, frame_ID, ITEM_SCALE, pressure_data, NULL);

      dialog_show (dlg);
    }
}

void *
convolve_paint_func (tool, state)
     Tool * tool;
     int state;
{
  PaintCore * paint_core;

  paint_core = (PaintCore *) tool->private;

  switch (state)
    {
    case INIT_PAINT :
      /*  calculate the matrix  */
      calculate_matrix ();
      break;

    case MOTION_PAINT :
      convolve_motion (tool);
      break;

    case FINISH_PAINT :
      break;
    }

  return NULL;
}


Tool *
tools_new_convolve ()
{
  Tool * tool;
  PaintCore * private;

  tool = paint_core_new (CONVOLVE);

  private = (PaintCore *) tool->private;
  private->paint_func = convolve_paint_func;

  return tool;
}


void
tools_free_convolve (tool)
     Tool * tool;
{
  paint_core_free (tool);
}


void
convolve_motion (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  PaintCore * paint_core;
  TempBuf * new;
  PixelRegion srcPR, destPR;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  paint_core = (PaintCore *) tool->private;

  /*  If the image type is indexed, don't convolve  */
  if (gdisp->gimage->type == INDEXED_GIMAGE)
    return;

  x1 = paint_core->curx;
  y1 = paint_core->cury;
  x2 = paint_core->lastx;
  y2 = paint_core->lasty;

  /*  Get a region which can be used to paint to  */
  if (get_brush_interpolate ())
    new = paint_core_init_stroke (tool, x1, y1, x2, y2, NULL);
  else
    new = paint_core_init_discrete (tool, x1, y1, NULL);

  /*  Make sure we're in image bounds  */
  x1 = bounds (new->x, 0, gdisp->gimage->width);
  y1 = bounds (new->y, 0, gdisp->gimage->height);
  x2 = bounds (new->x + new->width, 0, gdisp->gimage->width);
  y2 = bounds (new->y + new->height, 0, gdisp->gimage->height);

  /*  configure the pixel regions correctly  */
  srcPR.bytes = new->bytes;
  srcPR.w = (x2 - x1);
  srcPR.h = (y2 - y1);
  srcPR.rowstride = srcPR.bytes * gdisp->gimage->width;
  srcPR.data = gdisp->gimage->raw_image + srcPR.rowstride * y1 + srcPR.bytes * x1;

  destPR.rowstride = new->width * new->bytes;
  destPR.data = temp_buf_data (new) + destPR.rowstride * (y1 - new->y) + 
    new->bytes * (x1 - new->x);

  /*  convolve the source image with the convolve mask  */
  if (srcPR.w >= matrix_size && srcPR.h >= matrix_size)
    convolve_region (&srcPR, &destPR, matrix, matrix_size, matrix_divisor, NORMAL);
  else
    copy_region (&srcPR, &destPR);

  /*  paste the newly painted canvas to the gimage which is being worked on  */
  paint_core_paste_canvas (tool, new->x, new->y, new->width, new->height);

}

static void
convolve_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (dlg);
      dlg = NULL;
      break;
    case CANCEL_ID:
      convolve_type = type_value;
      pressure = pressure_value;
      dialog_close (dlg);
      dlg = NULL;
      break;
    default:
      if (item_ID == pressure_ID)
	pressure = *((long*) call_data);
      else if (item_ID == blur_ID && *((long*) call_data))
	{
	  convolve_type = BLUR;
	  if (custom_matrix_dlg)
	    {
	      dialog_close (custom_matrix_dlg);
	      custom_matrix_dlg = NULL;
	    }
	}
      else if (item_ID == sharpen_ID && *((long*) call_data))
	{
	  convolve_type = SHARPEN;
	  if (custom_matrix_dlg)
	    {
	      dialog_close (custom_matrix_dlg);
	      custom_matrix_dlg = NULL;
	    }
	}
      else if (item_ID == custom_ID && *((long*) call_data))
	{
	  convolve_type = CUSTOM;
	  custom_matrix_dlg_popup();
	}

      /*  recalculate the matrix  */
      calculate_matrix ();

      break;
    }
}


static void
custom_matrix_callback   (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  int i;

  switch (item_ID)
    {
    case OK_ID:
      dialog_close (custom_matrix_dlg);
      custom_matrix_dlg = NULL;
      break;
    case CANCEL_ID:
      dialog_close (custom_matrix_dlg);
      custom_matrix_dlg = NULL;
      break;
    default:
      for (i = 0; i < 25; i++)
	if (item_ID == matrix_entry_IDs [i])
	  custom_matrix [i] = atof ((char *) call_data);
      break;
    }

  /*  recalculate the matrix  */
  calculate_matrix ();

}


static void
custom_matrix_dlg_popup  ()
{
  int row_group_ID;
  int col_group_ID;
  int i, j;
  int index;
  char buf [32];
  short columns = FIELD_COLS;
  Widget w;

  if (!custom_matrix_dlg)
    {
      custom_matrix_dlg = dialog_new ("Convolution Matrix", custom_matrix_callback, NULL);
      
      row_group_ID = dialog_new_item (custom_matrix_dlg, 0, GROUP_ROWS, NULL, NULL);

      for (i = 0; i < 5; i++)
	{
	  col_group_ID = 
	    dialog_new_item (custom_matrix_dlg, row_group_ID, GROUP_COLUMNS, NULL, NULL);
	  
	  for (j = 0; j < 5; j++)
	    {
	      index = i * 5 + j;
	      matrix_entry_IDs [index] = 
		dialog_new_item (custom_matrix_dlg, col_group_ID, ITEM_TEXT, buf, NULL);

	      w = dialog_get_item_widget (custom_matrix_dlg, matrix_entry_IDs [index]);

	      /*  Make sure the text field is not too large...  */
	      XtVaSetValues (w, XmNcolumns, columns, NULL);
	    }
	}

      dialog_show (custom_matrix_dlg);
    }

  for (i = 0; i < 25; i++)
    {
      sprintf (buf, "%.4f", custom_matrix [i]);
      dialog_change_item (custom_matrix_dlg, matrix_entry_IDs [i], buf);
    }
}


static void
calculate_matrix ()
{
  float percent;

  /*  find percent of tool pressure  */
  percent = (float) pressure / 1000.0;

  /*  get the appropriate convolution matrix and size and divisor  */
  switch (convolve_type)
    {
    case BLUR :
      matrix_size = 5;
      blur_matrix [12] = MIN_BLUR + percent * (MAX_BLUR - MIN_BLUR);
      copy_matrix (blur_matrix, custom_matrix, matrix_size);
      break;

    case SHARPEN :
      matrix_size = 5;
      sharpen_matrix [12] = MIN_SHARPEN + percent * (MAX_SHARPEN - MIN_SHARPEN);
      copy_matrix (sharpen_matrix, custom_matrix, matrix_size);
      break;
      
    case CUSTOM :
      matrix_size = 5;
      break;
    }

  integer_matrix (custom_matrix, matrix, matrix_size);
  matrix_divisor = sum_matrix (matrix, matrix_size);

  if (!matrix_divisor)
    matrix_divisor = 1;

}


static void
integer_matrix (source, dest, size)
     float * source;
     int * dest;
     int size;
{
  int i;

#define PRECISION  10000

  for (i = 0; i < size*size; i++)
    *dest++ = (int) (*source ++ * PRECISION);
}


static void
copy_matrix (src, dest, size)
     float * src;
     float * dest;
     int size;
{
  int i;

  for (i = 0; i < size*size; i++)
    *dest++ = *src++;
}


static int
sum_matrix (matrix, size)
     int * matrix;
     int size;
{
  int sum = 0;

  size *= size;

  while (size --)
    sum += *matrix++;

  return sum;
}


