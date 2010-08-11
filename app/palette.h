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
#ifndef __PALETTE_H__
#define __PALETTE_H__

#include "color_select.h"
#include "image_buf.h"
#include "linked.h"

#define FOREGROUND 0
#define BACKGROUND 1

/*  The states for updating a color in the palette via palette_set_* calls */
#define COLOR_NEW      0
#define COLOR_UPDATE   1
#define COLOR_FINISH   2

typedef struct _Palette _Palette, *PaletteP;
typedef struct _PaletteEntry _PaletteEntry, *PaletteEntryP;

struct _Palette {
  Widget shell;
  Widget dialog;
  Widget color_area;
  Widget scroll_bar;
  Widget preview_widget;
  Widget text_widget[3];
  link_ptr entries;
  int n_entries;
  PaletteEntryP color[2];
  XColor preview_color;
  Pixmap preview_pixmap;
  ImageBuf color_image;
  ColorSelectP color_select;
  int color_select_active;
  int scroll_offset;
  int color_state;
  int alloc_colors;
  int updating;
  GC gc;
};

struct _PaletteEntry {
  unsigned char color[3];
  int position;
};

void palette_create (void);
void palette_free (void);
void palette_get_foreground (unsigned char *, unsigned char *, unsigned char *);
void palette_get_background (unsigned char *, unsigned char *, unsigned char *);
void palette_set_active_color (int, int, int, int);
void palette_set_foreground (int, int, int, int);
void palette_set_background (int, int, int, int);

#endif /* __PALETTE_H__ */
