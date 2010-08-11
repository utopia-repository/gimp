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
#include "gdisplay.h"
#include "general.h"
#include "global_edit.h"
#include "linked.h"
#include "selection.h"
#include "undo.h"
#include "widget.h"


/*  Edit options stuff...  */
static void         options_dialog_callback (int, int, void *, void *);

/*  local variables  */

/*  backups of the above variables  */
static int          undo_levels_value;
static int          undo_bytes_value;

static AutoDialog   options_dlg = NULL;
static int          undo_levels_ID;
static int          undo_bytes_ID;


/*  global done variable  */
static int done = 0;


/*  Dialog widgets  */
Widget new_named = NULL;
Widget get_named = NULL;


/*  The named buffer structure...  */
typedef struct _named_buffer NamedBuffer;

struct _named_buffer
{
  Selection *  select;
  char *       name;
};


/*  The named buffer list  */
link_ptr named_buffers = NULL;

/*  The global edit buffer  */
Selection * global_select = NULL;


static Selection *
edit_copy_sel (select)
     Selection * select;
{
  Selection * new;
  TempBuf * buf;

  if (select->float_buf)
    buf = temp_buf_copy (select->float_buf, NULL);
  else
    buf = NULL;

  /*  Copy the items in the current selection to this new selection  */
  new = selection_generic_new (gregion_copy (select->region, NULL),
			       select->offset_x, select->offset_y, buf);

  return new;
}


Selection *
edit_cut (gdisp_ptr)
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  Selection * cut;
  int x1, y1, x2, y2;
  int sm;   /*  selection margin  */

  gdisp = (GDisplay *) gdisp_ptr;

  if (!selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2))
    return NULL;

  /*  cut the selection out  */
  selection_cut_floating (gdisp->select, (void *) gdisp->gimage);

  /*  create a dummy selection structure  */
  cut = selection_generic_new (gregion_new (gdisp->gimage->height, 
					    gdisp->gimage->width),
			       0, 0, NULL);

  selection_swap (gdisp->select, cut);

  /*  clear the orig buf in the cut buffer  */
  if (cut->orig_buf)
    {
      temp_buf_free (cut->orig_buf);
      cut->orig_buf = NULL;
    }

  /*  If we can't push this cut operation onto the undo stack,
      we must delete the old global_select structure           */
  if (! undo_push_cut (gdisp->ID, global_select, cut, gdisp->select->fresh_cut))
    selection_generic_free (global_select);
  global_select = cut;

  /* update the gdisplays  */
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);

  return cut;
}


Selection *
edit_copy (gdisp_ptr)
     void * gdisp_ptr;
{
  GDisplay * gdisp;
  Selection * copy;
  TempBuf * buf;
  int x1, y1, x2, y2;

  gdisp = (GDisplay *) gdisp_ptr;

  if (!selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2))
    return NULL;

  if (!gdisp->select->float_buf)
    buf = temp_buf_load (NULL, gdisp->gimage, x1, y1, (x2 - x1), (y2 - y1));
  else
    buf = temp_buf_copy (gdisp->select->float_buf, NULL);

  /*  Copy the items in the current selection to this new selection  */
  copy = selection_generic_new (gregion_copy (gdisp->select->region, NULL),
				0, 0, buf);
			       
  /*  If we can't push this copy operation onto the undo stack,
      we must delete the old global_select structure           */
  if (! undo_push_copy (gdisp->ID, global_select, copy))
    selection_generic_free (global_select);
  global_select = copy;

  return copy;
}


int
edit_paste (gdisp_ptr, select)
     void * gdisp_ptr;
     Selection * select;
{
  GDisplay * gdisp;
  Selection * paste;
  TempBuf * buf = NULL;
  int x1, y1, x2, y2;
  int cx, cy;

  if (!select)
    return 0;

  gdisp = (GDisplay *) gdisp_ptr;

  /*  Find the center of the current selection or the image if there
      is no current selection...  */
  if (selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2))
    {
      cx = (x1 + x2) >> 1;
      cy = (y1 + y2) >> 1;
    }
  else
    {
      cx = gdisp->gimage->width >> 1;
      cy = gdisp->gimage->height >> 1;
    }

  if (select->float_buf)
    {
      /*  We create a new float buf with the correct number of bytes  */
      buf  = temp_buf_new (select->float_buf->width, select->float_buf->height,
			   gdisp->gimage->bpp, 0, 0, NULL);
      
      /*  now copy the contents of the global buffer to the new one  */
      temp_buf_copy (select->float_buf, buf);
    }

  gregion_find_bounds (select->region, &x1, &y1, &x2, &y2);
  paste = selection_generic_new (gregion_copy (select->region, NULL),
				 cx - ((x1 + x2) >> 1), cy - ((y1 + y2) >> 1), buf);

  selection_replace (gdisp->select, gdisp, paste);

  return 1;
}


Boolean
global_edit_cut (gdisp_ptr)
     void * gdisp_ptr;
{
  if (!edit_cut (gdisp_ptr))
    return False;
  else
    return True;
}


