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
#include "gdisplay.h"
#include "gdisplay_ops.h"
#include "scale.h"
#include "tools.h"
#include "visual.h"

void
bounds_checking (gdisp)
     GDisplay *gdisp;
{
  Dimension sx, sy;

  sx = SCALE(gdisp, gdisp->gimage->width);
  sy = SCALE(gdisp, gdisp->gimage->height);
  
  gdisp->offset_x = bounds (gdisp->offset_x, 0,
			    LOWPASS (sx - gdisp->disp_image->width));
  
  gdisp->offset_y = bounds (gdisp->offset_y, 0, 
			    LOWPASS (sy - gdisp->disp_image->height));


}


void
resize_display (gdisp)
     GDisplay *gdisp;
{
  /* freeze the active tool */
  active_tool_control (PAUSE, (void *) gdisp);

  gdisplay_resize_image (gdisp);

  bounds_checking (gdisp);
  setup_scale (gdisp);

  gdisplay_disp_region (gdisp, 0, 0,
			gdisp->disp_image->width, 
			gdisp->disp_image->height, 0);

  /* re-enable the active tool */
  active_tool_control (RESUME, (void *) gdisp);

}


void
change_scale (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  int dir;
  GDisplay *gdisp;
  unsigned char scalesrc, scaledest;
  double offset_x, offset_y;
  long sx, sy;

  gdisp = gdisplay_active (w);

  if (gdisp)
    {
      scalesrc = SCALESRC(gdisp);
      scaledest = SCALEDEST(gdisp);

      offset_x = gdisp->offset_x + (gdisp->disp_image->width/2.0);
      offset_y = gdisp->offset_y + (gdisp->disp_image->height/2.0);

      offset_x *= ((double) scalesrc / (double) scaledest);
      offset_y *= ((double) scalesrc / (double) scaledest);

      dir = (int) client_data;

      switch (dir)
	{
	case ZOOMIN :
	  if (scalesrc > 1)
	    scalesrc--;
	  else
	    if (scaledest < 0xff)
	      scaledest++;

	  break;
	case ZOOMOUT :
	  if (scaledest > 1)
	    scaledest--;
	  else
	    if (scalesrc < 0xff)
	      scalesrc++;

	  break;
	default :
	  break;
	}

      sx = (gdisp->gimage->width * scaledest) / scalesrc;
      sy = (gdisp->gimage->height * scaledest) / scalesrc;

      /*  The slider value is a short, so make sure we are within its
	  range.  If we are trying to scale past it, then stop the scale  */
      if (sx < 0xffff && sy < 0xffff)
	{
	  gdisp->scale = (scaledest << 8) + scalesrc;

	  /*  set the offsets  */
	  offset_x *= ((double) scaledest / (double) scalesrc);
	  offset_y *= ((double) scaledest / (double) scalesrc);
	  
	  gdisp->offset_x = (long) (offset_x - (gdisp->disp_image->width/2.0));
	  gdisp->offset_y = (long) (offset_y - (gdisp->disp_image->height/2.0));

	  /*  resize the image  */
	  resize_display (gdisp);

	}
    }
}


void
setup_scale (gdisp)
     GDisplay *gdisp;
{
  Dimension sx, sy;

  sx = SCALE(gdisp, gdisp->gimage->width);
  sy = SCALE(gdisp, gdisp->gimage->height);

  XtVaSetValues (gdisp->hsb,
		 XmNvalue, gdisp->offset_x,
		 XmNmaximum, sx,
		 XmNsliderSize, gdisp->disp_image->width,
		 XmNpageIncrement, gdisp->disp_image->width,
		 NULL);

  XtVaSetValues (gdisp->vsb,
		 XmNvalue, gdisp->offset_y,
		 XmNmaximum, sy,
		 XmNsliderSize, gdisp->disp_image->height,
		 XmNpageIncrement, gdisp->disp_image->height,
		 NULL);

}


