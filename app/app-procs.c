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
#include <signal.h>
#include <stdlib.h>

#include "appenv.h"
#include "brushes.h"
#include "gdisplay.h"
#include "colormaps.h"
#include "global_edit.h"
#include "plug_in.h"
#include "undo.h"
#include "visual.h"
#include "paint_funcs.h"
#include "palette.h"
#include "temp_buf.h"
#include "widget.h"

/*  Global variable for application modal dialog boxes  */
static int done = 0;

/*  Function prototype for affirmation dialog when exiting application  */
static int really_quit_dialog (void);


void
app_init ()
{
  get_standard_visuals ();
  create_standard_colormaps ();
  dither_start_engine ();
  get_active_brush ();
  paint_funcs_setup ();
}


void
app_exit (kill_it)
     int kill_it;
{
  if (kill_it == 0)  /*  If it's the user's perogative  */
    if (!really_quit_dialog ())
      return;
  free_standard_colormaps ();
  dither_shutdown_engine ();
  gdisplays_delete ();
  undo_free ();
  global_edit_free ();
  named_buffers_free ();
  swapping_free ();
  brushes_free ();
  brush_select_dialog_free ();
  palette_free ();
  plug_in_kill ();
  free_chunks ();
  exit (0);
}

/********************************************************
 *   Routines to query exiting the application          *
 ********************************************************/

static void
really_quit_callback (w, client_data, call_data)
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


static int
really_quit_dialog ()
{
  Widget dialog;
  XmString warning;

  done = 0;

  /*  If no images need saving, don't bother  */
  if (!gdisplays_dirty ())
    return 1;

  /*  Create the dialog box to ask whether image should be saved  */
  warning = XmStringCreateLocalized ("Some files unsaved.  Quit the GIMP?");
  dialog = XmCreateWarningDialog (toplevel, "Warning!", NULL, 0);
  XtVaSetValues (dialog, XmNmessageString, warning, NULL);
  XtVaSetValues (dialog,
		 XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		 XmNdefaultPosition, False, NULL);
  XtUnmanageChild (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
  XtAddCallback (dialog, XmNokCallback, really_quit_callback, (XtPointer) &done);
  XtAddCallback (dialog, XmNcancelCallback, really_quit_callback, (XtPointer) &done);
  XtAddCallback (dialog, XmNmapCallback, map_dialog, NULL);
  XmStringFree (warning);

  XtManageChild (dialog);
  XtPopup (XtParent (dialog), XtGrabNone);

  while (done == 0)
    XtAppProcessEvent (app_context, XtIMAll);

  XtPopdown (XtParent (dialog));

  switch (done)
    {
    case XmCR_OK :
      return 1;
      break;
    case XmCR_CANCEL :
      return 0;
      break;
    default :
      return 0;
    }
}


