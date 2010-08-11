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
#include "boundary.h"
#include "colormaps.h"
#include "errors.h"
#include "gdisplay.h"
#include "gdisplay_ops.h"
#include "global_edit.h"
#include "paint_funcs.h"
#include "palette.h"
#include "selection.h"
#include "undo.h"

/*  local functions  */
static void  opacity_dialog_callback (int, int, void *, void *);


/*  The possible internal drawing states...  */
#define INVISIBLE         0
#define INTRO             1
#define MARCHING          2


#define INITIAL_DELAY     15  /* in milleseconds */

static unsigned char ant_data[9][8] =
{
  {
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
  },
  {
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
  },
  {
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
  },
  {
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
  },
  {
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
  },
  {
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
  },
  {
    0x3C,    /*  --####--  */
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
  },
  {
    0x78,    /*  -####---  */
    0xF0,    /*  ####----  */
    0xE1,    /*  ###----#  */
    0xC3,    /*  ##----##  */
    0x87,    /*  #----###  */
    0x0F,    /*  ----####  */
    0x1E,    /*  ---####-  */
    0x3C,    /*  --####--  */
  },
  {
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
    0xFF,    /*  ########  */
  },
};

Pixmap marching_ants[9] = { 0 };
/*Pixmap ant_diffs[8] = { 0 };*/


static void
selection_draw (select)
     Selection * select;
{
  if (select->xelems && !select->hidden)
    XDrawSegments (DISPLAY, select->win, select->gc,
		   select->xelems, select->num_xelems);
}


static void
selection_generate_xelems (select)
     Selection * select;
{
  /*  Use the gregion boundary function to calculate the array of XSegments
   *  which outline the currently selection region
   */
  select->xelems = find_region_boundary (select->region, select->offset_x,
					 select->offset_y, select->gdisp,
					 &select->num_xelems);
}


static void
selection_free_xelems (select)
     Selection * select;
{
  if (select->xelems)
    xfree (select->xelems);

  select->xelems     = NULL;
  select->num_xelems = 0;
}


static void
selection_march_ants (client_data, call_data)
     XtPointer client_data;
     XtIntervalId * call_data;
{
  Selection * select;

  select = (Selection *) client_data;

  /*  if the RECALC bit is set, reprocess the boundaries  */
  if (select->recalc)
    {
      selection_free_xelems (select);
      selection_generate_xelems (select);
      /* Toggle the RECALC flag */
      select->recalc = False;
    }

  /*  increment stipple index  */
  select->index++;
  if (select->index > 7)
    select->index = 0;

  /*  Make sure the state is set to marching  */
  select->state = MARCHING;

  /*  Draw the ants  */
  XSetStipple (DISPLAY, select->gc, marching_ants[select->index]);
  selection_draw (select);

  /*  Reset the timer  */
  select->timer = XtAppAddTimeOut (app_context, select->speed,
				   selection_march_ants,
				   (XtPointer) select);
}


/*  Public functions  */

Selection *
selection_create (win, gdisp_ptr, size, width, speed)
     Window win;
     void * gdisp_ptr;
     int size;
     int width;
     int speed;
{
  GDisplay * gdisp;
  Selection * new;
  XGCValues gcv;
  int i;

  gdisp = (GDisplay *) gdisp_ptr;

  if (!marching_ants[0])
    for (i = 0; i < 9; i++)
      marching_ants[i] = XCreateBitmapFromData (DISPLAY, win, (char*) ant_data[i], 8, 8);

  new = (Selection *) xmalloc (sizeof (Selection));

  new->win        = win;
  new->state      = INVISIBLE;
  new->paused     = 0;
  new->hidden     = False;
  new->recalc     = True;
  new->speed      = speed;
  new->xelems     = NULL;
  new->num_xelems = 0;
  new->index      = 0;
  new->region     = gregion_new (size, width);
  new->gdisp      = gdisp_ptr;
  new->offset_x   = 0;
  new->offset_y   = 0;
  new->opacity    = 1.0;
  new->old_opacity= 1.0;
  new->dlg        = NULL;
  new->float_buf  = NULL;
  new->orig_buf   = NULL;

  /*  get black and white pixels for this gdisplay  */
  gcv.foreground = gdisplay_black_pixel (gdisp);
  gcv.background = gdisplay_white_pixel (gdisp);

  /*  create a new graphics context  */
  new->gc = XCreateGC (DISPLAY, new->win, GCForeground | GCBackground, &gcv);

  /*  XSetFillStyle (DISPLAY, new->gc, FillStippled); */
  XSetFillStyle (DISPLAY, new->gc, FillOpaqueStippled);
  XSetLineAttributes (DISPLAY, new->gc,
		      1, LineSolid,
		      CapButt, JoinMiter);

  return new;
}


