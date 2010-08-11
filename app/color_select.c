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
#include "color_select.h"
#include "colormaps.h"
#include "errors.h"
#include "visual.h"

#define XY_DEF_WIDTH  256
#define XY_DEF_HEIGHT 256
#define Z_DEF_WIDTH   20
#define Z_DEF_HEIGHT  256

typedef enum {
  HUE = 0,
  SATURATION,
  VALUE,
  RED,
  GREEN,
  BLUE,
  HUE_SATURATION,
  HUE_VALUE,
  SATURATION_VALUE,
  RED_GREEN,
  RED_BLUE,
  GREEN_BLUE
} ColorSelectFillType;

typedef enum {
  UPDATE_VALUES = 1 << 0,
  UPDATE_POS = 1 << 1,
  UPDATE_XY_COLOR = 1 << 2,
  UPDATE_Z_COLOR = 1 << 3,
  UPDATE_NEW_COLOR = 1 << 4,
  UPDATE_ORIG_COLOR = 1 << 5
} ColorSelectUpdateType;

typedef struct _ColorSelectFill ColorSelectFill;
typedef void (*ColorSelectFillUpdateProc) (ColorSelectFill *);

struct _ColorSelectFill {
  unsigned char *buffer;
  int y;
  int width;
  int height;
  int *values;
  ColorSelectFillUpdateProc update;
};

static void color_select_update (ColorSelectP, ColorSelectUpdateType);
static void color_select_update_values (ColorSelectP);
static void color_select_update_rgb_values (ColorSelectP);
static void color_select_update_hsv_values (ColorSelectP);
static void color_select_update_pos (ColorSelectP);
static void color_select_update_sliders (ColorSelectP);
static void color_select_update_colors (ColorSelectP, int);

