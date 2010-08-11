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

#ifndef __PIXEL_SURROUND_H__
#define __PIXEL_SURROUND_H__


/* PixelSurround describes a (read-only)
 *  region around a pixel in a tile manager
 */

typedef struct _PixelSurround
{
  Tile        *tile;
  TileManager *mgr;
  guchar      *buff;
  gint         buff_size;
  gint         bpp;
  gint         w;
  gint         h;
  guchar       bg[MAX_CHANNELS];
  gint         row_stride;
} PixelSurround;


void     pixel_surround_init      (PixelSurround *ps,
				   TileManager   *tm,
				   gint           w,
				   gint           h,
				   guchar         bg[MAX_CHANNELS]);

/* return a pointer to a buffer which contains all the surrounding pixels
 * strategy: if we are in the middle of a tile, use the tile storage
 * otherwise just copy into our own malloced buffer and return that
 */
guchar * pixel_surround_lock      (PixelSurround *ps,
				   gint           x,
				   gint           y);

gint     pixel_surround_rowstride (PixelSurround *ps);

void     pixel_surround_release   (PixelSurround *ps);

void     pixel_surround_clear     (PixelSurround *ps);


#endif /* __PIXEL_SURROUND_H__ */
