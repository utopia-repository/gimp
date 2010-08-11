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
#include "callbacks.h"
#include "widget.h"
#include "menus.h"
#include "visual.h"
#include "gimage.h"
#include "general.h"
#include "linked.h"
#include "buildmenu.h"
#include "errors.h"

#define  bounds(a,x,y)  ((a < x) ? x : ((a > y) ? y : a))

/*  Global main interface FORM widget */
Widget interface;

#include "pixmaps.h"

static char * tool_classes[] =
{
  "rectSelectButton",
  "ellipseSelectButton",
  "freeSelectButton",
  "fuzzySelectButton",
  "bezierSelectButton",
  "iscissorsButton",
  "cropButton",
  "transformToolButton",
  "flipHToolButton",
  "flipVToolButton",
  "colorPickerButton",
  "bucketFillButton",
  "paintbrushToolButton",
  "airbrushToolButton",
  "cloneToolButton",
  "convolveToolButton",
  "blendToolButton",
  "textToolButton",
};

static char **tools_data[] = 
{
  (char **) rect_select_bits,
  (char **) ellipse_select_bits,
  (char **) free_select_bits,
  (char **) fuzzy_select_bits,
  (char **) bezier_select_bits,
  (char **) iscissors_bits,
  (char **) crop_bits,
  (char **) scale_tool_bits,
  (char **) flip_htool_bits,
  (char **) flip_vtool_bits,
  (char **) color_picker_bits,
  (char **) bucket_fill_bits,
  (char **) brush_bits,
  (char **) airbrush_bits,
  (char **) clone_bits,
  (char **) convolve_bits,
  (char **) blend_bits,
  (char **) text_tool_bits,
};

static int pixmap_colors[8][3] =
{
  { 0x00, 0x00, 0x00 }, /* a - 0   */
  { 0x24, 0x24, 0x24 }, /* b - 36  */
  { 0x49, 0x49, 0x49 }, /* c - 73  */
  { 0x6D, 0x6D, 0x6D }, /* d - 109 */
  { 0x92, 0x92, 0x92 }, /* e - 146 */
  { 0xB6, 0xB6, 0xB6 }, /* f - 182 */
  { 0xDB, 0xDB, 0xDB }, /* g - 219 */
  { 0xFF, 0xFF, 0xFF }, /* h - 255 */
};

/*  the tool widget array  */
Widget tool_widgets [XtNumber (tool_classes)];


#define COLUMNS 6
#define ROWS    3
#define MARGIN  2

