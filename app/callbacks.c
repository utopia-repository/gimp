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
#include "gdisplay_ops.h"
#include "gdither.h"
#include "callbacks.h"
#include "colormaps.h"
#include "general.h"
#include "global_edit.h"
#include "info_window.h"
#include "palette.h"
#include "tools.h"
#include "view_ops.h"
#include "widget.h"
#include "undo.h"

extern Widget interface;

typedef struct {
  int width_ID;
  int height_ID;
  int rgb_ID;
  int gray_ID;
  int width, height;
  int type;
  AutoDialog dlg;
} NewImageValues;


/*  local functions  */
static void new_image_callback (int, int, void *, void *);
static void select_gdisplay_callback (int, int, void *, void *);

/*  static variables  */
static   AutoDialog         select_from_dlg;
static   int                image_menu_ID;
static   GImage *           from_gimage;
static   int                last_width = 256;
static   int                last_height = 256;
static   int                last_type = RGB_GIMAGE;


void
install_callbacks (w)
     Widget w;
{
}


/*****************************************************/


void
new_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;
  NewImageValues *vals;
  char buffer[16];
  int main_ID;
  int group_ID;
  int frame_ID;
  int label_ID;
  long state;
  
  gdisp = gdisplay_active (w);
  vals = xmalloc (sizeof (NewImageValues));
  vals->dlg = dialog_new ("New Image", new_image_callback, vals);
  if (gdisp)
    {
      vals->width = gdisp->gimage->width;
      vals->height = gdisp->gimage->height;
      vals->type = gdisp->gimage->type;
    }
  else
    {
      vals->width = last_width;
      vals->height = last_height;
      vals->type = last_type;
    }

  dialog_new_item (vals->dlg, 0, ITEM_LABEL, "New Image", 0);
  main_ID = dialog_new_item (vals->dlg, 0, GROUP_ROWS, 0, 0);

  sprintf (buffer, "%d", vals->width);
  group_ID = dialog_new_item (vals->dlg, main_ID, GROUP_COLUMNS, 0, 0);
  label_ID = dialog_new_item (vals->dlg, group_ID, ITEM_LABEL, "Width", 0);
  vals->width_ID = dialog_new_item (vals->dlg, group_ID, ITEM_TEXT, buffer, 0);

  sprintf (buffer, "%d", vals->height);
  group_ID = dialog_new_item (vals->dlg, main_ID, GROUP_COLUMNS, 0, 0);
  label_ID = dialog_new_item (vals->dlg, group_ID, ITEM_LABEL, "Height", 0);
  vals->height_ID = dialog_new_item (vals->dlg, group_ID, ITEM_TEXT, buffer, 0);

  frame_ID = dialog_new_item (vals->dlg, main_ID, ITEM_FRAME, "Image Type", 0);
  group_ID = dialog_new_item (vals->dlg, frame_ID, GROUP_ROWS | GROUP_RADIO, 0, 0);
  vals->rgb_ID = dialog_new_item (vals->dlg, group_ID, ITEM_RADIO_BUTTON, "RGB", 0);
  vals->gray_ID = dialog_new_item (vals->dlg, group_ID, ITEM_RADIO_BUTTON, "Grayscale", 0);

  state = 1;
  switch (vals->type)
    {
    case RGB_GIMAGE:
      dialog_change_item (vals->dlg, vals->rgb_ID, &state);
      break;
    case GREY_GIMAGE:
      dialog_change_item (vals->dlg, vals->gray_ID, &state);
      break;
    }
  
  dialog_show (vals->dlg);
}


/*****************************************************/


void
quit_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  app_exit (0);
}


/*****************************************************/


void
named_cut_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  named_edit_cut (gdisp);
}


/*****************************************************/


void
named_copy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  named_edit_copy (gdisp);
}


/*****************************************************/


void
named_paste_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  named_edit_paste (gdisp);
}


/*****************************************************/


void
edit_cut_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  global_edit_cut (gdisp);
}


/*****************************************************/


void
edit_copy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  global_edit_copy (gdisp);
}


/*****************************************************/


void
edit_paste_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  global_edit_paste (gdisp);
}


/*****************************************************/


void
edit_clear_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;
  unsigned char r, g, b;

  gdisp = gdisplay_active (w);

  palette_get_background (&r, &g, &b);

  /*  push the image to the undo stack... */
  undo_push_image (gdisp->gimage->ID, 0, 0, 
		   gdisp->gimage->width, gdisp->gimage->height);
  gimage_fill (gdisp->gimage, r, g, b);
  gdisplay_paint (gdisp);
}


/*****************************************************/


void
edit_undo_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  if (!undo_pop ())
    XBell (DISPLAY, 0);
}


/*****************************************************/


void
edit_redo_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  if (!undo_redo ())
    XBell (DISPLAY, 0);
}


/*****************************************************/