static void color_select_ok_callback (Widget, XtPointer, XtPointer);
static void color_select_cancel_callback (Widget, XtPointer, XtPointer);
static void color_select_xy_draw_callback (Widget, XtPointer, XtPointer);
static void color_select_xy_resize_callback (Widget, XtPointer, XtPointer);
static void color_select_xy_motion (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_xy_button_press (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_xy_button_release (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_z_draw_callback (Widget, XtPointer, XtPointer);
static void color_select_z_resize_callback (Widget, XtPointer, XtPointer);
static void color_select_z_motion (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_z_button_press (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_z_button_release (Widget, XtPointer, XEvent *, Boolean *);
static void color_select_color_draw_callback (Widget, XtPointer, XtPointer);
static void color_select_slider_callback (Widget, XtPointer, XtPointer);
static void color_select_toggle_callback (Widget, XtPointer, XtPointer);

static void color_select_image_fill (ImageBuf, ColorSelectFillType, int *);

static void color_select_draw_z_marker (ColorSelectP, int);
static void color_select_draw_xy_marker (ColorSelectP, int);

static void color_select_update_red (ColorSelectFill *);
static void color_select_update_green (ColorSelectFill *);
static void color_select_update_blue (ColorSelectFill *);
static void color_select_update_hue (ColorSelectFill *);
static void color_select_update_saturation (ColorSelectFill *);
static void color_select_update_value (ColorSelectFill *);
static void color_select_update_red_green (ColorSelectFill *);
static void color_select_update_red_blue (ColorSelectFill *);
static void color_select_update_green_blue (ColorSelectFill *);
static void color_select_update_hue_saturation (ColorSelectFill *);
static void color_select_update_hue_value (ColorSelectFill *);
static void color_select_update_saturation_value (ColorSelectFill *);

static ColorSelectFillUpdateProc update_procs[] =
{
  color_select_update_hue,
  color_select_update_saturation,
  color_select_update_value,
  color_select_update_red,
  color_select_update_green,
  color_select_update_blue,
  color_select_update_hue_saturation,
  color_select_update_hue_value,
  color_select_update_saturation_value,
  color_select_update_red_green,
  color_select_update_red_blue,
  color_select_update_green_blue,
};

static ActionAreaItem action_items[2] = 
{
  { "OK", color_select_ok_callback, NULL, NULL },
  { "Cancel", color_select_cancel_callback, NULL, NULL },
};

ColorSelectP
color_select_new (r, g, b, callback)
     int r, g, b;
     ColorSelectCallback callback;
{
  extern Widget interface;

  static char *toggle_titles[6] = { "Hue", "Saturation", "Value", "Red", "Green", "Blue" };
  static char *slider_titles[6] = { "hSlider", "sSlider", "vSlider", "rSlider", "gSlider", "bSlider" };
  static int slider_max_vals[6] = { 3600, 1000, 1000, 255, 255, 255 };
  static int slider_decimals[6] = { 1, 1, 1, 0, 0, 0 };
  
  ColorSelectP csp;
  Widget xy_frame_widget;
  Widget z_frame_widget;
  Widget colors_frame_widget;
  Widget colors_form_widget;
  Widget action_widget;
  Widget row_col_widget;
  XGCValues gcv;
  Arg xt_args[6];
  Dimension width, height;
  int i, n;

  csp = xmalloc (sizeof (_ColorSelect));

  csp->callback = callback;
  csp->z_color_fill = HUE;
  csp->xy_color_fill = SATURATION_VALUE;

  csp->values[RED] = csp->orig_values[0] = r;
  csp->values[GREEN] = csp->orig_values[1] = g;
  csp->values[BLUE] = csp->orig_values[2] = b;
  color_select_update_hsv_values (csp);
  color_select_update_pos (csp);

  csp->shell = XtVaCreatePopupShell ("colorSelectDialog",
/*				     xmDialogShellWidgetClass, toplevel, */
				     topLevelShellWidgetClass, interface,
				     XmNtitle, "Color Selection",
				     XmNvisual, color_visual,
				     XmNcolormap, get_colormap (RGB_GIMAGE),
				     XmNdepth, color_depth,
				     NULL);

  csp->dialog = XtVaCreateWidget ("dialog",
				  xmFormWidgetClass, csp->shell,
				  NULL);

  xy_frame_widget = XtVaCreateManagedWidget ("xyColorFrame",
					     xmFrameWidgetClass, csp->dialog,
					     XmNshadowType, XmSHADOW_IN,
					     XmNshadowThickness, 2,
					     XmNleftAttachment, XmATTACH_FORM,
					     XmNtopAttachment, XmATTACH_FORM,
					     XmNleftOffset, 10,
					     XmNtopOffset, 10,
					     NULL);
  
  csp->xy_color = XtVaCreateManagedWidget ("xyColor",
					   xmDrawingAreaWidgetClass, xy_frame_widget,
					   XmNwidth, XY_DEF_WIDTH,
					   XmNheight, XY_DEF_HEIGHT,
					   NULL);

  csp->xy_color_image = image_buf_create (1, COLOR_BUF, XY_DEF_WIDTH, XY_DEF_HEIGHT);

  XtAddCallback (csp->xy_color, XmNexposeCallback, color_select_xy_draw_callback, csp);
  XtAddCallback (csp->xy_color, XmNresizeCallback, color_select_xy_resize_callback, csp);
  XtAddEventHandler (csp->xy_color, ButtonPressMask, False, color_select_xy_button_press, csp);
  XtAddEventHandler (csp->xy_color, ButtonReleaseMask, False, color_select_xy_button_release, csp);
  XtAddEventHandler (csp->xy_color, Button1MotionMask, False, color_select_xy_motion, csp);


  z_frame_widget = XtVaCreateManagedWidget ("zColorFrame",
					    xmFrameWidgetClass, csp->dialog,
					    XmNshadowType, XmSHADOW_IN,
					    XmNshadowThickness, 2,
					    XmNleftAttachment, XmATTACH_WIDGET,
					    XmNleftWidget, xy_frame_widget,
					    XmNtopAttachment, XmATTACH_FORM,
					    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
					    XmNbottomWidget, xy_frame_widget,
					    XmNleftOffset, 10,
					    XmNtopOffset, 10,
					    NULL);

  csp->z_color = XtVaCreateManagedWidget ("zColor",
					  xmDrawingAreaWidgetClass, z_frame_widget,
					  XmNwidth, Z_DEF_WIDTH,
					  NULL);
					  
  csp->z_color_image = image_buf_create (1, COLOR_BUF, Z_DEF_WIDTH, Z_DEF_HEIGHT);

  XtAddCallback (csp->z_color, XmNexposeCallback, color_select_z_draw_callback, csp);
  XtAddCallback (csp->z_color, XmNresizeCallback, color_select_z_resize_callback, csp);
  XtAddEventHandler (csp->z_color, ButtonPressMask, False, color_select_z_button_press, csp);
  XtAddEventHandler (csp->z_color, ButtonReleaseMask, False, color_select_z_button_release, csp);
  XtAddEventHandler (csp->z_color, Button1MotionMask, False, color_select_z_motion, csp);

  colors_frame_widget = XtVaCreateManagedWidget ("colorsFrame",
						 xmFrameWidgetClass, csp->dialog,
						 XmNshadowType, XmSHADOW_IN,
						 XmNshadowThickness, 2,
						 XmNleftAttachment, XmATTACH_WIDGET,
						 XmNleftWidget, z_frame_widget,
						 XmNtopAttachment, XmATTACH_FORM,
						 XmNrightAttachment, XmATTACH_FORM,
						 XmNleftOffset, 10,
						 XmNtopOffset, 10,
						 XmNrightOffset, 10,
						 NULL);

  colors_form_widget = XtVaCreateManagedWidget ("colorsForm",
						xmFormWidgetClass, colors_frame_widget,
						XmNfractionBase, 2,
						NULL);

  csp->new_color = XtVaCreateManagedWidget ("newColor",
					    xmDrawingAreaWidgetClass, colors_form_widget,
					    XmNleftAttachment, XmATTACH_FORM,
					    XmNrightAttachment, XmATTACH_POSITION,
					    XmNrightPosition, 1,
					    XmNtopAttachment, XmATTACH_FORM,
					    XmNbottomAttachment, XmATTACH_FORM,
					    NULL);
  XtAddCallback (csp->new_color, XmNexposeCallback, color_select_color_draw_callback, csp);

  csp->orig_color = XtVaCreateManagedWidget ("origColor", 
					     xmDrawingAreaWidgetClass, colors_form_widget,
					     XmNleftAttachment, XmATTACH_POSITION,
					     XmNleftPosition, 1,
					     XmNrightAttachment, XmATTACH_FORM,
					     XmNtopAttachment, XmATTACH_FORM,
					     XmNbottomAttachment, XmATTACH_FORM,
					     NULL);
  XtAddCallback (csp->orig_color, XmNexposeCallback, color_select_color_draw_callback, csp);


  row_col_widget = XtVaCreateWidget ("dialogPartition", 
				     xmRowColumnWidgetClass, csp->dialog,
				     XmNorientation, XmVERTICAL,
				     XmNnumColumns, 2,
				     XmNpacking, XmPACK_COLUMN,
				     XmNradioBehavior, True,
				     XmNleftAttachment, XmATTACH_WIDGET,
				     XmNleftWidget, z_frame_widget,
				     XmNtopAttachment, XmATTACH_NONE,
				     XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				     XmNbottomWidget, z_frame_widget,
				     XmNrightAttachment, XmATTACH_FORM,
				     XmNleftOffset, 10,
				     XmNtopOffset, 10,
				     XmNrightOffset, 10,
				     NULL);

  XtVaSetValues (colors_frame_widget,
		 XmNbottomAttachment, XmATTACH_WIDGET,
		 XmNbottomWidget, row_col_widget,
		 NULL);

  
  n = 0;
  XtSetArg (xt_args[n], XmNminimum, 0); n++;
  XtSetArg (xt_args[n], XmNmaximum, 255); n++;
  XtSetArg (xt_args[n], XmNvalue, 0); n++;
  XtSetArg (xt_args[n], XmNdecimalPoints, 0); n++;
  XtSetArg (xt_args[n], XmNshowValue, True); n++;
  XtSetArg (xt_args[n], XmNorientation, XmHORIZONTAL); n++;

  for (i = 0; i < 6; i++)
    {
      csp->toggles[i] = XtVaCreateWidget (toggle_titles[i], 
					  xmToggleButtonWidgetClass, row_col_widget,
					  XmNset, i == 0,
					  NULL);
    }

  for (i = 0; i < 6; i++)
    {
      XtSetArg (xt_args[1], XmNmaximum, slider_max_vals[i]);
      XtSetArg (xt_args[3], XmNdecimalPoints, slider_decimals[i]);

      csp->sliders[i] = XmCreateScale (row_col_widget, slider_titles[i], xt_args, n);
    }

  for (i = 0; i < 6; i++)
    {
      XtAddCallback (csp->toggles[i], XmNvalueChangedCallback, color_select_toggle_callback, csp);
      XtAddCallback (csp->sliders[i], XmNvalueChangedCallback, color_select_slider_callback, csp);

      XtManageChild (csp->toggles[i]);
      XtManageChild (csp->sliders[i]);
    }

  XtManageChild (row_col_widget);

  action_items[0].data = csp;
  action_items[1].data = csp;
  action_widget = build_action_area (csp->dialog, action_items, 2);
  XtVaSetValues (action_widget,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, xy_frame_widget,
		 XmNleftOffset, 10,
		 XmNrightOffset, 10,
		 XmNtopOffset, 10,
		 XmNbottomOffset, 10,
		 NULL);

  XtVaSetValues (csp->dialog, XmNdefaultPosition, False, NULL);

  XtManageChild (action_widget);
  XtManageChild (csp->dialog);
  XtPopup (csp->shell, XtGrabNone);

  XtVaGetValues (csp->new_color,
		 XtNwidth, &width,
		 XtNheight, &height,
		 NULL);
  csp->new_color_pixmap = XCreatePixmap (DISPLAY, XtWindow (csp->new_color),
					 width, height, color_depth);

  XtVaGetValues (csp->orig_color, 
		 XtNwidth, &width,
		 XtNheight, &height,
		 NULL);
  csp->orig_color_pixmap = XCreatePixmap (DISPLAY, XtWindow (csp->orig_color),
					  width, height, color_depth);

  csp->alloc_colors = (color_depth == 8);
  if (csp->alloc_colors)
    {
      Colormap cmap;
      unsigned long pixels[2];
      
      XtVaGetValues (csp->new_color, XmNcolormap, &cmap, NULL);
      
      if (!XAllocColorCells (DISPLAY, cmap, False, NULL, 0, pixels, 2))
	fatal_error ("unable to allocate 2 colormap entries");
      
      csp->ncolor.pixel = pixels[0];
      csp->ocolor.pixel = pixels[1];
    }

  gcv.foreground = BlackPixelOfScreen (XtScreen (csp->dialog));
  csp->gc = XCreateGC (DISPLAY, XtWindow (csp->dialog), GCForeground, &gcv);

  color_select_image_fill (csp->z_color_image, csp->z_color_fill, csp->values);
  color_select_image_fill (csp->xy_color_image, csp->xy_color_fill, csp->values);

  return csp;
}

void
color_select_show (csp)
     ColorSelectP csp;
{
  if (csp)
    XtPopup (csp->shell, XtGrabNone);
}

void
color_select_hide (csp)
     ColorSelectP csp;
{
  if (csp)
    XtPopdown (csp->shell);
}

void
color_select_free (csp)
     ColorSelectP csp;
{
  if (csp)
    {
      if (csp->alloc_colors)
	{
	  Colormap cmap;
	  unsigned long pixels[2];

	  XtVaGetValues (csp->new_color, XmNcolormap, &cmap, NULL);
	  
	  pixels[0] = csp->ncolor.pixel;
	  pixels[1] = csp->ocolor.pixel;
	  XFreeColors (DISPLAY, cmap, pixels, 2, 0);
	}

      XtDestroyWidget (csp->dialog);
      XtDestroyWidget (csp->shell);
      XFreeGC (DISPLAY, csp->gc);
      XFreePixmap (DISPLAY, csp->orig_color_pixmap);
      XFreePixmap (DISPLAY, csp->new_color_pixmap);
      image_buf_destroy (csp->xy_color_image);
      image_buf_destroy (csp->z_color_image);
      xfree (csp);
    }
}

void
color_select_set_color (csp, r, g, b, set_current)
     ColorSelectP csp;
     int r, g, b;
     int set_current;
{
  if (csp)
    {
      csp->orig_values[0] = r;
      csp->orig_values[1] = g;
      csp->orig_values[2] = b;

      color_select_update_colors (csp, 1);
      
      if (set_current)
	{
	  csp->values[RED] = r;
	  csp->values[GREEN] = g;
	  csp->values[BLUE] = b;
	  
	  color_select_update_hsv_values (csp);
	  color_select_update_pos (csp);
	  color_select_update_sliders (csp);
	  color_select_update_colors (csp, 0);
	  
	  color_select_update (csp, UPDATE_Z_COLOR);
	  color_select_update (csp, UPDATE_XY_COLOR);
	}
    }
}

static void 
color_select_update (csp, update)
     ColorSelectP csp;
     ColorSelectUpdateType update;
{
  if (csp)
    {
      if (update & UPDATE_POS)
	color_select_update_pos (csp);

      if (update & UPDATE_VALUES)
	{
	  color_select_update_values (csp);
	  color_select_update_sliders (csp);

	  if (!(update & UPDATE_NEW_COLOR))
	    color_select_update_colors (csp, 0);
	}

      if (update & UPDATE_XY_COLOR)
	{
	  color_select_image_fill (csp->xy_color_image, csp->xy_color_fill, csp->values);
	  image_buf_put (csp->xy_color_image, XtWindow (csp->xy_color), csp->gc);
	  color_select_draw_xy_marker (csp, 1);
	}

      if (update & UPDATE_Z_COLOR)
	{
	  color_select_image_fill (csp->z_color_image, csp->z_color_fill, csp->values);
	  image_buf_put (csp->z_color_image, XtWindow (csp->z_color), csp->gc);
	  color_select_draw_z_marker (csp, 1);
	}

      if (update & UPDATE_NEW_COLOR)
	color_select_update_colors (csp, 0);
      
      if (update & UPDATE_ORIG_COLOR)
	color_select_update_colors (csp, 1);
    }
}

static void
color_select_update_values (csp)
     ColorSelectP csp;
{
  if (csp)
    {
      switch (csp->z_color_fill)
	{
	case RED:
	  csp->values[BLUE] = csp->pos[0];
	  csp->values[GREEN] = csp->pos[1];
	  csp->values[RED] = csp->pos[2];
	  break;
	case GREEN:
	  csp->values[BLUE] = csp->pos[0];
	  csp->values[RED] = csp->pos[1];
	  csp->values[GREEN] = csp->pos[2];
	  break;
	case BLUE:
	  csp->values[GREEN] = csp->pos[0];
	  csp->values[RED] = csp->pos[1];
	  csp->values[BLUE] = csp->pos[2];
	  break;
	case HUE:
	  csp->values[VALUE] = csp->pos[0] * 1000 / 255;
	  csp->values[SATURATION] = csp->pos[1] * 1000 / 255;
	  csp->values[HUE] = csp->pos[2] * 3600 / 255;
	  break;
	case SATURATION:
	  csp->values[VALUE] = csp->pos[0] * 1000 / 255;
	  csp->values[HUE] = csp->pos[1] * 3600 / 255;
	  csp->values[SATURATION] = csp->pos[2] * 1000 / 255;
	  break;
	case VALUE:
	  csp->values[SATURATION] = csp->pos[0] * 1000 / 255;
	  csp->values[HUE] = csp->pos[1] * 3600 / 255;
	  csp->values[VALUE] = csp->pos[2] * 1000 / 255;
	  break;
	}
      
      switch (csp->z_color_fill)
	{
	case RED:
	case GREEN:
	case BLUE:
	  color_select_update_hsv_values (csp);
	  break;
	case HUE:
	case SATURATION:
	case VALUE:
	  color_select_update_rgb_values (csp);
	  break;
	}
    }
}

static void 
color_select_update_rgb_values (csp)
     ColorSelectP csp;
{
  float h, s, v;
  float f, p, q, t;
  
  if (csp)
    {
      h = csp->values[HUE] / 10.0;
      s = csp->values[SATURATION] / 1000.0;
      v = csp->values[VALUE] / 1000.0;

      if (s == 0)
	{
	  csp->values[RED] = v * 255;
	  csp->values[GREEN] = v * 255;
	  csp->values[BLUE] = v * 255;
	}
      else
	{
	  if (h == 360)
	    h = 0;

	  h /= 60;
	  f = h - (int) h;
	  p = v * (1 - s);
	  q = v * (1 - (s * f));
	  t = v * (1 - (s * (1 - f)));
	  
	  switch ((int) h)
	    {
	    case 0:
	      csp->values[RED] = v * 255;
	      csp->values[GREEN] = t * 255;
	      csp->values[BLUE] = p * 255;
	      break;
	    case 1:
	      csp->values[RED] = q * 255;
	      csp->values[GREEN] = v * 255;
	      csp->values[BLUE] = p * 255;
	      break;
	    case 2:
	      csp->values[RED] = p * 255;
	      csp->values[GREEN] = v * 255;
	      csp->values[BLUE] = t * 255;
	      break;
	    case 3:
	      csp->values[RED] = p * 255;
	      csp->values[GREEN] = q * 255;
	      csp->values[BLUE] = v * 255;
	      break;
	    case 4:
	      csp->values[RED] = t * 255;
	      csp->values[GREEN] = p * 255;
	      csp->values[BLUE] = v * 255;
	      break;
	    case 5:
	      csp->values[RED] = v * 255;
	      csp->values[GREEN] = p * 255;
	      csp->values[BLUE] = q * 255;
	      break;
	    }
	}
    }
}

static void 
color_select_update_hsv_values (csp)
     ColorSelectP csp;
{
  int r, g, b;
  float h, s, v;
  int min, max;
  int delta;

  if (csp)
    {
      r = csp->values[RED];
      g = csp->values[GREEN];
      b = csp->values[BLUE];

      if (r > g)
	{
	  if (r > b)
	    max = r;
	  else
	    max = b;

	  if (g < b)
	    min = g;
	  else
	    min = b;
	}
      else
	{
	  if (g > b)
	    max = g;
	  else
	    max = b;

	  if (r < b)
	    min = r;
	  else
	    min = b;
	}

      v = max;

      if (max != 0)
	s = (max - min) / (float) max;
      else
	s = 0;

      if (s == 0)
	h = 0;
      else
	{
	  delta = max - min;
	  if (r == max)
	    h = (g - b) / (float) delta;
	  else if (g == max)
	    h = 2 + (b - r) / (float) delta;
	  else if (b == max)
	    h = 4 + (r - g) / (float) delta;
	  h *= 60;

	  if (h < 0)
	    h += 360;
	}

      csp->values[HUE] = h * 10;
      csp->values[SATURATION] = s * 1000;
      csp->values[VALUE] = v * 1000 / 255;
    }
}

static void 
color_select_update_pos (csp)
     ColorSelectP csp;
{
  if (csp)
    {
      switch (csp->z_color_fill)
	{
	case RED:
	  csp->pos[0] = csp->values[BLUE];
	  csp->pos[1] = csp->values[GREEN];
	  csp->pos[2] = csp->values[RED];
	  break;
	case GREEN:
	  csp->pos[0] = csp->values[BLUE];
	  csp->pos[1] = csp->values[RED];
	  csp->pos[2] = csp->values[GREEN];
	  break;
	case BLUE:
	  csp->pos[0] = csp->values[GREEN];
	  csp->pos[1] = csp->values[RED];
	  csp->pos[2] = csp->values[BLUE];
	  break;
	case HUE:
	  csp->pos[0] = csp->values[VALUE] * 255 / 1000;
	  csp->pos[1] = csp->values[SATURATION] * 255 / 1000;
	  csp->pos[2] = csp->values[HUE] * 255 / 3600;
	  break;
	case SATURATION:
	  csp->pos[0] = csp->values[VALUE] * 255 / 1000;
	  csp->pos[1] = csp->values[HUE] * 255 / 3600;
	  csp->pos[2] = csp->values[SATURATION] * 255 / 1000;
	  break;
	case VALUE:
	  csp->pos[0] = csp->values[SATURATION] * 255 / 1000;
	  csp->pos[1] = csp->values[HUE] * 255 / 3600;
	  csp->pos[2] = csp->values[VALUE] * 255 / 1000;
	  break;
	}
    }
}

static void 
color_select_update_sliders (csp)
     ColorSelectP csp;
{
  int i;
  
  if (csp)
    {
      for (i = 0; i < 6; i++)
	XmScaleSetValue (csp->sliders[i], csp->values[i]);
    }
}

static void 
color_select_update_colors (csp, which)
     ColorSelectP csp;
     int which;
{
  Widget widget;
  Pixmap pixmap;
  Colormap cmap;
  XColor *color;
  Dimension width;
  Dimension height;

  if (csp)
    {
      if (which)
	{
	  widget = csp->orig_color;
	  pixmap = csp->orig_color_pixmap;
	  color = &csp->ocolor;
	  color->red = csp->orig_values[0] << 8;
	  color->green = csp->orig_values[1] << 8;
	  color->blue = csp->orig_values[2] << 8;
	  color->flags = DoRed | DoGreen | DoBlue;
	}
      else
	{
	  widget = csp->new_color;
	  pixmap = csp->new_color_pixmap;
	  color = &csp->ncolor;
	  color->red = csp->values[RED] << 8;
	  color->green = csp->values[GREEN] << 8;
	  color->blue = csp->values[BLUE] << 8;
	  color->flags = DoRed | DoGreen | DoBlue;
	}

      XtVaGetValues (widget, 
		     XmNcolormap, &cmap, 
		     XmNwidth, &width,
		     XmNheight, &height,
		     NULL);

      if (csp->alloc_colors)
	XStoreColor (DISPLAY, cmap, color);
      else
	if (!XAllocColor (DISPLAY, cmap, color))
	  fatal_error ("unable to allocate a single colormap entry");
      
      XSetForeground (DISPLAY, csp->gc, color->pixel);
      XFillRectangle (DISPLAY, pixmap, csp->gc,
		      0, 0, width, height);
      
      XCopyArea (DISPLAY, pixmap, XtWindow (widget), csp->gc,
		 0, 0, width, height, 0, 0);
    }
}

static void
color_select_ok_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      if (csp->callback)
	(* csp->callback) (csp->values[RED], csp->values[GREEN], csp->values[BLUE], 0);
    }
}

static void
color_select_cancel_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      if (csp->callback)
	(* csp->callback) (csp->orig_values[0], csp->orig_values[1], csp->orig_values[2], 1);
    }
}

static void
color_select_xy_draw_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      image_buf_put (csp->xy_color_image, XtWindow (csp->xy_color), csp->gc);
      color_select_draw_xy_marker (csp, 1);
    }
}

