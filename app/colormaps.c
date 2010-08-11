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
#include "general.h"
#include "visual.h"
#include "colormaps.h"

#define FREE_ENTRIES 8

Colormap rgbcmap;		/* rgb colormap */
XColor   rgbpal[256];		/* palette associated with rgbcmap  */

Colormap greycmap;		/* greyscale colormap */
XColor   greypal[256];		/* palette associated with rgbcmap  */

unsigned char *red_shades;
unsigned char *green_shades;
unsigned char *blue_shades;

unsigned int shades_r;
unsigned int shades_g;
unsigned int shades_b;
unsigned int shades_grey;

Pixel color_black_pixel;
Pixel color_white_pixel;
Pixel grey_black_pixel;
Pixel grey_white_pixel;

static unsigned int color_entries;
static unsigned int grey_entries;

/*
 *  Copies the contents of src to dest -- that is, all of the color defs
 */

static Colormap
create_colormap (visual, size)
     Visual * visual;
     unsigned int size;
{
  Colormap cmap;
  unsigned long pixels_return[256];
  
  cmap = XCreateColormap (DISPLAY,
			  XtWindow (toplevel),
			  visual, AllocNone);

  if (!XAllocColorCells (DISPLAY, cmap, False,
			 NULL, 0,
			 pixels_return, size))
    {
      fprintf (stderr, "Unable to allocate color cells.\n");
      exit (1);
    }
  
  return cmap;
			 
}

static void
copy_colormap (dest, src, index, size)
     Colormap dest;
     Colormap src;
     int index;
     int size;
{
  int i;
  XColor palette [256];

  for (i = index; i < index + size; i++)
    palette[i - index].pixel = i;

  XQueryColors (DISPLAY, src, palette, size);
  XStoreColors (DISPLAY, dest, palette, size);

}

static void
trim_colormap (cmap, index)
     Colormap cmap;
     int index;
{
  int i;
  int size;
  unsigned long pixels [256];

  size = (FREE_ENTRIES > index) ? index : FREE_ENTRIES;

  for (i = 0; i < size; i++)
    pixels [i] = index - i - 1;

  /*  Unallocate the cells not used by the application  */
  XFreeColors (DISPLAY, cmap, pixels, size, 0);

}

static void
match_RGB_colors (pal, match)
     XColor *pal;
     XColor *match;
{
  int i, cnt;
  int total, start;

  long red_mult, green_mult;
  float red_colors_per_shade;
  float green_colors_per_shade;
  float blue_colors_per_shade;

  total = shades_r * shades_g * shades_b;

  start = color_max_entries - total;

  cnt = 0;
  
  red_mult = shades_g * shades_b;
  red_colors_per_shade = 256.0 / shades_r;

  green_mult = shades_b;
  green_colors_per_shade = 256.0 / shades_g;

  blue_colors_per_shade = 256.0 / shades_b;

  for (i = 0; i < 256; i++)
    {
      red_shades[i] = ((unsigned char) (i / red_colors_per_shade)) * red_mult;
      green_shades[i] = ((unsigned char) (i / green_colors_per_shade)) * green_mult;
      blue_shades[i] = (unsigned char) (i / blue_colors_per_shade);
    }

  for (i = start; i < color_max_entries; i++)
    {
      pal[cnt].red = match[i].red >> 8;
      pal[cnt].green = match[i].green >> 8;
      pal[cnt].blue = match[i].blue >> 8;
      pal[cnt].pixel = match[i].pixel;
      
      cnt++;
    }
}