static void
build_tool_panel (row_col)
     Widget row_col;
{
  Pixel fg, bg;
  int depth;
  Pixmap pixmap1, pixmap2;
  Colormap cmap;
  XColor xcolor[8];
  GC gc;
  int i, j, icon;
  Dimension left, right, top, bottom;
  unsigned char value, last_value;
  int r, s, w;

  XtVaGetValues (interface,
                 XmNforeground, &fg,
                 XmNbackground, &bg,
		 XmNdepth, &depth,
                 NULL);

  icon = 0;

  gc = XCreateGC (DISPLAY, DefaultRootWindow (DISPLAY), 0, NULL);
  cmap = DefaultColormap (DISPLAY, DefaultScreen (DISPLAY));

  for (i = 0; i < 8; i++)
    {
      xcolor[i].red = pixmap_colors[i][0] << 8;
      xcolor[i].blue = pixmap_colors[i][1] << 8;
      xcolor[i].green = pixmap_colors[i][2] << 8;
      xcolor[i].flags = DoRed | DoBlue | DoGreen;

      XAllocColor (DISPLAY, cmap, &xcolor[i]);
    }

  for (j = 0; j < ROWS; j++)
    for (i = 0; i < COLUMNS && icon < XtNumber (tool_classes); i++)
      {
	pixmap1 = XCreatePixmap (DISPLAY, DefaultRootWindow (DISPLAY), 32, 32, depth);
	pixmap2 = XCreatePixmap (DISPLAY, DefaultRootWindow (DISPLAY), 32, 32, depth);
	
	XSetForeground (DISPLAY, gc, bg);
	XFillRectangle (DISPLAY, pixmap1, gc, 0, 0, 32, 32);
	
	XSetForeground (DISPLAY, gc, fg);
	XFillRectangle (DISPLAY, pixmap2, gc, 0, 0, 32, 32);
	
	for (r = 0; r < 32; r++)
	  {
	    w = 0;
	    last_value = 0;
	    
	    for (s = 0; s < 32; s++)
	      {
		value = tools_data[icon][r][s];
		
		if (w && (value != last_value))
		  {
		    XSetForeground (DISPLAY, gc, xcolor[last_value - 'a'].pixel);
		    XDrawLine (DISPLAY, pixmap1, gc, s-(w-1), r, s, r);
		    XDrawLine (DISPLAY, pixmap2, gc, s-(w-1), r, s, r);
		    w = 0;
		  }
		
		if (value != '.')
		  w++;
		
		last_value = value;
	      }
	    
	    if (w)
	      {
		XSetForeground (DISPLAY, gc, xcolor[last_value - 'a'].pixel);
		XDrawLine (DISPLAY, pixmap1, gc, s-(w-1), r, s, r);
		XDrawLine (DISPLAY, pixmap2, gc, s-(w-1), r, s, r);
	      }
	  }

	top = (Dimension) (((float) j / (float) ROWS) * (100 - MARGIN * 2)) + MARGIN;
	left = (Dimension) (((float) i / (float) COLUMNS) * (100 - MARGIN * 2)) + MARGIN;
	right = (Dimension) (((float) (i + 1) / (float) COLUMNS) * (100 - MARGIN * 2)) + MARGIN;
	bottom = (Dimension) (((float) (j+1) / (float) ROWS) * (100 - MARGIN * 2)) + MARGIN;
	
	tool_widgets[icon] = XtVaCreateManagedWidget (tool_classes[icon], 
						      xmPushButtonWidgetClass, row_col,
						      XmNlabelPixmap, pixmap1,
						      XmNuserData, pixmap2,
						      XmNlabelType, XmPIXMAP,
						      XmNtopAttachment, XmATTACH_POSITION,
						      XmNleftAttachment, XmATTACH_POSITION,
						      XmNrightAttachment, XmATTACH_POSITION,
						      XmNbottomAttachment, XmATTACH_POSITION,
						      XmNtopPosition, top,
						      XmNleftPosition, left,
						      XmNrightPosition, right,
						      XmNbottomPosition, bottom,
						      NULL);
	
	XtAddCallback (tool_widgets[icon], XmNactivateCallback,
		       select_tool_callback, (XtPointer) icon);
	
	icon ++;
      }
  
  XFreeGC (DISPLAY, gc);
}


void
popup_menu(w, client_data, event)
     Widget w;
     XtPointer client_data;
     XEvent *event;
{
  Widget popup = (Widget) client_data;
 
  XmMenuPosition(popup, (XButtonPressedEvent *) event);
  XtManageChild(popup);
}
 

Widget
get_top_shell (w)
     Widget w;
{
  while (w && !XtIsWMShell (XtParent(w)))
    {
      w = XtParent (w);
    }

  return w;
}