static void
color_select_xy_resize_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
    }
}

static void
color_select_xy_button_press (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonPressedEvent *bevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      bevent = (XButtonPressedEvent *) event;
      
      color_select_draw_xy_marker (csp, 1);

      csp->pos[0] = bevent->x;
      csp->pos[1] = 255 - bevent->y;

      if (csp->pos[0] < 0)
	csp->pos[0] = 0;
      if (csp->pos[0] > 255)
	csp->pos[0] = 255;
      if (csp->pos[1] < 0)
	csp->pos[1] = 0;
      if (csp->pos[1] > 255)
	csp->pos[1] = 255;

      color_select_draw_xy_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES);
    }
}

static void
color_select_xy_button_release (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonReleasedEvent *bevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      bevent = (XButtonReleasedEvent *) event;
     
      color_select_draw_xy_marker (csp, 1);

      csp->pos[0] = bevent->x;
      csp->pos[1] = 255 - bevent->y;

      if (csp->pos[0] < 0)
	csp->pos[0] = 0;
      if (csp->pos[0] > 255)
	csp->pos[0] = 255;
      if (csp->pos[1] < 0)
	csp->pos[1] = 0;
      if (csp->pos[1] > 255)
	csp->pos[1] = 255;

      color_select_draw_xy_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES);
    }
}

