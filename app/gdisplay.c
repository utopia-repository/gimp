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
#include <stdio.h>
#include <stdlib.h>
#include "appenv.h"
#include "linked.h"
#include "status.h"
#include "colormaps.h"
#include "cursorutil.h"
#include "gdisplay.h"
#include "gdisplay_ops.h"
#include "gconvert.h"
#include "general.h"
#include "disp-callbacks.h"
#include "scale.h"
#include "scroll.h"
#include "tools.h"
#include "undo.h"
#include "visual.h"
#include "widget.h"
#include "workprocs.h"

/* variable declarations */
link_ptr               display_list = NULL;
static int             display_num  = 1;
static unsigned short  current_tool_cursor = 132;


#define ROUND(x) ((int) (x + 0.5))

#define MAX_TITLE_BUF 4096

static char *image_type_strs[] = { "RGB color", "grayscale", "indexed color" };

GDisplay*
gdisplay_gimage (gimage, scale)
     GImage *gimage;
     unsigned int scale;
{
  Widget new_shell, drawing_area, vsb, hsb, popup;
  GDisplay *gdisp;
  Colormap cmap;
  Visual *visual;
  char title [MAX_TITLE_BUF];
  int instance;

  /* format the title */
  sprintf (title, "%s-%d.%d (%s)",
	   prune_filename (gimage->filename),
	   gimage->ID, gimage->instance_count,
	   image_type_strs[gimage->type]);

  instance = gimage->instance_count;
  gimage->instance_count++;
  gimage->ref_count++;
  
  cmap = get_colormap (gimage->type);

  /* If this is an indexed color image and the color image depth is 8
   *  then store the colormap values.
   */
  if ((gimage->type == INDEXED_GIMAGE) && (color_depth == 8))
    gdisplay_fit_colormap (cmap, gimage->cmap, gimage->num_cols);
    
  visual = (gimage->type == GREY_GIMAGE) ? grey_visual : color_visual;

  create_display_shell (&new_shell, &drawing_area, &hsb, &vsb, &popup,
			gimage->width, gimage->height, &scale, title,
			cmap, visual, gimage->type);

  XtAddCallback (drawing_area, XmNexposeCallback, expose_resize, NULL);
  XtAddCallback (drawing_area, XmNresizeCallback, expose_resize, NULL);
  XtAddCallback (drawing_area, XtNdestroyCallback, destroy_window, NULL);
  XtAddCallback (drawing_area, XmNinputCallback, input_callback, NULL);
  
  /*
   *  manage the display--sets all of the display variables and returns a new
   *  GDisplay structure.
   */
  
  gdisp = gdisplay_manage (new_shell, drawing_area, hsb, vsb, popup, scale, gimage);

  XtAddCallback (vsb, XmNvalueChangedCallback, vert_scrolled, (XtPointer) gdisp);
  XtAddCallback (hsb, XmNvalueChangedCallback, horz_scrolled, (XtPointer) gdisp);
  XtAddCallback (vsb, XmNdragCallback, vert_scrolled, (XtPointer) gdisp);
  XtAddCallback (hsb, XmNdragCallback, horz_scrolled, (XtPointer) gdisp);

  XtAddEventHandler (drawing_area, Button2MotionMask, False,
		     grab_and_scroll, (XtPointer) gdisp);

  XtAddEventHandler (drawing_area, Button1MotionMask, False,
		     tool_motion, (XtPointer) gdisp);

  XtAddEventHandler (drawing_area, ButtonPressMask, False,
		     button_pressed, (XtPointer) gdisp);

  XtAddEventHandler (drawing_area, ButtonReleaseMask, False,
		     button_released, (XtPointer) gdisp);

  /*  associate the GDisplay variable with the top level shell  */
  XtVaSetValues (get_top_shell (drawing_area), XmNuserData, gdisp, NULL);

  gdisp->ID = display_num++;
  gdisp->instance = instance;

  return gdisp;
}


