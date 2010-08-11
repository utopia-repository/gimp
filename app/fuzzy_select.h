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
#ifndef __FUZZY_SELECT_H__
#define __FUZZY_SELECT_H__

#include "gimage.h"
#include "gregion.h"

/*  fuzzy select action functions  */

void          fuzzy_select_button_press      (Tool *, XButtonEvent *, XtPointer);
void          fuzzy_select_button_release    (Tool *, XButtonEvent *, XtPointer);
void          fuzzy_select_motion            (Tool *, XMotionEvent *, XtPointer);
void          fuzzy_select_control           (Tool *, int, void *);


/*  fuzzy select functions  */

XSegment *    fuzzy_select_calculate    (Tool *, void *, int *);
void          fuzzy_select_draw         (Tool *);
Tool *        tools_new_fuzzy_select    ();
void          tools_free_fuzzy_select   (Tool *);


/*  functions  */
void          find_contiguous_region    (GRegion *, GImage *, int, int, 
					 int, int, unsigned char *);


#endif  /*  __FUZZY_SELECT_H__  */
