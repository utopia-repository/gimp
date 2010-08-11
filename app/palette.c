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
#include "actionarea.h"
#include "buildmenu.h"
#include "colormaps.h"
#include "errors.h"
#include "palette.h"
#include "visual.h"

#define SQUARE_SIZE 15
#define SPACING 3
#define COLUMNS 15

#define PREVIEW_WIDTH ((SQUARE_SIZE * COLUMNS) + (SPACING * (COLUMNS + 1)))
#define PREVIEW_HEIGHT PREVIEW_WIDTH

#define COLOR_TEXT_WIDTH 75

static void palette_change_color (int, int, int, int, int);
static void palette_destroy_callback (Widget, XtPointer, XtPointer);
static void palette_expose_callback (Widget, XtPointer, XtPointer);
static void palette_scroll_callback (Widget, XtPointer, XtPointer);
static void palette_color_expose_callback (Widget, XtPointer, XtPointer);
static void palette_menu_callback (Widget, XtPointer, XtPointer);
static void palette_text_callback (Widget, XtPointer, XtPointer);
static void palette_new_callback (Widget, XtPointer, XtPointer);
static void palette_delete_callback (Widget, XtPointer, XtPointer);
static void palette_edit_callback (Widget, XtPointer, XtPointer);
static void palette_close_callback (Widget, XtPointer, XtPointer);
static void palette_select_callback (int, int, int, int);

static void palette_preview_button_press (Widget, XtPointer, XEvent *, Boolean *);
static void palette_entry_button_press (Widget, XtPointer, XEvent *, Boolean *);

static void palette_draw_entries (PaletteP);
static void palette_draw_current_entry (PaletteP);
static void palette_draw_preview (PaletteP);
static void palette_draw_text (PaletteP);

static PaletteEntryP palette_add_entry (PaletteP, int, int, int);
static void palette_add_standard_entries (PaletteP);
static void palette_delete_entry (PaletteP);
static void palette_calc_scrollbar (PaletteP);

static PaletteP palette = NULL;
static link_ptr entries = NULL;

static MenuItem option_items[] = 
{
  { "Foreground", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      palette_menu_callback, (void*) FOREGROUND, NULL, NULL },
  { "Background", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      palette_menu_callback, (void*) BACKGROUND, NULL, NULL },
  { NULL },
};

static ActionAreaItem action_items[] = 
{
  { "New", palette_new_callback, NULL, NULL },
  { "Edit", palette_edit_callback, NULL, NULL },
  { "Delete", palette_delete_callback, NULL, NULL },
  { "Close", palette_close_callback, NULL, NULL },
};