GDisplay *
gdisplay_manage (shell, canvas, hsb, vsb, popup, scale, gimage)
     Widget shell;
     Widget canvas;
     Widget hsb, vsb;
     Widget popup;
     int scale;
     GImage *gimage;
{
  GDisplay *gdisp;

  gdisp = (GDisplay *) xmalloc (sizeof (GDisplay));

  /*
   *  Set all GDisplay parameters...
   */

  gdisp->offset_x = gdisp->offset_y = 0;
  gdisp->scale = scale;    /*  scale ratio = 1:1  */
  gdisp->gimage = gimage;
  gdisp->hsb = hsb;
  gdisp->vsb = vsb;
  gdisp->popup = popup;
  gdisp->shell = shell;
  gdisp->view_options_dialog = NULL;
  gdisp->window_info_dialog = NULL;
  gdisp->dither_type = app_data.dither_type;
  gdisp->win = XtWindow (shell);
  gdisp->gdither = NULL;

  /*  set the window cursor to the current tool cursor  */
  change_win_cursor (DISPLAY, gdisp->win, current_tool_cursor);
  
  /*  set the colormap field  */
  XtVaGetValues (shell, XmNcolormap, &gdisp->colormap, NULL);  

  /*  set the disp_image field  */
  gdisplay_create_image (gdisp, shell, canvas);

  /*  pull this value out of the GXImage data structure  */
  gdisp->depth = gdisp->disp_image->depth;
  
  /*  if the display depth is 8, and we have an RGB type image,
   *  we must allocate a dithering object
   */
  if (gdisp->depth == 8 && gdisp->gimage->type == RGB_GIMAGE)
    gdisp->gdither = dither_new ();

  /*  if we are at 8 bit display depth (pseudocolor visual) and want to 
   *  display greyscale or rgb color, we create a second raw image for 
   *  speed which directly references the colormap
   */
  if (gdisp->depth == 8 && 
      (gdisp->gimage->type == RGB_GIMAGE || gdisp->gimage->type == GREY_GIMAGE))
    gimage_allocate_indexed (gimage, gdisp->gimage->width*gdisp->gimage->height);

  /*  create the selection object  */
  gdisp->select = selection_create (XtWindow (canvas), gdisp,
				    gdisp->gimage->height,
				    gdisp->gimage->width, DEFAULT_SPEED);

  /*  create timer objects  */
  gdisp->main_timer = make_timer ();
  gdisp->line_timer = make_timer ();

  /*  add the new display to the list so that it isn't lost  */
  display_list = append_to_list (display_list, (void *) gdisp);

  /*  setup scale properly  */
  setup_scale (gdisp);

  return gdisp;
}


GDisplay *
gdisplay_active (w)
     Widget w;
{
  GDisplay *gdisp;
  Widget top;
  
  top = get_top_shell (w);

  if (!top)
    {
      return NULL;
    }
  else
    {
      XtVaGetValues (top, XmNuserData, &gdisp, NULL);
      return gdisp;
    }
}


GDisplay *
gdisplay_get_ID (ID)
     int ID;
{
  GDisplay *gdisp;
  link_ptr list = display_list;

  /*  Traverse the list of displays, returning the one that matches the ID  */
  /*  If no display in the list is a match, return NULL.                    */
  while (list)
    {
      gdisp = (GDisplay *) list->data;
      if (gdisp->ID == ID)
	return gdisp;

      list = next_item (list);
    }

  return NULL;
}


void
gdisplay_remove_processes (gdisp)
     GDisplay *gdisp;
{
  /*  remove the dither process  */
  if (gdisp->gdither)
    if (gdisp->gdither->running)
      {
	XtRemoveWorkProc (gdisp->dither_work_proc_id);
	gdisp->gdither->interrupted = True;
	dither_work_proc ((XtPointer) gdisp);

	/*  since this dither has been interrupted, update the
	 *  area to represent that which hasn't yet been dithered
	 */
	gdisp->gdither->y      += gdisp->gdither->scanline;
	gdisp->gdither->height -= gdisp->gdither->scanline;
      }

}


static void
gdisplay_delete (gdisp)
     GDisplay *gdisp;
{
  active_tool_control (HALT, (void *) gdisp);
  gdisplay_remove_processes (gdisp);
  dither_free (gdisp->gdither);
  selection_free (gdisp->select);

  /*  if its an indexed color image then destroy the X colormap */
  if (gdisp->gimage->type == INDEXED_GIMAGE && gdisp->depth == 8)
    XFreeColormap (DISPLAY, gdisp->colormap);

  gimage_delete (gdisp->gimage);
  delete_image (gdisp->disp_image);

  /*  insure that if a view option dialog exists, it is removed  */
  if (gdisp->view_options_dialog)
    XtDestroyWidget (gdisp->view_options_dialog);

  /*  insure that if a window information dialog exists, it is removed  */
  if (gdisp->window_info_dialog)
    XtDestroyWidget (gdisp->window_info_dialog);

  free_timer (gdisp->main_timer);
  free_timer (gdisp->line_timer);
  xfree (gdisp);

}