static void
color_select_xy_motion (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XMotionEvent *mevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      mevent = (XMotionEvent *) event;
      
      color_select_draw_xy_marker (csp, 1);

      csp->pos[0] = mevent->x;
      csp->pos[1] = 255 - mevent->y;

      if (csp->pos[0] < 0)
	csp->pos[0] = 0;
      if (csp->pos[0] > 255)
	csp->pos[0] = 255;
      if (csp->pos[1] < 0)
	csp->pos[1] = 0;
      if (csp->pos[1] > 255)
	csp->pos[1] = 255;

      color_select_draw_xy_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES);
    }
}

static void
color_select_z_draw_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      image_buf_put (csp->z_color_image, XtWindow (csp->z_color), csp->gc);
      color_select_draw_z_marker (csp, 1);
    }
}

static void
color_select_z_resize_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
    }
}

static void
color_select_z_button_press (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonPressedEvent *bevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      bevent = (XButtonPressedEvent *) event;

      color_select_draw_z_marker (csp, 1);

      csp->pos[2] = 255 - bevent->y;
      if (csp->pos[2] < 0)
	csp->pos[2] = 0;
      if (csp->pos[2] > 255)
	csp->pos[2] = 255;

      color_select_draw_z_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES | UPDATE_XY_COLOR);
    }
}