void
palette_create ()
{
  extern Widget interface;
  Widget scrolled_widget;
  Widget frame_widget;
  Widget colors_frame_widget;
  Widget option_menu;
  Widget form_widget;
  Widget label_widget[3];
  Widget action_widget;
  char *labels[] = { "Red", "Green", "Blue" };
  XGCValues gcv;
  Dimension width, height;
  int i;
  
  if (!palette)
    {
      palette = xmalloc (sizeof (_Palette));

      palette->entries = entries;
      palette->n_entries = 0;
      palette->color[0] = NULL;
      palette->color[1] = NULL;
      palette->color_select = NULL;
      palette->color_select_active = 0;
      palette->scroll_offset = 0;
      palette->color_state = FOREGROUND;

      palette->shell = XtVaCreatePopupShell ("paletteDialog",
/*					     xmDialogShellWidgetClass, toplevel, */
					     topLevelShellWidgetClass, interface,
					     XmNtitle, "Color Palette",
					     XmNvisual, color_visual,
					     XmNcolormap, get_colormap (RGB_GIMAGE),
					     XmNdepth, color_depth,
					     XmNdeleteResponse, XmDESTROY,
					     NULL);
      
      palette->dialog = XtVaCreateWidget ("dialog",
					  xmFormWidgetClass, palette->shell,
					  NULL);

      scrolled_widget = XtVaCreateWidget ("scrolledColorPreview",
					  xmScrolledWindowWidgetClass, palette->dialog,
					  XmNscrollingPolicy, XmAPPLICATION_DEFINED,
					  XmNvisualPolicy, XmVARIABLE,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNleftOffset, 10,
					  XmNtopOffset, 10,
					  NULL);

      frame_widget = XtVaCreateManagedWidget ("previewFrame",
					      xmFrameWidgetClass, scrolled_widget,
					      XmNshadowType, XmSHADOW_IN,
					      XmNshadowThickness, 2,
					      NULL);
      
      palette->color_area = XtVaCreateManagedWidget ("previewColorArea",
						     xmDrawingAreaWidgetClass, frame_widget,
						     XmNwidth, PREVIEW_WIDTH,
						     XmNheight, PREVIEW_HEIGHT,
						     NULL);
      XtAddCallback (palette->color_area, XmNexposeCallback, palette_expose_callback, palette);
      XtAddEventHandler (palette->color_area, ButtonPressMask, False,
			 palette_entry_button_press, palette);

      palette->scroll_bar = XtVaCreateManagedWidget ("scrollBar",
						     xmScrollBarWidgetClass, scrolled_widget,
						     XmNorientation, XmVERTICAL,
						     XmNminimum, 0,
						     XmNmaximum, 1,
						     XmNvalue, 0,
						     XmNsliderSize, 1,
						     XmNpageIncrement, 1,
						     NULL);
      XtAddCallback (palette->scroll_bar, XmNdragCallback, 
		     palette_scroll_callback, palette);
      XtAddCallback (palette->scroll_bar, XmNvalueChangedCallback, 
		     palette_scroll_callback, palette);

      XmScrolledWindowSetAreas (scrolled_widget,
				NULL,
				palette->scroll_bar,
				palette->color_area);
      XtManageChild (scrolled_widget);

      form_widget = XtVaCreateWidget ("dialogPartition",
				      xmFormWidgetClass, palette->dialog,
				      XmNleftAttachment, XmATTACH_WIDGET,
				      XmNleftWidget, scrolled_widget,
				      XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				      XmNbottomWidget, scrolled_widget,
				      XmNrightAttachment, XmATTACH_FORM,
				      XmNleftOffset, 10,
				      XmNrightOffset, 10,
				      NULL);

      for (i = 0; i < 3; i++)
	{
	  palette->text_widget[i] = XtVaCreateWidget ("colorTextWidget",
						      xmTextFieldWidgetClass, form_widget,
						      XmNwidth, COLOR_TEXT_WIDTH,
						      XmNrightAttachment, XmATTACH_FORM,
						      XmNtopAttachment, 
						        (i == 0) ? XmATTACH_FORM : XmATTACH_WIDGET,
						      XmNtopWidget,
						        (i == 0) ? NULL : palette->text_widget[i-1],
						      XmNrightOffset, 0,
						      XmNtopOffset, (i == 0) ? 0 : 10,
						      NULL);
	  XtAddCallback (palette->text_widget[i], XmNvalueChangedCallback, 
			 palette_text_callback, palette);
	  
	  label_widget[i] = XtVaCreateWidget (labels[i], 
					      xmLabelWidgetClass, form_widget,
					      XmNleftAttachment, XmATTACH_FORM,
					      XmNrightAttachment, XmATTACH_WIDGET,
					      XmNrightWidget, palette->text_widget[i],
					      XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
					      XmNtopWidget, palette->text_widget[i],
					      XmNleftOffset, 0,
					      XmNrightOffset, 10,
					      NULL);

	  XtManageChild (label_widget[i]);
	  XtManageChild (palette->text_widget[i]);
	}

      XtManageChild (form_widget);

      option_menu = BuildMenu (palette->dialog, XmMENU_OPTION, "Color",
			       0, False, option_items,
			       DefaultVisualOfScreen (XtScreen (toplevel)),
			       DefaultColormapOfScreen (XtScreen (toplevel)),
			       DefaultDepthOfScreen (XtScreen (toplevel)));
      XtVaSetValues (option_menu,
		     XmNleftAttachment, XmATTACH_WIDGET,
		     XmNleftWidget, scrolled_widget,
		     XmNrightAttachment, XmATTACH_FORM,
		     XmNbottomAttachment, XmATTACH_WIDGET,
		     XmNbottomWidget, form_widget,
		     XmNleftOffset, 10,
		     XmNrightOffset, 10,
		     XmNbottomOffset, 10,
		     NULL);
      XtManageChild (option_menu);

      colors_frame_widget = XtVaCreateManagedWidget ("colorsFrame",
						     xmFrameWidgetClass, palette->dialog,
						     XmNshadowType, XmSHADOW_IN,
						     XmNshadowThickness, 2,
						     XmNleftAttachment, XmATTACH_WIDGET,
						     XmNleftWidget, scrolled_widget,
						     XmNtopAttachment, XmATTACH_FORM,
						     XmNrightAttachment, XmATTACH_FORM,
						     XmNbottomAttachment, XmATTACH_WIDGET,
						     XmNbottomWidget, option_menu,
						     XmNleftOffset, 10,
						     XmNtopOffset, 10,
						     XmNrightOffset, 10,
						     XmNbottomOffset, 10,
						     NULL);

      palette->preview_widget = XtVaCreateManagedWidget ("previewColor",
							 xmDrawingAreaWidgetClass, colors_frame_widget,
							 NULL);

      XtAddCallback (palette->preview_widget, XmNexposeCallback, 
		     palette_color_expose_callback, palette);
      XtAddEventHandler (palette->preview_widget, ButtonPressMask, False,
			 palette_preview_button_press, palette);

      action_items[0].data = palette;
      action_items[1].data = palette;
      action_items[2].data = palette;
      action_items[3].data = palette;
      action_widget = build_action_area (palette->dialog, action_items, 4);
      XtVaSetValues (action_widget,
		     XmNleftAttachment, XmATTACH_FORM,
		     XmNrightAttachment, XmATTACH_FORM,
		     XmNbottomAttachment, XmATTACH_FORM,
		     XmNtopAttachment, XmATTACH_WIDGET,
		     XmNtopWidget, scrolled_widget,
		     XmNleftOffset, 10,
		     XmNrightOffset, 10,
		     XmNtopOffset, 10,
		     XmNbottomOffset, 10,
		     NULL);

      XtAddCallback (palette->shell, XtNdestroyCallback, palette_destroy_callback, palette);
      XtVaSetValues (palette->dialog, XmNdefaultPosition, False, NULL);

      XtManageChild (action_widget);
      XtManageChild (palette->dialog);

      XtPopup (palette->shell, XtGrabNone);

      palette->alloc_colors = (color_depth == 8);
      if (palette->alloc_colors)
	{
	  Colormap cmap;
	  
	  XtVaGetValues (palette->preview_widget, XmNcolormap, &cmap, NULL);

	  if (!XAllocColorCells (DISPLAY, cmap, False, NULL, 0, &palette->preview_color.pixel, 1))
	    fatal_error ("unable to allocate 1 colormap entry");
	}
      
      XtVaGetValues (palette->preview_widget,
		     XtNwidth, &width,
		     XtNheight, &height,
		     NULL);
      palette->preview_pixmap = XCreatePixmap (DISPLAY, XtWindow (palette->preview_widget),
					       width, height, color_depth);
      
      XtVaGetValues (palette->color_area,
		     XtNwidth, &width,
		     XtNheight, &height,
		     NULL);
      palette->color_image = image_buf_create (1, COLOR_BUF, width, height);
      
      gcv.foreground = BlackPixelOfScreen (XtScreen (palette->color_area));
      palette->gc = XCreateGC (DISPLAY, XtWindow (palette->shell), GCForeground, &gcv);

      palette->updating = 1;
      if (!palette->entries)
	palette_add_standard_entries (palette);
      palette_calc_scrollbar (palette);
      palette_draw_text (palette);
      palette->updating = 0;
    }
  else
    {
      XtPopup (palette->shell, XtGrabNone);
    }
}