/*
 *   The generic selection routines are for selections which will
 *   not be displayed or 'owned' by a gdisplay.  They are temporary
 */

Selection *
selection_generic_new (region, offset_x, offset_y, float_buf)
     GRegion * region;
     int offset_x, offset_y;
     TempBuf * float_buf;
{
  Selection * new;

  new = (Selection *) xmalloc (sizeof (Selection));

  new->region = region;
  new->offset_x = offset_x;
  new->offset_y = offset_y;
  new->float_buf = float_buf;
  new->orig_buf = NULL;

  return new;
}


void
selection_generic_free (select)
     Selection * select;
{
  if (!select)
    return;
  
  if (select->region)
    gregion_free (select->region);
  if (select->float_buf)
    temp_buf_free (select->float_buf);
  if (select->orig_buf)
    temp_buf_free (select->orig_buf);

  xfree (select);
}


void
selection_swap (select1, select2)
     Selection * select1;
     Selection * select2;
{
  int tmp_off;
  GRegion * tmp_region;
  TempBuf * tmp_buf;
  
  /*  Used to swap the items of importance in two selections  */

  /*  1)  Swap the regions  */
  tmp_region = select1->region;
  select1->region = select2->region;
  select2->region = tmp_region;

  /*  2)  Swap the buffers  */
  tmp_buf = select1->float_buf;
  select1->float_buf = select2->float_buf;
  select2->float_buf = tmp_buf;

  tmp_buf = select1->orig_buf;
  select1->orig_buf = select2->orig_buf;
  select2->orig_buf = tmp_buf;

  /*  3)  Swap the offsets  */
  tmp_off = select1->offset_x;
  select1->offset_x = select2->offset_x;
  select2->offset_x = tmp_off;

  tmp_off = select1->offset_y;
  select1->offset_y = select2->offset_y;
  select2->offset_y = tmp_off;
  
}


void
selection_clear (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;
  selection_invis (select);
  
  /*  push the current selection onto the stack  */
  undo_push_selection (gdisp->ID, select);

  /*  we don't need to free up the region because it goes on the *
   *  undo stack.  The same thing goes for the edit buffers       */

  select->region    = gregion_new (gdisp->gimage->height, gdisp->gimage->width);
  select->float_buf  = NULL;
  select->orig_buf  = NULL;
  select->offset_x  = 0;
  select->offset_y  = 0;
}


void
selection_pause (select)
     Selection * select;
{
  if (select->state != INVISIBLE)
    XtRemoveTimeOut (select->timer);

  select->paused ++;
}


void
selection_resume (select)
     Selection * select;
{
  if (select->paused == 1)
    select->timer = XtAppAddTimeOut (app_context, INITIAL_DELAY,
				     selection_march_ants,
				     (XtPointer) select);

  select->paused --;
}


