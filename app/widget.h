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
#ifndef __WIDGET_H__
#define __WIDGET_H__

typedef void (*SelectImageCallback) (void*);

/* variable declarations */
extern Widget tool_widgets[];      /*  array of widgets  */


/* function declarations */
Widget get_top_shell (Widget);
void create_widgets (Widget);
void create_display_shell (Widget *, Widget *, Widget *, Widget *, Widget *,
			   long, long, unsigned int *, char *, Colormap, Visual *, int);
void map_dialog (Widget, XtPointer, XtPointer);

#endif	/* __WIDGET_H__ */