void
palette_free ()
{
  Colormap cmap;
  
  if (palette)
    {
      XtVaGetValues (palette->preview_widget, XmNcolormap, &cmap, NULL);
      XFreeColors (DISPLAY, cmap, &palette->preview_color.pixel, 1, 0);

      XtDestroyWidget (palette->dialog);
      XtDestroyWidget (palette->shell);
      XFreeGC (DISPLAY, palette->gc);
      XFreePixmap (DISPLAY, palette->preview_pixmap);
      image_buf_destroy (palette->color_image);

      if (palette->color_select)
	color_select_free (palette->color_select);

      entries = palette->entries;
      xfree (palette);

      palette = NULL;
    }
}

void
palette_get_foreground (r, g, b)
     unsigned char *r, *g, *b;
{
  if (palette)
    {
      *r = palette->color[FOREGROUND]->color[0];
      *g = palette->color[FOREGROUND]->color[1];
      *b = palette->color[FOREGROUND]->color[2];
    }
  else
    {
      *r = 0;
      *g = 0;
      *b = 0;
    }
}

void
palette_get_background (r, g, b)
     unsigned char *r, *g, *b;
{
  if (palette)
    {
      *r = palette->color[BACKGROUND]->color[0];
      *g = palette->color[BACKGROUND]->color[1];
      *b = palette->color[BACKGROUND]->color[2];
    }
  else
    {
      *r = 255;
      *g = 255;
      *b = 255;
    }
}

