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
#ifndef __AUTODIALOG_H__
#define __AUTODIALOG_H__

#include "dialog_types.h"
#include "linked.h"

#define OK_ID 2
#define CANCEL_ID 3

#define IMAGE_CONSTRAIN_RGB     1 << 0
#define IMAGE_CONSTRAIN_GRAY    1 << 1
#define IMAGE_CONSTRAIN_INDEXED 1 << 2
#define IMAGE_CONSTRAIN_ALL     0xFF

typedef void (*ItemCallback) (int, int, void *, void *);
typedef struct _AutoDialog      _AutoDialog, *AutoDialog;
typedef struct _AutoDialogItem  _AutoDialogItem, *AutoDialogItem;

struct _AutoDialog {
  int          dialog_ID;
  int          next_item_ID;
  int          default_ID;
  link_ptr     items; 
  ItemCallback callback;
  void       * callback_data;
  Widget       shell;
  Widget       dialog;
};

struct _AutoDialogItem {
  int             item_ID;
  int             item_type;
  char            data[32];
  AutoDialogItem  parent;
  link_ptr        children;
  Widget          widget;
};

AutoDialog dialog_new (char *, ItemCallback, void *);
void dialog_show (AutoDialog);
void dialog_hide (AutoDialog);
void dialog_close (AutoDialog);

int dialog_new_item (AutoDialog, int, int, void *, void *);
void dialog_show_item (AutoDialog, int);
void dialog_hide_item (AutoDialog, int);
void dialog_change_item (AutoDialog, int, void *);
void dialog_delete_item (AutoDialog, int);
Widget dialog_get_item_widget (AutoDialog, int);

#endif /*  __AUTODIALOG_H__  */

