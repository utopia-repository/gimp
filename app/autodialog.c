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
#include <Xm/SashP.h>
#include <stdlib.h>
#include <string.h>
#include "appenv.h"
#include "buildmenu.h"
#include "errors.h"
#include "autodialog.h"
#include "general.h"
#include "gimage.h"
#include "linked.h"
#include "widget.h"

#define TYPE_MASK       0x0000FFFF
#define BITSET(a, b)    ((a & b) == b)

static AutoDialogItem dialog_find_item (AutoDialog, int);
static void dialog_delete_children (AutoDialog, AutoDialogItem);
static void dialog_item_callback (Widget, XtPointer, XtPointer);
static void dialog_image_menu_callback (Widget, XtPointer, XtPointer);
static Widget dialog_create_image_menu (AutoDialog, Widget, char *, void *);

/*  Needed this to access an autodialog item's widget  */
Widget dialog_get_item_widget (AutoDialog, int);

static int dialog_ID = 1;

AutoDialog
dialog_new (title, callback, callback_data)
     char *title;
     ItemCallback callback;
     void *callback_data;
{
  AutoDialog dlg;

  dlg = xmalloc (sizeof (_AutoDialog));

  dlg->dialog_ID = dialog_ID++;
  dlg->default_ID = 0;
  dlg->next_item_ID = 1;
  dlg->items = NULL;
  dlg->callback = callback;
  dlg->callback_data = callback_data;

  /*  Create the main dialog shell  */
  dlg->shell = XtVaCreatePopupShell (title,
				     xmDialogShellWidgetClass, toplevel,
				     XmNtitle, title,
				     XmNallowShellResize, True,
				     XmNdeleteResponse, XmDESTROY,
				     NULL);

  /* Create the main dialog area */
  dlg->dialog = XtVaCreateWidget ("dialog",
				  xmFormWidgetClass, dlg->shell,
				  XmNfractionBase, 22,
				  NULL);

  {
    int default_ID;
    int ok_ID;
    int cancel_ID;
    int help_ID;
    AutoDialogItem default_item;
    AutoDialogItem ok_item;
    AutoDialogItem cancel_item;
    AutoDialogItem help_item;

    default_ID = dialog_new_item (dlg, 0, ITEM_FRAME, NULL, NULL);
    
    ok_ID = dialog_new_item (dlg, 0, ITEM_PUSH_BUTTON, "OK", NULL);
    cancel_ID = dialog_new_item (dlg, 0, ITEM_PUSH_BUTTON, "Cancel", NULL);
    help_ID = dialog_new_item (dlg, 0, ITEM_PUSH_BUTTON, "Help", NULL);

    default_item = dialog_find_item (dlg, default_ID);
    ok_item = dialog_find_item (dlg, ok_ID);
    cancel_item = dialog_find_item (dlg, cancel_ID);
    help_item = dialog_find_item (dlg, help_ID);

    XtVaSetValues (default_item->widget,
		   XmNleftAttachment, XmATTACH_POSITION,
		   XmNleftPosition, 1,
		   XmNrightAttachment, XmATTACH_POSITION,
		   XmNrightPosition, 21,
		   XmNtopAttachment, XmATTACH_FORM,
		   XmNbottomAttachment, XmATTACH_NONE,
		   XmNtopOffset, 10,
		   XmNbottomOffset, 10,
		   NULL);

    XtVaSetValues (ok_item->widget,
		   XmNtopAttachment, XmATTACH_WIDGET,
		   XmNtopWidget, default_item->widget,
		   XmNbottomAttachment, XmATTACH_FORM,
		   XmNleftAttachment, XmATTACH_POSITION,
		   XmNleftPosition, 1,
		   XmNrightAttachment, XmATTACH_POSITION,
		   XmNrightPosition, 7,
		   XmNshowAsDefault, True,
		   XmNdefaultButtonShadowThickness, 1,
		   XmNtopOffset, 10,
		   XmNbottomOffset, 10,
		   NULL);

    XtVaSetValues (cancel_item->widget,
		   XmNtopAttachment, XmATTACH_WIDGET,
		   XmNtopWidget, default_item->widget,
		   XmNbottomAttachment, XmATTACH_FORM,
		   XmNleftAttachment, XmATTACH_POSITION,
		   XmNleftPosition, 8,
		   XmNrightAttachment, XmATTACH_POSITION,
		   XmNrightPosition, 14,
		   XmNshowAsDefault, False,
		   XmNdefaultButtonShadowThickness, 1,
		   XmNtopOffset, 10,
		   XmNbottomOffset, 10,
		   NULL);

    XtVaSetValues (help_item->widget,
		   XmNsensitive, False,
		   XmNtopAttachment, XmATTACH_WIDGET,
		   XmNtopWidget, default_item->widget,
		   XmNbottomAttachment, XmATTACH_FORM,
		   XmNleftAttachment, XmATTACH_POSITION,
		   XmNleftPosition, 15,
		   XmNrightAttachment, XmATTACH_POSITION,
		   XmNrightPosition, 21,
		   XmNshowAsDefault, False,
		   XmNdefaultButtonShadowThickness, 1,
		   XmNtopOffset, 10,
		   XmNbottomOffset, 10,
		   NULL);

    dlg->default_ID = default_ID;
  }

  XtVaSetValues (dlg->dialog, XmNdefaultPosition, False, NULL);
  XtAddCallback (dlg->dialog, XmNmapCallback, map_dialog, NULL);

  return dlg;
}