static void
palette_change_color (r, g, b, color, state)
     int r, g, b;
     int color;
     int state;
{
  if (palette)
    {
      switch (state)
	{
	case COLOR_NEW :
	  palette->color[color] = palette_add_entry (palette, r, g, b);
	  palette->updating = 1;
	  palette_draw_text (palette);
	  palette->updating = 0;
	  
	  palette_calc_scrollbar (palette);
	  palette_draw_preview (palette);
	  palette_draw_entries (palette);
	  palette_draw_current_entry (palette);
	  break;

	case COLOR_UPDATE :
	  palette->color[color]->color[0] = r;
	  palette->color[color]->color[1] = g;
	  palette->color[color]->color[2] = b;
	  palette_draw_preview (palette);
	  break;

	case COLOR_FINISH :
	  palette_draw_preview (palette);
	  palette_draw_text(palette);
	}
    }
}

void
palette_set_active_color (r, g, b, state)
     int r, g, b;
     int state;
{
  if (palette)
    palette_change_color (r, g, b, palette->color_state, state);
}

void 
palette_set_foreground (r, g, b, state)
     int r, g, b;
     int state;
{
  palette_change_color (r, g, b, FOREGROUND, state);
}

void 
palette_set_background (r, g, b, state)
     int r, g, b;
     int state;
{
  palette_change_color (r, g, b, BACKGROUND, state);
}

static void 
palette_destroy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  if (client_data == palette)
    palette_free ();
  else
    fatal_error ("sanity check failed: unexpected palette");
}

static void 
palette_expose_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;

  palette = client_data;
  if (palette)
    {
      palette_draw_entries (palette);
      palette_draw_current_entry (palette);
    }
}

static void 
palette_scroll_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;
  XmScrollBarCallbackStruct *cbs;
  
  palette = client_data;
  cbs = call_data;
  
  if (palette)
    {
      palette->scroll_offset = cbs->value;
      palette_draw_entries (palette);
      palette_draw_current_entry (palette);
    }
}

static void 
palette_color_expose_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  palette = client_data;

  if (palette)
    palette_draw_preview (palette);
}

static void 
palette_menu_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  unsigned char *color;
  
  if (palette)
    {
      palette->color_state = (int) client_data;

      if (palette->color_select_active)
	{
	  color = palette->color[palette->color_state]->color;
	  color_select_set_color (palette->color_select, color[0], color[1], color[2], 0);
	}

      palette->updating = 1;
      palette_draw_text (palette);
      palette->updating = 0;
      
      palette_calc_scrollbar (palette);
      palette_draw_preview (palette);
      palette_draw_entries (palette);
      palette_draw_current_entry (palette);
    }
}