void
select_all_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_select_all (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void 
select_none_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_select_none (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void 
select_anchor_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_anchor (gdisp->select, (void *) gdisp);
  selection_start (gdisp->select, 0, True);
}


/*****************************************************/


void 
select_sharpen_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_sharpen (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void 
select_invert_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_invert (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void 
select_hide_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_hide (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void 
select_opacity_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  selection_opacity_dialog (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void
select_from_gdisp_callback  (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;
  long data[32];
  char *p;

  gdisp = gdisplay_active (w);
  if (gdisp->gimage->type == GREY_GIMAGE)
    from_gimage = gdisp->gimage;
  else
    from_gimage = NULL;

  select_from_dlg = dialog_new ("Source Mask", select_gdisplay_callback, (void *) gdisp);
      
  /*  heinous kludge to get autodialog image menus working  */
  memcpy (data, "Masks", 6);
  data[31] = 0;
  p = (char*) &data[30];
  p[2] = 0;
  p[3] = IMAGE_CONSTRAIN_GRAY;

  image_menu_ID = dialog_new_item (select_from_dlg, 0, ITEM_IMAGE_MENU, data, gdisp->gimage);

  dialog_show (select_from_dlg);
}


/*****************************************************/


void
select_to_gdisp_callback  (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);
  selection_to_gdisplay (gdisp->select, (void *) gdisp);
}


/*****************************************************/


void
edit_options_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;

  gdisp = gdisplay_active (w);

  edit_options ((void *) gdisp);
}


/*****************************************************/


void
select_tool_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmPushButtonCallbackStruct * pbc;
  int tool_type;

  pbc = (XmPushButtonCallbackStruct *) call_data;
  tool_type = (int) client_data;

  if (pbc->click_count > 1)
    tools_options (tool_type);

  tools_select (tool_type);
}


/*****************************************************/


void
create_new_view_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;

  gdisp = gdisplay_active (w);

  gdisplay_new_view (gdisp);
}


/*****************************************************/


void
window_info_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay * gdisp;
  Widget * dialog;

  gdisp = gdisplay_active (w);

  if (gdisp)
    dialog = &gdisp->window_info_dialog;

  if (!*dialog)
    *dialog = create_info_window ((XtPointer) gdisp);

  XtPopup (*dialog, XtGrabNone);
}


/*****************************************************/


void
close_window_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;

  gdisp = gdisplay_active (w);

  gdisplay_close_window (gdisp);
}


/*****************************************************/


void
shrink_wrap_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;
  
  gdisp = gdisplay_active (w);

  if (gdisp)
    gdisplay_shrink_wrap (gdisp);
  
}


/*****************************************************/


void
view_options_dialog_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;
  Widget *dialog;

  gdisp = gdisplay_active (w);

  if (gdisp)
    dialog = &gdisp->view_options_dialog;

  if (!*dialog)
    *dialog = create_view_options_dialog ((XtPointer) gdisp);

  XtPopup (*dialog, XtGrabNone);
}


/*****************************************************/


void
brush_dialog_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  create_brush_dialog ();
}


/*****************************************************/


void
palette_dialog_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  palette_create ();
}


/****************************************************/
/**           LOCAL FUNCTIONS                      **/
/****************************************************/


static void
new_image_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  NewImageValues *vals;
  GImage *gimage;
  GDisplay *gdisplay;
  unsigned char r, g, b;
  int bpp;

  vals = client_data;
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (vals->dlg);

      switch (vals->type)
	{
	case RGB_GIMAGE:
	  bpp = 3;
	  break;
	case GREY_GIMAGE:
	  bpp = 1;
	  break;
	}
      
      if ((vals->width > 0) && (vals->height > 0))
	{
	  /*  update the last dimensions used  */
	  last_width = vals->width;
	  last_height = vals->height;
	  last_type = vals->type;

	  palette_get_background (&r, &g, &b);
	  gimage = gimage_new (vals->width, vals->height, vals->type, bpp);
	  gimage->dirty = 0;
	  
	  gimage_fill (gimage, r, g, b);
	  gdisplay = gdisplay_gimage (gimage, 0x0101);
	  gdisplay_paint (gdisplay);
	}
			   
      xfree (vals);
      break;
    case CANCEL_ID:
      dialog_close (vals->dlg);
      xfree (vals);
      break;
    default:
      if (item_ID == vals->width_ID)
	{
	  vals->width = atoi (call_data);
	}
      else if (item_ID == vals->height_ID)
	{
	  vals->height = atoi (call_data);
	}
      else if (item_ID == vals->rgb_ID)
	{
	  if (*((long*) call_data))
	    vals->type = RGB_GIMAGE;
	}
      else if (item_ID == vals->gray_ID)
	{
	  if (*((long*) call_data))
	    vals->type = GREY_GIMAGE;
	}

      break;
    }
}


static void
select_gdisplay_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  int gimage_ID;
  GDisplay * gdisp;

  switch (item_ID)
    {
    case OK_ID:
      dialog_close (select_from_dlg);
      if (from_gimage)
	{
	  gdisp = (GDisplay *) client_data;
	  selection_from_gdisplay (gdisp->select, gdisp, (void *) from_gimage);
	}
      break;
    case CANCEL_ID:
      dialog_close (select_from_dlg);
      break;
    default:
      if ((item_ID == image_menu_ID) || (item_ID == -1))
	{
	  gimage_ID = (int) *((long *) call_data);
	  from_gimage = (gimage_ID) ? gimage_get_ID (gimage_ID) : ((GDisplay*) client_data)->gimage;
	}
      break;
    }
}
