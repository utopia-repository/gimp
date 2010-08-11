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
#ifndef __IMAGE_BUF__
#define __IMAGE_BUF__

#include <X11/extensions/XShm.h>

#define GREY_BUF  0
#define COLOR_BUF 1

typedef void *ImageBuf;

ImageBuf image_buf_create (int, int, int, int);
void image_buf_destroy (ImageBuf);
void image_buf_put (ImageBuf, Drawable, GC);
void image_buf_put_area (ImageBuf, Drawable, GC, int, int, int, int);
void image_buf_draw_row (ImageBuf, unsigned char *, int, int, int);
void* image_buf_data (ImageBuf);
int image_buf_width (ImageBuf);
int image_buf_height (ImageBuf);
int image_buf_depth (ImageBuf);
int image_buf_color (ImageBuf);
int image_buf_bytes (ImageBuf);
int image_buf_row_bytes (ImageBuf);


#endif /* __IMAGE_BUF__ */