Boolean
global_edit_copy (gdisp_ptr)
     void * gdisp_ptr;
{
  if (edit_copy (gdisp_ptr))
    return False;
  else
    return True;
}


Boolean
global_edit_paste (gdisp_ptr)
     void * gdisp_ptr;
{
  if (edit_paste (gdisp_ptr, global_select))
    return True;
  else
    return False;
}


void
global_edit_free ()
{
  if (global_select)
    selection_generic_free (global_select);

  global_select = NULL;
}


/*********************************************/
/*        Named buffer operations            */



static NamedBuffer *
find_named_buffer (name)
     char * name;
{
  link_ptr list;
  NamedBuffer * nb;

  if (!name) return NULL;

  list = named_buffers;
  while (list)
    {
      nb = (NamedBuffer *) list->data;
      if (! strcmp (nb->name, name))
	return nb;
      list = next_item (list);
    }
  return NULL;

}


static void
set_list_of_named_buffers (listbox)
     Widget listbox;
{
  XmString * str;
  int size = 0, i;
  link_ptr list;
  NamedBuffer * nb;

  list = named_buffers;

  while (list)
    {
      size++;
      list = next_item (list);
    }
  
  str = (XmString *) xmalloc (size * sizeof (XmString));
  list = named_buffers;
  i = 0;
  while (list)
    {
      nb = (NamedBuffer *) list->data;
      str[i++] = XmStringCreateLocalized (nb->name);
      list = next_item (list);
    }

  XtVaSetValues (listbox,
		 XmNlistItems, str,
		 XmNlistItemCount, size,
		 XmNmustMatch, True,
		 NULL);
  
  for (i = 0; i < size; i++)
    XmStringFree (str[i]);

  xfree (str);
}


static void
named_callback (w, client_data, call_data)
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


static void
delete_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Widget dialog, text;
  char * name;
  NamedBuffer * nb;

  dialog = (Widget) client_data;

  text = XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT);

  XtVaGetValues (text, XmNvalue, &name, NULL);

  nb = find_named_buffer (name);

  if (nb)
    {
      named_buffers = remove_from_list (named_buffers, (void *) nb);
      xfree (nb->name);
      selection_generic_free (nb->select);
      xfree (nb);

      set_list_of_named_buffers (dialog);
    }

}