void
selection_start (select, wait_time, recalc)
     Selection * select;
     long wait_time;
     int recalc;
{
  /*  If this selection is paused, do not start it  */
  if (select->paused > 0)
    return;

  /*  A call to selection_start with recalc == True means that 
   *  we want to recalculate the selection boundary--usually
   *  after scaling or panning the display, or modifying the
   *  selection in some way.  If recalc == False, the already
   *  calculated boundary is simply redrawn.
   */
  if (recalc)
    select->recalc = True;

  if (select->state != INVISIBLE)
    XtRemoveTimeOut (select->timer);
    
  select->state = INTRO;  /*  The state before the first draw  */
  select->timer = XtAppAddTimeOut (app_context, wait_time,
				   selection_march_ants,
				   (XtPointer) select);
}


void
selection_invis (select)
     Selection * select;
{
  int sm;   /*  selection margin  */

  if (select->state == INVISIBLE)
    return;

  XtRemoveTimeOut (select->timer);

  if (select->state == MARCHING)
    {
      GDisplay * gdisp;
      int x1, y1, x2, y2;

      gdisp = (GDisplay *) select->gdisp;

      /*  Find the bounds of the selection  */
      selection_find_bounds (select, &x1, &y1, &x2, &y2);
      gdisplay_transform_coords (gdisp, x1, y1, &x1, &y1);
      gdisplay_transform_coords (gdisp, x2, y2, &x2, &y2);

      /*  Make sure the extents are within bounds  */
      x1 = bounds (x1, 0, gdisp->disp_image->width);
      y1 = bounds (y1, 0, gdisp->disp_image->height);
      /*  Account for the fact that selection extend one pixel beyond bounds  */
      sm = UNSCALE (gdisp, 1);
      if (sm < 1) sm = 1;
      x2 = bounds (x2 + sm, 0, gdisp->disp_image->width);
      y2 = bounds (y2 + sm, 0, gdisp->disp_image->height);

      /*  Put the gximage to the display  */
      gdisplay_put_image (gdisp, x1, y1, (x2 - x1), (y2 - y1));
    }

  select->state = INVISIBLE;
}


Boolean
selection_point_inside (select, gdisp_ptr, x, y)
     Selection * select;
     void * gdisp_ptr;
     int x, y;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;

  gdisplay_untransform_coords (gdisp, x, y, &x, &y, False);

  x -= select->offset_x;
  y -= select->offset_y;

  return gregion_point_inside (select->region, x, y);
}


Boolean
selection_find_bounds (select, x1, y1, x2, y2)
     Selection * select;
     int * x1, * y1;
     int * x2, * y2;
{
  if (! gregion_find_bounds (select->region, x1, y1, x2, y2))
    return False;

  *x1 += select->offset_x;
  *x2 += select->offset_x;
  *y1 += select->offset_y;
  *y2 += select->offset_y;

  return True;
}


void
selection_translate (select, x1, y1, x2, y2, gdisp_ptr)
     Selection * select;
     int x1, y1;
     int x2, y2;
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  int bx1, by1, bx2, by2;
  int sm;  /*  selection margin  */

  gdisp = (GDisplay *) gdisp_ptr;

  select->offset_x = x1;
  select->offset_y = y1;
  selection_cut_floating (select, (void *) gdisp->gimage);

  select->offset_x = x2;
  select->offset_y = y2;
  selection_paste_floating (select, (void *) gdisp->gimage, BLEND_STENCIL);

  /*  Find bounds on area to update  */
  gregion_find_bounds (select->region, &bx1, &by1, &bx2, &by2);
      
  bx1 = MINIMUM ((x1 + bx1), (x2 + bx1));
  bx2 = MAXIMUM ((x1 + bx2), (x2 + bx2));
  by1 = MINIMUM ((y1 + by1), (y2 + by1));
  by2 = MAXIMUM ((y1 + by2), (y2 + by2));

  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, bx1, by1, (bx2 - bx1 + sm), (by2 - by1 + sm), 0);

}


