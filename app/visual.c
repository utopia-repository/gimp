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
#include "visual.h"
#include "errors.h"

/*  Global variables  */

Visual *color_visual;		/* RGB color visual  */
Visual *grey_visual;		/* Greyscale visual  */

int     color_max_entries;      /* Max entries in RGB colormap */
int     grey_max_entries;       /* Max entries in grey colormap */

int     color_depth;            /* depth of color visual  */
int     grey_depth;             /* depth of grey visual   */

int     color_class;            /* class of color visual  */
int     grey_class;             /* class of grey visual  */

Boolean emulate_grey;           /* emulate grey with color? */


/*  Amount to shift color components to assemble color spec  */
int     red_shift;
int     green_shift;
int     blue_shift;

/*  Loss of precision from 8 bits--(i.e. with 5 bits, prec=3)  */
int     red_prec;
int     green_prec;
int     blue_prec; 


/*  These arrays are calculated for quick 24 bit to 16 color conversions  */
unsigned long  lookup_red [256];
unsigned long  lookup_green [256];
unsigned long  lookup_blue [256];


/*  We need to select the visuals with highest depth for the
 *  display, unless the user has explicitly requested a lesser
 *  depth.
 */

typedef struct _visual_type
{
  int class;
  int depth;
} visual_type;

visual_type color_visual_types[] =
{
  { TrueColor, 24 },
  { TrueColor, 16 },
  { TrueColor, 15 },
  { PseudoColor, 8 },
  { GrayScale, 8 },
};

visual_type grey_visual_types[] =
{
  { PseudoColor, 8 },
  { GrayScale, 8 },
};

int num_color_visual_types = XtNumber (color_visual_types);
int num_grey_visual_types = XtNumber (grey_visual_types);


static int
query_for_visual (depth, class, visual_info)
     int depth;
     int class;
     XVisualInfo *visual_info;
{
  if (app_data.depth)
    if (depth != app_data.depth)
      return 0;
  
  return XMatchVisualInfo (DISPLAY,
			   DefaultScreen (DISPLAY),
			   depth, class, visual_info);
}


static void
get_color_mask (mask, shift, prec)
     unsigned long mask;
     int * shift;
     int * prec;
{
  *shift = 0;
  *prec = 8;

  while (! (mask & 0x1))
    {
      /* Add 1 to the shift amount.
       * Note: "*shift++" does not work.
       */
      (*shift)++;
      mask >>= 1;
    }

  while (mask & 0x1)
    {
      /* Subtract 1 from the precision.
       * Note: "*prec--" does not work.
       */
      (*prec)--;
      mask >>= 1;
    }
}


static void
fill_lookup_array (array, shift, prec)
     unsigned long * array;
     int shift;
     int prec;
{
  int i;

  for (i = 0; i < 256; i++)
    array [i] = ((i >> prec) << shift);
}


void
get_standard_visuals ()
{
  XVisualInfo visual_info;
  int type;

  /*  find color visual  */
  color_visual = NULL;

  for (type = 0; type < num_color_visual_types; type++)
    if (query_for_visual (color_visual_types[type].depth,
			  color_visual_types[type].class,
			  &visual_info))
      {
	color_visual = visual_info.visual;
	color_depth = visual_info.depth;
	color_class = visual_info.class;
	color_max_entries = visual_info.colormap_size;
	if (visual_info.class == TrueColor)
	  {
	    get_color_mask (visual_info.red_mask, &red_shift, &red_prec);
	    get_color_mask (visual_info.green_mask, &green_shift, &green_prec);
	    get_color_mask (visual_info.blue_mask, &blue_shift, &blue_prec);

	    /*  initialize 24 bit to 16 bit color lookup acceleration  */
	    fill_lookup_array (lookup_red, red_shift, red_prec);
	    fill_lookup_array (lookup_green, green_shift, green_prec);
	    fill_lookup_array (lookup_blue, blue_shift, blue_prec);
	  }
	break;
      }

  /*  find grey visual  */
  grey_visual = NULL;

  for (type = 0; type < num_grey_visual_types; type++)
    if (query_for_visual (grey_visual_types[type].depth,
			  grey_visual_types[type].class,
			  &visual_info))
      {
	grey_visual = visual_info.visual;
	grey_depth = visual_info.depth;
	grey_class = visual_info.class;
	grey_max_entries = visual_info.colormap_size;
	break;
      }

  /*  If we can't find an appropriate 8 bit visual for greyscale,
      we'll emulate it with a color visual                        */
  if (!grey_visual)
    {
      emulate_grey = True;
      grey_visual = color_visual;
      grey_depth = color_depth;
      grey_class = color_class;
      grey_max_entries = color_max_entries;
    }

  if (!color_visual)
    fatal_error ("Unable to find suitable visual for image display.");

}

