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
#ifndef __VISUAL_H__
#define __VISUAL_H__

extern Visual *color_visual;
extern Visual *grey_visual;

extern int     color_max_entries;
extern int     grey_max_entries;

extern int     color_depth;
extern int     grey_depth;

extern int     color_class;
extern int     grey_class;

extern Boolean emulate_grey;


extern int     red_shift;
extern int     green_shift;
extern int     blue_shift;

extern int     red_prec;
extern int     green_prec;
extern int     blue_prec;

/*  These arrays are calculated for quick 24 bit to 16 color conversions  */
extern unsigned long  lookup_red [];
extern unsigned long  lookup_green [];
extern unsigned long  lookup_blue [];

/*  This is a macro for quickly doing a conversion from 24 bit color
 *  to 16 bit color...
 */
#define COLOR_COMPOSE(r,g,b) (lookup_red [r] | lookup_green [g] | lookup_blue [b])

void get_standard_visuals ();


#endif  /*  __VISUAL_H__  */