void
selection_anchor (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  GRegion * new;
  GSegment *seg;
  GDisplay * gdisp;
  link_ptr list;
  int i, y;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  Make the current selection invisible--white it out  */
  selection_invis (select);

  new = gregion_new (gdisp->gimage->height, gdisp->gimage->width);

  /*  push the old_select structure onto the undo stack  */
  undo_push_selection (gdisp->ID, select);

  /*  Now, actually anchor the selection  */
  for (i = 0; i < select->region->extent; i++)
    {
      y = i + select->offset_y;

      if ( y >= 0 && y < new->extent)
	{
	  list = select->region->segments[i];
	  while (list)
	    {
	      seg = (GSegment *) list->data;
	      gregion_add_segment (new, seg->start + select->offset_x, y,
				   (seg->end - seg->start), seg->value);
	      
	      list = next_item (list);
	    }
	}
    }

  select->region   = new;
  select->float_buf = NULL;
  select->orig_buf = NULL;
  select->offset_x = 0;
  select->offset_y = 0;
}


void
selection_stencil (select, gimage_ptr, x1, y1, x2, y2, mode)
     Selection * select;
     void * gimage_ptr;
     int x1, y1;
     int x2, y2;
     int mode;
{
  PixelRegion src1PR, src2PR, destPR;
  int rx1, ry1, rx2, ry2;
  GImage * gimage;
  unsigned char bg_col [3];
  float opacity;

  /*  Get the background color  */
  palette_get_background (&bg_col[0], &bg_col[1], &bg_col[2]);

  gimage = (GImage *) gimage_ptr;

  /*  Set up the pixel regions  */
  src1PR.bytes = gimage->bpp;
  src1PR.w = (x2 - x1);
  src1PR.h = (y2 - y1);
  src1PR.rowstride = select->float_buf->width * src1PR.bytes;
  src1PR.data = temp_buf_data (select->float_buf) + y1 * src1PR.rowstride + x1 * src1PR.bytes;

  src2PR.rowstride = select->orig_buf->width * src1PR.bytes;  
  src2PR.data = temp_buf_data (select->orig_buf) + y1 * src2PR.rowstride + x1 * src1PR.bytes;

  destPR.w = gimage->width;
  destPR.h = gimage->height;
  destPR.rowstride = gimage->width * src1PR.bytes;
  destPR.data = gimage->raw_image;

  /*  Find the gregion bounds to stencil to  */
  gregion_find_bounds (select->region, &rx1, &ry1, &rx2, &ry2);

  /*  In the case of SHADE_STENCIL mode, we don't want to use the selection's opacity
   *  This condition occurs when cutting a selection.  This shouldn't be dependant on
   *  the current opacity of the selection.
   */
  opacity = (mode == SHADE_STENCIL) ? 1.0 : select->opacity;

  /*  Blend the float buffer, orig buffer and gimage in various ways based on "mode"  */
  gregion_stencil (select->region, select->offset_x, select->offset_y, opacity, 
		   &src1PR, &src2PR, &destPR, bg_col,
		   rx1 + x1, ry1 + y1, rx1 + x2, ry1 + y2,
		   mode, (gimage->type == INDEXED_GIMAGE) ? 1 : 0);
}


void
selection_cut_floating (select, gimage_ptr)
     Selection * select;
     void * gimage_ptr;
{
  int x1, y1, x2, y2;

  selection_find_bounds (select, &x1, &y1, &x2, &y2);

  /*  If there is no selection to speak of...forget it!  */
  if (!(x2 - x1) || !(y2 - y1))
    return;

  /*  If this is the initial cut...  */
  if (!select->float_buf && !select->orig_buf)
    {
      /*  create & init the new float buffer  */
      select->float_buf = temp_buf_load (NULL, gimage_ptr, x1, y1, (x2 - x1), (y2 - y1));

      /*  create the new orig buffer  */
      select->orig_buf = temp_buf_load (NULL, gimage_ptr, x1, y1, (x2 - x1), (y2 - y1));

      /*  cut out background color outline  */
      selection_stencil (select, gimage_ptr, 0, 0, select->float_buf->width,
			 select->float_buf->height, SHADE_STENCIL);

      /*  set the status flag indicating that this operation cut the selection freshly  */
      select->fresh_cut = 1;
    }
  /*  If the selection already has a floating buffer...  */
  else
    {
      /*  paste the original buffer down on the gimage  */
      temp_buf_paste (select->orig_buf, gimage_ptr, x1, y1, (x2 - x1), (y2 - y1));

      select->fresh_cut = 0;
    }
}