static void
palette_text_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;
  unsigned char *color;
  char *str;
  int i;

  palette = client_data;
  if (palette)
    {
      color = palette->color[palette->color_state]->color;

      for (i = 0; i < 3; i++)
	if (palette->text_widget[i] == w)
	  break;

      if (i == 3)
	fatal_error ("sanity check failed: unexpected text widget");

      str = XmTextGetString (palette->text_widget[i]);
      color[i] = atoi (str);

      if (!palette->updating)
	{
	  palette_draw_preview (palette);
	  palette_draw_entries (palette);
	  palette_draw_current_entry (palette);
	}
    }
}

static void 
palette_new_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;
  unsigned char *color;

  palette = client_data;
  if (palette)
    {
      color = palette->color[palette->color_state]->color;
      palette->color[palette->color_state] = 
	palette_add_entry (palette, color[0], color[1], color[2]);
      palette_calc_scrollbar (palette);
      palette_draw_entries (palette);
      palette_draw_current_entry (palette);
    }
}

static void 
palette_delete_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;

  palette = client_data;
  if (palette)
    palette_delete_entry (palette);
}

static void 
palette_edit_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;
  unsigned char *color;

  palette = client_data;
  if (palette)
    {
      color = palette->color[palette->color_state]->color;

      if (!palette->color_select)
	{
	  palette->color_select = color_select_new (color[0], color[1], color[2],
						    palette_select_callback);
	  palette->color_select_active = 1;
	}
      else
	{
	  if (!palette->color_select_active)
	    {
	      color_select_show (palette->color_select);
	      palette->color_select_active = 1;
	    }
	  
	  color_select_set_color (palette->color_select, color[0], color[1], color[2], 1);
	}
    }
}

static void 
palette_close_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PaletteP palette;
  
  palette = client_data;
  if (palette)
    {
      if (palette->color_select_active)
	{
	  palette->color_select_active = 1;
	  color_select_hide (palette->color_select);
	}
      XtPopdown (palette->shell);
    }
}

static void 
palette_select_callback (r, g, b, cancelled)
     int r, g, b;
     int cancelled;
{
  unsigned char *color;
  
  if (palette)
    {
      color_select_hide (palette->color_select);
      palette->color_select_active = 0;

      if (!cancelled)
	{
	  color = palette->color[palette->color_state]->color;
	  
	  color[0] = r;
	  color[1] = g;
	  color[2] = b;

	  palette->updating = 1;
	  palette_draw_text (palette);
	  palette->updating = 0;
	  
	  palette_calc_scrollbar (palette);
	  palette_draw_preview (palette);
	  palette_draw_entries (palette);
	  palette_draw_current_entry (palette);
	}
    }
}

static void 
palette_preview_button_press (w, client_data, event, continue_to_dispatch)
     Widget w;
     XtPointer client_data;
     XEvent *event;
     Boolean *continue_to_dispatch;
{
  palette_edit_callback (w, client_data, event);
}

static void 
palette_entry_button_press (w, client_data, event, continue_to_dispatch)
     Widget w;
     XtPointer client_data;
     XEvent *event;
     Boolean *continue_to_dispatch;
{
  XButtonPressedEvent *bevent;
  PaletteP palette;
  link_ptr tmp_link;
  int width;
  int square_size;
  int row, col;
  int pos;
  
  palette = client_data;
  if (palette)
    {
      bevent = (XButtonPressedEvent*) event;

      width = image_buf_width (palette->color_image);
      square_size = ((width - (SPACING * (COLUMNS + 1))) / COLUMNS) + SPACING;
      
      col = (bevent->x - 1) / square_size;
      row = (palette->scroll_offset + bevent->y - 1) / square_size;
      pos = row * COLUMNS + col;
      
      tmp_link = nth_item (palette->entries, pos);
      if (tmp_link)
	{
	  palette_draw_current_entry (palette);
	  palette->color[palette->color_state] = tmp_link->data;
	  palette_draw_current_entry (palette);
	  palette_draw_preview (palette);
	  
	  palette->updating = 1;
	  palette_draw_text (palette);
	  palette->updating = 0;
	}
    }
}

