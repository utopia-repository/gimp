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
#include "colormaps.h"
#include "info_window.h"
#include "gdisplay.h"
#include "general.h"
#include "widget.h"
#include "visual.h"


/*  The different classes of visuals  */
static char *visual_classes[] =
{
  "StaticGray",
  "GrayScale",
  "StaticColor",
  "PseudoColor",
  "TrueColor",
  "DirectColor",
};


/*  Callbacks - forward declarations  */
static void OK_info_callback      (Widget, XtPointer, XtPointer);
static void destroy_info_callback (Widget, XtPointer, XtPointer);


static void
create_info_line (parent, category, info)
     Widget parent;
     char * category;
     char * info;
{
  XtVaCreateManagedWidget (category, xmLabelGadgetClass, parent, NULL);

  XtVaCreateManagedWidget (info, xmTextWidgetClass, parent,
			   XmNeditable, False,
			   XmNcursorPositionVisible, False,
			   XmNvalue, info,
			   NULL);
}

static void
get_shades (gdisp, buf)
     GDisplay * gdisp;
     char * buf;
{
  switch (gdisp->gimage->type)
    {
    case GREY_GIMAGE :
      if (emulate_grey)
	switch (gdisp->depth)
	  {
	  case 15 : case 16 :
	    sprintf (buf, "%d",
		     MINIMUM (MINIMUM ((1 << (8 - red_prec)), (1 << (8 - green_prec))),
			      (1 << (8 - blue_prec))));
	    break;
	  case 24 :
	    sprintf (buf, "256");
	    break;
	  }
      else
	sprintf (buf, "%d", shades_grey);
      break;
    case RGB_GIMAGE :
      switch (gdisp->depth)
	{
	case 8 :
	  sprintf (buf, "%d / %d / %d", shades_r, shades_g, shades_b);
	  break;
	case 15 : case 16 :
	  sprintf (buf, "%d / %d / %d",
		   (1 << (8 - red_prec)),
		   (1 << (8 - green_prec)),
		   (1 << (8 - blue_prec)));
	  break;
	case 24 :
	  sprintf (buf, "256 / 256 / 256");
	  break;
	}
      break;

    case INDEXED_GIMAGE :
      sprintf (buf, "%d", gdisp->gimage->num_cols);
      break;
    }
}



  /*  displays information:
   *    image name
   *    image width, height
   *    zoom ratio
   *    image color type
   *    System info:
   *      Using shared X Images?
   *      visual class
   *      visual depth
   */

Widget
create_info_window (gdisp_ptr)
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  Widget shell, dialog, rowcol, form, info;
  Widget button;
  char buf[256];
  char * title, * title_buf;
  
  gdisp = (GDisplay *) gdisp_ptr;

  title = prune_filename (gdisp->gimage->filename);

  /*  allocate the title buffer  */
  title_buf = (char *) xmalloc (sizeof (char) * (strlen (title) + 15));
  sprintf (title_buf, "%s: Window Info", title);

  /*  Create the main dialog shell  */
  shell = XtVaCreatePopupShell ("windowInfoDialog",
				xmDialogShellWidgetClass, toplevel,
				XmNtitle, title_buf,
				XmNallowResize, False,
				XmNdeleteResponse, XmDESTROY,
				NULL);

  /*  free the title buffer  */
  xfree (title_buf);

  dialog = XtVaCreateWidget ("bulletinBoard", xmBulletinBoardWidgetClass, shell,
			     NULL);

  rowcol = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass, dialog,
			     XmNpacking, XmPACK_TIGHT,
			     XmNorientation, XmVERTICAL,
			     XmNnumColumns, 1,
			     NULL);

  info = XtVaCreateWidget ("info", xmRowColumnWidgetClass, rowcol,
			   XmNpacking, XmPACK_COLUMN,
			   XmNnumColumns, 7,
			   XmNorientation, XmHORIZONTAL,
			   XmNisAligned, True,
			   XmNentryAlignment, XmALIGNMENT_BEGINNING,
			   NULL);

  /*  width and height  */
  sprintf (buf, "%d x %d", (int) gdisp->gimage->width, (int) gdisp->gimage->height);
  create_info_line (info, "Dimensions (w x h):", buf);

  /*  zoom ratio  */
  sprintf (buf, "%d:%d", SCALEDEST (gdisp), SCALESRC (gdisp));
  create_info_line (info, "Scale Ratio:", buf);

  /*  color type  */
  if (gdisp->gimage->type == RGB_GIMAGE)
    sprintf (buf, "%s", "RGB Color");
  else if (gdisp->gimage->type == GREY_GIMAGE)
    sprintf (buf, "%s", "Grayscale");
  else if (gdisp->gimage->type == INDEXED_GIMAGE)
    sprintf (buf, "%s", "Indexed Color");
  create_info_line (info, "Display Type:", buf);

  /*  Shared memory?  */
  if (gdisp->disp_image->type == MIT_SHM)
    sprintf (buf, "%s", "MIT SHM extension");
  else
    sprintf (buf, "%s", "Standard Xlib");
  create_info_line (info, "XImage Model:", buf);

  /*  visual class  */
  if (gdisp->gimage->type == RGB_GIMAGE || gdisp->gimage->type == INDEXED_GIMAGE)
    sprintf (buf, "%s", visual_classes[color_class]);
  else if (gdisp->gimage->type == GREY_GIMAGE)
    sprintf (buf, "%s", visual_classes[grey_class]);
  create_info_line (info, "Visual Class:", buf);

  /*  visual depth  */
  sprintf (buf, "%d", gdisp->disp_image->depth);
  create_info_line (info, "Visual Depth:", buf);

  /*  pure color shades  */
  get_shades (gdisp, buf);
  if (gdisp->gimage->type == RGB_GIMAGE)
    create_info_line (info, "Shades of Color:", buf);
  else if (gdisp->gimage->type == INDEXED_GIMAGE)
    create_info_line (info, "Shades:", buf);
  else if (gdisp->gimage->type == GREY_GIMAGE)
    create_info_line (info, "Shades of Gray:", buf);

  /* Create the action area  */
  form = XtVaCreateWidget ("form", xmFormWidgetClass, rowcol,
			   XmNfractionBase, 3,
			   NULL);

  button = XtVaCreateManagedWidget ("OK", xmPushButtonGadgetClass, form,
				    XmNtopAttachment, XmATTACH_FORM,
				    XmNbottomAttachment, XmATTACH_FORM,
				    XmNleftAttachment, XmATTACH_POSITION,
				    XmNrightAttachment, XmATTACH_POSITION,
				    XmNleftPosition, 1,
				    XmNrightPosition, 2,
				    XmNshowAsDefault, True,
				    NULL);

  XtAddCallback (button, XmNactivateCallback, OK_info_callback, shell);
  XtAddCallback (button, XmNdestroyCallback, destroy_info_callback, gdisp_ptr);

  XtVaSetValues (dialog, XmNdefaultPosition, False, NULL);
  XtAddCallback (dialog, XmNmapCallback, map_dialog, NULL);

  XtManageChild (form);
  XtManageChild (info);
  XtManageChild (rowcol);
  XtManageChild (dialog);

  return shell;
}


static void
OK_info_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Widget shell = (Widget) client_data;

  XtPopdown (shell);
}


static void
destroy_info_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{ 
  GDisplay *gdisp;

  gdisp = (GDisplay *) client_data;

  if (gdisp)
    gdisp->window_info_dialog = NULL;
}




