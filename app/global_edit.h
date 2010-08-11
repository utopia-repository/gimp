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
#ifndef __GLOBAL_EDIT_H__
#define __GLOBAL_EDIT_H__

/*  The interface functions  */
Boolean        global_edit_cut        (void *);
Boolean        global_edit_copy       (void *);
Boolean        global_edit_paste      (void *);
void           global_edit_free       ();

Boolean        named_edit_cut         (void *);
Boolean        named_edit_copy        (void *);
Boolean        named_edit_paste       (void *);
void           named_buffers_free     ();

/*  The options dialog  */
void           edit_options           (void *);

#endif  /*  __GLOBAL_EDIT_H__  */
