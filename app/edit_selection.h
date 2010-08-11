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
#ifndef __EDIT_SELECTION_H__
#define __EDIT_SELECTION_H__

#include "selection.h"
#include "tools.h"

/*  action functions  */
void   edit_selection_button_release (Tool *, XButtonEvent *, XtPointer);
void   edit_selection_motion         (Tool *, XMotionEvent *, XtPointer);
void   edit_selection_control        (Tool *, int, void *);
void   edit_selection_draw           (Tool *);
void   edit_sel_arrow_keys_func      (Tool *, XKeyEvent *, void *);


void   init_edit_selection           (Tool *, Selection *, void *, int, int);

#endif  /*  __EDIT_SELECTION_H__  */
