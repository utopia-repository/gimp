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
#include "paint_funcs.h"
#include "memutils.h"


/*  local variables  */
static unsigned char add_bounds [512];  /*  for accelerated adding  */


void
paint_funcs_setup ()
{
  int i; 

  for (i = 0; i < 512; i++)
    add_bounds [i] = (i < 256) ? i : 255;
}


void
color_pixels (dest, color, w, bytes)
     unsigned char * dest;
     unsigned char * color;
     int w;
     int bytes;
{
  int b;

  while (w--)
    for (b = 0; b < bytes; b++)
      *dest++ = color[b];
}


void
blend_pixels (src1, src2, dest, blend, w, bytes)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int blend;
     int w;
     int bytes;
{
  int length = w * bytes;
  unsigned char blend2 = (255 - blend);

  while (length --)
    *dest++ = (*src1++ * blend + *src2++ * blend2) / 255;

}


void
shade_pixels (src, dest, col, blend, w, bytes)
     unsigned char * src;
     unsigned char * dest;
     unsigned char * col;
     int blend;
     int w;
     int bytes;
{
  int b;
  unsigned char blend2 = (255 - blend);

  while (w --)
    {
      for (b = 0; b < bytes; b++)
	*dest++ = (*src++ * blend2 + col[b] * blend) / 255;
    }
}


void
composite_pixels (src1, src2, dest, mask, length, bytes)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     unsigned char * mask;
     int length;
     int bytes;
{
  int b;

  while (length--)
    {
      for (b = 0; b < bytes; b++)
	*dest++ = (*src1++ * *mask + *src2++ * (255 - *mask)) / 255;

      mask++;
    }
}


void
darken_pixels (src1, src2, dest, length, bytes)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int length;
     int bytes;
{
  int b;
  unsigned char s1, s2;

  while (length--)
    for (b = 0; b < bytes; b++)
      {
	s1 = *src1++;
	s2 = *src2++;
	*dest++ = (s1 < s2) ? s1 : s2;
      }
}


void
lighten_pixels (src1, src2, dest, length, bytes)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int length;
     int bytes;
{
  int b;
  unsigned char s1, s2;

  while (length--)
    for (b = 0; b < bytes; b++)
      {
	s1 = *src1++;
	s2 = *src2++;
	*dest++ = (s1 < s2) ? s2 : s1;
      }
}


void
rgb_only_pixels (src1, src2, dest, length, mode)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int length;
     int mode;
{
  int r1, g1, b1;
  int r2, g2, b2;

  while (length--)
    {
      r1 = src1[0]; g1 = src1[1]; b1 = src1[2];
      r2 = src2[0]; g2 = src2[1]; b2 = src2[2];

      switch (mode)
	{
	case RED_ONLY:
	  r2 = r1;
	  break;
	case GREEN_ONLY:
	  g2 = g1;
	  break;
	case BLUE_ONLY:
	  b2 = b1;
	  break;
	}

      /*  set the destination  */
      dest[0] = r2; dest[1] = g2; dest[2] = b2;
      
      src1 += 3;
      src2 += 3;
      dest += 3;
    }
}


void
hsv_only_pixels (src1, src2, dest, length, mode)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int length;
     int mode;
{
  int r1, g1, b1;
  int r2, g2, b2;

  while (length--)
    {
      r1 = src1[0]; g1 = src1[1]; b1 = src1[2];
      r2 = src2[0]; g2 = src2[1]; b2 = src2[2];
      rgb_to_hsv (&r1, &g1, &b1);
      rgb_to_hsv (&r2, &g2, &b2);

      switch (mode)
	{
	case HUE_ONLY:
	  r2 = r1;
	  break;
	case SATURATION_ONLY:
	  g2 = g1;
	  break;
	case VALUE_ONLY:
	  b2 = b1;
	  break;
	}

      /*  set the destination  */
      hsv_to_rgb (&r2, &g2, &b2);
      dest[0] = r2; dest[1] = g2; dest[2] = b2;
      
      src1 += 3;
      src2 += 3;
      dest += 3;
    }
}


void
add_pixels (src1, src2, dest, length)
     unsigned char * src1;
     unsigned char * src2;
     unsigned char * dest;
     int length;
{
  while (length --)
    *dest++ = add_bounds [*src1++ + *src2++];
}


void
B_W_pixels (src, dest, length)
     unsigned char * src;
     unsigned char * dest;
     int length;
{
  while (length --)
    *dest++ = (*src++) ? 255 : 0;
}


void
swap_pixels (src, dest, swap, length)
     unsigned char * src;
     unsigned char * dest;
     unsigned char * swap;
     int length;
{
  memcpy (swap, src, length);
  memcpy (src, dest, length);
  memcpy (dest, swap, length);
}


void
scale_pixels (src, dest, length, scale)
     unsigned char * src;
     unsigned char * dest;
     int length;
     int scale;
{
  while (length --)
    *dest++ = (unsigned char) ((*src++ * scale) / 255);
}

