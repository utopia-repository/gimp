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
#include <stdlib.h>
#include "appenv.h"
#include "gconvert.h"
#include "colormaps.h"
#include "visual.h"


void
_24_to_8_map (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;
     long w, h;
{
  unsigned char *src, *s;
  unsigned char *dest, *d;
  unsigned char pix;
  long src_width, dest_width;
  int i, j;
  XColor *col;

  src_width = gdisp->gimage->width * 3;
  src = gdisp->gimage->raw_image + src_width * y + x * 3;

  dest_width = gdisp->gimage->width;
  dest = gdisp->gimage->indexed_raw_image + dest_width * y + x;

  j = h;
  while (j--)
    {
      s = src;
      d = dest;
      i = w;
      while (i--)
	{
	  pix =  red_shades[*s++];
	  pix += green_shades[*s++];
	  pix += blue_shades[*s++];
      
	  col = rgbpal + pix;

	  *d++ = col->pixel;
	}
      src += src_width;
      dest += dest_width;

    }

}


void
_24_to_16_map (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;
     long w, h;
{
  unsigned char *src, *s;
  unsigned short *dest, *d;
  unsigned short pix;
  long src_width, dest_width;
  int i, j;

  src_width = gdisp->gimage->width * 3;
  src = gdisp->gimage->raw_image + src_width * y + x * 3;

  dest_width = gdisp->gimage->width;
  dest = (unsigned short *) gdisp->gimage->indexed_raw_image;
  dest += dest_width * y + x;

  j = h;
  while (j--)
    {
      s = src;
      d = dest;
      i = w;

      while (i--)
	{
	  pix =  ((unsigned short) (*s++ >> red_prec) << red_shift);
	  pix |= ((unsigned short) (*s++ >> green_prec) << green_shift);
	  pix |= ((unsigned short) (*s++ >> blue_prec) << blue_shift);

	  *d++ = pix;
	}

      src += src_width;
      dest += dest_width;
    }
}


void
intensity_map_to_8 (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;
     long w, h;
{
  unsigned char *src;
  unsigned char *dest;
  long src_width, dest_width;
  int j;

  src_width = gdisp->gimage->width;
  src = gdisp->gimage->raw_image + src_width * y + x;

  dest_width = gdisp->gimage->width;
  dest = gdisp->gimage->indexed_raw_image + dest_width * y + x;

  j = h;
  while (j--)
    {
      greyscale_8 (src, dest, w);
      
      src += src_width;
      dest += dest_width;
    }
}


void
intensity_map_to_16 (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;
     long w, h;
{
  unsigned char *src;
  unsigned short *dest;
  long src_width, dest_width;
  int j;

  src_width = gdisp->gimage->width;
  src = gdisp->gimage->raw_image + src_width * y + x;

  dest_width = gdisp->gimage->width;
  dest = (unsigned short *) gdisp->gimage->indexed_raw_image;
  dest += dest_width * y + x;

  j = h;
  while (j--)
    {
      greyscale_16 (src, dest, w);

      src += src_width;
      dest += dest_width;
    }
}


void 
greyscale_8 (src, dest, width)
     unsigned char * src;
     unsigned char * dest;
     long width;
{
  static XColor *col;
  
  while (width--)
    {
      col = greypal + *src++;
      *dest++ = col->pixel;
    }
}


void 
greyscale_16 (src, dest, width)
     unsigned char * src;
     unsigned short * dest;
     long width;
{
  while (width--)
    *dest++ = COLOR_COMPOSE (*src, *src, *src++);
}


void 
greyscale_24 (src, dest, width)
     unsigned char * src;
     unsigned long * dest;
     long width;
{
  while (width--)
    *dest++ = COLOR_COMPOSE (*src, *src, *src++);
}