static void
color_select_z_button_release (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XButtonReleasedEvent *bevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      bevent = (XButtonReleasedEvent *) event;

      color_select_draw_z_marker (csp, 1);

      csp->pos[2] = 255 - bevent->y;
      if (csp->pos[2] < 0)
	csp->pos[2] = 0;
      if (csp->pos[2] > 255)
	csp->pos[2] = 255;

      color_select_draw_z_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES | UPDATE_XY_COLOR);
    }
}

static void
color_select_z_motion (drawing_a, client_data, event, continue_to_dispatch)
     Widget drawing_a;
     XtPointer client_data;
     XEvent * event;
     Boolean *continue_to_dispatch;
{
  XMotionEvent *mevent;
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      mevent = (XMotionEvent *) event;
     
      color_select_draw_z_marker (csp, 1);

      csp->pos[2] = 255 - mevent->y;
      if (csp->pos[2] < 0)
	csp->pos[2] = 0;
      if (csp->pos[2] > 255)
	csp->pos[2] = 255;

      color_select_draw_z_marker (csp, 1);

      color_select_update (csp, UPDATE_VALUES | UPDATE_XY_COLOR);
    }
}

static void
color_select_color_draw_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;

  csp = (ColorSelectP) client_data;
  if (csp)
    {
      if (w == csp->new_color)
	color_select_update (csp, UPDATE_NEW_COLOR);
      else if (w == csp->orig_color)
	color_select_update (csp, UPDATE_ORIG_COLOR);
    }
}

