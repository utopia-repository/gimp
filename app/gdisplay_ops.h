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
#ifndef __GDISPLAY_OPS_H__
#define __GDISPLAY_OPS_H__

#include "gdisplay.h"
#include "temp_buf.h"


Pixel    gdisplay_black_pixel  (GDisplay *);
Pixel    gdisplay_white_pixel  (GDisplay *);
void     gdisplay_new_view     (GDisplay *);
void     gdisplay_close_window (GDisplay *);
void     gdisplay_shrink_wrap  (GDisplay *);
void     gdisplay_change_type  (GDisplay *, int);
Boolean  gdisplay_resize_image (GDisplay *);
void     gdisplay_fit_colormap (Colormap, unsigned char *, int);

int      temp_buf_to_gdisplay  (TempBuf *);
TempBuf *gdisplay_to_temp_buf  (GImage *);

int      save_before_closing_dialog (char *);


#endif  /* __GDISPLAY_OPS_H__ */
