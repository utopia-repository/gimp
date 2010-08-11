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
#include "autodialog.h"
#include "blend.h"
#include "draw_core.h"
#include "fuzzy_select.h"
#include "gdisplay.h"
#include "paint_funcs.h"
#include "palette.h"
#include "selection.h"
#include "tools.h"
#include "undo.h"

/*  target size  */
#define  TARGET_HEIGHT     15
#define  TARGET_WIDTH      15

/*  the gradient types  */
#define    LINEAR          0x1
#define    BI_LINEAR       0x2
#define    RADIAL          0x3

#define    SQR(x) ((x) * (x))

/*  the BlendTool structure  */

typedef struct _BlendTool BlendTool;

struct _BlendTool
{
  DrawCore *      core;       /*  Core select object          */

  int             startx;     /*  starting x coord            */
  int             starty;     /*  starting y coord            */

  int             endx;       /*  ending x coord              */
  int             endy;       /*  ending y coord              */

};


/*  local function prototypes  */
static void  blend_button_press          (Tool *, XButtonEvent *, XtPointer);
static void  blend_button_release        (Tool *, XButtonEvent *, XtPointer);
static void  blend_motion                (Tool *, XMotionEvent *, XtPointer);
static void  blend_control               (Tool *, int, void *);
static void  gradient_fill_gimage        (GImage *, int, int, int, int, int);
static void  gradient_fill_selection     (GImage *, Selection *, int, int, int, int, int);
static void  gradient_fill_buf           (TempBuf *, int, int, int, int, int);
static void  gradient_fill_line_radial   (unsigned char *, double, int, int, int, int);
static void  gradient_fill_line_linear   (unsigned char *, double, double *, int, int, int, int);
static void  gradient_fill_line_bilinear (unsigned char *, double, double *, int, int, int, int);

static void  blend_dialog_callback (int, int, void *, void *);

/*  local variables  */
static unsigned char fg_col [3];
static unsigned char bg_col [3];

static long         opacity = 1000;
static long         gradient_type = LINEAR;
static long         blend_offset = 0;

static AutoDialog   blend_dlg = NULL;
static int          opacity_scale_ID;
static long         opacity_value;
static long         opacity_data[4] = { 0, 1000, 1000, 1 };

static int          offset_ID;
static long         blend_offset_value;

static int          linear_ID;
static int          bi_linear_ID;
static int          radial_ID;
static long         gradient_type_value;

static long         paint_mode = NORMAL;

void
blend_dialog ()
{
  int radio_ID;
  int group_ID;
  int colgroup_ID;
  int frame_ID;
  long button_set [3];
  char buf [32];

  if (!blend_dlg)
    {
      blend_dlg = dialog_new ("Blend Fill Options", blend_dialog_callback, NULL);
      
      group_ID = dialog_new_item (blend_dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (blend_dlg, group_ID, ITEM_FRAME, "Gradient Type", NULL);
      radio_ID = dialog_new_item (blend_dlg, frame_ID, (GROUP_ROWS | GROUP_RADIO), NULL, NULL);

      gradient_type_value = gradient_type;
      button_set [0] = (gradient_type_value == LINEAR);
      button_set [1] = (gradient_type_value == BI_LINEAR);
      button_set [2] = (gradient_type_value == RADIAL);

      linear_ID = dialog_new_item (blend_dlg, radio_ID, ITEM_RADIO_BUTTON, "Linear", NULL);
      dialog_change_item (blend_dlg, linear_ID, button_set);
      bi_linear_ID = dialog_new_item (blend_dlg, radio_ID, ITEM_RADIO_BUTTON, "Bi-linear", NULL);
      dialog_change_item (blend_dlg, bi_linear_ID, button_set + 1);
      radial_ID = dialog_new_item (blend_dlg, radio_ID, ITEM_RADIO_BUTTON, "Radial", NULL);
      dialog_change_item (blend_dlg, radial_ID, button_set + 2);

      colgroup_ID = dialog_new_item (blend_dlg, group_ID, GROUP_COLUMNS, NULL, NULL);
      dialog_new_item (blend_dlg, colgroup_ID, ITEM_LABEL, "Blend Offset:", NULL);
      sprintf (buf, "%ld", blend_offset);
      blend_offset_value = blend_offset;
      offset_ID = dialog_new_item (blend_dlg, colgroup_ID, ITEM_TEXT, buf, NULL);

      frame_ID = dialog_new_item (blend_dlg, group_ID, ITEM_FRAME, "Opacity", NULL);
      opacity_value = opacity;
      opacity_data[2] = opacity_value;
      opacity_scale_ID = dialog_new_item (blend_dlg, frame_ID, ITEM_SCALE, opacity_data, NULL);

      dialog_show (blend_dlg);
    }
}


static void
blend_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  BlendTool * blend_tool;

  gdisp = (GDisplay *) gdisp_ptr;
  blend_tool = (BlendTool *) tool->private;

  /*  Keep the coordinates of the target  */
  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, 
			       &blend_tool->startx, &blend_tool->starty, False);

  blend_tool->endx = blend_tool->startx;
  blend_tool->endy = blend_tool->starty;
  
  /*  Make the tool active and set the gdisplay which owns it  */
  XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, None, bevent->time);

  tool->gdisp_ptr = gdisp_ptr;
  tool->state = ACTIVE;

  /*  Start drawing the blend tool  */
  draw_core_start (blend_tool->core, XtWindow (gdisp->disp_image->canvas), tool);

}


