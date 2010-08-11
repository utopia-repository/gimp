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
#include <string.h>
#include <X11/IntrinsicP.h>
#include "appenv.h"
#include "colormaps.h"
#include "cursorutil.h"
#include "disp-callbacks.h"
#include "fileops.h"
#include "gconvert.h"
#include "gdisplay_ops.h"
#include "gimage.h"
#include "gximage.h"
#include "scale.h"
#include "widget.h"

/*  a macro for finding the intensity of a pixel  */
#define INTENSITY(r,g,b) (r * 0.30 + g * 0.59 + b * 0.11)

/*  arrays that need to be accessed by the qsort function  */
int system_intensity[256];
int indexed_intensity[256];

/*  function prototypes for sort compare functions  */
int system_index_compare (const void *, const void *);
int indexed_index_compare (const void *, const void *);

/*  global done variable for modal system dialog  */
static int done;

/*
 *  This file is for operations on the gdisplay object
 */   

Pixel
gdisplay_black_pixel (gdisp)
     GDisplay * gdisp;
{
  Pixel p;
  float val, min;
  int i;

  /*  set foreground and background colors  */
  if (gdisp->depth == 8 && gdisp->gimage->type == INDEXED_GIMAGE)
    {
      /*  Need to find the closest color to black possible  */
      p = 0;
      min = INTENSITY (gdisp->gimage->cmap [0],
		       gdisp->gimage->cmap [1],
		       gdisp->gimage->cmap [2]);
      for (i = 1; i < gdisp->gimage->num_cols; i++)
	{
	  val = INTENSITY (gdisp->gimage->cmap [i * 3],
			   gdisp->gimage->cmap [i * 3 + 1],
			   gdisp->gimage->cmap [i * 3 + 2]);
	  if (val < min)
	    {
	      min = val;
	      p = i;
	    }
	}

      return p;
    }
  if (gdisp->gimage->type == RGB_GIMAGE || gdisp->gimage->type == INDEXED_GIMAGE)
    return color_black_pixel;
  else if (gdisp->gimage->type == GREY_GIMAGE)
    return grey_black_pixel;

  return 0;
}


Pixel
gdisplay_white_pixel (gdisp)
     GDisplay * gdisp;
{
  Pixel p;
  float val, max;
  int i;

  /*  set foreground and background colors  */
  if (gdisp->depth == 8 && gdisp->gimage->type == INDEXED_GIMAGE)
    {
      /*  Need to find the closest color to white possible  */
      p = 0;
      max = INTENSITY (gdisp->gimage->cmap [0],
		       gdisp->gimage->cmap [1],
		       gdisp->gimage->cmap [2]);
      for (i = 1; i < gdisp->gimage->num_cols; i++)
	{
	  val = INTENSITY (gdisp->gimage->cmap [i * 3],
			   gdisp->gimage->cmap [i * 3 + 1],
			   gdisp->gimage->cmap [i * 3 + 2]);
	  if (val > max)
	    {
	      max = val;
	      p = i;
	    }
	}

      return p;
    }
  if (gdisp->gimage->type == RGB_GIMAGE || gdisp->gimage->type == INDEXED_GIMAGE)
    return color_white_pixel;
  else if (gdisp->gimage->type == GREY_GIMAGE)
    return grey_white_pixel;

  return 1;
}


void
gdisplay_new_view (gdisp)
     GDisplay *gdisp;
{
  GImage *gimage;
  GDisplay *new_gdisp;

  /* make sure the image has been fully loaded... */
  if (!gdisp->gimage->busy)
    {
      gimage = gdisp->gimage;
      
      if (gimage)
	{
	  new_gdisp = gdisplay_gimage (gimage, gdisp->scale);

	  new_gdisp->dither_type   = gdisp->dither_type;
/*
	  new_gdisp->scale         = gdisp->scale;
	  new_gdisp->offset_x      = gdisp->offset_x;
	  new_gdisp->offset_y      = gdisp->offset_y;
*/

	  /*  Make sure the scale setup is correct  */
/*	  resize_display (new_gdisp); */

	  gdisplay_paint (new_gdisp);
	}
    }
  /* complain if the image isn't fully loaded yet */
  else
    XBell (DISPLAY, 0);

}


void
gdisplay_close_window (gdisp)
     GDisplay *gdisp;
{
  Widget shell;
  
  shell = gdisp->disp_image->shell;

  /*  If the image has been modified, give the user a chance to save
   *  it before nuking it--this only applies if its the last view
   *  to an image canvas.  (a gimage with ref_count = 1)
   */
  if (gdisp->gimage->ref_count == 1 && gdisp->gimage->dirty > 0)
    {
      int result;

      result = save_before_closing_dialog ("Image X");
      if (result == -1)
	return;
      else if (result == 1)
	{
	  file_save_callback (gdisp->shell, NULL, NULL);
	}
    }

  XtVaSetValues (get_top_shell (gdisp->disp_image->canvas),
		 XmNuserData, NULL,
		 NULL);

  gdisplay_remove_and_delete (gdisp);

  XtDestroyWidget (shell);
}


