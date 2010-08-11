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
 * Duplicates the input image.
 */

#include "gimp.h"

/* Declare local functions.
 */
static void duplicate (Image);

static char *prog_name;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input;
  
  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      input = gimp_get_input_image (0);
      if (input)
	{
	  gimp_init_progress ("duplicate");
	  duplicate (input);
	}

      /* Free the image.
       */
      if (input)
	gimp_free_image (input);

      /* Quit
       */
      gimp_quit ();
    }

  return 0;
}

static void
duplicate (input)
     Image input;
{
  Image output;
  unsigned char *s, *d;
  long width, height;
  int i, j;

  output = gimp_new_image (0, 
			   gimp_image_width (input), 
			   gimp_image_height (input),
			   gimp_image_type (input));

  s = gimp_image_data (input);
  d = gimp_image_data (output);

  width = gimp_image_width (input);
  height = gimp_image_height (input);

  width *= gimp_image_channels (input);

  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	*d++ = *s++;

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  if (gimp_image_type (input) == INDEXED_IMAGE)
    gimp_set_image_colors (output, gimp_image_cmap (input), gimp_image_colors (input));

  gimp_display_image (output);
  gimp_update_image (output);
  gimp_free_image (output);
}
