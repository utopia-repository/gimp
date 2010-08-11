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
#include "errors.h"
#include "progress.h"
#include "widget.h"

#define PROGRESS_WIDTH 250
#define PROGRESS_HEIGHT 20

static void progress_draw_callback (Widget, XtPointer, XtPointer);
static void progress_cancel_callback (Widget, XtPointer, XtPointer);

static ActionAreaItem action_items[2] = 
{
  { "Cancel", progress_cancel_callback, NULL, NULL },
  { "Cancel All", progress_cancel_callback, NULL, NULL },
};

ProgressP 
progress_new (title, label, callback, callback_data)
     char *title, *label;
     CancelCallback callback;
     void *callback_data;
{
  ProgressP progress;
  Widget frame_widget;
  Widget label_widget;
  Widget action_widget;
  XGCValues gcv;
  GC gc;

  progress = xmalloc (sizeof (_Progress));

  progress->amount = 0;
  progress->callback = callback;
  progress->callback_data = callback_data;
  progress->width = PROGRESS_WIDTH;
  progress->height = PROGRESS_HEIGHT;

  progress->shell = XtVaCreatePopupShell ("progressDialog",
					  xmDialogShellWidgetClass, toplevel,
					  XmNtitle, title,
					  XmNdeleteResponse, XmDESTROY,
					  NULL);

  progress->dialog = XtVaCreateWidget ("dialog",
				       xmFormWidgetClass, progress->shell,
				       XmNfractionBase, 22,
				       NULL);

  label_widget = XtVaCreateManagedWidget (label,
					  xmLabelWidgetClass, progress->dialog,
					  XmNalignment, XmALIGNMENT_BEGINNING,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_NONE,
					  XmNleftOffset, 10,
					  XmNrightOffset, 10,
					  XmNtopOffset, 10,
					  XmNbottomOffset, 10,
					  NULL);
					  
				   
  frame_widget = XtVaCreateManagedWidget ("frame",
					  xmFrameWidgetClass, progress->dialog,
					  XmNshadowType, XmSHADOW_IN,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_WIDGET,
					  XmNtopWidget, label_widget,
					  XmNbottomAttachment, XmATTACH_NONE,
					  XmNleftOffset, 10,
					  XmNrightOffset, 10,
					  XmNtopOffset, 10,
					  XmNbottomOffset, 10,
					  NULL);
  
  progress->pbar = XtVaCreateManagedWidget ("drawingArea", 
					    xmDrawingAreaWidgetClass, frame_widget,
					    XmNwidth, progress->width,
					    XmNheight, progress->height,
					    NULL);
  XtAddCallback (progress->pbar, XmNexposeCallback, progress_draw_callback, NULL);
  XtAddCallback (progress->pbar, XmNresizeCallback, progress_draw_callback, NULL);


  action_items[0].data = progress;
  action_items[1].data = progress;
  action_widget = build_action_area (progress->dialog, action_items, 2);
  XtVaSetValues (action_widget,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, frame_widget,
		 XmNbottomOffset, 10,
		 XmNtopOffset, 10,
		 NULL);
  XtManageChild (action_widget);

  progress->cancel_cur = action_items[0].widget;
  progress->cancel_all = action_items[1].widget;

  XtVaSetValues (progress->dialog, XmNdefaultPosition, False, NULL);
/*  XtAddCallback (progress->dialog, XmNmapCallback, map_dialog, NULL); */

  XtManageChild (progress->dialog);

  progress->pixmap = XCreatePixmap (DISPLAY, XtWindow (progress->pbar), 
				    progress->width, progress->height,
				    DefaultDepthOfScreen (XtScreen (progress->pbar)));

  gcv.foreground = BlackPixelOfScreen (XtScreen (progress->pbar));
  gc = XCreateGC (DISPLAY, XtWindow (progress->pbar), GCForeground, &gcv);
  XtVaSetValues (progress->pbar, XmNuserData, gc, NULL);
  progress_update (progress, 0);

  return progress;
}

