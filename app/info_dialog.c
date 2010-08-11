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
#include <string.h>
#include "appenv.h"
#include "info_dialog.h"


/*  static functions  */
static InfoField * info_field_new (Widget, char *, char *);
static void        update_field (InfoField *);


static InfoField *
info_field_new (parent, class, text_ptr)
     Widget parent;
     char * class;
     char * text_ptr;
{
  char * buf;
  InfoField * field;

  buf = (char *) xmalloc (sizeof (char) * (strlen (class) + 4));
  sprintf (buf, "%sVal", class);
  field = (InfoField *) xmalloc (sizeof (InfoField));

  XtVaCreateManagedWidget (class, xmLabelGadgetClass, parent, NULL);
  field->w = XtVaCreateManagedWidget (buf, xmLabelGadgetClass, parent, NULL);
  field->text_ptr = text_ptr;

  xfree (buf);

  return field;
}


static void
update_field (field)
     InfoField * field;
{
  XmString field_string;
  XmString old_text;

  /*  only update the field if its new value differs from the old  */

  XtVaGetValues (field->w, XmNlabelString, &old_text, NULL);
  field_string = XmStringCreateLocalized (field->text_ptr);

  if (! XmStringCompare (old_text, field_string))
    {
      XtVaSetValues (field->w, XmNlabelString, field_string, NULL);
    }

  XmStringFree (old_text);
  XmStringFree (field_string);
  
}


/*  function definitions  */

InfoDialog *
info_dialog_new (class, title)
     char * class;
     char * title;
{
  InfoDialog * idialog;
  Widget shell, dialog;
  Widget info_area, info_frame;
  Widget rowcol;
  
  idialog = (InfoDialog *) xmalloc (sizeof (InfoDialog));
  idialog->field_list = NULL;

  /*  Create the main dialog shell  */
  shell = XtVaCreatePopupShell (class,
				xmDialogShellWidgetClass, toplevel,
				XmNtitle, title,
				XmNdeleteResponse, XmDESTROY,
				NULL);

  dialog = XtVaCreateWidget ("iDialogBulletin", xmBulletinBoardWidgetClass,
			     shell, NULL);

  rowcol = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass, dialog,
			     XmNpacking, XmPACK_TIGHT,
			     XmNorientation, XmVERTICAL,
			     NULL);

  info_frame = XtVaCreateWidget ("infoFrame", xmFrameWidgetClass, rowcol,
				 XmNshadowType, XmSHADOW_ETCHED_IN,
				 NULL);
				 
  info_area = XtVaCreateWidget ("infoArea", xmRowColumnWidgetClass, info_frame,
				XmNpacking, XmPACK_COLUMN,
				XmNorientation, XmHORIZONTAL,
				XmNnumColumns, 0,
				NULL);

  XtVaSetValues (dialog, XmNdefaultPosition, False, NULL);

  idialog->shell = shell;
  idialog->dialog = dialog;
  idialog->rowcol = rowcol;
  idialog->info_area = info_area;
  idialog->action_area = NULL;

  XtManageChild (idialog->info_area);
  XtManageChild (info_frame);
  XtManageChild (idialog->rowcol);
  
  return idialog;
}


void
info_dialog_free (idialog)
     InfoDialog * idialog;
{
  link_ptr list;

  if (!idialog)
    return;

  /*  Free each item in the field list  */
  list = idialog->field_list;

  while (list)
    {
      xfree (list->data);
      list = next_item (list);
    }
  
  /*  Free the actual field linked list  */
  free_list (idialog->field_list);

  /*  Destroy the associated widgets  */
  XtDestroyWidget (idialog->shell);

  /*  Free the info dialog memory  */
  xfree (idialog);
}


void
info_dialog_add_field (idialog, class, text_ptr)
     InfoDialog * idialog;
     char * class;
     char * text_ptr;
{
  short num_columns;
  InfoField * new_field;

  if (!idialog)
    return;

  new_field = info_field_new (idialog->info_area, class, text_ptr);
  idialog->field_list = add_to_list (idialog->field_list, (void *) new_field);

  XtVaGetValues (idialog->info_area, XmNnumColumns, &num_columns, NULL);
  XtVaSetValues (idialog->info_area, XmNnumColumns, (num_columns + 1), NULL);
}

void
info_dialog_popup (idialog)
     InfoDialog * idialog;
{
  if (!idialog)
    return;

  if (!XtIsManaged (idialog->dialog))
    XtManageChild (idialog->dialog);
}


void
info_dialog_popdown (idialog)
     InfoDialog * idialog;
{
  if (!idialog)
    return;

  XtUnmanageChild (idialog->dialog);
}


void
info_dialog_update (idialog)
     InfoDialog * idialog;
{
  link_ptr list;

  if (!idialog)
    return;

  list = idialog->field_list;

  while (list)
    {
      update_field ((InfoField *) list->data);
      list = next_item (list);
    }

}




