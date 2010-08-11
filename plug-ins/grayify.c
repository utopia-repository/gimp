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
 * This filter converts the provided region to grayscale
 * assuming the original image was color.
 */

#include "gimp.h"

/* Declare a local function.
 */
static void grayify (Image, Image);

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
      /* This is a regular filter. What that means is that it operates
       *  on the input image. Output is put into the ouput image. The
       *  filter should not worry, or even care where these images come
       *  from. The only guarantee is that they are the same size and
       *  depth.
       */
      input = gimp_get_input_image (0);

      /* If input image is available and the input is color, then do some
       *  work. (Grayify). Then update the output image.
       */
      if (input)
	switch (gimp_image_type (input))
	  {
	  case RGB_IMAGE:
	    output = gimp_get_output_image (0);
	    if (output)
	      {
		grayify (input, output);
		gimp_update_image (output);
	      }
	    break;
	  case GRAY_IMAGE:
	    gimp_message ("grayify: cannot operate on grayscale images");
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("grayify: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("grayify: cannot operate on unknown image types");
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

static void
grayify (input, output)
     Image input, output;
{
  long width, height;
  long channels, rowstride;
  unsigned char *src_row, *dest_row;
  unsigned char *src, *dest;
  float val;
  short row, col;
  int x1, y1, x2, y2;
  
  /* Get the input area. This is the bounding box of the selection in 
   *  the image (or the entire image if there is no selection). Only
   *  operating on the input area is simply an optimization. It doesn't
   *  need to be done for correct operation. (It simply makes it go
   *  faster, since fewer pixels need to be operated on).
   */
  gimp_image_area (input, &x1, &y1, &x2, &y2);

  /* Get the size of the input image. (This will/must be the same
   *  as the size of the output image.
   */
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  src_row = gimp_image_data (input);
  dest_row = gimp_image_data (output);

  /* Advance the source and destination pointers
   */
  src_row += rowstride * y1 + (x1 * channels);
  dest_row += rowstride * y1 + (x1 * channels);

  for (row = y1; row < y2; row++)
    {
      src     = src_row;
      dest    = dest_row;
      /*  Calculate across the scanline  */
      for (col = x1; col < x2; col++)
	{
	  /*  Calculate an intensity value based on RGB  */
	  val  = 0.30 * *src++;  /* Red component */
	  val += 0.59 * *src++;  /* Green component */
	  val += 0.11 * *src++;  /* Blue component */
	  
	  *dest++ = (unsigned char) val;
	  *dest++ = (unsigned char) val;
	  *dest++ = (unsigned char) val;
	}	  

      src_row += rowstride;
      dest_row += rowstride;
    }
}