static void
blend_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  Selection * select;
  BlendTool * blend_tool;
  int selection;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) gdisp_ptr;
  blend_tool = (BlendTool *) tool->private;
  select = gdisp->select;

  XUngrabPointer (DISPLAY, bevent->time);

  /*  if the 3rd button isn't pressed, fill the selected region  */
  if (! (bevent->state & Button3Mask))
    {
      /*  If there is a floating buffer...  */
      if (gdisp->select->float_buf)
	{
	  TempBuf * new_buf;

	  /*  save the current floating buffer  */
	  new_buf = temp_buf_copy_area (select->float_buf, NULL, 0, 0,
					select->float_buf->width,
					select->float_buf->height);

	  /*  push the new buffer to the undo stack...  */
	  undo_push_mod_sel (gdisp->ID, new_buf);

	  /*  find the bounds for updating  */
	  selection_find_bounds (select, &x1, &y1, &x2, &y2);

	  /*  do a gradient fill in the float buffer  */
	  gradient_fill_buf (select->float_buf, gradient_type, 
			     blend_tool->startx - x1, blend_tool->starty - y1,
			     blend_tool->endx - x1, blend_tool->endy - y1);

	  /*  paste the floating buffer down on the gimage */
	  selection_stencil (select, (void *) gdisp->gimage,
			     0, 0, select->float_buf->width,
			     select->float_buf->height, BLEND_STENCIL);
	}

      /*  If there is not a floating buffer, then we need to blend the
       *  foreground color with the gimage
       */
      else
	{
	  /*  find the extents of the changed portion of the image  */
	  if (! (selection = gdisplay_find_bounds (gdisp, &x1, &y1, &x2, &y2)))
	    {
	      x1 = y1 = 0;
	      x2 = gdisp->gimage->width;
	      y2 = gdisp->gimage->height;
	    }

	  /*  push the image to the undo stack... */
	  undo_push_image (gdisp->gimage->ID, x1, y1, x2, y2);
	      
	  /*  fill the selected region of the gimage...  */
	  if (selection)
	    gradient_fill_selection (gdisp->gimage, gdisp->select, gradient_type,
				     blend_tool->startx, blend_tool->starty, 
				     blend_tool->endx, blend_tool->endy);
	  /*  otherwise, fill in the entire gimage  */
	  else
	    gradient_fill_gimage (gdisp->gimage, gradient_type,
				  blend_tool->startx, blend_tool->starty, 
				  blend_tool->endx, blend_tool->endy);
	}

      /*  update the region  */
      gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1), (y2 - y1), 0);

    }

  draw_core_stop (blend_tool->core, tool);
  tool->state = INACTIVE;
}


static void
blend_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  BlendTool * blend_tool;

  gdisp = (GDisplay *) gdisp_ptr;
  blend_tool = (BlendTool *) tool->private;

  /*  undraw the current tool  */
  draw_core_pause (blend_tool->core, tool);

  /*  Get the current coordinates  */
  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, 
			       &blend_tool->endx, &blend_tool->endy, False);

  /*  redraw the current tool  */
  draw_core_resume (blend_tool->core, tool);
}


