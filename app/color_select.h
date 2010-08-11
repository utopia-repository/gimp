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
#ifndef __COLOR_SELECT_H__
#define __COLOR_SELECT_H__

#include "image_buf.h"

typedef struct _ColorSelect _ColorSelect, *ColorSelectP;
typedef void (*ColorSelectCallback) (int, int, int, int);

struct _ColorSelect {
  Widget shell;
  Widget dialog;
  Widget xy_color;
  Widget z_color;
  Widget sliders[6];
  Widget toggles[6];
  Widget new_color;
  Widget orig_color;
  Pixmap new_color_pixmap;
  Pixmap orig_color_pixmap;
  int alloc_colors;
  XColor ncolor;
  XColor ocolor;
  ImageBuf xy_color_image;
  ImageBuf z_color_image;
  int pos[3];
  int values[6];
  int z_color_fill;
  int xy_color_fill;
  int orig_values[3];
  ColorSelectCallback callback;
  GC gc;
};

ColorSelectP color_select_new (int, int, int, ColorSelectCallback);
void color_select_show (ColorSelectP);
void color_select_hide (ColorSelectP);
void color_select_free (ColorSelectP);
void color_select_set_color (ColorSelectP, int, int, int, int);

#endif /* __COLOR_SELECT_H__ */
