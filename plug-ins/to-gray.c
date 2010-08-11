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
 * This filter takes a color input image and creates a new grayscale
 *  image.
 */

#include "gimp.h"

/* Declare a local function.
 */
static void to_gray (Image, Image);

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

  /* Call 'filter_init' to initialize this filter.
   * 'filter_init' makes sure that the filter was properly called and
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
	  case INDEXED_IMAGE:
	    /* If the input image was a color image, then create a new
	     *  image that is the same size, except that it is grayscale.
	     */
	    output = gimp_new_image (0,
				     gimp_image_width (input),
				     gimp_image_height (input),
				     GRAY_IMAGE);
	    
	    /* Do the conversion.
	     */
	    if (output)
	      {
		gimp_display_image (output);
		to_gray (input, output);
		gimp_update_image (output);
	      }
	    break;
	  case GRAY_IMAGE:
	    gimp_message ("to-gray: cannot operate on grayscale images");
	    break;
	  default:
	    gimp_message ("to-gray: cannot operate on unknown image types");
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

/* Simply takes the input image and converts it from color
 *  to grayscale. Note: The input must be an RGB color or 
 *  indexed image and the output must be a grey image and 
 *  they must both be the same size.
 */

#define INTENSITY(r,g,b) (r * 0.30 + g * 0.59 + b * 0.11)

static void
to_gray (input, output)
     Image input, output;
{
  long width, height;
  long channels, rowstride;
  unsigned char *src, *dest;
  unsigned char *cmap;
  float val;
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
    case RGB_IMAGE:
      /* Loop over the pixels, averaging as we go.
       */
      for (row = 0; row < height; row++)
	{
	  for (col = 0; col < width; col++)
	    {
	      val = INTENSITY (src[0], src[1], src[2]);
	      *dest++ = (unsigned char) val;
	      src += 3;
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
	      val = INTENSITY(cmap[offset+0], cmap[offset+1], cmap[offset+2]);
	      *dest++ = (unsigned char) val;
	    }
	}
      break;
    default:
      break;
    }
}
