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
#ifndef __GCONVERT_H__
#define __GCONVERT_H__

#include "gdisplay.h"
#include "gimage.h"

void _24_to_8_map        (GDisplay *, long, long, long, long);
void _24_to_16_map       (GDisplay *, long, long, long, long);
void intensity_map_to_8  (GDisplay *, long, long, long, long);
void intensity_map_to_16 (GDisplay *, long, long, long, long);

/*  convenience routines...  */
void greyscale_8         (unsigned char *, unsigned char *, long);
void greyscale_16        (unsigned char *, unsigned short *, long);
void greyscale_24        (unsigned char *, unsigned long *, long);


#endif