void
create_widgets (top)
     Widget top;
{
  int i;
  Widget form;
  Widget tool_frame, tool_panel;
  Widget menu_bar;
  Visual *visual;
  Colormap cmap;
  int depth;

  cmap = DefaultColormapOfScreen (XtScreen (toplevel));
  visual = DefaultVisualOfScreen (XtScreen (toplevel));
  depth = DefaultDepthOfScreen (XtScreen (toplevel));

  /* Create main widget as a main window widget */
  interface = XtVaCreateWidget ("interface", xmMainWindowWidgetClass, toplevel,
				XmNvisual, visual,
				XmNcolormap, cmap,
				XmNdepth, depth,
				NULL);

  /*  create the menu information  */
  menu_bar = XtVaCreateManagedWidget ("menuBar", xmRowColumnWidgetClass, interface,
				      XmNrowColumnType, XmMENU_BAR,
				      NULL);
  for (i = 0; i < num_menus; i++)
    mainmenu_widgets [i] = BuildMenu (menu_bar, XmMENU_PULLDOWN,
				      get_menu_class (i), 0, True,
				      get_menu_options (i),
				      visual, cmap, depth);

  form = XtVaCreateWidget ("mainRowCol", xmRowColumnWidgetClass, interface,
/*			   XmNorientation, XmVERTICAL,
			   XmNpacking, XmPACK_TIGHT,
			   XmNnumColumns, 1,*/
			   NULL);
			   
  tool_frame = XtVaCreateManagedWidget ("toolFrame", xmFrameWidgetClass, form,
					XmNshadowType, XmSHADOW_ETCHED_IN,
					NULL);
      
/*
  XtVaCreateManagedWidget ("toolFrameLabel", xmLabelGadgetClass, tool_frame,
			   XmNchildType, XmFRAME_TITLE_CHILD,
			   XmNchildVerticalAlignment, XmALIGNMENT_WIDGET_TOP,
			   NULL);
*/
  tool_panel = XtVaCreateWidget ("toolPanel",
				 xmFormWidgetClass, tool_frame,
				 NULL);
  
  /* status panel */
/*  status_panel = XtVaCreateWidget ("statusPanel",
				   xmRowColumnWidgetClass, form,
				   XmNorientation, XmHORIZONTAL,
				   XmNpacking, XmPACK_TIGHT,
				   NULL);
*/
   /* load label pixmap */
/*  pixmap = XmGetPixmap (XtScreen (status_panel), app_data.insignia, fg, bg);
			

  if (pixmap != XmUNSPECIFIED_PIXMAP)
    XtVaCreateManagedWidget ("insignia", xmLabelGadgetClass,
			     status_panel,
			     XmNlabelType, XmPIXMAP,
			     XmNlabelPixmap, pixmap,
			     NULL);
*/
  /* status display field */
/*  XtVaCreateManagedWidget ("Current Status: ", xmLabelGadgetClass,
			   status_panel, NULL);

  m->status_bar = XtVaCreateManagedWidget ("statusBar", xmTextFieldWidgetClass,
					   status_panel,
					   XmNcursorPositionVisible, False,
					   NULL);
*/
  build_tool_panel (tool_panel);

  XtManageChild (interface);
  XtManageChild (form);
  XtManageChild (tool_panel);

/*  XtManageChild (status_panel);*/
}