static NamedBuffer *
new_named_buffer ()
{
  NamedBuffer * new;
  char * value;
  Widget dialog;
  XmString warning;

  done        = 0;
  new         = (NamedBuffer *) xmalloc (sizeof (NamedBuffer));
  new->name   = NULL;
  new->select = NULL;

  /*  Create the dialog box to ask for a name  */
  if (!new_named)
    {
      new_named = XmCreatePromptDialog (toplevel, "new_named", NULL, 0);
      XtVaSetValues (new_named,
		     XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		     XmNdefaultPosition, False,
		     NULL);
      XtUnmanageChild (XmSelectionBoxGetChild (new_named, XmDIALOG_HELP_BUTTON));
      XtAddCallback (new_named, XmNokCallback, named_callback, (XtPointer) &done);
      XtAddCallback (new_named, XmNcancelCallback, named_callback, (XtPointer) &done);
      XtAddCallback (new_named, XmNmapCallback, map_dialog, NULL);
    }

  XtManageChild (new_named);
  XtPopup (XtParent (new_named), XtGrabNone);

  while (done == 0)
    XtAppProcessEvent (app_context, XtIMAll);

  XtPopdown (XtParent (new_named));

  if (done == XmCR_CANCEL)
    {
      xfree (new);
      return NULL;
    }
  else
    {
      XtVaGetValues (XmSelectionBoxGetChild (new_named, XmDIALOG_TEXT),
		     XmNvalue, &value, NULL);
      /*  if a buffer with the name already exists...  */
      if (find_named_buffer (value))
	{
	  warning = XmStringCreateLocalized ("A buffer with that name already exists.");
	  dialog = XmCreateWarningDialog (new_named, "Warning", NULL, 0);
	  XtVaSetValues (dialog, XmNmessageString, warning, NULL);
	  XtUnmanageChild (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
	  XtUnmanageChild (XmMessageBoxGetChild (dialog, XmDIALOG_OK_BUTTON));

	  XmStringFree (warning);
	  XtManageChild (dialog);
	  XtPopup (XtParent (dialog), XtGrabNone);
	  xfree (new);
	  return NULL;
	}
      else
	{
	  new->name = xstrdup (value);
	  return new;
	}
    }
}


static NamedBuffer *
get_named_buffer ()
{
  char * value;
  XmString delete_title;
  Widget button;

  done = 0;

  /*  create the dialog box to access the list of named buffers  */
  if (! get_named)
    {
      get_named = XmCreateSelectionDialog (toplevel, "get_named", NULL, 0);
      XtVaSetValues (get_named,
		     XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		     XmNdefaultPosition, False,
		     NULL);

      /*  Change the apply button to delete */
      delete_title = XmStringCreateLocalized ("Delete");
      button = XmSelectionBoxGetChild (get_named, XmDIALOG_APPLY_BUTTON);
      XtVaSetValues (button,
		     XmNlabelString, delete_title,
		     NULL);
      XmStringFree (delete_title);

      XtUnmanageChild (XmSelectionBoxGetChild (get_named, XmDIALOG_HELP_BUTTON));
      XtAddCallback (get_named, XmNokCallback, named_callback, (XtPointer) &done);
      XtAddCallback (get_named, XmNapplyCallback, delete_callback, (XtPointer) get_named);
      XtAddCallback (get_named, XmNcancelCallback, named_callback, (XtPointer) &done);
      XtAddCallback (get_named, XmNmapCallback, map_dialog, NULL);
    }

  set_list_of_named_buffers (get_named);

  XtManageChild (get_named);
  XtPopup (XtParent (get_named), XtGrabNone);

  while (done == 0)
    XtAppProcessEvent (app_context, XtIMAll);

  XtPopdown (XtParent (get_named));

  if (done == XmCR_CANCEL)
    return NULL;
  else
    {
      XtVaGetValues (XmSelectionBoxGetChild (get_named, XmDIALOG_TEXT),
		     XmNvalue, &value, NULL);
      return find_named_buffer (value);
    }
}


Boolean
named_edit_cut (gdisp_ptr)
     void * gdisp_ptr;
{
  Selection * new;
  NamedBuffer * nb;

  if (! (new = edit_cut (gdisp_ptr)))
    return False;
  else if ((nb = new_named_buffer ()))
    {
      nb->select = edit_copy_sel (new);
      named_buffers = append_to_list (named_buffers, (void *) nb);
      return True;
    }
  else
    return False;
}


Boolean
named_edit_copy (gdisp_ptr)
     void * gdisp_ptr;
{
  Selection * new;
  NamedBuffer * nb;

  if (! (new = edit_copy (gdisp_ptr)))
    return False;
  else if ((nb = new_named_buffer ()))
    {
      nb->select = edit_copy_sel (new);
      named_buffers = append_to_list (named_buffers, (void *) nb);
      return True;
    }
  else
    return False;
}


Boolean
named_edit_paste (gdisp_ptr)
     void * gdisp_ptr;
{
  NamedBuffer * nb;

  if (! (nb = get_named_buffer ()))
    return False;

  if (edit_paste (gdisp_ptr, nb->select))
    return True;
  else
    return False;
}


void
named_buffers_free ()
{
  link_ptr list;
  NamedBuffer * nb;

  list = named_buffers;

  while (list)
    {
      nb = (NamedBuffer *) list->data;
      selection_generic_free (nb->select);
      xfree (nb->name);
      xfree (nb);
      list = next_item (list);
    }

  free_list (named_buffers);
  named_buffers = NULL;
}



/*************************************************************/


void
edit_options (gdisp_ptr)
     void * gdisp_ptr;
{
  int group_ID;
  int colgroup_ID;
  int rowgroup_ID;
  int frame_ID;
  char buf [32];

  if (!options_dlg)
    {
      options_dlg = dialog_new ("Edit Options", options_dialog_callback, NULL);
      dialog_new_item (options_dlg, 0, ITEM_LABEL, "Options", NULL);

      group_ID = dialog_new_item (options_dlg, 0, GROUP_ROWS, NULL, NULL);
      frame_ID = dialog_new_item (options_dlg, group_ID, ITEM_FRAME, "Undo", NULL);
      rowgroup_ID = dialog_new_item (options_dlg, frame_ID, GROUP_ROWS, NULL, NULL);

      colgroup_ID = dialog_new_item (options_dlg, rowgroup_ID, GROUP_COLUMNS, NULL, NULL);
      dialog_new_item (options_dlg, colgroup_ID, ITEM_LABEL, "Levels of undo:", NULL);
      sprintf (buf, "%d", app_data.levels_of_undo);
      undo_levels_value = app_data.levels_of_undo;
      undo_levels_ID = dialog_new_item (options_dlg, colgroup_ID, ITEM_TEXT, buf, NULL);

      colgroup_ID = dialog_new_item (options_dlg, rowgroup_ID, GROUP_COLUMNS, NULL, NULL);
      dialog_new_item (options_dlg, colgroup_ID, ITEM_LABEL, "Bytes of undo:", NULL);
      sprintf (buf, "%d", app_data.bytes_of_undo);
      undo_bytes_value = app_data.bytes_of_undo;
      undo_bytes_ID = dialog_new_item (options_dlg, colgroup_ID, ITEM_TEXT, buf, NULL);

      dialog_show (options_dlg);
    }
  
}


static void
options_dialog_callback   (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (options_dlg);
      options_dlg = NULL;
      break;
    case CANCEL_ID:
      dialog_close (options_dlg);
      options_dlg = NULL;
      app_data.levels_of_undo = undo_levels_value;
      app_data.bytes_of_undo = undo_bytes_value;
      break;
    default:
      if (item_ID == undo_levels_ID)
	app_data.levels_of_undo = atoi ((char *) call_data);
      if (item_ID == undo_bytes_ID)
	app_data.bytes_of_undo = atoi ((char *) call_data);
      break;
    }

}