static void
blend_draw (tool)
     Tool * tool;
{
  GDisplay * gdisp;
  BlendTool * blend_tool;
  int tx1, ty1, tx2, ty2;

  gdisp = (GDisplay *) tool->gdisp_ptr;
  blend_tool = (BlendTool *) tool->private;

  gdisplay_transform_coords (gdisp, blend_tool->startx, blend_tool->starty, &tx1, &ty1);
  gdisplay_transform_coords (gdisp, blend_tool->endx, blend_tool->endy, &tx2, &ty2);

  /*  Draw start target  */
  XDrawLine (DISPLAY, blend_tool->core->win, blend_tool->core->gc, 
	     tx1 - (TARGET_WIDTH >> 1), ty1,
	     tx1 + (TARGET_WIDTH >> 1), ty1);
  XDrawLine (DISPLAY, blend_tool->core->win, blend_tool->core->gc, 
	     tx1, ty1 - (TARGET_HEIGHT >> 1),
	     tx1, ty1 + (TARGET_HEIGHT >> 1));

  /*  Draw end target  */
  XDrawLine (DISPLAY, blend_tool->core->win, blend_tool->core->gc, 
	     tx2 - (TARGET_WIDTH >> 1), ty2,
	     tx2 + (TARGET_WIDTH >> 1), ty2);
  XDrawLine (DISPLAY, blend_tool->core->win, blend_tool->core->gc, 
	     tx2, ty2 - (TARGET_HEIGHT >> 1),
	     tx2, ty2 + (TARGET_HEIGHT >> 1));

  /*  Draw the line between the start and end coords  */
  XDrawLine (DISPLAY, blend_tool->core->win, blend_tool->core->gc, 
	     tx1, ty1, tx2, ty2);
}


static void
blend_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     void * gdisp_ptr;
{
  BlendTool * blend_tool;

  blend_tool = (BlendTool *) tool->private;

  switch (action)
    {
    case PAUSE : 
      draw_core_pause (blend_tool->core, tool);
      break;
    case RESUME :
      draw_core_resume (blend_tool->core, tool);
      break;
    case HALT :
      draw_core_stop (blend_tool->core, tool);
      break;
    }
}


Tool *
tools_new_blend ()
{
  Tool * tool;
  BlendTool * private;

  tool = (Tool *) xmalloc (sizeof (Tool));
  private = (BlendTool *) xmalloc (sizeof (BlendTool));

  private->core = draw_core_new (blend_draw);

  tool->type = BLEND;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;  /*  Disallow scrolling  */
  tool->private = private;
  tool->button_press_func = blend_button_press;
  tool->button_release_func = blend_button_release;
  tool->motion_func = blend_motion;
  tool->arrow_keys_func = standard_arrow_keys_func;
  tool->control_func = blend_control;

  return tool;
}


void
tools_free_blend (tool)
     Tool * tool;
{
  BlendTool * blend_tool;

  blend_tool = (BlendTool *) tool->private;

  if (tool->state == ACTIVE)
    draw_core_stop (blend_tool->core, tool);

  draw_core_free (blend_tool->core);

  xfree (blend_tool);
}


static void  
gradient_fill_line_radial (data, dist, width, bytes, x, y)
     unsigned char * data;
     double dist;
     int width;
     int bytes;
     int x, y;
{
  int i;
  int b;
  int y_sqr;
  double r;
  double offset;
  double rat;

  if (!dist)
    color_pixels (data, bg_col, width, bytes);
  else
    {
      /*  calculate the radial offset from the start in pixels  */
      offset = blend_offset / dist;
      y_sqr = SQR (y);

      for (i = 0; i < width; i++)
	{
	  r = SQR (x) + y_sqr;
	  rat = r / dist;
	  
	  x++;
	  
	  for (b = 0; b < bytes; b++)
	    {
	      if (rat > 1.0)
		*data++ = bg_col[b];
	      else if (rat < offset)
		*data++ = fg_col[b];
	      else
		*data++ = fg_col[b] + (bg_col[b] - fg_col[b]) * rat;
	    }
	}
    }

}


static void  
gradient_fill_line_linear  (data, dist, vec, width, bytes, x, y)
     unsigned char * data;
     double dist;
     double *vec;
     int width;
     int bytes;
     int x, y;
{
  int i;
  int b;
  double y_part;
  double r;
  double rat;

  if (!dist)
    color_pixels (data, bg_col, width, bytes);
  else
    {
      y_part = vec[1] * y;

      for (i = 0; i < width; i++)
	{
	  r = vec[0] * x + y_part;
	  rat = r / dist;
	  
	  x++;
	  
	  for (b = 0; b < bytes; b++)
	    {
	      if (rat < 0.0)
		*data++ = fg_col[b];
	      else if (rat > 1.0)
		*data++ = bg_col[b];
	      else
		*data++ = fg_col[b] + (bg_col[b] - fg_col[b]) * rat;
	    }
	}
    }

}



