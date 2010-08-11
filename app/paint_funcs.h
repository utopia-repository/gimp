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
#ifndef __PAINT_FUNCS_H__
#define __PAINT_FUNCS_H__

typedef struct _PixelRegion PixelRegion;

struct _PixelRegion
{
  unsigned char *    data;       /*  pointer to region data  */
  int                rowstride;  /*  bytes per pixel row  */
  int                w, h;       /*  width and  height of region  */
  int                bytes;      /*  bytes per pixel  */
};


/*  Called initially to setup accelerated rendering features  */
void  paint_funcs_setup     ();

/*  set pixels to specified color  */
void  color_pixels          (unsigned char *, unsigned char *, int, int);

/*  blend a series of pixels with a blend value  */
void  blend_pixels          (unsigned char *, unsigned char *, 
			     unsigned char *, int, int, int);

/*  shade a series of pixels based on some blend value  */
void  shade_pixels          (unsigned char *, unsigned char *, 
			     unsigned char *, int, int, int);

/*  composite two series of pixels using a mask  */
void  composite_pixels      (unsigned char *, unsigned char *, 
	 		     unsigned char *, unsigned char *, int, int);

void  darken_pixels         (unsigned char *, unsigned char *, 
	 		     unsigned char *, int, int);

void  lighten_pixels        (unsigned char *, unsigned char *, 
	 		     unsigned char *, int, int);

void  rgb_only_pixels       (unsigned char *, unsigned char *, 
	 		     unsigned char *, int, int);

void  hsv_only_pixels       (unsigned char *, unsigned char *, 
	 		     unsigned char *, int, int);

void  add_pixels            (unsigned char *, unsigned char *,
			     unsigned char *, int);

void  B_W_pixels            (unsigned char *, unsigned char *, int);

/*  swap the pixels in two arrays  */
void  swap_pixels           (unsigned char *, unsigned char *, 
			     unsigned char *, int);

/*  scale the intensity of pixels  */
void  scale_pixels          (unsigned char *, unsigned char *, 
			     int, int);



/*  Region functions  */
void  color_region          (PixelRegion *, unsigned char *);

void  blend_region          (PixelRegion *, PixelRegion *,              
			     PixelRegion *, int);

void  shade_region          (PixelRegion *, PixelRegion *, 
			     unsigned char *, int);

void  copy_region           (PixelRegion *, PixelRegion *);


void  composite_region      (PixelRegion *, PixelRegion *,
			     PixelRegion *, PixelRegion *);

void  darken_region         (PixelRegion *, PixelRegion *, 
			     PixelRegion *);

void  lighten_region        (PixelRegion *, PixelRegion *, 
			     PixelRegion *);

void  rgb_only_region       (PixelRegion *, PixelRegion *, 
			     PixelRegion *, int);

void  hsv_only_region       (PixelRegion *, PixelRegion *, 
			     PixelRegion *, int);

void  add_region            (PixelRegion *, PixelRegion *, 
			     PixelRegion *);

void  B_W_region            (PixelRegion *, PixelRegion *);


/*  The types of convolutions  */
#define NORMAL     0   /*  Negative numbers truncated  */
#define ABSOLUTE   1   /*  Absolute value              */
#define NEGATIVE   2   /*  add 127 to values           */

void  convolve_region       (PixelRegion *, PixelRegion *,
			     int *, int, int, int);

void  swap_region           (PixelRegion *, PixelRegion *);


/*  Color conversion routines  */
void  rgb_to_hsv            (int *, int *, int *);
void  hsv_to_rgb            (int *, int *, int *);


/*  Applying paint modes...  */

/*  Paint Modes  */
#define NORMAL           0
#define DARKEN_ONLY      1
#define LIGHTEN_ONLY     2
#define RED_ONLY         3
#define GREEN_ONLY       4
#define BLUE_ONLY        5
#define HUE_ONLY         6
#define SATURATION_ONLY  7
#define VALUE_ONLY       8


void  apply_paint_mode      (unsigned char *, unsigned char *,
			     unsigned char *, int, int, int);

#endif  /*  __PAINT_FUNCS_H__  */