static void 
color_select_slider_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;
  XmScaleCallbackStruct *info;
  int old_values[6];
  int update_z_marker;
  int update_xy_marker;
  int i, j;

  csp = (ColorSelectP) client_data;
  info = (XmScaleCallbackStruct*) call_data;

  if (csp)
    {
      for (i = 0; i < 6; i++)
	if (csp->sliders[i] == w)
	  break;

      for (j = 0; j < 6; j++)
	old_values[j] = csp->values[j];
      
      csp->values[i] = info->value;
      
      if ((i >= HUE) && (i <= VALUE))
	color_select_update_rgb_values (csp);
      else if ((i >= RED) && (i <= BLUE))
	color_select_update_hsv_values (csp);
      color_select_update_sliders (csp);

      update_z_marker = 0;
      update_xy_marker = 0;
      for (j = 0; j < 6; j++)
	{
	  if (j == csp->z_color_fill)
	    {
	      if (old_values[j] != csp->values[j])
		update_z_marker = 1;
	    }
	  else
	    {
	      if (old_values[j] != csp->values[j])
		update_xy_marker = 1;
	    }
	}

      if (update_z_marker)
	{
	  color_select_draw_z_marker (csp, 1);
	  color_select_update (csp, UPDATE_POS | UPDATE_XY_COLOR);
	  color_select_draw_z_marker (csp, 1);
	}
      else
	{
	  if (update_z_marker)
	    color_select_draw_z_marker (csp, 1);
	  if (update_xy_marker)
	    color_select_draw_xy_marker (csp, 1);
	  
	  color_select_update (csp, UPDATE_POS);
	  
	  if (update_z_marker)
	    color_select_draw_z_marker (csp, 1);
	  if (update_xy_marker)
	    color_select_draw_xy_marker (csp, 1);
	}
      
      color_select_update (csp, UPDATE_NEW_COLOR);
    }
}