static void  
gradient_fill_line_bilinear  (data, dist, vec, width, bytes, x, y)
     unsigned char * data;
     double dist;
     double *vec;
     int width;
     int bytes;
     int x, y;
{
  int i;
  int b;
  double offset;
  double y_part;
  double r;
  double rat;

  if (!dist)
    color_pixels (data, bg_col, width, bytes);
  else
    {
      /*  calculate linear offset from the start line outward  */
      offset = blend_offset / dist;
      y_part = vec[1] * y;

      for (i = 0; i < width; i++)
	{
	  r = vec[0] * x + y_part;
	  rat = r / dist;
	  
	  x++;
	  
	  for (b = 0; b < bytes; b++)
	    {
	      if (rat < -1.0 || rat > 1.0)
		*data++ = bg_col[b];
	      else if (fabs (rat) < offset)
		*data++ = fg_col[b];
	      else
		*data++ = fg_col[b] + (bg_col[b] - fg_col[b]) * fabs (rat);
	    }
	}
    }

}


static void
gradient_fill_gimage (gimage, type, sx, sy, ex, ey)
     GImage * gimage;
     int type;
     int sx, sy;
     int ex, ey;
{
  int i;
  double dist;
  double vec[2];
  int rowstride;
  unsigned char * data;
  unsigned char * image_ptr;

  /* get the foreground and background colors  */
  palette_get_background (&bg_col[0], &bg_col[1], &bg_col[2]);
  palette_get_foreground (&fg_col[0], &fg_col[1], &fg_col[2]);

  data = xmalloc (sizeof (char) * gimage->width * gimage->bpp);
  rowstride = gimage->width * gimage->bpp;
  image_ptr = gimage->raw_image;

  switch (type)
    {
    case RADIAL :
      /* If this is a radial fill, calculate the distance between start and end  */
      dist = SQR (ex - sx) + SQR (ey - sy);
      break;

    case LINEAR : case BI_LINEAR :
      /* If this is a linear fill, calculate:
       * 1) distance between start and end
       * 2) vector from start to end, normalized
       */
      dist = sqrt (SQR (ex - sx) + SQR (ey - sy));
      if (dist > 0.0)
	{
	  vec[0] = (ex - sx) / dist;
	  vec[1] = (ey - sy) / dist;
	}
      break;

    }
       
  for (i = 0; i < gimage->height; i++)
    {
      switch (type)
	{
	case RADIAL :
	  gradient_fill_line_radial (data, dist, gimage->width, 
				     gimage->bpp, -sx, i - sy);
	  break;
	  
	case LINEAR :
	  gradient_fill_line_linear (data, dist, vec, gimage->width,
				     gimage->bpp, -sx, i - sy);
	  break;

	case BI_LINEAR :
	  gradient_fill_line_bilinear (data, dist, vec, gimage->width,
				       gimage->bpp, -sx, i - sy);
	  break;
	}

      /*  Now, blend the pixels with what's in the gimage  */
      blend_pixels (data, image_ptr, data, 
		    (unsigned char) (255 * (opacity / 1000.0)),
		    gimage->width, gimage->bpp);

      /*  Apply the painting mode  */
      apply_paint_mode (data, image_ptr, image_ptr, gimage->width, gimage->bpp, paint_mode);

      image_ptr += rowstride;
    }

  xfree (data);
}