/**************************************************/
/*    REGION FUNCTIONS                            */
/**************************************************/


void
color_region (src, col)
     PixelRegion * src;
     unsigned char * col;
{
  int h;
  unsigned char * s;

  h = src->h;
  s = src->data;

  while (h--)
    {
      color_pixels (s, col, src->w, src->bytes);
      s += src->rowstride;
    }
}


void  
blend_region (src1, src2, dest, blend)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
     int blend;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
      blend_pixels (s1, s2, d, blend, src1->w, src1->bytes);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }
}


void  
shade_region (src, dest, col, blend)
     PixelRegion * src, * dest;
     unsigned char * col;
     int blend;
{
  int h;
  unsigned char * s, * d;

  s = src->data;
  d = dest->data;
  h = src->h;

  while (h --)
    {
      blend_pixels (s, d, col, blend, src->w, src->bytes);
      s += src->rowstride;
      d += dest->rowstride;
    }
}


void
copy_region (src, dest)
     PixelRegion * src, * dest;
{
  int h;
  int pixelwidth;
  unsigned char * s, * d;

  pixelwidth = src->w * src->bytes;
  s = src->data;
  d = dest->data;
  h = src->h;

  while (h --)
    {
      memcpy (d, s, pixelwidth);
      s += src->rowstride;
      d += dest->rowstride;
    }
}


void  
composite_region (src1, src2, dest, mask)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
     PixelRegion * mask;
{
  int h;
  unsigned char * s1, * s2, * d, * m;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  m = mask->data;
  h = src1->h;

  while (h --)
    {
      composite_pixels (s1, s2, d, m, src1->w, src1->bytes);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
      m += mask->rowstride;
    }

}


void  
darken_region (src1, src2, dest)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
      darken_pixels (s1, s2, d, src1->w, src1->bytes);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }

}


void  
lighten_region (src1, src2, dest)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
      lighten_pixels (s1, s2, d, src1->w, src1->bytes);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }

}


void  
rgb_only_region (src1, src2, dest, mode)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
     int mode;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
      rgb_only_pixels (s1, s2, d, src1->w, mode);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }

}


void  
hsv_only_region (src1, src2, dest, mode)
     PixelRegion * src1, * src2;
     PixelRegion * dest;
     int mode;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
      hsv_only_pixels (s1, s2, d, src1->w, mode);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }

}


void 
add_region (src1, src2, dest)
     PixelRegion * src1;
     PixelRegion * src2;
     PixelRegion * dest;
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  h = src1->h;
  d = dest->data;

  while (h --)
    {
      add_pixels (s1, s2, d, src1->w);
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }
}


void 
B_W_region (src, dest)
     PixelRegion * src;
     PixelRegion * dest;
{
  int h;
  unsigned char * s, * d;

  s = src->data;
  h = src->h;
  d = dest->data;

  while (h --)
    {
      B_W_pixels (s, d, src->w);
      s += src->rowstride;
      d += dest->rowstride;
    }
}


void
convolve_region (srcR, destR, matrix, size, divisor, mode)
     PixelRegion * srcR;
     PixelRegion * destR;
     int * matrix;
     int size;
     int divisor;
     int mode;
{
  /*  Convolve the src image using the convolution matrix, writing to dest  */
  unsigned char *src, *s_row, * s;
  unsigned char *dest, * d;
  int * m;
  int total [3];
  int b, bytes;
  int length;
  int wraparound;
  int margin;      /*  margin imposed by size of conv. matrix  */
  int i, j;
  int x, y;
  int offset;
  
  /*  If the mode is NEGATIVE, the offset should be 128  */
  if (mode == NEGATIVE)
    {
      offset = 128;
      mode = 0;
    }
  else
    offset = 0;

  /*  check for the boundary cases  */
  if (srcR->w < (size - 1) || srcR->h < (size - 1))
    return;

  /*  Initialize some values  */
  bytes = srcR->bytes;
  length = bytes * srcR->w;
  margin = size / 2;
  src = srcR->data;
  dest = destR->data;

  /*  calculate the source wraparound value  */
  wraparound = srcR->rowstride - size * bytes;

  /* copy the first (size / 2) scanlines of the src image... */
  for (i = 0; i < margin; i++)
    {
      memcpy (dest, src, length);
      src += srcR->rowstride;
      dest += destR->rowstride;
    }

  src = srcR->data;

  for (y = margin; y < srcR->h - margin; y++)
    {
      s_row = src;
      s = s_row + srcR->rowstride*margin;
      d = dest;

      /* handle the first margin pixels... */
      b = bytes * margin;
      while (b --)
	*d++ = *s++;

      /* now, handle the center pixels */
      x = srcR->w - margin*2;
      while (x--)
	{
	  s = s_row;

	  m = matrix;
	  total [0] = total [1] = total [2] = 0;
	  i = size;
	  while (i --)
	    {
	      j = size;
	      while (j --)
		{
		  for (b = 0; b < bytes; b++)
		    total [b] += *m * *s++;
		  m ++;
		}

	      s += wraparound;
	    }	      

	  for (b = 0; b < bytes; b++)
	    {
	      total [b] = total [b] / divisor + offset;

	      /*  only if mode was ABSOLUTE will mode by non-zero here  */
	      if (total [b] < 0 && mode)
		total [b] = - total [b];

	      if (total [b] < 0)
		*d++ = 0;
	      else
		*d++ = (total [b] > 255) ? 255 : (unsigned char) total [b];
	    }

	  s_row += bytes;

	}

      /* handle the last pixel... */
      s = s_row + (srcR->rowstride + bytes) * margin;
      b = bytes * margin;
      while (b --)
	*d++ = *s++;

      /* set the memory pointers */
      src += srcR->rowstride;
      dest += destR->rowstride;
    }

  src += srcR->rowstride*margin;

  /* copy the last (margin) scanlines of the src image... */
  for (i = 0; i < margin; i++)
    {
      memcpy (dest, src, length);
      src += srcR->rowstride;
      dest += destR->rowstride;
    }

}


