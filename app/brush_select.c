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
#include "actionarea.h"
#include "brushes.h"
#include "brush_select.h"
#include "buildmenu.h"
#include "colormaps.h"
#include "errors.h"
#include "gconvert.h"
#include "paint_funcs.h"
#include "visual.h"


#define MIN_WIDTH         32
#define MIN_HEIGHT        32
#define MAX_WIN_WIDTH     245
#define MAX_WIN_HEIGHT    245
#define NUM_BRUSH_COLUMNS 7
#define MARGIN_WIDTH      3
#define MARGIN_HEIGHT     3


/*  local function prototypes  */
static void create_preview               (BrushSelectP);
static void display_brush                (BrushSelectP, GBrushP, int, int);
static void display_brushes              (BrushSelectP);
static void display_setup                (BrushSelectP);
static void draw_preview                 (BrushSelectP);
static void brush_select_show_selected   (BrushSelectP, int, int);
static void update_active_brush_field    (BrushSelectP);
static void brush_select_close_callback  (Widget, XtPointer, XtPointer);
static void brush_select_refresh_callback(Widget, XtPointer, XtPointer);
static void brush_select_cancel_callback (Widget, XtPointer, XtPointer);
static void brush_select_button_press    (Widget, XtPointer, XEvent *, Boolean *);
static void preview_draw_callback        (Widget, XtPointer, XtPointer);
static void paint_mode_menu_callback     (Widget, XtPointer, XtPointer);
static void opacity_scale_callback       (Widget, XtPointer, XtPointer);
static void interpolate_callback         (Widget, XtPointer, XtPointer);
static void max_brush_extents            (int *, int *);