static void
gradient_fill_selection (gimage, select, type, sx, sy, ex, ey)
     GImage * gimage;
     Selection * select;
     int type;
     int sx, sy;
     int ex, ey;
{
  GRegion * region;
  GSegment * seg;
  link_ptr list;
  int i;
  double dist;
  double vec[2];
  int rowstride;
  int x1, y1, x2, y2;
  unsigned char * data;
  unsigned char * image_ptr, * ip;

  /* get the foreground and background colors  */
  palette_get_background (&bg_col[0], &bg_col[1], &bg_col[2]);
  palette_get_foreground (&fg_col[0], &fg_col[1], &fg_col[2]);

  region = select->region;
  gregion_find_bounds (region, &x1, &y1, &x2, &y2);
  data = xmalloc (sizeof (char) * (x2 - x1) * gimage->bpp);
  rowstride = gimage->width * gimage->bpp;
  image_ptr = gimage->raw_image;

  switch (type)
    {
    case RADIAL :
      /* If this is a radial fill, calculate the distance between start and end  */
      dist = SQR (ex - sx) + SQR (ey - sy);
      break;

    case LINEAR : case BI_LINEAR :
      /* If this is a linear fill, calculate:
       * 1) distance between start and end
       * 2) vector from start to end, normalized
       */
      dist = sqrt (SQR (ex - sx) + SQR (ey - sy));
      if (dist > 0.0)
	{
	  vec[0] = (ex - sx) / dist;
	  vec[1] = (ey - sy) / dist;
	}
      break;

    }

  /*  advance the image_ptr to the start of the region  */       
  image_ptr += rowstride * y1;

  for (i = y1; i < y2; i++)
    {
      list = region->segments [i];
      
      while (list)
	{
	  seg = (GSegment *) list->data;
	  ip = image_ptr + (seg->start * gimage->bpp);

	  switch (type)
	    {
	    case RADIAL :
	      gradient_fill_line_radial (data, dist, (seg->end - seg->start),
					 gimage->bpp, seg->start - sx, i - sy);
	      break;
	  
	    case LINEAR :
	      gradient_fill_line_linear (data, dist, vec, (seg->end - seg->start),
					 gimage->bpp, seg->start - sx, i - sy);
	      break;

	    case BI_LINEAR :
	      gradient_fill_line_bilinear (data, dist, vec, (seg->end - seg->start),
					   gimage->bpp, seg->start - sx, i - sy);
	      break;
	    }

	  /*  Now, blend the pixels with what's in the buffer  */
	  blend_pixels (data, ip, data, 
			(unsigned char) ((opacity / 1000.0) * seg->value),
			(seg->end - seg->start), gimage->bpp);

	  /*  Apply the painting mode  */
	  apply_paint_mode (data, ip, ip, (seg->end - seg->start), gimage->bpp, paint_mode);

	  list = next_item (list);
	}

      image_ptr += rowstride;
    }

  xfree (data);
}


static void
gradient_fill_buf (buf, type, sx, sy, ex, ey)
     TempBuf * buf;
     int type;
     int sx, sy;
     int ex, ey;
{
  int i;
  double dist;
  double vec[2];
  int rowstride;
  unsigned char * data;
  unsigned char * image_ptr;

  /* get the foreground and background colors  */
  palette_get_background (&bg_col[0], &bg_col[1], &bg_col[2]);
  palette_get_foreground (&fg_col[0], &fg_col[1], &fg_col[2]);

  data = xmalloc (sizeof (char) * buf->width * buf->bytes);
  rowstride = buf->width * buf->bytes;
  image_ptr = temp_buf_data (buf);

  switch (type)
    {
    case RADIAL :
      /* If this is a radial fill, calculate the distance between start and end  */
      dist = SQR (ex - sx) + SQR (ey - sy);
      break;

    case LINEAR : case BI_LINEAR :
      /* If this is a linear fill, calculate:
       * 1) distance between start and end
       * 2) vector from start to end, normalized
       */
      dist = sqrt (SQR (ex - sx) + SQR (ey - sy));
      if (dist > 0.0)
	{
	  vec[0] = (ex - sx) / dist;
	  vec[1] = (ey - sy) / dist;
	}
      break;

    }
       
  for (i = 0; i < buf->height; i++)
    {
      switch (type)
	{
	case RADIAL :
	  gradient_fill_line_radial (data, dist, buf->width, 
				     buf->bytes, -sx, i - sy);
	  break;
	  
	case LINEAR :
	  gradient_fill_line_linear (data, dist, vec, buf->width,
				     buf->bytes, -sx, i - sy);
	  break;

	case BI_LINEAR :
	  gradient_fill_line_bilinear (data, dist, vec, buf->width,
				       buf->bytes, -sx, i - sy);
	  break;
	}

      /*  Now, blend the pixels with what's in the buffer  */
      blend_pixels (data, image_ptr, data, 
		    (unsigned char) (255 * (opacity / 1000.0)),
		    buf->width, buf->bytes);

      /*  Apply the painting mode  */
      apply_paint_mode (data, image_ptr, image_ptr, buf->width, buf->bytes, paint_mode);

      image_ptr += rowstride;
    }

  xfree (data);
}


static void
blend_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (blend_dlg);
      blend_dlg = NULL;
      break;
    case CANCEL_ID:
      opacity = opacity_value;
      dialog_close (blend_dlg);
      blend_dlg = NULL;
      break;
    default:
      if (item_ID == opacity_scale_ID)
	opacity = *((long*) call_data);
      else if (item_ID == offset_ID)
	blend_offset = atoi ((char *) call_data);
      else if (item_ID == linear_ID && *((long*) call_data))
	gradient_type = LINEAR;
      else if (item_ID == bi_linear_ID && *((long*) call_data))
	gradient_type = BI_LINEAR;
      else if (item_ID == radial_ID && *((long*) call_data))
	gradient_type = RADIAL;
      break;
    }
}