static void
create_8_bit_RGB ()
{
  unsigned int r, g, b, i;
  unsigned int dr, dg, db;
  unsigned int total;
  Colormap default_cmap, cmap;
  XColor palette [256];

  total = shades_r * shades_g * shades_b;

  color_entries = total;

  cmap = XCreateColormap (DISPLAY,
			  XtWindow (toplevel),
			  color_visual, AllocAll);
  
  default_cmap = DefaultColormapOfScreen (XtScreen (toplevel));

  for (i=0; i<color_max_entries; i++)
    palette[i].pixel = i;

  XQueryColors (DISPLAY, default_cmap, palette, color_max_entries);

  i = color_max_entries - total;

  dr = (shades_r > 1) ? (shades_r - 1) : (1);
  dg = (shades_g > 1) ? (shades_g - 1) : (1);
  db = (shades_b > 1) ? (shades_b - 1) : (1);

  for (r = 0; r < shades_r; r++)
    for (g = 0; g < shades_g; g++)
      for (b = 0; b < shades_b; b++)
	{
	  palette[i].pixel = i;
	  palette[i].red = (unsigned int) ((r * color_max_entries * 0xff) / dr);
	  palette[i].green = (unsigned int) ((g * color_max_entries * 0xff) / dg);
	  palette[i].blue = (unsigned int) ((b * color_max_entries * 0xff) / db);
	  palette[i].flags = DoRed | DoGreen | DoBlue;

	  i++;
	}

  /*  Set values of white and black pixels  */
  color_black_pixel = color_max_entries - total;
  color_white_pixel = color_max_entries - 1;

  match_RGB_colors (rgbpal, palette);

  XStoreColors (DISPLAY, cmap, palette, color_max_entries);

  /*  Free up some colors for X  */
  rgbcmap = create_colormap (color_visual, color_max_entries);
  copy_colormap (rgbcmap, cmap, 0, color_max_entries);
  trim_colormap (rgbcmap, (color_max_entries - color_entries));
 
  XFreeColormap (DISPLAY, cmap);
}


static void
match_greyscale_colors (pal, match)
     XColor * pal;
     XColor * match;
{
  unsigned int i, col, index;
  unsigned int last, next;

  index = (grey_max_entries - shades_grey);

  last = 0;

  for (i = 0; i < grey_max_entries; i++)
    {
      next = (match[index + 1].red >> 8);
      
      /*  our current shade of grey is closer to the next shade  */
      if (abs (next - i) < abs (last - i))
	col = index + 1;
      else
	col = index;

      pal->pixel = col;

      if (i == next) 
	{
	  last = next;
	  index ++;
	}
      pal++;
    }
	  
}


static void
create_8_bit_greyscale (num_greys)
     unsigned int num_greys;
{
  unsigned int i, d, start;
  Colormap default_cmap, cmap;
  XColor palette [256];

  if (num_greys > grey_max_entries)
    num_greys = grey_max_entries;

  grey_entries = num_greys;

  cmap = XCreateColormap (DISPLAY,
			  XtWindow (toplevel),
			  grey_visual, AllocAll);
  
  default_cmap = DefaultColormapOfScreen (XtScreen (toplevel));

  for (i = 0; i < grey_max_entries; i++)
    palette[i].pixel = i;

  XQueryColors (DISPLAY, default_cmap, palette, grey_max_entries);

  start = grey_max_entries - num_greys;
  d = (num_greys > 1) ? (num_greys - 1) : (1);

  for (i = start; i < grey_max_entries; i++)
    {
      palette[i].pixel = i;

      palette[i].red = palette[i].green = palette[i].blue = 
	(unsigned int) (((i - start) * 0xffff) / d);
      
      palette[i].flags = DoRed | DoGreen | DoBlue;

    }

  /*  Set values of white and black pixels  */
  grey_black_pixel = start;
  grey_white_pixel = grey_max_entries - 1;

  match_greyscale_colors (greypal, palette);

  XStoreColors (DISPLAY, cmap, palette, grey_max_entries);

  /*  Free up some colors for X  */
  greycmap = create_colormap (grey_visual, grey_max_entries);
  copy_colormap (greycmap, cmap, 0, grey_max_entries);
  trim_colormap (greycmap, (grey_max_entries - grey_entries));

  XFreeColormap (DISPLAY, cmap);
}