void
dialog_show (dlg)
     AutoDialog dlg;
{
  if (dlg)
    XtManageChild (dlg->dialog);
}

void
dialog_hide (dlg)
     AutoDialog dlg;
{
  if (dlg)
    XtUnmanageChild (dlg->dialog);
}

void
dialog_close (dlg)
     AutoDialog dlg;
{
  link_ptr tmplink;
  AutoDialogItem dlgitem;
  
  if (dlg)
    {
      tmplink = dlg->items;
      while (tmplink)
	{
	  dlgitem = tmplink->data;
	  tmplink = tmplink->next;
	  
	  free (dlgitem);
	}
      free_list (dlg->items);
     
      XtDestroyWidget (dlg->shell);
    }
}

int
dialog_new_item (dlg, parent_ID, type, data, extra)
     AutoDialog dlg;
     int parent_ID, type;
     void *data;
     void *extra;
{
  Widget parent;
  Widget item_widget;
  AutoDialogItem parent_item;
  AutoDialogItem item;
  int re_manage_dialog;

  parent_item = dialog_find_item (dlg, parent_ID);
  if (parent_item)
    parent = parent_item->widget;
  else
    parent = dlg->dialog;

  re_manage_dialog = 0;
  if (XtIsManaged (dlg->dialog))
    {
      XtUnmanageChild (dlg->dialog);
      re_manage_dialog = 1;
    }

  switch (type & TYPE_MASK)
    {
    case GROUP_ROWS:
      item_widget = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass, parent,
				      XmNorientation, XmVERTICAL,
				      XmNradioBehavior, BITSET (type, GROUP_RADIO),
				      NULL);
      break;
    case GROUP_COLUMNS:
      item_widget = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass, parent,
				      XmNorientation, XmHORIZONTAL,
				      XmNradioBehavior, BITSET (type, GROUP_RADIO),
				      NULL);
      break;
    case GROUP_FORM:
      if ((long) data)
	item_widget = XtVaCreateWidget ("dialogPartition", xmFormWidgetClass, parent, 
					XmNfractionBase, (long) data, 
					NULL);
      else
	item_widget = XtVaCreateWidget ("dialogPartition", xmFormWidgetClass, parent, NULL);
      break;
    case ITEM_PUSH_BUTTON:
      item_widget = XtVaCreateWidget (data, xmPushButtonGadgetClass, parent, NULL);
      XtAddCallback (item_widget, XmNactivateCallback, dialog_item_callback, dlg);
      break;
    case ITEM_CHECK_BUTTON:
    case ITEM_RADIO_BUTTON:
      item_widget = XtVaCreateWidget (data, xmToggleButtonWidgetClass, parent, NULL);
      XtAddCallback (item_widget, XmNvalueChangedCallback, dialog_item_callback, dlg);
      break;
    case ITEM_IMAGE_MENU:
      item_widget = dialog_create_image_menu (dlg, parent, data, extra);
      break;
    case ITEM_SCALE:
      {
	Arg xt_args[6];
	long *ldata;
	int n;
	
	ldata = data;

	n = 0;
	XtSetArg (xt_args[n], XmNminimum, ldata[0]); n++;
	XtSetArg (xt_args[n], XmNmaximum, ldata[1]); n++;
	XtSetArg (xt_args[n], XmNvalue, ldata[2]); n++;
	XtSetArg (xt_args[n], XmNdecimalPoints, ldata[3]); n++;
	XtSetArg (xt_args[n], XmNshowValue, True); n++;
	XtSetArg (xt_args[n], XmNorientation, XmHORIZONTAL); n++;

	item_widget = XmCreateScale (parent, "scale", xt_args, n);
	XtAddCallback (item_widget, XmNvalueChangedCallback, dialog_item_callback, dlg);
      }
      break;
    case ITEM_FRAME:
      item_widget = XtVaCreateWidget ("frame", xmFrameWidgetClass, parent,
				      XmNshadowType, XmSHADOW_ETCHED_IN,
				      NULL);
      
      if (data && (strlen (data) > 0))
	XtVaCreateManagedWidget (data, xmLabelGadgetClass, item_widget,
				 XmNchildType, XmFRAME_TITLE_CHILD,
				 XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
				 NULL);
      break;
    case ITEM_LABEL:
      if (parent_item && (parent_item->item_type == ITEM_FRAME))
	{
	  item_widget = XtVaCreateWidget (data, xmLabelGadgetClass, parent,
					  XmNchildType, XmFRAME_TITLE_CHILD,
					  XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
					  NULL);
	}
      else
	{
	  item_widget = XtVaCreateWidget (data, xmLabelWidgetClass, parent, NULL);  
	}
      break;
    case ITEM_TEXT:
      item_widget = XtVaCreateWidget (data, xmTextFieldWidgetClass, parent, NULL);

      if (data && (strlen (data) > 0))
	XmTextSetString (item_widget, data);

      XtAddCallback (item_widget, XmNvalueChangedCallback, dialog_item_callback, dlg);
      break;
    default:
      warning ("Unknown type for new item");
      return -1;      
    }

  if (parent_item)
    switch (parent_item->item_type)
      {
      case GROUP_FORM:
	XtVaSetValues (item_widget, 
		       XmNtopAttachment, XmATTACH_FORM,
		       XmNbottomAttachment, XmATTACH_FORM,
		       XmNrightAttachment, XmATTACH_FORM,
		       XmNleftAttachment, XmATTACH_FORM,
		       NULL);
	break;
      }

  if (re_manage_dialog)
    XtManageChild (dlg->dialog);
  
  item = xmalloc (sizeof (_AutoDialogItem));
  
  item->item_ID = dlg->next_item_ID++;
  item->item_type = type;
  memset (item->data, 0, 32);
  item->widget = item_widget;
  item->parent = parent_item;
  item->children = NULL;

  if (parent_item)
    parent_item->children = add_to_list (parent_item->children, item);
  dlg->items = add_to_list (dlg->items, item);
  
  XtVaSetValues (item_widget, XmNuserData, item, NULL);
  XtManageChild (item_widget);
  
  return item->item_ID;
}