void 
progress_free (progress)
     ProgressP progress;
{
  GC gc;

  if (progress)
    {
      XtVaGetValues (progress->pbar, XmNuserData, &gc, NULL);
      XFreeGC (DISPLAY, gc);
      XFreePixmap (DISPLAY, progress->pixmap);
      XtDestroyWidget (progress->pbar);
      XtDestroyWidget (progress->dialog);
      XtDestroyWidget (progress->shell);
      xfree (progress);
    }
}

void 
progress_update (progress, amount)
     ProgressP progress;
     long amount;
{
  if (progress)
    {
      progress->amount = amount;
      progress_draw_callback (progress->pbar, progress, NULL);
    }
}

static void
progress_draw_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmDrawingAreaCallbackStruct *cbs;
  GC gc;
  ProgressP progress;
  Dimension width, height;
  Dimension amount, sthickness;
  Pixel foreground;
  Pixel background;
  Pixel top_shadow;
  Pixel bottom_shadow;
  int i;

  if (call_data)
    {
      cbs = (XmDrawingAreaCallbackStruct*) call_data;
      if (cbs->reason == XmCR_RESIZE)
	return;
    }

  progress = (ProgressP) client_data;
  if (progress)
    {
      XtVaGetValues (progress->pbar, 
		     XmNuserData, &gc, 
		     XmNwidth, &width,
		     XmNheight, &height,
		     NULL);

      if ((width != progress->width) || (height != progress->height))
	{
	  progress->width = width;
	  progress->height = height;

	  XFreePixmap (DISPLAY, progress->pixmap);
	  progress->pixmap = XCreatePixmap (DISPLAY, XtWindow (progress->pbar), 
					    progress->width, progress->height,
					    DefaultDepthOfScreen (XtScreen (progress->pbar)));
	}

      if (!call_data)
	{
	  XtVaGetValues (progress->pbar,
			 XmNbackground, &foreground,
			 NULL);
	  
	  XtVaGetValues (progress->cancel_cur,
			 XmNshadowThickness, &sthickness,
			 XmNarmColor, &background,
			 XmNtopShadowColor, &top_shadow,
			 XmNbottomShadowColor, &bottom_shadow,
			 NULL);
	  
	  amount = (progress->amount * width) / 100;
	  
	  XSetForeground (DISPLAY, gc, foreground);
	  XFillRectangle (DISPLAY, progress->pixmap, gc, 
			  0, 0, amount, height);
	  
	  XSetForeground (DISPLAY, gc, background);
	  XFillRectangle (DISPLAY, progress->pixmap, gc,
			  amount, 0, width, height);
	  
	  XSetForeground (DISPLAY, gc, bottom_shadow);
	  
	  for (i = 0; i < sthickness; i++)
	    {
	      XDrawLine (DISPLAY, progress->pixmap, gc,
			 i, height-1-i, amount-1, height-1-i);
	      XDrawLine (DISPLAY, progress->pixmap, gc,
			 amount-1-i, i, amount-1-i, height-1);
	    }
	  
	  XSetForeground (DISPLAY, gc, top_shadow);
	  for (i = 0; i < sthickness; i++)
	    {
	      XDrawLine (DISPLAY, progress->pixmap, gc,
			 i, i, amount-1-i, i);
	      XDrawLine (DISPLAY, progress->pixmap, gc,
			 i, i, i, height-i);
	    }
	}
      
      XCopyArea (DISPLAY, progress->pixmap, XtWindow (progress->pbar), gc,
		 0, 0, width, height, 0, 0);
    }
}

static void
progress_cancel_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  ProgressP progress;
  long value;
  
  progress = (ProgressP) client_data;
  if (progress && progress->callback)
    {
      if (w == progress->cancel_cur)
	value = 1;
      else if (w == progress->cancel_all)
	value = 2;
      
      (* progress->callback) (progress->callback_data, (void*) value);
    }
}
