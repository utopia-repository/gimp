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
#ifndef __BUILDMENU_H__
#define __BUILDMENU_H__


/* Structures */

typedef struct _menu_item
{
  char *label;
  WidgetClass *class;
  char mnemonic;
  char *accelerator;
  char *accel_text;
  void (*callback) ();
  XtPointer callback_data;
  struct _menu_item *subitems;
  Widget w;
} MenuItem;

/* Function declarations */

Widget BuildMenu (Widget, int, char *, int, int, MenuItem *, Visual *, Colormap, int);

#endif /* BUILDMENU_H */