void
gdisplay_remove_and_delete (gdisp)
     GDisplay *gdisp;
{
  /* remove the display from the list */
  display_list = remove_from_list (display_list, (void *) gdisp);
  gdisplay_delete (gdisp);

  /*  remove any instances of this gdisplay or the associated gimage
   *  from the undo stack.
   */
  undo_clean_stack ();
}


void
gdisplays_delete ()
{
  link_ptr list = display_list;

  /*  traverse the linked list of displays  */
  while (list)
    {
      gdisplay_delete ((GDisplay *) list->data);
      list = next_item (list);
    }

  /*  free up linked list data  */
  free_list (display_list);
}


int
gdisplays_dirty ()
{
  int dirty = 0;
  link_ptr list = display_list;

  /*  traverse the linked list of displays  */
  while (list)
    {
      if (((GDisplay *) list->data)->gimage->dirty > 0)
	dirty = 1;
      list = next_item (list);
    }

  return dirty;
}


/*  install and remove tool cursor from all gdisplays...  */
void
gdisplay_install_tool_cursor (cursor_no)
     int cursor_no;
{
  link_ptr list = display_list;
  GDisplay * gdisp;

  current_tool_cursor = cursor_no;

  while (list)
    {
      gdisp = (GDisplay *) list->data;
      change_win_cursor (DISPLAY, gdisp->win, cursor_no);
      list = next_item (list);
    }
}


void
gdisplay_remove_tool_cursor ()
{
  link_ptr list = display_list;
  GDisplay * gdisp;

  while (list)
    {
      gdisp = (GDisplay *) list->data;
      unset_win_cursor (DISPLAY, gdisp->win);
      list = next_item (list);
    }

}

void
gdisplay_create_image (gdisp, shell, canvas)
     GDisplay *gdisp;
     Widget shell;
     Widget canvas;
{
  Dimension win_width, win_height;
  Visual *visual;
  int depth;

  visual = (gdisp->gimage->type == GREY_GIMAGE) ? grey_visual : color_visual;
  depth = (gdisp->gimage->type == GREY_GIMAGE) ? grey_depth : color_depth;

  XtVaGetValues (canvas,
                 XmNwidth, &win_width,
                 XmNheight, &win_height,
                 NULL);

  gdisp->disp_image = create_image (shell, canvas, win_width,
				    win_height, visual, depth);
}


void
gdisplay_update (ID, x, y, w, h, wait_time)
     int ID;
     long x, y;
     long w, h;
     long wait_time;
{
  GDisplay *gdisp;
  link_ptr list = display_list;
  int count = 0;

  /*  traverse the linked list of displays  */
  while (list)
    {
      gdisp = (GDisplay *) list->data;
      if (gdisp->gimage->ID == ID)
	{
	  /*  We only need to repaint the first instance that
	      we find of this gimage ID.  Otherwise, we would
	      be reconverting the same region unnecessarily.   */
	  if ( !count)
	    gdisplay_repaint (gdisp, x, y, w, h, wait_time);
	  else
	    gdisplay_gimage_region (gdisp, x, y, w, h, wait_time);

	  count++;
	}

      list = next_item (list);
    }

}


void
gdisplay_update_full (ID, wait_time)
     int ID;
     long wait_time;
{
  GDisplay *gdisp;
  link_ptr list = display_list;
  int count = 0;

  /*  traverse the linked list of displays, handling each one  */
  while (list)
    {
      gdisp = (GDisplay *) list->data;
      if (gdisp->gimage->ID == ID)
	{
	  if (!count)
	    gdisplay_repaint (gdisp, 0, 0, 
			      gdisp->gimage->width, 
			      gdisp->gimage->height, 
			      wait_time);
	  else
	    gdisplay_gimage_region (gdisp, 0, 0, 
				    gdisp->gimage->width,
				    gdisp->gimage->height,
				    wait_time);

	  count++;
	}

      list = next_item (list);
    }
}