static void
parse_colorcube_resource (cc_str, r, g, b)
     char * cc_str;
     unsigned int *r;
     unsigned int *g;
     unsigned int *b;
{
  int count = 0;
  char * local_copy;
  char * colorcube;
  char * last;

  if (!cc_str)
    return;

  local_copy = xstrdup (cc_str);
  last = colorcube = local_copy;

  while (*colorcube)
    {
      if (*colorcube == '.')
	{
	  *colorcube = '\0';
	  switch (count)
	    {
	    case 0:
	      *r = atoi (last);
	      break;
	    case 1:
	      *g = atoi (last);
	      break;
	    }
	  count ++;
	  last = colorcube + 1;
	}
      colorcube++;
    }

  *b = atoi (last);

  if (*r < 2 || *g < 2 || *b < 2)
    {
      fprintf (stderr, "Invalid color cube specification.\n");
      if (*r < 2)
	*r = 2;
      if (*g < 2)
	*g = 2;
      if (*b < 2)
	*b = 2;
      fprintf (stderr, "Defaulting to [%d / %d / %d].\n", *r, *g, *b);
    }

  while ((*r * *g * *b) > color_max_entries)
    {
      fprintf (stderr, "Unable to create colorcube [%d, %d, %d].\n",
	       *r, *g, *b);

      if (*r > *g)
	if (*r > *b)
	  (*r)--;
        else
	  (*b)--;
      else if (*g > *b)
	(*g)--;
      else
	(*b)--;
	
      fprintf (stderr, "Defaulting to [%d, %d, %d]...\n",
	       *r, *g, *b);
    }

  /*  Free local copy of colorcube description string  */
  xfree (local_copy);

  return;  

}


/*************************************************************************/


Colormap
get_colormap (color_type)
     int color_type;
{
  Colormap cmap, default_cmap;

  switch (color_type)
    {
    case RGB_GIMAGE:
      switch (color_depth)
	{
	case 8 :
	case 16 : 
	case 15 : 
	case 24 :
	  return rgbcmap;
	  break;
	default :
	  return 0;
	  break;
	}
      break;
    case GREY_GIMAGE:
      /*  The greyscale colormap is always the same--no need to create one  */
      return greycmap;
      break;
    case INDEXED_GIMAGE :
      switch (color_depth)
	{
	case 8 :
	  default_cmap = DefaultColormapOfScreen (XtScreen (toplevel));
	  cmap = create_colormap (color_visual, 256);
	  copy_colormap (cmap, default_cmap, 0, 256);
	  return cmap;
	  break;

	case 16 : 
	case 15 : 
	case 24 :
	  return rgbcmap;
	  break;

	default :
	  return 0;
	  break;

	}
      break;
    default :
      return 0;
      break;
    }
}


void
create_standard_colormaps ()
{
  red_shades = (unsigned char*) xmalloc (sizeof (char) * 256);
  green_shades = (unsigned char*) xmalloc (sizeof (char) * 256);
  blue_shades = (unsigned char*) xmalloc (sizeof (char) * 256);

  /*  Parse the requested color cube and check for errors...  */
  if ((color_class != TrueColor) || (grey_class != TrueColor))
    {
      parse_colorcube_resource (app_data.colorcube,
				&shades_r, &shades_g, &shades_b);
    }
  else
    {
      switch (color_depth)
	{
	case 15:
	case 16:
	  shades_r = shades_g = shades_b = 5;
	  break;
	case 24:
	  shades_r = shades_g = shades_b = 8;
	  break;
	}
    }

  /*  For color visuals, we have to decide on how to create an appropriate
   *  colormap based on depth
   */
  switch (color_depth)
    {
    case 8 :
      create_8_bit_RGB ();
      break;
    case 15 : 
    case 16 : 
    case 24 :
      rgbcmap = XCreateColormap (DISPLAY,
				 XtWindow (toplevel),
				 color_visual, AllocNone);
      color_black_pixel = COLOR_COMPOSE (0, 0, 0);
      color_white_pixel = COLOR_COMPOSE (255, 255, 255);
      break;
    }
  

  /*  Calculate the number of grey tones from the total number of
      Colors requested in the RGB color space                      */
  shades_grey = shades_r * shades_g * shades_b;

  /* Handles the case of emulating greyscale with a truecolor visual...  */
  if (emulate_grey)
    {
      greycmap = XCreateColormap (DISPLAY,
				  XtWindow (toplevel),
				  grey_visual, AllocNone);

      grey_black_pixel = color_black_pixel;
      grey_white_pixel = color_white_pixel;
    }
  /* In the case of an 8 bit greyscale... */
  else
    create_8_bit_greyscale (shades_grey);

}


void
free_standard_colormaps ()
{
  if (rgbcmap)
    XFreeColormap (DISPLAY, rgbcmap);
  if (greycmap)
    XFreeColormap (DISPLAY, greycmap);
}