unsigned char *
accelerate_scaling (width, start, bpp, scalesrc, scaledest)
     long width;
     long start;
     int bpp;
     int scalesrc;
     int scaledest;
{
  long i;
  unsigned char *scale;

  scale = (unsigned char *) xmalloc (sizeof (unsigned char) * width + 1);
  if (!scale)
    {
      fprintf(stderr, "Unable to allocate memory for accelerated scaling.\n");
      exit (1);
    }

/*
  for (i = start+1; i < start+width; i++)
    scale[i-start-1] = (i % scaledest) ? 0 : scalesrc*bpp;
*/

  for (i = 0; i <= width; i++)
    scale[i] = ((i + start + 1) % scaledest) ? 0 : scalesrc * bpp;
    
  return scale;
}



/*****************************************************************/
/*  This function is the core of the display--it offsets and     */
/*  scales the image according to the current parameters in the  */
/*  gdisp object.  It handles color, greyscale, 8, 15, 16, 24,   */
/*  & 32 bit output depths.                                      */
/*                                                               */
/*****************************************************************/


void
scale_image (gdisp, x, y, w, h)
     GDisplay *gdisp;
     long x, y;       /* vert & horiz offsets */
     long w, h;       /* width and height to be scaled */
{
  unsigned char *src, *s;
  unsigned char *dest, *d;
  unsigned short *d16_bit;
  unsigned long *d24_bit;
  short bpp;
  long width, height;
  long srcwidth, destwidth, destlength;
  unsigned char scalesrc, scaledest;
  unsigned char *scale, *scalewalk;
  unsigned char *cmap;
  int red_byte, green_byte, blue_byte;
  int val;
  long i, j;
  long sx, sy;
  int bytes_per_pixel;
  Boolean initial;
  Boolean byte_order;

  byte_order = ImageByteOrder (DISPLAY);

  scalesrc = SCALESRC (gdisp);
  scaledest = SCALEDEST (gdisp);

  width = gdisp->gimage->width;
  height = gdisp->gimage->height;

  red_byte = 0;
  green_byte = 0;
  blue_byte = 0;

  switch (gdisp->depth)
    {
    case 8 :
      switch (gdisp->gimage->type)
	{
	case RGB_GIMAGE:
	  bpp = 1;
	  src = gdisp->gimage->indexed_raw_image;
	  break;
	case GREY_GIMAGE:
	  /*  Set the appropriate variables for emulating greyscale  */
	  bpp = 1;
	  src = gdisp->gimage->indexed_raw_image;
	  break;
	case INDEXED_GIMAGE:
	  bpp = 1;
	  src = gdisp->gimage->raw_image;
	  break;
	}

      bytes_per_pixel = (gdisp->disp_image->bits_per_pixel >> 3);
      break;

    case 15 : 
    case 16 : 
    case 24 :
      switch (gdisp->gimage->type)
	{
	case RGB_GIMAGE:
	  bpp = 3;
	  red_byte = 0;
	  green_byte = 1;
	  blue_byte = 2;
	  break;
	case GREY_GIMAGE:
	  /*  Set the appropriate variables for emulating greyscale  */
	  bpp = 1;
	  red_byte = 0;
	  green_byte = 0;
	  blue_byte = 0;
	  break;
	case INDEXED_GIMAGE:
	  bpp = 1;
	  red_byte = 0;
	  green_byte = 0;
	  blue_byte = 0;
	  break;
	}

      bytes_per_pixel = (gdisp->disp_image->bits_per_pixel >> 3);
      src = gdisp->gimage->raw_image;
      break;

    default :
      return;
    }

  srcwidth = width * bpp;
  destwidth =  gdisp->disp_image->bytes_per_line;
  destlength = w * bytes_per_pixel;
  
  sy = ((gdisp->offset_y + y) * scalesrc) / scaledest;
  sx = ((gdisp->offset_x + x) * scalesrc) / scaledest;

  src += (sy * srcwidth) + (sx * bpp);

  dest = gdisp->disp_image->data;
  dest += (y * destwidth) + (x * bytes_per_pixel);

  x += gdisp->offset_x;
  y += gdisp->offset_y;

  scale = accelerate_scaling (w, x, bpp, scalesrc, scaledest);

  i = y + h;
  initial = True;

  switch (gdisp->depth)
    {
    case 8:
      for ( ; y < i; y++)
	{
	  s = src;
	  d = dest;
	  if ((y % scaledest) && !initial)
	    memcpy (dest, d - destwidth, destlength);
	  else
	    {
	      scalewalk = scale;
	      j = w;
	      while (j--) 
		{
		  *d++ = *s;
		  s = s + *scalewalk++;
		}
	      
	      src += srcwidth * scalesrc;
	    }
	  
	  dest += destwidth;
	  initial = False;
	}
      break;
      
    case 15:
    case 16:
      if (gdisp->gimage->type == INDEXED_GIMAGE)
	{
	  cmap = gdisp->gimage->cmap;
	  
	  for ( ; y < i; y++)
	    {
	      s = src;
	      d16_bit = (unsigned short *) dest;
	      if ((y % scaledest) && !initial)
		memcpy (dest, dest - destwidth, destlength);
	      else
		{
		  scalewalk = scale;
		  j = w;

		  while (j--)
		    {
		      val = *s * 3;
		      *d16_bit++ =
			COLOR_COMPOSE (cmap[val], cmap[val+1], cmap[val+2]);
		      s += *scalewalk++;
		    }
		  src += srcwidth * scalesrc;
		}
	      
	      dest += destwidth;
	      initial = False;
	    }
	}
      else
	{
	  for ( ; y < i; y++)
	    {
	      s = src;
	      d16_bit = (unsigned short *) dest;
	      if ((y % scaledest) && !initial)
		memcpy (dest, dest - destwidth, destlength);
	      else
		{
		  scalewalk = scale;
		  j = w;
		  
		  while (j--) 
		    {
		      *d16_bit++ = 
			COLOR_COMPOSE (s[red_byte], s[green_byte], s[blue_byte]);
		      s += *scalewalk++;
		    }
		  src += srcwidth * scalesrc;
		}
	      
	      dest += destwidth;
	      initial = False;
	    }
	}
      break;

    case 24:
      /* 3 bytes per pixel is not yet handled */
      if (bytes_per_pixel == 4)
	{
	  if (gdisp->gimage->type == INDEXED_GIMAGE)
	    {
	      cmap = gdisp->gimage->cmap;
	  
	      for ( ; y < i; y++)
		{
		  s = src;
		  d24_bit = (unsigned long *) dest;
		  if ((y % scaledest) && !initial)
		    memcpy (dest, dest - destwidth, destlength);
		  else
		    {
		      scalewalk = scale;
		      j = w;
		      
		      while (j--) 
			{
			  val = *s * 3;
			  *d24_bit++ = 
			    COLOR_COMPOSE (cmap[val], cmap[val+1], cmap[val+2]);
			  s += *scalewalk++;
			}
		      src += srcwidth * scalesrc;
		    }
		  
		  dest += destwidth;
		  initial = False;
		}
	    }
	  else
	    {
	      for ( ; y < i; y++)
		{
		  s = src;
		  d24_bit = (unsigned long *) dest;
		  if ((y % scaledest) && !initial)
		    memcpy (dest, dest - destwidth, destlength);
		  else
		    {
		      scalewalk = scale;
		      j = w;
		      
		      while (j--) 
			{
			  *d24_bit++ = 
			    COLOR_COMPOSE (s[red_byte], s[green_byte], s[blue_byte]);
			  s += *scalewalk++;
			}
		      src += srcwidth * scalesrc;
		    }
		  
		  dest += destwidth;
		  initial = False;
		}
	    }
	}
      break;
    }
  
  xfree (scale);
}
