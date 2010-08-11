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

/* 
 * This filter takes a grayscale input image and creates a new color
 *  image.
 */

#include "gimp.h"

/* Declare a local function.
 */
static void to_color (Image, Image);

static char *prog_name;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input, output;

  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      input = 0;
      output = 0;

      /* This is a regular filter. What that means is that it operates
       *  on the input image. Output is put into the ouput image. The
       *  filter should not worry, or even care where these images come
       *  from. The only guarantee is that they are the same size and
       *  depth.
       */
      input = gimp_get_input_image (0);
      if (input)
	switch (gimp_image_type (input))
	  {
	  case RGB_IMAGE:
	    gimp_message ("to-color: cannot operate on RGB images");
	    break;
	  case GRAY_IMAGE:
	  case INDEXED_IMAGE:
	    /* If the input image was a grayscale image, then create a new
	     *  image that is the same size, except that it is color.
	     */
	    output = gimp_new_image (0,
				     gimp_image_width (input),
				     gimp_image_height (input),
				     RGB_IMAGE);
	    
	    /* Do the conversion.
	     */
	    if (output)
	      {
		gimp_display_image (output);
		to_color (input, output);
		gimp_update_image (output);
	      }
	    break;
	  default:
	    gimp_message ("to-color: cannot operate on unknown image types");
	    break;
	}

      /* Free both images.
       */
      if (input)
	gimp_free_image (input);
      if (output)
	gimp_free_image (output);

      /* Quit
       */
      gimp_quit ();
    }

  return 0;
}

/* Simply takes the input image and converts it from grayscale
 *  to color. Note: The input must be a color image and the 
 *  output must be a grayscale image and they must both be the
 *  same size.
 */

static void
to_color (input, output)
     Image input, output;
{
  long width, height;
  long channels, rowstride;
  unsigned char *src, *dest;
  unsigned char *cmap;
  int offset;
  short row, col;
  
  /* Get the size of the input image. (This will/must be the same
   *  as the size of the output image.
   */
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  /* Get the source.
   */
  src = gimp_image_data (input);

  /* Get the destination.
   */
  dest = gimp_image_data (output);

  switch (gimp_image_type (input))
    {
    case GRAY_IMAGE:
      /* Loop over the pixels, combining as we go.
       */
      for (row = 0; row < height; row++)
	{
	  for (col = 0; col < width; col++)
	    {
	      *dest++ = *src;
	      *dest++ = *src;
	      *dest++ = *src++;
	    }
	}
      break;
    case INDEXED_IMAGE:
      cmap = gimp_image_cmap (input);
      for (row = 0; row < height; row++)
	{
	  for (col = 0; col < width; col++)
	    {
	      offset = *src++ * 3;
	      *dest++ = cmap[offset+0];
	      *dest++ = cmap[offset+1];
	      *dest++ = cmap[offset+2];
	    }
	}
      break;
    default:
      break;
    }
}
