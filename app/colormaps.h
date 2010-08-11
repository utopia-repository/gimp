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
#ifndef __COLORMAPS_H__
#define __COLORMAPS_H__

#include "gimage.h"             /* For the image types  */

extern Colormap rgbcmap;	/* RGB colormap */
extern XColor rgbpal[256];	/* palette associated with rgbcmap  */

extern Colormap greycmap;	/* greyscale colormap */
extern XColor greypal[256];	/* palette associated with greycmap  */

extern unsigned char *red_shades;
extern unsigned char *green_shades;
extern unsigned char *blue_shades;

extern unsigned int shades_r;   /*  shades of red in the RGB colormap  */
extern unsigned int shades_g;   /*  shades of green in the RGB colormap  */
extern unsigned int shades_b;   /*  shades of blue in the RGB colormap  */
extern unsigned int shades_grey;/*  shades of grey in the greyscale colormap  */

/*  Pixel values of black and white in the color and grey visuals  */
extern Pixel color_black_pixel;
extern Pixel color_white_pixel;
extern Pixel grey_black_pixel;
extern Pixel grey_white_pixel;


Colormap	get_colormap (int);
void		create_standard_colormaps ();
void		free_standard_colormaps ();


#endif  /*  __COLORMAPS_H__  */