void
gdisplay_update_title (ID)
     int ID;
{
  GDisplay *gdisp;
  link_ptr list = display_list;
  char title [MAX_TITLE_BUF];

  /*  traverse the linked list of displays, handling each one  */
  while (list)
    {
      gdisp = (GDisplay *) list->data;
      if (gdisp->gimage->ID == ID)
	{
	  /* format the title */
	  sprintf (title, "%s-%d.%d (%s)",
		   prune_filename (gdisp->gimage->filename),
		   gdisp->gimage->ID, gdisp->instance,
		   image_type_strs[gdisp->gimage->type]);
  
	  XtVaSetValues (gdisp->shell,
			 XmNtitle, title,
			 NULL);
	}

      list = next_item (list);
    }
}

void
gdisplay_paint (gdisp)
     GDisplay *gdisp;
{
  gdisplay_repaint (gdisp,
		    0, 0,
		    gdisp->gimage->width,
		    gdisp->gimage->height,
		    0);
}

void
gdisplay_repaint (gdisp, x, y, w, h, wait_time)
     GDisplay *gdisp;
     long x, y;
     long w, h;
     long wait_time;
{
  int x2, y2;
  /*  do some bounds checking...  */
  
  x2 = x + w;  
  y2 = y + h;
  x  = bounds (x, 0, gdisp->gimage->width);
  y  = bounds (y, 0, gdisp->gimage->height);
  x2 = bounds (x2, 0, gdisp->gimage->width);
  y2 = bounds (y2, 0, gdisp->gimage->height);

  w = (x2 - x);
  h = (y2 - y);

  /* if the image is degenerate, return */
  if (!w || !h)
    return;

  switch (gdisp->depth)
    {
    case 8: 
      gdisplay_convert (gdisp, x, y, w, h);
      gdisplay_gimage_region (gdisp, x, y, w, h, wait_time);
      break;
    case 15: 
    case 16: 
    case 24:
      gdisplay_gimage_region (gdisp, x, y, w, h, wait_time);
      break;
    }

}

void
gdisplay_dither (gdisp, x, y, w, h, wait_time)
     GDisplay *gdisp;
     long x, y;
     long w, h;
     long wait_time;
{
  if (gdisp->depth == 8         && 
      gdisp->gimage->type == RGB_GIMAGE)
    {
      gdisp->dither_work_proc_id = 
	XtAppAddWorkProc (app_context, dither_work_proc, gdisp);

      timer_start (gdisp->main_timer);
      
      switch (gdisp->dither_type)
	{
	case ORDERED:
	  gdisp->gdither = dither_create (dither_ordered, gdisp,
					  x, y, w, h, wait_time);
	  
	  break;
	case FLOYD_STEINBERG:
	  gdisp->gdither = dither_create (dither_floyd_steinberg, gdisp,
					  x, y, w, h, wait_time);
	  break;
	}
    }
}