void
selection_paste_floating (select, gimage_ptr, mode)
     Selection * select;
     void * gimage_ptr;
     int mode;
{
  int x1, y1, x2, y2;

  /*  return if there is no floating buffer  */
  if (!select->float_buf)
    return;

  selection_find_bounds (select, &x1, &y1, &x2, &y2);

  /*  save the background gimage -- if the orig buf is NULL, create it */
  if (! select->orig_buf)
    select->orig_buf = temp_buf_new (select->float_buf->width, select->float_buf->height,
				     select->float_buf->bytes, x1, y1, NULL);

  temp_buf_load (select->orig_buf, gimage_ptr, x1, y1, (x2 - x1), (y2 - y1));

  /*  paste the floating buffer down on the gimage */
  selection_stencil (select, gimage_ptr, 0, 0, select->float_buf->width,
		     select->float_buf->height, mode);

}


void
selection_replace (select, gdisp_ptr, new_select)
     Selection * select;
     void * gdisp_ptr;
     Selection * new_select;
{
  GDisplay * gdisp;
  int x1, y1, x2, y2;
  int x3, y3, x4, y4;
  int sm;   /*  selection margin  */

  /*  Replace the current selection  gdisp_ptr's select == select with a new
   *  one.  Push the old selection onto the undo stack...
   */

  gdisp = (GDisplay *) gdisp_ptr;
  if (gdisp->select != select)
    {
      warning ("Invalid args to selection_replace!  gdisp->select != select");
      return;
    }

  /*  swap the items in the selection  */
  selection_swap (select, new_select);

  /*  paste the selection's float buf to the gimage  */
  selection_paste_floating (gdisp->select, (void *) gdisp->gimage, BLEND_STENCIL);

  /* update the gdisplays  */
  /*  Find the bounding box of the new selection for pasting  */
  selection_find_bounds (select, &x1, &y1, &x2, &y2);

  /*  possibly modify the paste bounding box based on the former selection  */
  if (selection_find_bounds (new_select, &x3, &y3, &x4, &y4))
    {
      x1 = MINIMUM (x1, x3);
      y1 = MINIMUM (y1, y3);
      x2 = MAXIMUM (x2, x4);
      y2 = MAXIMUM (y2, y4);
    }

  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1,
		   (x2 - x1 + sm), (y2 - y1 + sm), 0);
  
  /*  If we can't push this paste operation onto the undo stack,
      we must delete the old gdisplay select structure           */
  if (! undo_push_paste (gdisp->ID, new_select))
    selection_generic_free (new_select);

}


void
selection_select_all (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  int i;

  /*  Clear up the current debris  */
  selection_clear (select, (void *) gdisp_ptr);

  for (i = 0; i < select->region->extent; i++)
    gregion_add_segment (select->region, 0, i, select->region->width, 255);

  selection_start (select, 0, True);
}


void
selection_select_none (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  selection_clear (select, (void *) gdisp_ptr);
  selection_start (select, 0, True);
}


void
selection_invert (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  selection_anchor (select, gdisp_ptr);
  gregion_invert (select->region);
  selection_start (select, 0, True);
}



void
selection_sharpen (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  GDisplay * gdisp;

  gdisp = (GDisplay *) gdisp_ptr;
  selection_invis (select);
  
  /*  push the current selection onto the stack  */
  undo_push_region (gdisp->ID, select->region);

  select->region = gregion_sharpen (select->region);

  selection_start (select, 0, True);
}


void
selection_hide (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  selection_invis (select);

  /*  toggle the visibility  */
  select->hidden = select->hidden ? False : True;

  selection_start (select, 0, True);
}


