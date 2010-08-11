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
#include "gdither.h"
#include "colormaps.h"
#include "gdisplay.h"
#include "general.h"
#include "view_ops.h"
#include "widget.h"

/*  Forward function declarations  */
static void destroy_dialog           (Widget, XtPointer, XtPointer);
static void popdown_dialog           (Widget, XtPointer, XtPointer);
static void ordered_callback         (Widget, XtPointer, XtPointer);
static void floyd_steinberg_callback (Widget, XtPointer, XtPointer);

/*  external variables  */
extern Widget interface;


static void
destroy_dialog (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GDisplay *gdisp;

  gdisp = (GDisplay *) client_data;

  if (gdisp)
    gdisp->view_options_dialog = NULL;
}

static void
popdown_dialog (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Widget shell = (Widget) client_data;

  XtPopdown (shell);
}


static void
ordered_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmToggleButtonCallbackStruct * cbs;

  cbs = (XmToggleButtonCallbackStruct *) call_data;

  if (cbs->set)
    dither_method_change ((GDisplay *) client_data, ORDERED);
}


static void
floyd_steinberg_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmToggleButtonCallbackStruct * cbs;

  cbs = (XmToggleButtonCallbackStruct *) call_data;

  if (cbs->set)
    dither_method_change ((GDisplay *) client_data, FLOYD_STEINBERG);
}


Widget
create_view_options_dialog (gdisp_ptr)
     XtPointer gdisp_ptr;
{
  Widget dialog, rowcol, form, options;
  Widget dither_frame;
  Widget radio_box, button;
  GDisplay * gdisp;
  int dither_type;
  Dimension x, y, width, height;
  char * title, * title_buf;
  
  gdisp = (GDisplay *) gdisp_ptr;

  XtVaGetValues (gdisp->disp_image->shell,
		 XmNx, &x,
		 XmNy, &y,
		 XmNwidth, &width,
		 XmNheight, &height,
		 NULL);

  dither_type = (gdisp) ? gdisp->dither_type : app_data.dither_type;

  /*  calculate the title for this dialog  */
  title = prune_filename (gdisp->gimage->filename);

  /*  allocate the title buffer  */
  title_buf = (char *) xmalloc (sizeof (char) * (strlen (title) + 15));
  sprintf (title_buf, "%s: View Options", title);

  /*  Create the main dialog shell  */
  dialog = XtVaCreatePopupShell ("viewOptions",
				 xmDialogShellWidgetClass, get_top_shell (interface),
				 XmNtitle, title_buf,
				 XmNdeleteResponse, XmDESTROY,
				 XmNx, x + (width>>1),
				 XmNy, y + (height>>1),
				 NULL);

  xfree (title_buf);

  rowcol = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass,
			     dialog,
			     XmNpacking, XmPACK_TIGHT,
			     XmNorientation, XmVERTICAL,
			     XmNnumColumns, 1,
			     NULL);

  /*  here is the 8-bit color option area  */
  if (gdisp->gimage->type == RGB_GIMAGE && gdisp->depth == 8)
    {
      options = XtVaCreateManagedWidget ("options", xmRowColumnWidgetClass, rowcol,
					 XmNpacking, XmPACK_COLUMN,
					 XmNorientation, XmHORIZONTAL,
					 XmNnumColumns, 1,
					 NULL);
      
      /*  Create the dithering options section  */
      dither_frame = XtVaCreateManagedWidget ("ditherFrame", xmFrameWidgetClass, options,
					      XmNshadowType, XmSHADOW_ETCHED_IN,
					      NULL);
      
      XtVaCreateManagedWidget ("ditherOptions",
			       xmLabelGadgetClass, dither_frame,
			       XmNchildType, XmFRAME_TITLE_CHILD,
			       XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
			       NULL);
      
      radio_box = XmCreateRadioBox (dither_frame, "dither_radio", NULL, 0);
      
      
      button = XtVaCreateManagedWidget ("ordered",
					xmToggleButtonGadgetClass, radio_box,
					XmNset, (dither_type == ORDERED),
					NULL);
      XtAddCallback (button, XmNvalueChangedCallback,
		     ordered_callback, (XtPointer) gdisp);
      
      button = XtVaCreateManagedWidget ("floyd_steinberg",
					xmToggleButtonGadgetClass, radio_box,
					XmNset, (dither_type == FLOYD_STEINBERG),
					NULL);
      XtAddCallback (button, XmNvalueChangedCallback,
		     floyd_steinberg_callback, (XtPointer) gdisp);
      
      XtManageChild (radio_box);
      
    }
  
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

  XtAddCallback (button, XmNactivateCallback, popdown_dialog, dialog);

  XtAddCallback (dialog, XtNdestroyCallback, destroy_dialog, gdisp);

  XtManageChild (form);
  XtManageChild (rowcol);

  return dialog;
}