static int
palette_draw_color_row (colors, ncolors, y, buffer, image)
     unsigned char **colors;
     int ncolors, y;
     unsigned char *buffer;
     ImageBuf image;
{
  unsigned char *p;
  unsigned char bcolor[3];
  int width, height;
  int square_size;
  int vsize;
  int vspacing;
  int i, j;
  
  bcolor[0] = 0;
  bcolor[1] = 255;
  bcolor[2] = 0;

  width = image_buf_width (image);
  height = image_buf_height (image);
  square_size = (width - (SPACING * (COLUMNS + 1))) / COLUMNS;

  if ((y >= 0) && ((y + SPACING) < height))
    vspacing = SPACING;
  else if (y < 0)
    vspacing = SPACING + y;
  else
    vspacing = height - y;
    
  if (vspacing > 0)
    {
      if (y < 0)
	y += SPACING - vspacing;

      for (i = SPACING - vspacing; i < SPACING; i++, y++)
	{
	  p = buffer;
	  for (j = 0; j < width; j++)
	    {
	      *p++ = bcolor[i];
	      *p++ = bcolor[i];
	      *p++ = bcolor[i];
	    }
	  
	  image_buf_draw_row (image, buffer, 0, y, image_buf_width (image));
	}

      if (y > SPACING)
	y += SPACING - vspacing;
    }
  else
    y += SPACING;
  
  vsize = (y >= 0) ? (square_size) : (square_size + y);

  if ((y >= 0) && ((y + square_size) < height))
    vsize = square_size;
  else if (y < 0)
    vsize = square_size + y;
  else
    vsize = height - y;

  if (vsize > 0)
    {
      p = buffer;
      for (i = 0; i < ncolors; i++)
	{
	  for (j = 0; j < SPACING; j++)
	    {
	      *p++ = bcolor[j];
	      *p++ = bcolor[j];
	      *p++ = bcolor[j];
	    }
	  
	  for (j = 0; j < square_size; j++)
	    {
	      *p++ = colors[i][0];
	      *p++ = colors[i][1];
	      *p++ = colors[i][2];
	    }
	}

      for (i = 0; i < (COLUMNS - ncolors); i++)
	{
	  for (j = 0; j < (SPACING + square_size); j++)
	    {
	      *p++ = 0;
	      *p++ = 0;
	      *p++ = 0;
	    }
	}
      
      for (j = 0; j < SPACING; j++)
	{
	  *p++ = bcolor[j];
	  *p++ = bcolor[j];
	  *p++ = bcolor[j];
	}
      
      if (y < 0)
	y += square_size - vsize;
      for (i = 0; i < vsize; i++, y++)
	image_buf_draw_row (image, buffer, 0, y, image_buf_width (image));
      if (y > square_size)
	y += square_size - vsize;
    }
  else
    y += square_size;
  
  return y;
}

static void 
palette_draw_entries (palette)
     PaletteP palette;
{
  PaletteEntryP entry;
  unsigned char *buffer;
  unsigned char *colors[COLUMNS];
  link_ptr tmp_link;
  int width, height;
  int square_size;
  int row_vsize;
  int index, y;
  
  if (palette && palette->entries && !palette->updating)
    {
      width = image_buf_width (palette->color_image);
      height = image_buf_height (palette->color_image);
      square_size = (width - (SPACING * (COLUMNS + 1))) / COLUMNS;
      
      buffer = xmalloc (width * 3);

      y = -palette->scroll_offset;
      row_vsize = SPACING + square_size;
      tmp_link = palette->entries;
      index = 0;

      while ((tmp_link) && (y < -row_vsize))
	{
	  tmp_link = tmp_link->next;

	  if (++index == COLUMNS)
	    {
	      index = 0;
	      y += row_vsize;
	    }
	}

      index = 0;
      while (tmp_link)
	{
	  entry = tmp_link->data;
	  tmp_link = tmp_link->next;
	  
	  colors[index] = entry->color;
	  index++;

	  if (index == COLUMNS)
	    {
	      index = 0;
	      y = palette_draw_color_row (colors, COLUMNS, y, buffer, palette->color_image);
	      if (y >= height)
		break;
	    }
	}

      while (y < height)
	{
	  y = palette_draw_color_row (colors, index, y, buffer, palette->color_image);
	  index = 0;
	}

      image_buf_put (palette->color_image, XtWindow (palette->color_area), palette->gc);

      xfree (buffer);
    }
}