void
selection_from_gdisplay (select, gdisp_ptr, gimage_ptr)
     Selection * select;
     void * gdisp_ptr;
     void * gimage_ptr;  /*  new selection mask  */
{
  MaskBuf * mask;

  mask = gdisplay_to_temp_buf ((GImage *) gimage_ptr);

  if (mask)
    {
      selection_clear (select, gdisp_ptr);
      mask_convert_to_region (mask, select->region);
      selection_start (select, 0, True);
      mask_buf_free (mask);
    }
}


void
selection_to_gdisplay (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  MaskBuf * mask;

  /*  this anchors the selection, freeing the current floating buffer
   *  This might be changed in the future, because it seems like it would be
   *  somewhat useful to have this floating buffer if selection_from_gdisplay
   *  is later called by the user...In any case, you can get the floating
   *  selection back by undo'ing since anchor pushes to the undo stack.
   */
  selection_anchor (select, gdisp_ptr);
  selection_start (select, 0, True);
  /*  Make sure the selection is started...Otherwise it pauses until the window is placed  */
  XSync (DISPLAY, False);

  mask = mask_convert_from_region ((void *) select->region, FALSE);

  if (mask)
    {
      temp_buf_to_gdisplay (mask);
      mask_buf_free (mask);
    }
}


void
selection_set_opacity (select, new_opacity)
     Selection * select;
     double new_opacity;
{
  long value;
  
  select->opacity = new_opacity;

  /*  if the dialog is up, change the slider value  */
  if (select->dlg)
    {
      value = (long) (1000 * new_opacity);
      dialog_change_item (select->dlg, select->opacity_ID, &value);
    }
}


void
selection_opacity_dialog (select, gdisp_ptr)
     Selection * select;
     void * gdisp_ptr;
{
  int frame_ID;
  static long opacity_data[4] = { 0, 1000, 1000, 1 };

  if (!select->dlg)
    {
      select->dlg = dialog_new ("Paste Opacity", opacity_dialog_callback, select);
      
      frame_ID = dialog_new_item (select->dlg, 0, ITEM_FRAME, "Opacity", NULL);

      select->old_opacity = select->opacity;
      opacity_data[2] = (long) (select->opacity * 1000);
      select->opacity_ID = dialog_new_item (select->dlg, frame_ID, ITEM_SCALE, opacity_data, NULL);

      dialog_show (select->dlg);
    }
}


void
selection_free (select)
     Selection * select;
{
  if (select->state != INVISIBLE)
    XtRemoveTimeOut (select->timer);

  XFreeGC (DISPLAY, select->gc);
  selection_free_xelems (select);
  gregion_free (select->region);
  if (select->float_buf)
    temp_buf_free (select->float_buf);
  if (select->orig_buf)
    temp_buf_free (select->orig_buf);
  if (select->dlg)
    dialog_close (select->dlg);
  xfree (select);
}


static void
opacity_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  Selection * select;
  GDisplay * gdisp;
  float previous;

  select = (Selection *) client_data;

  switch (item_ID)
    {
    case OK_ID:
      dialog_close (select->dlg);
      select->dlg = NULL;
      break;
    case CANCEL_ID:
      select->opacity = select->old_opacity;
      dialog_close (select->dlg);
      select->dlg = NULL;
      break;
    default:
      if (item_ID == select->opacity_ID)
	{
	  previous = select->opacity;
	  select->opacity = *((long*) call_data) / 1000.0;
	  gdisp = (GDisplay *) select->gdisp;
	  /*  Translating the selection by no amount has the effect we desire here:
	   *  namely to cut the selected region and paste it back down
	   *  Also, add an undo step...
	   */
	  selection_translate (select, select->offset_x, select->offset_y,
			       select->offset_x, select->offset_y, gdisp);
	  undo_push_change_opacity (gdisp->ID, previous, select->opacity, select->fresh_cut);
	}
      break;
    }
}