void
dialog_show_item (dlg, item_ID)
     AutoDialog dlg;
     int item_ID;
{
  AutoDialogItem item;

  item = dialog_find_item (dlg, item_ID);
  if (item && !XtIsManaged (item->widget))
    {
      if (!XtIsManaged (dlg->dialog))
	{
	  XtUnmanageChild (dlg->dialog);
	  XtManageChild (item->widget);
	  XtManageChild (dlg->dialog);
	}
      else
	XtManageChild (item->widget);
    }
}

void
dialog_hide_item (dlg, item_ID)
     AutoDialog dlg;
     int item_ID;
{
  AutoDialogItem item;

  item = dialog_find_item (dlg, item_ID);
  if (item && XtIsManaged (item->widget))
    {
      if (!XtIsManaged (dlg->dialog))
	{
	  XtUnmanageChild (dlg->dialog);
	  XtUnmanageChild (item->widget);
	  XtManageChild (dlg->dialog);
	}
      else
	XtUnmanageChild (item->widget);
    }
}

void
dialog_change_item (dlg, item_ID, data)
     AutoDialog dlg;
     int item_ID;
     void *data;
{
  AutoDialogItem item;

  item = dialog_find_item (dlg, item_ID);
  if (item)
    {
      switch (item->item_type)
	{
	case ITEM_CHECK_BUTTON:
	case ITEM_RADIO_BUTTON:
	  XmToggleButtonSetState (item->widget, (int) *((long*) data), True);
	  break;
	case ITEM_TEXT:
	  XmTextSetString (item->widget, data);
	  break;
	case ITEM_SCALE:
	  XmScaleSetValue (item->widget, (int) *((long *) data));
	  break;
	}
    }
}