void
create_display_shell (shell, drawing_area, hsb, vsb, popup, width,
		      height, scale, title, cmap, visual, type)
     Widget *shell;
     Widget *drawing_area;
     Widget *hsb, *vsb;
     Widget *popup;
     long width, height;
     unsigned int * scale;
     char *title;
     Colormap cmap;
     Visual * visual;
     Boolean type;
{
  Widget scrolled_win;
  int depth;
  Visual * default_visual;
  Colormap default_cmap;
/*  Dimension shadow;*/
  int default_depth;
  int n_width, n_height;
  int s_width, s_height;
  int scalesrc, scaledest;

  /*  adjust the initial scale -- so that window fits on screen */
  {
    s_width = WidthOfScreen (XtScreen (toplevel));
    s_height = HeightOfScreen (XtScreen (toplevel));

    scalesrc = *scale & 0x00ff;
    scaledest = *scale >> 8;

    n_width = (width * scaledest) / scalesrc;
    n_height = (height * scaledest) / scalesrc;

    /*  Limit to the size of the screen...  */
    while (n_width > s_width || n_height > s_height)
      {
	if (scaledest > 1)
	  scaledest--;
	else
	  if (scalesrc < 0xff)
	    scalesrc++;

	n_width = (width * scaledest) / scalesrc;
	n_height = (height * scaledest) / scalesrc;
      }

    *scale = (scaledest << 8) + scalesrc;

  }

  switch (type)
    {    
    case RGB_GIMAGE : case INDEXED_GIMAGE :
      depth = color_depth;
      break;
    case GREY_GIMAGE :
      depth = grey_depth;
      break;
    }

  *shell = XtVaCreatePopupShell ("imageWin",
				 topLevelShellWidgetClass, interface,
				 XtNtitle, title,
				 XmNvisual, visual,
				 XmNcolormap, cmap,
				 XmNdepth, depth,
				 NULL);

  scrolled_win = XtVaCreateWidget ("scrolledViewPort",
				   xmScrolledWindowWidgetClass, *shell,
				   XmNscrollingPolicy, XmAPPLICATION_DEFINED,
				   XmNvisualPolicy, XmVARIABLE,
				   NULL);

  *drawing_area = XtVaCreateManagedWidget ("canvas",
					   xmDrawingAreaWidgetClass, scrolled_win,
					   XmNwidth, n_width,
					   XmNheight, n_height,
					   NULL);

  *vsb = XtVaCreateManagedWidget ("scrollBar",
				  xmScrollBarWidgetClass, scrolled_win,
				  XmNorientation, XmVERTICAL,
				  NULL);
  
  *hsb = XtVaCreateManagedWidget ("scrollBar",
				  xmScrollBarWidgetClass, scrolled_win,
				  XmNorientation, XmHORIZONTAL,
				  NULL);

/*
  XtVaGetValues (scrolled_win, XmNshadowThickness, &shadow, NULL);

  XtVaSetValues (scrolled_win, XmNwidth, n_width + shadow*2,
		 XmNheight, n_height + shadow*2, NULL);
*/

  XmScrolledWindowSetAreas (scrolled_win, *hsb, *vsb, *drawing_area);
  
  /*  manage the dialog  */
  XtManageChild (scrolled_win);
  XtManageChild (*shell);

  /*  create the menu information  */
  /*  disable the tear-off feature for now  */

  default_cmap = DefaultColormapOfScreen (XtScreen (toplevel));
  default_visual = DefaultVisualOfScreen (XtScreen (toplevel));
  default_depth = DefaultDepthOfScreen (XtScreen (toplevel));

  *popup = BuildMenu (scrolled_win, XmMENU_POPUP, "imageMenu",
		      0, False, get_image_menu_options (), 
		      default_visual, default_cmap, default_depth);

  XtAddEventHandler (scrolled_win, ButtonPressMask, False, popup_menu, *popup);
}


void
map_dialog (dialog, client_data, call_data)
     Widget dialog;
     XtPointer client_data;
     XtPointer call_data;
{
  Window root, child;
  int root_x, root_y;
  int child_x, child_y;
  unsigned int mask;
  int s_width, s_height;
  Dimension d_width, d_height;
  Dimension start_x, start_y;
  
  XQueryPointer (DISPLAY, DefaultRootWindow (DISPLAY),
		 &root, &child,
		 &root_x, &root_y,
		 &child_x, &child_y,
		 &mask);
  
  XtVaGetValues (dialog,
		 XmNwidth, &d_width,
		 XmNheight, &d_height,
		 NULL);

  s_width = WidthOfScreen (XtScreen (dialog));
  s_height = HeightOfScreen (XtScreen (dialog));

  root_x -= (d_width>>1);
  root_y -= (d_height>>1);

  start_x = bounds (root_x, 0, (s_width - d_width));
  start_y = bounds (root_y, 0, (s_height - d_height));

  XtVaSetValues (dialog,
		 XmNx, start_x,
		 XmNy, start_y,
		 NULL);
}