static void
palette_draw_current_entry (palette)
     PaletteP palette;
{
  PaletteEntryP entry;
  int width, height;
  int square_size;
  int row, col;
  int x, y;

  if (palette && !palette->updating)
    {
      XSetFunction (DISPLAY, palette->gc, GXinvert);
      
      entry = palette->color[palette->color_state];

      row = entry->position / COLUMNS;
      col = entry->position % COLUMNS;

      square_size = (image_buf_width (palette->color_image) - 
		     (SPACING * (COLUMNS + 1))) / COLUMNS;

      x = col * (square_size + SPACING) + 1;
      y = row * (square_size + SPACING) + 1;
      y -= palette->scroll_offset;

      width = square_size + SPACING;
      height = square_size + SPACING;

      XDrawRectangle (DISPLAY, XtWindow (palette->color_area), 
		      palette->gc, x, y, width, height);
      XDrawRectangle (DISPLAY, XtWindow (palette->color_area), 
		      palette->gc, x+1, y+1, width-2, height-2);
      XDrawRectangle (DISPLAY, XtWindow (palette->color_area), 
		      palette->gc, x-1, y-1, width+2, height+2);
      
      XSetFunction (DISPLAY, palette->gc, GXcopy);
    }
}

static void 
palette_draw_preview (palette)
     PaletteP palette;
{
  Colormap cmap;
  XColor *xcolor;
  unsigned char *color;
  Dimension width;
  Dimension height;
  
  if (palette && !palette->updating)
    {
      color = palette->color[palette->color_state]->color;
      
      xcolor = &palette->preview_color;
      xcolor->red = color[0] << 8;
      xcolor->green = color[1] << 8;
      xcolor->blue = color[2] << 8;
      xcolor->flags = DoRed | DoGreen | DoBlue;

      XtVaGetValues (palette->preview_widget,
		     XmNcolormap, &cmap,
		     XmNwidth, &width,
		     XmNheight, &height,
		     NULL);

      if (palette->alloc_colors)
	XStoreColor (DISPLAY, cmap, xcolor);
      else
	if (!XAllocColor (DISPLAY, cmap, xcolor))
	  fatal_error ("unable to allocate a single colormap entry");

      XSetForeground (DISPLAY, palette->gc, xcolor->pixel);
      XFillRectangle (DISPLAY, palette->preview_pixmap, 
		      palette->gc, 0, 0, width, height);
      
      XCopyArea (DISPLAY, palette->preview_pixmap, XtWindow (palette->preview_widget), 
		 palette->gc, 0, 0, width, height, 0, 0);
    }
}

static void 
palette_draw_text (palette)
     PaletteP palette;
{
  unsigned char *color;
  char buffer[5];
  int i;
  
  if (palette)
    {
      color = palette->color[palette->color_state]->color;

      for (i = 0; i < 3; i++)
	{
	  sprintf (buffer, "%d", color[i]);
	  XmTextSetString (palette->text_widget[i], buffer);
	}
    }
}

static PaletteEntryP
palette_add_entry (palette, r, g, b)
     PaletteP palette;
     int r, g, b;
{
  PaletteEntryP entry;
  
  if (palette)
    {
      entry = xmalloc (sizeof (_PaletteEntry));

      entry->color[0] = r;
      entry->color[1] = g;
      entry->color[2] = b;
      entry->position = palette->n_entries;

      palette->entries = append_to_list (palette->entries, entry);
      palette->n_entries += 1;

      return entry;
    }
  
  return NULL;
}