void 
swap_region (src, dest)
     PixelRegion * src;
     PixelRegion * dest;
{
  int h;
  int length;
  unsigned char * s, * d, * swap;

  s = src->data;
  h = src->h;
  d = dest->data;
  length = src->w * src->bytes;
  swap = (unsigned char *) xmalloc (sizeof (char) * length);

  while (h --)
    {
      swap_pixels (s, d, swap, length);
      s += src->rowstride;
      d += dest->rowstride;
    }

  xfree (swap);
}



/*********************************
 *   color conversion routines   *
 *********************************/

void
rgb_to_hsv (r, g, b)
     int *r, *g, *b;
{
  int red, green, blue;
  float h, s, v;
  int min, max;
  int delta;
  
  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;
      
      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;
      
      if (red < blue)
	min = red;
      else
	min = blue;
    }
  
  v = max;
  
  if (max != 0)
    s = ((max - min) * 255) / (float) max;
  else
    s = 0;
  
  if (s == 0)
    h = 0;
  else
    {
      delta = max - min;
      if (red == max)
	h = (green - blue) / (float) delta;
      else if (green == max)
	h = 2 + (blue - red) / (float) delta;
      else if (blue == max)
	h = 4 + (red - green) / (float) delta;
      h *= 42.5;

      if (h < 0)
	h += 255;
      if (h > 255)
	h -= 255;
    }

  *r = h;
  *g = s;
  *b = v;
}


void
hsv_to_rgb (h, s, v)
     int *h, *s, *v;
{
  float hue, saturation, value;
  float f, p, q, t;

  if (*s == 0)
    {
      *h = *v;
      *s = *v;
      *v = *v;
    }
  else
    {
      hue = *h * 6.0 / 255.0;
      saturation = *s / 255.0;
      value = *v / 255.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - (saturation * f));
      t = value * (1.0 - (saturation * (1.0 - f)));
      
      switch ((int) hue)
	{
	case 0:
	  *h = value * 255;
	  *s = t * 255;
	  *v = p * 255;
	  break;
	case 1:
	  *h = q * 255;
	  *s = value * 255;
	  *v = p * 255;
	  break;
	case 2:
	  *h = p * 255;
	  *s = value * 255;
	  *v = t * 255;
	  break;
	case 3:
	  *h = p * 255;
	  *s = q * 255;
	  *v = value * 255;
	  break;
	case 4:
	  *h = t * 255;
	  *s = p * 255;
	  *v = value * 255;
	  break;
	case 5:
	  *h = value * 255;
	  *s = p * 255;
	  *v = q * 255;
	  break;
	}
    }
}


/************************************/
/*       apply paint modes          */
/************************************/

void
apply_paint_mode (src1, src2, dest, length, bytes, mode)
     unsigned char * src1, * src2;
     unsigned char * dest;
     int length, bytes;
     int mode;
{
  switch (mode)
    {
    case NORMAL :
      memcpy (dest, src1, length * bytes);
      break;
      
    case DARKEN_ONLY :
      darken_pixels (src1, src2, dest, length, bytes);
      break;
      
    case LIGHTEN_ONLY :
      lighten_pixels (src1, src2, dest, length, bytes);
      break;
      
    case RED_ONLY : case GREEN_ONLY : case BLUE_ONLY :
      /*  only works on RGB color images  */
      if (bytes == 3)
	rgb_only_pixels (src1, src2, dest, length, mode);
      else
	memcpy (dest, src1, length * bytes);
      break;
      
    case HUE_ONLY : case SATURATION_ONLY : case VALUE_ONLY :
      /*  only works on RGB color images  */
      if (bytes == 3)
	hsv_only_pixels (src1, src2, dest, length, mode);
      else
	memcpy (dest, src1, length * bytes);
      break;
      
    default :
      break;
    }

}