static void 
color_select_toggle_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ColorSelectP csp;
  XmToggleButtonCallbackStruct *tbcs;
  int i;

  csp = (ColorSelectP) client_data;
  tbcs = (XmToggleButtonCallbackStruct*) call_data;

  if (csp && tbcs->set)
    {
      for (i = 0; i < 6; i++)
	if (csp->toggles[i] == w)
	  break;

      switch ((ColorSelectFillType) i)
	{
	case HUE:
	  csp->z_color_fill = HUE;
	  csp->xy_color_fill = SATURATION_VALUE;
	  break;
	case SATURATION:
	  csp->z_color_fill = SATURATION;
	  csp->xy_color_fill = HUE_VALUE;
	  break;
	case VALUE:
	  csp->z_color_fill = VALUE;
	  csp->xy_color_fill = HUE_SATURATION;
	  break;
	case RED:
	  csp->z_color_fill = RED;
	  csp->xy_color_fill = GREEN_BLUE;
	  break;
	case GREEN:
	  csp->z_color_fill = GREEN;
	  csp->xy_color_fill = RED_BLUE;
	  break;
	case BLUE:
	  csp->z_color_fill = BLUE;
	  csp->xy_color_fill = RED_GREEN;
	  break;
	default:
	  break;
	}

      color_select_update (csp, UPDATE_POS);
      color_select_update (csp, UPDATE_Z_COLOR | UPDATE_XY_COLOR);
    }
}

static void 
color_select_image_fill (csi, type, values)
     ImageBuf csi;
     ColorSelectFillType type;
     int *values;
{
  ColorSelectFill csf;
  int height;

  csf.buffer = xmalloc (image_buf_width (csi) * 3);

  csf.update = update_procs[type];

  csf.y = -1;
  csf.width = image_buf_width (csi);
  csf.height = image_buf_height (csi);
  csf.values = values;

  height = image_buf_height (csi);
  if (height > 0)
    while (height--)
      {
	(* csf.update) (&csf);
	image_buf_draw_row (csi, csf.buffer, 0, csf.y, image_buf_width (csi));
      }

  xfree (csf.buffer);
}

static void
color_select_draw_z_marker (csp, update)
     ColorSelectP csp;
     int update;
{
  int width;
  int y;

  y = 255 - csp->pos[2];
  width = image_buf_width (csp->z_color_image);
  if (width <= 0)
    return;
  
  XSetFunction (DISPLAY, csp->gc, GXinvert);
  XDrawLine (DISPLAY, XtWindow (csp->z_color), csp->gc, 0, y, width, y);
  XSetFunction (DISPLAY, csp->gc, GXcopy);
}

static void
color_select_draw_xy_marker (csp, update)
     ColorSelectP csp;
     int update;
{
  int width;
  int height;
  int x, y;

  x = csp->pos[0];
  y = 255 - csp->pos[1];
  width = image_buf_width (csp->xy_color_image);
  height = image_buf_height (csp->xy_color_image);
  if ((width <= 0) || (height <= 0))
    return;
  
  XSetFunction (DISPLAY, csp->gc, GXinvert);
  XDrawLine (DISPLAY, XtWindow (csp->xy_color), csp->gc, 0, y, width, y);
  XDrawLine (DISPLAY, XtWindow (csp->xy_color), csp->gc, x, 0, x, height);
  XSetFunction (DISPLAY, csp->gc, GXcopy);
}

static void 
color_select_update_red (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, r;
  
  p = csf->buffer;

  csf->y += 1;
  r = (csf->height - csf->y + 1) * 255 / csf->height;

  if (r < 0)
    r = 0;
  if (r > 255)
    r = 255;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = r;
      *p++ = 0;
      *p++ = 0;
    }
}

static void
color_select_update_green (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, g;
  
  p = csf->buffer;

  csf->y += 1;
  g = (csf->height - csf->y + 1) * 255 / csf->height;

  if (g < 0)
    g = 0;
  if (g > 255)
    g = 255;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = 0;
      *p++ = g;
      *p++ = 0;
    }
}

static void
color_select_update_blue (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, b;
  
  p = csf->buffer;

  csf->y += 1;
  b = (csf->height - csf->y + 1) * 255 / csf->height;

  if (b < 0)
    b = 0;
  if (b > 255)
    b = 255;
  
  for (i = 0; i < csf->width; i++)
    {
      *p++ = 0;
      *p++ = 0;
      *p++ = b;
    }
}

static void
color_select_update_hue (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  float h, f;
  int r, g, b;
  int i;
  
  p = csf->buffer;

  csf->y += 1;
  h = csf->y * 360 / csf->height;

  h = 360 - h;

  if (h < 0)
    h = 0;
  if (h >= 360)
    h = 0;

  h /= 60;
  f = (h - (int) h) * 255;
  
  switch ((int) h)
    {
    case 0:
      r = 255;
      g = f;
      b = 0;
      break;
    case 1:
      r = 255 - f;
      g = 255;
      b = 0;
      break;
    case 2:
      r = 0;
      g = 255;
      b = f;
      break;
    case 3:
      r = 0;
      g = 255 - f;
      b = 255;
      break;
    case 4:
      r = f;
      g = 0;
      b = 255;
      break;
    case 5:
      r = 255;
      g = 0;
      b = 255 - f;
      break;
    }

  for (i = 0; i < csf->width; i++)
    {
      *p++ = r;
      *p++ = g;
      *p++ = b;
    }
}

