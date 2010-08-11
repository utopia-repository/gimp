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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __INFO_WINDOW_H__
#define __INFO_WINDOW_H__


InfoDialog * info_window_create        (GimpDisplay *gdisp);
void         info_window_free          (InfoDialog  *info_win);

void         info_window_update        (GimpDisplay *gdisp);
void         info_window_update_cursor (GimpDisplay *gdisp,
                                        gdouble      tx,
                                        gdouble      ty);
void         info_window_follow_auto   (Gimp        *gimp);


#endif /*  __INFO_WINDOW_H__  */
