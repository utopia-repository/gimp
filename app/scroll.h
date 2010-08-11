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
#ifndef __SCROLL_H__
#define __SCROLL_H__

#include "gdisplay.h"

/*  routines for scrolling the image via the scrollbars  */
void vert_scrolled (Widget, XtPointer, XtPointer);
void horz_scrolled (Widget, XtPointer, XtPointer);

/*  routines for grabbing the image and scrolling via the pointer  */
void start_grab_and_scroll (GDisplay *, XButtonPressedEvent *);
void end_grab_and_scroll (GDisplay *, XButtonReleasedEvent *);
void grab_and_scroll (Widget, XtPointer, XEvent *, Boolean *);
void scroll_to_pointer_position (GDisplay *, XMotionEvent *);

#endif  /*  __SCROLL_H__  */
