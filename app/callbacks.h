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
#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

#include <Xm/XmAll.h>

extern void app_exit (int);

void install_callbacks (Widget);

void new_callback                 (Widget, XtPointer, XtPointer);
void quit_callback                (Widget, XtPointer, XtPointer);
void edit_cut_callback            (Widget, XtPointer, XtPointer);
void edit_copy_callback           (Widget, XtPointer, XtPointer);
void edit_paste_callback          (Widget, XtPointer, XtPointer);
void edit_clear_callback          (Widget, XtPointer, XtPointer);
void named_cut_callback           (Widget, XtPointer, XtPointer);
void named_copy_callback          (Widget, XtPointer, XtPointer);
void named_paste_callback         (Widget, XtPointer, XtPointer);
void edit_undo_callback           (Widget, XtPointer, XtPointer);
void edit_redo_callback           (Widget, XtPointer, XtPointer);
void select_all_callback          (Widget, XtPointer, XtPointer);
void select_none_callback         (Widget, XtPointer, XtPointer);
void select_anchor_callback       (Widget, XtPointer, XtPointer);
void select_sharpen_callback      (Widget, XtPointer, XtPointer);
void select_invert_callback       (Widget, XtPointer, XtPointer);
void select_hide_callback         (Widget, XtPointer, XtPointer);
void select_opacity_callback      (Widget, XtPointer, XtPointer);
void select_from_gdisp_callback   (Widget, XtPointer, XtPointer);
void select_to_gdisp_callback     (Widget, XtPointer, XtPointer);
void edit_options_callback        (Widget, XtPointer, XtPointer);
void select_tool_callback         (Widget, XtPointer, XtPointer);
void create_new_view_callback     (Widget, XtPointer, XtPointer);
void close_window_callback        (Widget, XtPointer, XtPointer);
void shrink_wrap_callback         (Widget, XtPointer, XtPointer);
void window_info_callback         (Widget, XtPointer, XtPointer);
void view_options_dialog_callback (Widget, XtPointer, XtPointer);
void brush_dialog_callback        (Widget, XtPointer, XtPointer);
void palette_dialog_callback      (Widget, XtPointer, XtPointer);

#endif  /*  __CALLBACKS_H__  */