/*  the option menu items -- the paint modes  */
static MenuItem option_items[] = 
{
  { "Normal", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) NORMAL, NULL, NULL },
  { "Darken Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) DARKEN_ONLY, NULL, NULL },
  { "Lighten Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) LIGHTEN_ONLY, NULL, NULL },
  { "Red Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) RED_ONLY, NULL, NULL },
  { "Green Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) GREEN_ONLY, NULL, NULL },
  { "Blue Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) BLUE_ONLY, NULL, NULL },
  { "Hue Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) HUE_ONLY, NULL, NULL },
  { "Saturation Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) SATURATION_ONLY, NULL, NULL },
  { "Value Only", &xmPushButtonGadgetClass, 0, NULL, NULL, 
      paint_mode_menu_callback, (void*) VALUE_ONLY, NULL, NULL },
  { NULL },
};

/*  the action area structure  */
static ActionAreaItem action_items[] = 
{
  { "Close", brush_select_close_callback, NULL, NULL },
  { "Refresh", brush_select_refresh_callback, NULL, NULL },
  { "Cancel", brush_select_cancel_callback, NULL, NULL },
};

static float old_opacity;
static int old_interpolate;
static int old_paint_mode;


static void
create_preview (bsp)
     BrushSelectP bsp;
{
  Dimension win_width, win_height, shadow;

  /*  Get the maximum brush extents  */
  max_brush_extents (&bsp->cell_width, &bsp->cell_height);

  bsp->width = bsp->cell_width * NUM_BRUSH_COLUMNS;
  bsp->height = bsp->cell_height * ((num_brushes + NUM_BRUSH_COLUMNS - 1) / NUM_BRUSH_COLUMNS);
  if (!bsp->height)
    bsp->height = bsp->cell_height;

  XtVaGetValues (bsp->swin, XmNshadowThickness, &shadow, NULL);

  win_width = MAX_WIN_WIDTH + shadow*2;
  win_height = MAX_WIN_HEIGHT + shadow*2;

  XtVaSetValues (bsp->swin, XmNwidth, win_width, 
		 XmNheight, win_height, NULL);

  bsp->width = (bsp->width < MAX_WIN_WIDTH) ? MAX_WIN_WIDTH : bsp->width;
  bsp->height = (bsp->height < MAX_WIN_HEIGHT) ? MAX_WIN_HEIGHT : bsp->height;

  if (bsp->preview)
    XtDestroyWidget (bsp->preview);

  bsp->preview = XtVaCreateWidget ("brushPreview",
				   xmDrawingAreaWidgetClass, bsp->swin,
				   XmNwidth, (Dimension) bsp->width,
				   XmNheight, (Dimension) bsp->height,
				   NULL);

  if (bsp->image)
    image_buf_destroy (bsp->image);
  bsp->image = image_buf_create (1, GREY_BUF, bsp->width, bsp->height);
  if (!bsp->image)
    fatal_error ("Could not allocate image for brush select dialog.");

  /*  render the brushes into the newly created image structure  */
  display_brushes (bsp);

  /*  Add callbacks  */
  XtAddCallback (bsp->preview, XmNexposeCallback, preview_draw_callback, bsp);
  XtAddCallback (bsp->preview, XmNresizeCallback, preview_draw_callback, bsp);
  XtAddEventHandler (bsp->preview, ButtonPressMask, False,
		     brush_select_button_press, bsp);

  XtManageChild (bsp->preview);
}


BrushSelectP
brush_select_new ()
{
  extern Widget interface;
  BrushSelectP bsp;
  GBrushP active;
  Widget action_widget;
  Widget row_col_widget;
  Widget options_form_widget;
  Widget opacity_scale_widget;
  Widget interpolate_widget;
  Widget label_rc_widget;
  Widget active_label;
  Widget opacity_label;
  Widget option_menu;
  XGCValues gcv;
  Arg xt_args[6];
  int n;

  XSynchronize (DISPLAY, True);

  bsp = xmalloc (sizeof (_BrushSelect));
  bsp->preview = NULL;
  bsp->image = NULL;

  bsp->shell = XtVaCreatePopupShell ("brushSelectDialog",
/*				     xmDialogShellWidgetClass, toplevel, */
				     topLevelShellWidgetClass, interface,
				     XmNtitle, "Brush Selection",
				     XmNvisual, grey_visual,
				     XmNcolormap, get_colormap (GREY_GIMAGE),
				     XmNdepth, grey_depth,
				     XmNdeleteResponse, XmDESTROY,
				     NULL);

  bsp->dialog = XtVaCreateWidget ("dialog",
				  xmFormWidgetClass, bsp->shell,
				  NULL);

  row_col_widget = XtVaCreateWidget ("dialogPartition",
				     xmRowColumnWidgetClass, bsp->dialog,
				     XmNorientation, XmHORIZONTAL,
				     XmNnumColumns, 2,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNleftAttachment, XmATTACH_FORM,
				     XmNrightAttachment, XmATTACH_FORM,
				     XmNtopOffset, 10,
				     XmNleftOffset, 10,
				     XmNrightOffset, 10,
				     NULL);
			      
  bsp->swin = XtVaCreateWidget ("scrolledBrushPreview",
				xmScrolledWindowWidgetClass, row_col_widget,
				XmNscrollBarDisplayPolicy, XmAS_NEEDED,
				XmNscrollingPolicy, XmAUTOMATIC,
				XmNvisualPolicy, XmSTATIC,
				NULL);

  /*  Create the brush preview window and the underlying image  */
  create_preview (bsp);

  options_form_widget = XtVaCreateWidget ("brushOptionsRowCol",
					  xmRowColumnWidgetClass, row_col_widget,
					  NULL);

  /*  Create the paint mode option menu  */
  old_paint_mode = get_brush_paint_mode ();
  option_menu = BuildMenu (options_form_widget, XmMENU_OPTION, "Mode: ",
			   0, False, option_items,
			   DefaultVisualOfScreen (XtScreen (toplevel)),
			   DefaultColormapOfScreen (XtScreen (toplevel)),
			   DefaultDepthOfScreen (XtScreen (toplevel)));

  XtManageChild (option_menu);


  /*  Create the interpolation check box  */
  old_interpolate = get_brush_interpolate ();
  interpolate_widget = XtVaCreateWidget ("Interpolate", 
					 xmToggleButtonWidgetClass, options_form_widget,
					 XmNset, (get_brush_interpolate ()) ? True : False,
					 NULL);
  XtAddCallback (interpolate_widget, XmNvalueChangedCallback, interpolate_callback, NULL);

  XtManageChild (interpolate_widget);

  /*  Create the opacity text widget  */
  old_opacity = get_brush_opacity ();

  n = 0;
  XtSetArg (xt_args[n], XmNminimum, 0); n++;
  XtSetArg (xt_args[n], XmNmaximum, 1000); n++;
  XtSetArg (xt_args[n], XmNdecimalPoints, 1); n++;
  XtSetArg (xt_args[n], XmNvalue, (long) (old_opacity * 1000)); n++;
  XtSetArg (xt_args[n], XmNshowValue, True); n++;
  XtSetArg (xt_args[n], XmNorientation, XmHORIZONTAL); n++;

  label_rc_widget = XtVaCreateWidget ("labelRowCol",
				      xmRowColumnWidgetClass, options_form_widget,
				      XmNorientation, XmHORIZONTAL,
				      XmNnumColumns, 2, NULL);

  opacity_label = XtVaCreateManagedWidget ("Opacity:", 
					   xmLabelWidgetClass, label_rc_widget,
					   NULL);

  opacity_scale_widget = XmCreateScale (label_rc_widget, "brushOpacityScale", xt_args, n);

  XtAddCallback (opacity_scale_widget, XmNvalueChangedCallback, 
		 opacity_scale_callback, NULL);
	  
  XtManageChild (opacity_scale_widget);
  XtManageChild (label_rc_widget);


  /*  Create the active brush label  */

  label_rc_widget = XtVaCreateWidget ("labelRowCol",
				      xmRowColumnWidgetClass, options_form_widget,
				      XmNorientation, XmHORIZONTAL,
				      XmNnumColumns, 2, NULL);

  active_label = XtVaCreateManagedWidget ("Active:",
					  xmLabelGadgetClass, label_rc_widget,
					  NULL);

  bsp->active_brush = XtVaCreateManagedWidget ("activeBrushLabel",
					       xmLabelGadgetClass, label_rc_widget,
					       NULL);

  XtManageChild (label_rc_widget);

  /*  The action area  */
  action_items[0].data = bsp;
  action_items[1].data = bsp;
  action_items[2].data = bsp;
  action_widget = build_action_area (bsp->dialog, action_items, XtNumber (action_items));

  XtVaSetValues (action_widget,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, row_col_widget,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNtopOffset, 10,
		 XmNleftOffset, 10,
		 XmNrightOffset, 10,
		 XmNbottomOffset, 10,
		 NULL);
  XtVaSetValues (bsp->dialog, XmNdefaultPosition, False, NULL);

  /*  Manage the widgets  */
  XtManageChild (action_widget);
  XtManageChild (bsp->swin);
  XtManageChild (options_form_widget);
  XtManageChild (row_col_widget);
  XtManageChild (bsp->dialog);  

  XtPopup (bsp->shell, XtGrabNone);

  gcv.foreground = BlackPixelOfScreen (XtScreen (bsp->dialog));
  bsp->gc = XCreateGC (DISPLAY, XtWindow (bsp->dialog), GCForeground, &gcv);

  /*  update the active selection  */
  active = get_active_brush ();
  if (active)
    brush_select_select (bsp, active->index);

  return bsp;
}


void
brush_select_select (bsp, index)
     BrushSelectP bsp;
     int index;
{
  int row, col;

  update_active_brush_field (bsp);
  row = index / NUM_BRUSH_COLUMNS;
  col = index - row * NUM_BRUSH_COLUMNS;

  brush_select_show_selected (bsp, row, col);
}


void
brush_select_free (bsp)
     BrushSelectP bsp;
{
  if (bsp)
    {
      XFreeGC (DISPLAY, bsp->gc);
      if (bsp->image)
	image_buf_destroy (bsp->image);
      xfree (bsp);
    }
}


static void
display_brush (bsp, brush, x, y)
     BrushSelectP bsp;
     GBrushP brush;
     int x, y;
{
  unsigned char * src, *s;
  unsigned char * buf, *b;
  long src_width;
  int i, j;

  buf = (unsigned char *) xmalloc (sizeof (char) * brush->mask->width);

  /*  Draw the brush mask to the image buffer  */
  src_width = brush->mask->width * brush->mask->bytes;
  src = temp_buf_data (brush->mask);

  for (i = y; i < y + brush->mask->height; i++)
    {
      /*  Invert the mask for display.  We're doing this because a value of 255 in the
       *  mask means that is full intensity.  However, it makes more sense for full
       *  intensity to show up as black in this brush preview window...
       */
      s = src;
      b = buf;
      j = src_width;
      while (j--)
	*b++ = 255 - *s++;

      image_buf_draw_row (bsp->image, buf, x, i, brush->mask->width);

      src += src_width;
    }

  xfree (buf);
}


static void
display_setup (bsp)
     BrushSelectP bsp;
{
  unsigned char * buf;
  int i;

  buf = (unsigned char *) xmalloc (sizeof (char) * image_buf_width (bsp->image));

  /*  Set the buffer to white  */
  memset (buf, 255, image_buf_width (bsp->image));

  /*  Set the image buffer to white  */
  for (i = 0; i < image_buf_height (bsp->image); i++)
    image_buf_draw_row (bsp->image, buf, 0, i, image_buf_width (bsp->image));

  xfree (buf);

}


static void
display_brushes (bsp)
     BrushSelectP bsp;
{
  link_ptr list = brush_list;    /*  the global brush list  */
  GBrushP brush;
  int row, col;                  /*  current row and column  */
  int offset_x, offset_y;        /*  offset into brush box  */

  /*  setup the display area  */
  display_setup (bsp);

  row = col = 0;
  while (list)
    {
      brush = (GBrushP) list->data;

      /*  calculate the offset into the image  */
      offset_x = col * bsp->cell_width + ((bsp->cell_width - brush->mask->width) >> 1);
      offset_y = row * bsp->cell_height + ((bsp->cell_height - brush->mask->height) >> 1);

      /*  draw the brush  */
      display_brush (bsp, brush, offset_x, offset_y);

      /*  increment the counts  */
      if (++col == NUM_BRUSH_COLUMNS)
	{
	  row ++;
	  col = 0;
	}

      list = next_item (list);
    }
}


static void
brush_select_show_selected (bsp, row, col)
     BrushSelectP bsp;
     int row, col;
{
  static int old_row = 0;
  static int old_col = 0;
  unsigned char * buf;
  int offset_x, offset_y;
  int i;

  buf = (unsigned char *) xmalloc (sizeof (char) * bsp->cell_width);

  /*  remove the old selection  */
  offset_x = old_col * bsp->cell_width;
  offset_y = old_row * bsp->cell_height;

  /*  set the buf to white  */
  memset (buf, 255, bsp->cell_width);

  for (i = 0; i < bsp->cell_height; i++)
    {
      if (i == 0 || i == (bsp->cell_height - 1))
	image_buf_draw_row (bsp->image, buf, offset_x, offset_y + i, bsp->cell_width);
      else
	{
	  image_buf_draw_row (bsp->image, buf, offset_x, offset_y + i, 1);
	  image_buf_draw_row (bsp->image, buf, offset_x + bsp->cell_width - 1, offset_y + i, 1);
	}
    }
  image_buf_put_area (bsp->image, XtWindow (bsp->preview), bsp->gc, offset_x, offset_y,
					    bsp->cell_width, bsp->cell_height);

  /*  make the new selection  */
  offset_x = col * bsp->cell_width;
  offset_y = row * bsp->cell_height;

  /*  set the buf to black  */
  memset (buf, 0, bsp->cell_width);

  for (i = 0; i < bsp->cell_height; i++)
    {
      if (i == 0 || i == (bsp->cell_height - 1))
	image_buf_draw_row (bsp->image, buf, offset_x, offset_y + i, bsp->cell_width);
      else
	{
	  image_buf_draw_row (bsp->image, buf, offset_x, offset_y + i, 1);
	  image_buf_draw_row (bsp->image, buf, offset_x + bsp->cell_width - 1, offset_y + i, 1);
	}
    }
  image_buf_put_area (bsp->image, XtWindow (bsp->preview), bsp->gc, offset_x, offset_y,
					    bsp->cell_width, bsp->cell_height);

  old_row = row;
  old_col = col;

  xfree (buf);
}


static void 
draw_preview (bsp)
     BrushSelectP bsp;
{
  /*  Draw the image buf to the preview window  */
  image_buf_put (bsp->image, XtWindow (bsp->preview), bsp->gc);
}


static void
update_active_brush_field (bsp)
     BrushSelectP bsp;
{
  GBrushP brush;
  XmString str;

  brush = get_active_brush ();

  if (!brush) 
    return;

  str = XmStringCreateLocalized (brush->name);

  XtVaSetValues (bsp->active_brush, XmNlabelString, str, NULL);

  XmStringFree (str);
}


static void
brush_select_close_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  BrushSelectP bsp;

  bsp = (BrushSelectP) client_data;

  old_paint_mode = get_brush_paint_mode ();
  old_opacity = get_brush_opacity ();
  old_interpolate = get_brush_interpolate ();

  XtPopdown (bsp->shell);
}


static void
brush_select_refresh_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  BrushSelectP bsp;
  GBrushP active;

  bsp = (BrushSelectP) client_data;

  /*  re-init the brush list  */
  brushes_init ();

  /*  recreate the preview window  */
  create_preview (bsp);

  /*  update the active selection  */
  active = get_active_brush ();
  if (active)
    brush_select_select (bsp, active->index);

  /*  update the display  */
  draw_preview (bsp);
}


static void
brush_select_cancel_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  BrushSelectP bsp;

  bsp = (BrushSelectP) client_data;

  set_brush_paint_mode (old_paint_mode);
  set_brush_opacity (old_opacity);
  set_brush_interpolate (old_interpolate);

  XtPopdown (bsp->shell);
}


static void
brush_select_button_press (w, client_data, event, continue_to_dispatch)
     Widget w;
     XtPointer client_data;
     XEvent * event;
     Boolean * continue_to_dispatch;
{
  XButtonPressedEvent *bevent;
  BrushSelectP bsp;
  GBrushP brush;
  int row, col, index;
  
  bsp = (BrushSelectP) client_data;
  bevent = (XButtonPressedEvent*) event;

  col = bevent->x / bsp->cell_width;
  row = bevent->y / bsp->cell_height;
  index = row * NUM_BRUSH_COLUMNS + col;

  brush = get_brush_by_index (index);

  if (brush)
    /*  Make this brush the active brush  */
    select_brush (brush);
}


static void
preview_draw_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  draw_preview ((BrushSelectP) client_data);
}


static void
paint_mode_menu_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  set_brush_paint_mode ((int) client_data);
}


static void
opacity_scale_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmScaleCallbackStruct *info;
  float opacity;

  info = (XmScaleCallbackStruct*) call_data;

  /*  convert opacity value  */
  opacity = info->value / 1000.0;

  set_brush_opacity (opacity);
}


static void
interpolate_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmToggleButtonCallbackStruct * tbcs = (XmToggleButtonCallbackStruct *) call_data;

  set_brush_interpolate (tbcs->set);
}


static void
max_brush_extents (width, height)
     int * width, * height;
{
  link_ptr list = brush_list;   /*  the global brush list  */
  GBrushP brush;

  *width = MIN_WIDTH;
  *height = MIN_HEIGHT;

  while (list)
    {
      brush = (GBrushP) list->data;

      if (*width < brush->mask->width)
	*width = brush->mask->width;

      if (*height < brush->mask->height)
	*height = brush->mask->height;

      list = next_item (list);
    }
  *width += MARGIN_WIDTH;
  *height += MARGIN_HEIGHT;
}