static void
palette_add_standard_entries (palette)
     PaletteP palette;
{
#define STEPS 60

  int i, r, g, b;
  link_ptr tmp_link;

  if (palette)
    {
      for (i = 0; i < STEPS; i++)
	{
	  r = g = b = (i * 255) / (STEPS - 1);
	  palette_add_entry (palette, r, g, b);
	}

      for (i = 0; i < STEPS; i++)
	{
	  r = (i * 255) / (STEPS - 1);
	  g = b = 0;
	  palette_add_entry (palette, r, g, b);
	}

      for (i = 0; i < STEPS; i++)
	{
	  g = (i * 255) / (STEPS - 1);
	  r = b = 0;
	  palette_add_entry (palette, r, g, b);
	}

      for (i = 0; i < STEPS; i++)
	{
	  b = (i * 255) / (STEPS - 1);
	  r = g = 0;
	  palette_add_entry (palette, r, g, b);
	}

      tmp_link = nth_item (palette->entries, 0);
      palette->color[FOREGROUND] = tmp_link->data;

      tmp_link = nth_item (palette->entries, STEPS - 1);
      palette->color[BACKGROUND] = tmp_link->data;
    }
}

static void
palette_delete_entry (palette)
     PaletteP palette;
{
  PaletteEntryP entry;
  link_ptr tmp_link;
  int pos;
  
  if (palette)
    {
      entry = palette->color[palette->color_state];
      if (entry == palette->color[!palette->color_state])
	palette->color[!palette->color_state] = NULL;
      palette->entries = remove_from_list (palette->entries, entry);

      pos = entry->position;
      xfree (entry);

      tmp_link = nth_item (palette->entries, pos);

      if (tmp_link)
	{
	  palette->color[palette->color_state] = tmp_link->data;
	  if (!palette->color[!palette->color_state])
	    palette->color[!palette->color_state] = tmp_link->data;
	  
	  while (tmp_link)
	    {
	      entry = tmp_link->data;
	      tmp_link = tmp_link->next;
	      entry->position = pos++;
	    }
	}
      else
	{
	  tmp_link = nth_item (palette->entries, pos - 1);
	  if (tmp_link)
	    {
	      palette->color[palette->color_state] = tmp_link->data;
	      if (!palette->color[!palette->color_state])
		palette->color[!palette->color_state] = tmp_link->data;
	    }
	}

      palette->n_entries -= 1;
      if (palette->n_entries == 0)
	{
	  palette->color[0] = palette_add_entry (palette, 0, 0, 0);
	  palette->color[1] = palette->color[0];
	}

      palette_calc_scrollbar (palette);
      palette_draw_preview (palette);
      palette_draw_entries (palette);
      palette_draw_current_entry (palette);
    }
}

static void 
palette_calc_scrollbar (palette)
     PaletteP palette;
{
  int n_entries;
  int cur_entry_row;
  int nrows;
  int row_vsize;
  int vsize;
  int page_size;
  int new_offset;
  
  if (palette)
    {
      n_entries = palette->n_entries;
      nrows = n_entries / COLUMNS;
      if (n_entries % COLUMNS)
	nrows += 1;
      row_vsize = SPACING + ((image_buf_width (palette->color_image) - 
			      (SPACING * (COLUMNS + 1))) / COLUMNS);
      vsize = row_vsize * nrows;
      page_size = row_vsize * COLUMNS;

      cur_entry_row = palette->color[palette->color_state]->position / COLUMNS;
      new_offset = cur_entry_row * row_vsize;

      /* scroll only if necessary */
      if (new_offset < palette->scroll_offset)
	{
	  palette->scroll_offset = new_offset;
	}
      else if (new_offset > palette->scroll_offset)
	{
	  /* only scroll the minimum amount to bring the current color into view */
	  if ((palette->scroll_offset + page_size - row_vsize) < new_offset)
	    palette->scroll_offset = new_offset - (page_size - row_vsize);
	}

      /* sanity check to make sure the scrollbar offset is valid */
      if (vsize > page_size)
	if (palette->scroll_offset > (vsize - page_size))
	  palette->scroll_offset = vsize - page_size;

      XtVaSetValues (palette->scroll_bar,
		     XmNminimum, 0,
		     XmNmaximum, vsize,
		     XmNvalue, palette->scroll_offset,
		     XmNsliderSize, (page_size < vsize) ? page_size : vsize,
		     XmNpageIncrement, page_size,
		     XmNincrement, row_vsize,
		     NULL);
    }
}