void
gdisplay_shrink_wrap (gdisp)
     GDisplay *gdisp;
{
  long width, height;
  Dimension vsb_width, hsb_height;
  Dimension shell_x, shell_y;
  Dimension shadow, spacing;
  Dimension x, y;
  int s_width, s_height;

  s_width = WidthOfScreen (XtScreen (toplevel));
  s_height = HeightOfScreen (XtScreen (toplevel));

  width = SCALE (gdisp, gdisp->gimage->width);
  height = SCALE (gdisp, gdisp->gimage->height);

  if (width < s_width && height < s_height)
    {
      XtVaGetValues (XtParent (gdisp->disp_image->canvas),
		     XmNspacing, &spacing,
		     NULL);
      XtVaGetValues (XtParent (gdisp->disp_image->canvas), 
		     XmNshadowThickness, &shadow,
		     NULL);
      XtVaGetValues (gdisp->hsb,
		     XtNheight, &hsb_height,
		     NULL);
      XtVaGetValues (gdisp->vsb,
		     XtNwidth, &vsb_width,
		     NULL);

      width += (spacing*2 + shadow*2 + vsb_width);
      height += (spacing*2 + shadow*2 + hsb_height);

      XtVaGetValues (gdisp->disp_image->shell,
		     XtNx, &shell_x,
		     XtNy, &shell_y,
		     NULL);
      
      x = HIGHPASS (shell_x, bounds (s_width - width, 0, s_width));
      y = HIGHPASS (shell_y, bounds (s_height - height, 0, s_height));
      
      XtConfigureWidget (gdisp->disp_image->shell,
			 x, y, width, height,
			 0);
    }
  else
    XBell (DISPLAY, 0);
}


Boolean
gdisplay_resize_image (gdisp)
     GDisplay *gdisp;
{
  Dimension win_width, win_height;
  Dimension hsb_height, vsb_width;
  Dimension spacing, border, ht;
  Dimension sx, sy;
  int width, height;
  int offset_x, offset_y;

  /*  set the shadow thickness for the manager widget to 1  */
  ht = 1;

  /*  find the maximum size available for the work area  */
  XtVaGetValues (gdisp->hsb,
		 XmNheight, &hsb_height,
		 NULL);
  XtVaGetValues (gdisp->vsb,
		 XmNwidth, &vsb_width,
		 NULL);
  XtVaGetValues (XtParent (gdisp->disp_image->canvas),
                 XmNwidth, &win_width,
                 XmNheight, &win_height,
		 XmNborderWidth, &border,
		 XmNspacing, &spacing,
                 NULL);

  win_width  -= (vsb_width + (spacing) * 2);
  win_height -= (hsb_height + (spacing) * 2);

  /*  Calculate the width and height of the new canvas  */
  sx = SCALE (gdisp, gdisp->gimage->width);
  sy = SCALE (gdisp, gdisp->gimage->height);
  width = HIGHPASS (sx, win_width);
  height = HIGHPASS (sy, win_height);

  gdisplay_remove_processes (gdisp);

  /*  calculate the offset from the scrolled window to the work area window  */
  offset_x = ((win_width - width) >> 1) + ht;
  offset_y = ((win_height - height) >> 1) + ht;

  /* if the new dimensions of the ximage are different than the old...resize */
  if (width != gdisp->disp_image->width || height != gdisp->disp_image->height)
    {
      /*  adjust the gdisplay offsets -- we need to set them so that the
       *  center of our viewport is at the center of the image.
       */
      gdisp->offset_x = (sx / 2) - (width / 2);
      gdisp->offset_y = (sy / 2) - (height / 2);

      resize_image (gdisp->disp_image, width, height);

      /*  Clear the window for a cleaner redraw  */
      XClearWindow (DISPLAY, XtWindow (gdisp->disp_image->canvas));
    }

  XtConfigureWidget (gdisp->disp_image->canvas,
		     offset_x, offset_y,
		     width, height, border);

  return True;
}


/*  match an indexed colormap to the system colormap as closely as possible  */

void
gdisplay_fit_colormap (xcmap, cmap, num_cols)
     Colormap xcmap;
     unsigned char * cmap;
     int num_cols;
{
  int i, index;
  Colormap sys_cmap;
  int system_index[256];
  int indexed_index[256];
  XColor indexed_pal[256];
  XColor system_pal[256];

  sys_cmap = DefaultColormapOfScreen (XtScreen (toplevel));
  XQueryColors (DISPLAY, sys_cmap, system_pal, 256);

  for (i = 0; i < 256; i++)
    {
      system_intensity [i] = INTENSITY ((system_pal [i].red >> 8),
					(system_pal [i].green >> 8),
					(system_pal [i].blue >> 8));
      indexed_intensity [i] = INTENSITY (cmap [i * 3],
					 cmap [i * 3 + 1],
					 cmap [i * 3 + 2]);
      system_index[i] = i;
      indexed_index[i] = i;
    }

  /*  sort the two index arrays to reflect increasing intensities  */
  qsort (system_index, 256, sizeof (int), system_index_compare);
  qsort (indexed_index, 256, sizeof (int), indexed_index_compare);

  for (i = 0; i < 256; i++)
    {
      index = system_index [indexed_index [i]];
      indexed_pal[i].pixel = index;
      indexed_pal[i].red = ((int) cmap[index*3]) << 8;
      indexed_pal[i].green = ((int) cmap[index*3 + 1]) << 8;
      indexed_pal[i].blue = ((int) cmap[index*3 + 2]) << 8;
      indexed_pal[i].flags = DoRed | DoGreen | DoBlue;
    }
      
  XStoreColors (DISPLAY, xcmap, indexed_pal, 256);
}

