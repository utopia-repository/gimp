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
#include "gimp.h"

/* Declare a local function.
 */
static void blur (Image, Image);

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
      output = gimp_get_output_image (0);

      /* If both an input and output image were available, then do some
       *  work. (Blur). Then update the output image.
       */
      if (input && output)
	{
	  if ((gimp_image_type (input) == RGB_IMAGE) ||
	      (gimp_image_type (input) == GRAY_IMAGE))
	    {
	      gimp_init_progress ("blur");
	      blur (input, output);
	      gimp_update_image (output);
	    }
	  else
	    gimp_message ("blur: cannot operate on indexed color images");
	}
      
      /* Free both images.
       */
      gimp_free_image (input);
      gimp_free_image (output);

      /* Quit
       */
      gimp_quit ();
    }

  return 0;
}

static void
blur (input, output)
     Image input, output;
{
  long width, height;
  long channels, rowstride;
  long val;
  unsigned char *dest;
  unsigned char *prev_row;
  unsigned char *cur_row;
  unsigned char *next_row;
  short row, col;
  int x1, y1, x2, y2;
  int left, right;
  int top, bottom;

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

  left = (x1 == 0);
  right = (x2 == width);
  top = (y1 == 0);
  bottom = (y2 == height);
  
  if (left)
    x1++;
  if (right)
    x2--;
  if (top)
    y1++;
  if (bottom)
    y2--;

  x1 *= channels;
  x2 *= channels;

  prev_row = gimp_image_data (input);
  prev_row += (y1 - 1) * rowstride;
  cur_row = prev_row + rowstride;
  next_row = cur_row + rowstride;

  dest = gimp_image_data (output);
  if (top)
    {
      if (left)
	for (col = 0; col < channels; col++)
	  {
	    val = ((long) cur_row[col] + (long) cur_row[col + channels] +
		   (long) next_row[col] + (long) next_row[col + channels]) / 4;
	    *dest++ = val;
	  }
      
      for (col = x1; col < x2; col++)
	{
	  val = ((long) cur_row[col - channels] + (long) cur_row[col] +
		 (long) cur_row[col + channels] + (long) next_row[col - channels] +
		 (long) next_row[col] + (long) next_row[col + channels]) / 6;
	  *dest++ = val;
	}

      if (right)
	for (col = col; col < rowstride; col++)
	  {
	    val = ((long) cur_row[col] + (long) cur_row[col - channels] +
		   (long) next_row[col] + (long) next_row[col - channels]) / 4;
	    *dest++ = val;
	  }
    }
  else
    {
      dest += y1 * rowstride;
    }

  for (row = y1; row < y2; row++)
    {
      if (left)
	{
	  for (col = 0; col < channels; col++)
	    {
	      val = ((long) prev_row[col] + (long) prev_row[col + channels] +
		     (long) cur_row[col] + (long) cur_row[col + channels] +
		     (long) next_row[col] + (long) next_row[col + channels]) / 6;
	      *dest++ = val;
	    }
	}
      else
	{
	  dest += x1;
	}
      
      for (col = x1; col < x2; col++)
	{
	  val = ((long) prev_row[col - channels] + (long) prev_row[col] + 
		 (long) prev_row[col + channels] + (long) cur_row[col - channels] +
		 (long) cur_row[col] + (long) cur_row[col + channels] +
		 (long) next_row[col - channels] + (long) next_row[col] +
		 (long) next_row[col + channels]) / 9;
	  *dest++ = val;
	}

      if (right)
	{
	  for (col = col; col < rowstride; col++)
	    {
	      val = ((long) prev_row[col] + (long) prev_row[col - channels] +
		     (long) cur_row[col] + (long) cur_row[col - channels] +
		     (long) next_row[col] + (long) next_row[col - channels]) / 6;
	      *dest++ = val;
	    }
	}
      else
	{
	  dest += rowstride - x2;
	}

      prev_row = cur_row;
      cur_row = next_row;
      next_row += rowstride;
      
      if ((row % 5) == 0)
	gimp_do_progress (row, y2 - y1);
    }
  
  if (bottom)
    {
      if (left)
	for (col = 0; col < channels; col++)
	  {
	    val = ((long) prev_row[col] + (long) prev_row[col + channels] +
		   (long) cur_row[col] + (long) cur_row[col + channels]) / 4;
	    *dest++ = val;
	  }
      
      for (col = x1; col < x2; col++)
	{
	  val = ((long) prev_row[col - channels] + (long) prev_row[col] +
		 (long) prev_row[col + channels] + (long) cur_row[col - channels] +
		 (long) cur_row[col] + (long) cur_row[col + channels]) / 6;
	  *dest++ = val;
	}

      if (right)
	for (col = col; col < rowstride; col++)
	  {
	    val = ((long) prev_row[col] + (long) prev_row[col - channels] +
		   (long) cur_row[col] + (long) cur_row[col - channels]) / 4;
	    *dest++ = val;
	  }
    }
}