static void
color_select_update_saturation (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int s;
  int i;
  
  p = csf->buffer;

  csf->y += 1;
  s = csf->y * 255 / csf->height;

  if (s < 0)
    s = 0;
  if (s > 255)
    s = 255;

  s = 255 - s;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = s;
      *p++ = s;
      *p++ = s;
    }
}

static void
color_select_update_value (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int v;
  int i;
  
  p = csf->buffer;

  csf->y += 1;
  v = csf->y * 255 / csf->height;

  if (v < 0)
    v = 0;
  if (v > 255)
    v = 255;

  v = 255 - v;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = v;
      *p++ = v;
      *p++ = v;
    }
}

static void 
color_select_update_red_green (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, r, b;
  float g, dg;
  
  p = csf->buffer;

  csf->y += 1;
  b = csf->values[BLUE];
  r = (csf->height - csf->y + 1) * 255 / csf->height;

  if (r < 0)
    r = 0;
  if (r > 255)
    r = 255;

  g = 0;
  dg = 255.0 / csf->width;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = r;
      *p++ = g;
      *p++ = b;

      g += dg;
    }
}

static void 
color_select_update_red_blue (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, r, g;
  float b, db;
  
  p = csf->buffer;

  csf->y += 1;
  g = csf->values[GREEN];
  r = (csf->height - csf->y + 1) * 255 / csf->height;

  if (r < 0)
    r = 0;
  if (r > 255)
    r = 255;

  b = 0;
  db = 255.0 / csf->width;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = r;
      *p++ = g;
      *p++ = b;

      b += db;
    }
}

static void 
color_select_update_green_blue (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  int i, g, r;
  float b, db;
  
  p = csf->buffer;

  csf->y += 1;
  r = csf->values[RED];
  g = (csf->height - csf->y + 1) * 255 / csf->height;

  if (g < 0)
    g = 0;
  if (g > 255)
    g = 255;

  b = 0;
  db = 255.0 / csf->width;

  for (i = 0; i < csf->width; i++)
    {
      *p++ = r;
      *p++ = g;
      *p++ = b;

      b += db;
    }
}

static void 
color_select_update_hue_saturation (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  float h, v, s, ds;
  int f;
  int i;

  p = csf->buffer;

  csf->y += 1;
  h = (255 - csf->y) * 359 / csf->height;

  if (h < 0)
    h = 0;
  if (h > 359)
    h = 359;

  h /= 60;
  f = (h - (int) h) * 255;
  
  s = 0;
  ds = 1.0 / csf->width;

  v = csf->values[VALUE] / 1000.0;
  
  switch ((int) h)
    {
    case 0:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * 255 * (1 - s);

	  s += ds;
	}
      break;
    case 1:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);

	  s += ds;
	}
      break;
    case 2:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v *255;
	  *p++ = v * (255 - (s * (255 - f)));

	  s += ds;
	}
      break;
    case 3:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;

	  s += ds;
	}
      break;
    case 4:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * (255 * (1 - s));
	  *p++ = v * 255;

	  s += ds;
	}
      break;
    case 5:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);

	  s += ds;
	}
      break;
    }
}

static void 
color_select_update_hue_value (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  float h, v, dv, s;
  int f;
  int i;

  p = csf->buffer;

  csf->y += 1;
  h = (255 - csf->y) * 359 / csf->height;

  if (h < 0)
    h = 0;
  if (h > 359)
    h = 359;

  h /= 60;
  f = (h - (int) h) * 255;
  
  v = 0;
  dv = 1.0 / csf->width;

  s = csf->values[SATURATION] / 1000.0;
  
  switch ((int) h)
    {
    case 0:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * 255 * (1 - s);

	  v += dv;
	}
      break;
    case 1:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);

	  v += dv;
	}
      break;
    case 2:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v *255;
	  *p++ = v * (255 - (s * (255 - f)));

	  v += dv;
	}
      break;
    case 3:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;

	  v += dv;
	}
      break;
    case 4:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * (255 * (1 - s));
	  *p++ = v * 255;

	  v += dv;
	}
      break;
    case 5:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);

	  v += dv;
	}
      break;
    }
}

static void 
color_select_update_saturation_value (csf)
     ColorSelectFill *csf;
{
  unsigned char *p;
  float h, v, dv, s;
  int f;
  int i;

  p = csf->buffer;

  csf->y += 1;
  s = (float) csf->y / csf->height;

  if (s < 0)
    s = 0;
  if (s > 1)
    s = 1;

  s = 1 - s;

  h = (float) csf->values[HUE] / 10;
  if (h >= 360)
    h -= 360;
  h /= 60;
  f = (h - (int) h) * 255;
  
  v = 0;
  dv = 1.0 / csf->width;

  switch ((int) h)
    {
    case 0:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * 255 * (1 - s);

	  v += dv;
	}
      break;
    case 1:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);

	  v += dv;
	}
      break;
    case 2:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v *255;
	  *p++ = v * (255 - (s * (255 - f)));

	  v += dv;
	}
      break;
    case 3:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);
	  *p++ = v * 255;

	  v += dv;
	}
      break;
    case 4:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * (255 - (s * (255 - f)));
	  *p++ = v * (255 * (1 - s));
	  *p++ = v * 255;

	  v += dv;
	}
      break;
    case 5:
      for (i = 0; i < csf->width; i++)
	{
	  *p++ = v * 255;
	  *p++ = v * 255 * (1 - s);
	  *p++ = v * (255 - s * f);

	  v += dv;
	}
      break;
    }
}