void
gdisplay_convert (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;
     long w, h;
{
  if (gdisp->gimage->type == RGB_GIMAGE)
    {
      switch (gdisp->depth)
	{
	case 8 :
	  _24_to_8_map (gdisp, x, y, w, h);
	  break;
	default :
	  break;
	}
    }
  else if (gdisp->gimage->type == GREY_GIMAGE)
    {
      switch (gdisp->depth)
	{
	case  8:
	  intensity_map_to_8 (gdisp, x, y, w, h);
	  break;
	default :
	  break;
	}
    }

}


void
gdisplay_put_image (gdisp, x, y, w, h)
     GDisplay * gdisp;
     long x, y;
     long w, h;
{
  /* freeze the active tool */
  active_tool_control (PAUSE, (void *) gdisp);

  put_image (gdisp->disp_image, x, y, w, h);

  /* re-enable the active tool */
  active_tool_control (RESUME, (void *) gdisp);
}


void
gdisplay_disp_region (gdisp, x, y, w, h, wait_time)
     GDisplay *gdisp;
     long x, y;
     long w, h;
     long wait_time;
{
  int x2, y2;

  x2 = x + w;  y2 = y + h;
  /* do some bounds checking...the region MUST lie within the disp_image */
  x  = bounds (x, 0, gdisp->disp_image->width);
  y  = bounds (y, 0, gdisp->disp_image->height);
  x2 = bounds (x2, 0, gdisp->disp_image->width);
  y2 = bounds (y2, 0, gdisp->disp_image->height);

  w = (x2 - x);
  h = (y2 - y);

  /* if the image is degenerate, return */
  if (!w || !h)
    return;

  /* remove any ongoing processes such as dithering and selection */
  gdisplay_remove_processes (gdisp);

  /*  Scale the image  */
  scale_image (gdisp, x, y, w, h);

  /*  stop the currently active tool  */
  active_tool_control (PAUSE, (void *) gdisp);

  put_image (gdisp->disp_image, x, y, w, h);

  /* start the currently active tool */
  active_tool_control (RESUME, (void *) gdisp);

  /* recalculate the selection boundaries */
  selection_start (gdisp->select, wait_time, True);

  /* restart the dithering process */
  gdisplay_dither (gdisp, x, y, w, h, wait_time);

}


/*  This routine is similar (and in fact calls) gdisplay_disp_region
 *  except that it accepts arguments which relate to positions inside
 *  the actual gimage and not the disp_image
 */

void
gdisplay_gimage_region (gdisp, x, y, w, h, wait_time)
     GDisplay *gdisp;
     long x, y;
     long w, h;
     long wait_time;
{
  long x1, y1, x2, y2;    /*  coordinate of rectangle corners  */

  x1 = SCALE (gdisp, x) - gdisp->offset_x;
  y1 = SCALE (gdisp, y) - gdisp->offset_y;
  x2 = SCALE (gdisp, (x+w)) - gdisp->offset_x;
  y2 = SCALE (gdisp, (y+h)) - gdisp->offset_y;

  gdisplay_disp_region (gdisp, x1, y1, (x2 - x1), (y2 - y1), wait_time);

}


void
gdisplay_transform_coords (gdisp, x, y, nx, ny)
     GDisplay * gdisp;
     int x, y;
     int *nx, *ny;
{
  float scale;

  /*  transform from gimp coordinates to screen coordinates  */
  scale = (SCALESRC (gdisp) == 1) ? SCALEDEST (gdisp) :
    1.0 / SCALESRC (gdisp);

  *nx = (int) (scale * x - gdisp->offset_x);
  *ny = (int) (scale * y - gdisp->offset_y);
}


void
gdisplay_untransform_coords (gdisp, x, y, nx, ny, round)
     GDisplay * gdisp;
     int x, y;
     int *nx, *ny;
     int round;
{
  float scale;

  /*  transform from screen coordinates to gimp coordinates  */
  scale = (SCALESRC (gdisp) == 1) ? SCALEDEST (gdisp) :
    1.0 / SCALESRC (gdisp);

  if (round)
    {
      *nx = ROUND ((x + gdisp->offset_x) / scale);
      *ny = ROUND ((y + gdisp->offset_y) / scale);
    }
  else
    {
      *nx = (int) ((x + gdisp->offset_x) / scale);
      *ny = (int) ((y + gdisp->offset_y) / scale);
    }
}


Boolean
gdisplay_find_bounds (gdisp, x1, y1, x2, y2)
     GDisplay * gdisp;
     int * x1, * y1;
     int * x2, * y2;
{
  if (! selection_find_bounds (gdisp->select, x1, y1, x2, y2))
    return False;

  *x1 = bounds (*x1, 0, gdisp->gimage->width);
  *y1 = bounds (*y1, 0, gdisp->gimage->height);
  *x2 = bounds (*x2, 0, gdisp->gimage->width);
  *y2 = bounds (*y2, 0, gdisp->gimage->height);

  return True;
}

void
gdisplay_reset_colormap (gdisp)
     GDisplay * gdisp;
{
  XColor palette[256];
  int i, j;

  /* If this is an indexed color image and the color image depth is 8
   *  then store the colormap values.
   */
  if ((gdisp->gimage->type == INDEXED_GIMAGE) && (color_depth == 8))
    {
      for (i = 0, j = 0; i < 256; i++)
	{
	  palette[i].pixel = i;
	  palette[i].red = ((int) gdisp->gimage->cmap[j++]) << 8;
	  palette[i].green = ((int) gdisp->gimage->cmap[j++]) << 8;
	  palette[i].blue = ((int) gdisp->gimage->cmap[j++]) << 8;
	  palette[i].flags = DoRed | DoGreen | DoBlue;
	}
      
      XStoreColors (DISPLAY, gdisp->colormap, palette, 256);
    }
}