void
dialog_delete_item (dlg, item_ID)
     AutoDialog dlg;
     int item_ID;
{
  AutoDialogItem item;

  item = dialog_find_item (dlg, item_ID);
  if (item)
    {
      dialog_delete_children (dlg, item);

      if (item->parent)
	item->parent->children = remove_from_list (item->parent->children, item);
      dlg->items = remove_from_list (dlg->items, item);

      if (XtIsManaged (dlg->dialog))
	{
	  XtUnmanageChild (dlg->dialog);
	  XtManageChild (dlg->dialog);
	}

      XtDestroyWidget (item->widget);
      xfree (item);
    }
}


Widget
dialog_get_item_widget (dlg, item_ID)
     AutoDialog dlg;
     int item_ID;
{
  AutoDialogItem item;

  item = dialog_find_item (dlg, item_ID);
  if (item)
    return item->widget;
  return NULL;
}


static AutoDialogItem
dialog_find_item (dlg, ID)
     AutoDialog dlg;
     int ID;
{
  AutoDialogItem item;
  link_ptr tmp;

  item = NULL;
  
  if (!ID)
    {
      if (dlg->default_ID)
	ID = dlg->default_ID;
      else
	return NULL;
    }
  
  tmp = dlg->items;
  while (tmp)
    {
      item = tmp->data;
      tmp = next_item (tmp);
      
      if (item->item_ID == ID)
	return item;
    }
  
  warning ("Unable to find dialog item: %d", ID);
  
  return NULL;
}

static void
dialog_delete_children (dlg, item)
     AutoDialog dlg;
     AutoDialogItem item;
{
  AutoDialogItem child;
  link_ptr tmp;

  tmp = item->children;
  while (tmp)
    {
      child = tmp->data;

      dialog_delete_children (dlg, child);
      dlg->items = remove_from_list (dlg->items, child);
      xfree (child);

      tmp = next_item (tmp);
    }

  if (item->children)
    free_list (item->children);
}

