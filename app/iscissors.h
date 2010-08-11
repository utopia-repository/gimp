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
#ifndef __ISCISSORS_H__
#define __ISCISSORS_H__

/*  rect select action functions  */

void          iscissors_button_press      (Tool *, XButtonEvent *, XtPointer);
void          iscissors_button_release    (Tool *, XButtonEvent *, XtPointer);
void          iscissors_motion            (Tool *, XMotionEvent *, XtPointer);
void          iscissors_control           (Tool *, int, void *);


/*  rect select functions  */

void          iscissors_draw         (Tool *);
Tool *        tools_new_iscissors    ();
void          tools_free_iscissors   (Tool *);


#endif  /*  __ISCISSORS_H__  */