int system_index_compare (el1, el2)
     const void * el1;
     const void * el2;
{
  int e1, e2;
  
  e1 = *((int *) el1);
  e2 = *((int *) el2);

  if (system_intensity [e1] < system_intensity [e2])
    return -1;
  else if (system_intensity [e1] == system_intensity [e2])
    return 0;
  else
    return 1;
}

     
int indexed_index_compare (el1, el2)
     const void * el1;
     const void * el2;
{
  int e1, e2;
  
  e1 = *((int *) el1);
  e2 = *((int *) el2);

  if (indexed_intensity [e1] < indexed_intensity [e2])
    return -1;
  else if (indexed_intensity [e1] == indexed_intensity [e2])
    return 0;
  else
    return 1;

}

     
/*  convert temp buffers to gdisplays & vice-versa  */


int
temp_buf_to_gdisplay (buf)
     TempBuf * buf;
{
  /*  This function creates the new gdisplay  */
  if (buf->width && buf->height)
  {
    GImage * new_gimage;
    GDisplay * new_gdisp;
    int type;
    
    type = (buf->bytes == 3) ? RGB_GIMAGE : GREY_GIMAGE;

    new_gimage = gimage_new (buf->width, buf->height, type, buf->bytes);
    
    /*  copy the src image to the dest image  */
    memcpy (new_gimage->raw_image, temp_buf_data (buf), buf->height * buf->width * buf->bytes);

    /*  create the new gdisplay  */
    new_gdisp = gdisplay_gimage (new_gimage, 0x0101);
    
    /*  paint the new gdisplay  */
    gdisplay_paint (new_gdisp);

    return 1;
  }

  return 0;
}


/*  IMPORTANT:  this takes a GImage, not a GDisplay  */

TempBuf *
gdisplay_to_temp_buf (gimage)
     GImage * gimage;
{
  TempBuf * buf;

  buf = temp_buf_load (NULL, (void *) gimage, 0, 0, 
		       gimage->width, gimage->height);

  return buf;
}



/********************************************************
 *   Routines to query before closing a dirty image     *
 ********************************************************/

static void
save_before_closing_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  int * done;
  XmSelectionBoxCallbackStruct * cbs = 
    (XmSelectionBoxCallbackStruct *) call_data;

  done = (int *) client_data;

  *done = cbs->reason;
}


int
save_before_closing_dialog (image_name)
     char * image_name;
{
  Widget dialog;
  XmString warning;
  XmString yes, no, cancel;
  char * warning_buf;

  done = 0;

  warning_buf = (char *) xmalloc (strlen (image_name) + 50);
  sprintf (warning_buf, "Changes made to %s.  Save before closing?", image_name);

  /*  Create the dialog box to ask whether image should be saved  */
  warning = XmStringCreateLocalized (warning_buf);
  yes = XmStringCreateLocalized ("Yes");
  no = XmStringCreateLocalized ("No");
  cancel = XmStringCreateLocalized ("Cancel");
  dialog = XmCreateWarningDialog (toplevel, "Warning!", NULL, 0);
  XtVaSetValues (dialog, XmNmessageString, warning, NULL);
  XtVaSetValues (XmMessageBoxGetChild (dialog, XmDIALOG_OK_BUTTON),
		 XmNlabelString, yes, NULL);
  XtVaSetValues (XmMessageBoxGetChild (dialog, XmDIALOG_CANCEL_BUTTON),
		 XmNlabelString, no, NULL);
  XtVaSetValues (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON),
		 XmNlabelString, cancel, NULL);
  XtVaSetValues (dialog,
		 XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		 XmNdefaultPosition, False, NULL);
  XtAddCallback (dialog, XmNokCallback, save_before_closing_callback, (XtPointer) &done);
  XtAddCallback (dialog, XmNcancelCallback, save_before_closing_callback, (XtPointer) &done);
  XtAddCallback (dialog, XmNhelpCallback, save_before_closing_callback, (XtPointer) &done);
  XtAddCallback (dialog, XmNmapCallback, map_dialog, NULL);
  XmStringFree (warning);
  XmStringFree (yes);
  XmStringFree (no);
  XmStringFree (cancel);

  XtManageChild (dialog);
  XtPopup (XtParent (dialog), XtGrabNone);

  while (done == 0)
    XtAppProcessEvent (app_context, XtIMAll);

  XtPopdown (XtParent (dialog));

  xfree (warning_buf);

  switch (done)
    {
    case XmCR_OK :
      return 1;
      break;
    case XmCR_CANCEL :
      return 0;
      break;
    case XmCR_HELP :
      return -1;
      break;
    default :
      return 0;
    }
}