static void
dialog_item_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  AutoDialog dlg;
  AutoDialogItem item;

  dlg = client_data;
  XtVaGetValues (w, XmNuserData, &item, NULL);

  switch (item->item_type)
    {
    case ITEM_PUSH_BUTTON:
      break;
    case ITEM_CHECK_BUTTON:
    case ITEM_RADIO_BUTTON:
      {
	XmToggleButtonCallbackStruct *tbcs;
	long *data;

	tbcs = (XmToggleButtonCallbackStruct*) call_data;
	data = (long*) item->data;
	*data = tbcs->set;
      }
      break;
    case ITEM_SCALE:
      {
	XmScaleCallbackStruct *scb;
	long *data;

	scb = (XmScaleCallbackStruct*) call_data;
	data = (long*) item->data;
	*data = scb->value;
      }
      break;
    case ITEM_TEXT:
      {
	char *data;
	
	data = XmTextGetString (w);
	strncpy (item->data, data, 32);
	XtFree (data);
      }
      break;
    }

  if (dlg->callback && item)
    (* dlg->callback) (dlg->dialog_ID, item->item_ID, dlg->callback_data, item->data);
}

static void
dialog_image_menu_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  AutoDialog dlg;
  long *data;

  dlg = client_data;
  XtVaGetValues (w, XmNuserData, &data, NULL);

  if (dlg->callback)
    (* dlg->callback) (dlg->dialog_ID, data[0], dlg->callback_data, &data[1]);
}

static Widget
dialog_create_image_menu (dlg, parent, title, image)
     AutoDialog dlg;
     Widget parent;
     char *title;
     void *image;
{
  extern link_ptr image_list;

  GImage *constrain;
  GImage *gimage;
  Widget menu, option, item;
  XmString str;
  Arg args[2];
  char label[64];
  char constraint;
  link_ptr tmp;
  long ID;
  long *data;
  long first_item;
  int n;

  menu = XmCreatePulldownMenu (parent, "_pulldown", NULL, 0);
  
  n = 0;
  str = XmStringCreateLocalized (title);
  XtSetArg (args[n], XmNsubMenuId, menu); n++;
  XtSetArg (args[n], XmNlabelString, str); n++;
  option = XmCreateOptionMenu (parent, title, args, n);
  XmStringFree (str);

  constraint = title[123];
  ID = ((long*) title)[31];
  constrain = (ID == 0) ? image : gimage_get_ID (ID);

  first_item = 0;
  tmp = image_list;
  while (tmp)
    {
      gimage = tmp->data;
      tmp = next_item (tmp);

      if (((constraint & IMAGE_CONSTRAIN_RGB) && (gimage->type == RGB_GIMAGE)) ||
	  ((constraint & IMAGE_CONSTRAIN_GRAY) && (gimage->type == GREY_GIMAGE)) ||
	  ((constraint & IMAGE_CONSTRAIN_INDEXED) && (gimage->type == INDEXED_GIMAGE)))
	if ((gimage->width == constrain->width) && (gimage->height == constrain->height))
	  {
	    sprintf (label, "%s-%d\n", prune_filename (gimage->filename), gimage->ID);
	    item = XtVaCreateManagedWidget (label, xmPushButtonGadgetClass, menu, NULL);
	    XtAddCallback (item, XmNactivateCallback, dialog_image_menu_callback, dlg);
	    
	    data = xmalloc (sizeof (long) * 2);
	    data[0] = dlg->next_item_ID;
	    data[1] = (gimage->ID == constrain->ID) ? 0 : gimage->ID;
	    XtVaSetValues (item, XmNuserData, data, NULL);
	    
	    if (!first_item)
	      first_item = data[1];
	    
	    if (gimage->ID == constrain->ID)
	      {
		first_item = -1;
		XtVaSetValues (menu, XmNmenuHistory, item, NULL);
	      }
	  }
    }

  if ((first_item > 0) && (dlg->callback))
    (* dlg->callback) (dlg->dialog_ID, -1, dlg->callback_data, &first_item);

  return option;
}


