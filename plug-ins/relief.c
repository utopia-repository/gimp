/* relief filter for the GIMP
 *  -Peter Mattis
 *
 * The code for this filter is heavily based upon the code for
 *  "ppmrelief" in the "netpbm" package. That code is freely modifyable
 *  as long as the following notice appears.
 */

/* ppmrelief.c - generate a relief map of a portable pixmap
**
** Copyright (C) 1990 by Wilson H. Bent, Jr.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <stdio.h>
#include "gimp.h"

static void relief (Image, Image);
static char *prog_name;

int
main (argc, argv)
     int argc;
     char *argv[];
{
  Image input, output;

  prog_name = argv[0];
  if (gimp_init (argc, argv))
    {
      input = gimp_get_input_image (0);
      output = gimp_get_output_image (0);

      if (input && output)
	switch (gimp_image_type (input))
	  {
	  case RGB_IMAGE:
	  case GRAY_IMAGE:
	    gimp_init_progress ("relief");
	    relief (input, output);
	    gimp_update_image (output);
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("relief: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("relief: cannot operate on unknown image types");
	    break;
	  }

      gimp_free_image (input);
      gimp_free_image (output);

      gimp_quit ();
    }

  return 0;
}

void
relief (input, output)
     Image input, output;
{
  unsigned char *dest;
  unsigned char *prev_row, *cur_row;
  long width, height;
  long channels, rowstride;
  int row, col;
  int mv2, val;
  int x1, y1, x2, y2;
  int left, right;
  int top, bottom;
  int cur_progress;
  int max_progress;

  gimp_image_area (input, &x1, &y1, &x2, &y2);

  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  left = (x1 == 0);
  right = (x2 == width);
  top = (y1 == 0);
  bottom = (y2 == height);

  if (x1 == 0)
    x1++;
  if (y1 == 0)
    y1++;
  if (x2 == width)
    x2--;
  if (y2 == height)
    y2--;

  x1 *= channels;
  x2 *= channels;

  prev_row = gimp_image_data (input);
  prev_row += (y1 - 1) * rowstride;
  cur_row = prev_row + rowstride;

  dest = gimp_image_data (output);

  mv2 = 128;

  if (top)
    {
      for (col = 0; col < rowstride; col++)
	*dest++ = 0;
    }
  else
    {
      dest += y1 * rowstride;
    }

  cur_progress = y1;
  max_progress = y2;

  /* Now the rest of the image - read in the 3rd row of inputbuf,
  ** and convolve with the first row into the output buffer.
  */
  for (row = y1; row < y2; ++row)
    {
      if (left)
	{
	  for (col = 0; col < channels; col++)
	    *dest++ = 0;
	}
      else
	{
	  dest += x1;
	}

      for (col = x1; col < x2; col++)
	{
	  val = prev_row[col - channels] - cur_row[col + channels] + mv2;
	  if (val < 0) val = 0;
	  if (val > 255) val = 255;
	  *dest++ = val;
	}
     
      if (right)
	{
	  for (col = col; col < rowstride; col++)
	    *dest++ = 0;
	}
      else
	{
	  dest += rowstride - x2;
	}

      prev_row = cur_row;
      cur_row += rowstride;

      cur_progress++;
      if ((cur_progress % 5) == 0)
	gimp_do_progress (cur_progress, max_progress);
    }

  /* And write the last row, zeros again. */
  if (bottom)
    for (col = 0; col < rowstride; col++)
      *dest++ = 0;
}
