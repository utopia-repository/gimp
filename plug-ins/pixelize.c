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

/* pixelize filter written for the GIMP
 *  -Tracy Scott
 *
 * This filter acts as a low pass filter on the color components of
 * the provided region
 */

#include "gimp.h"

/* Declare a local function.
 */
static void scale_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void pixelize (Image, Image);

static char *prog_name;

/*  pixel width  (the decrease in resolution)  */
long pixelizewidth = 4;
int dialog_ID;

#define R 0
#define G 1
#define B 2


int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input, output;
  void *data;
  int scale_ID;

  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      data = gimp_get_params ();
      if (data)
	pixelizewidth = *((long*) data);
      
      input = output = 0;

      /* This is a regular filter. What that means is that it operates
       *  on the input image. Output is put into the ouput image. The
       *  filter should not worry, or even care where these images come
       *  from. The only guarantee is that they are the same size and
       *  depth.
       */
      input = gimp_get_input_image (0);

      /* If input image is available and the input is color, then do some
       *  work. (Pixelize). Then update the output image.
       */
      if (input)
	switch (gimp_image_type (input))
	  {
	  case RGB_IMAGE:
	  case GRAY_IMAGE:
	    dialog_ID = gimp_new_dialog ("Pixelize");
	    scale_ID = gimp_new_scale (dialog_ID, DEFAULT, 1, 
				       gimp_image_width (input), 
				       pixelizewidth, 0);
	    gimp_add_callback (dialog_ID, scale_ID, scale_callback, &pixelizewidth);
	    gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
	    gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
	    
	    if (gimp_show_dialog (dialog_ID))
	      {
		gimp_set_params (sizeof (pixelizewidth), &pixelizewidth);
		
		output = gimp_get_output_image (0);
		if (output)
		  {
		    pixelize (input, output);
		    gimp_update_image (output);
		  }
	      }
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("pixelize: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("pixelize: cannot operate on unknown image types");
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
scale_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  *((long*) client_data) = *((long*) call_data);
}

static void
ok_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  gimp_close_dialog (dialog_ID, 1);
}

static void
cancel_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  gimp_close_dialog (dialog_ID, 0);
}

static void
pixelize (input, output)
     Image input, output;
{
  long width, height;
  long channels, rowstride;
  unsigned char *src_row, *dest_row;
  unsigned char *src, *dest;
  short row, col;
  int x1, y1, x2, y2, i, j;
  unsigned int red_average, green_average, blue_average, count;
  
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
  src_row += rowstride * y1;
  dest_row += rowstride * y1;

  for (row = y1; row < y2; row+=pixelizewidth-(row%pixelizewidth))
    {
      src     = src_row;
      dest    = dest_row;
      /*  Calculate across the scanline  */
      for (col = x1; col < x2; col+=((pixelizewidth-(col%pixelizewidth))))
	{
	  red_average = 0;
	  green_average = 0;
	  blue_average = 0;
	  count = 0;
	  /* Compute the average values of the pixelize square area */
	  for (j = 0; (j < pixelizewidth - (row % pixelizewidth)) && ((j + row) < y2); j++) {
	      src = src_row + j * rowstride + col * channels;
	      for (i = 0; (i < pixelizewidth - (col % pixelizewidth)) && ((i + col) < x2); i++) {
		  red_average += *src++;
		  if (channels > 1) {
		      green_average += *src++;
		      blue_average += *src++;
		  }
		  count++;
	      }
	  }

	  if (count)
	    {
	      red_average = red_average/count;
	      green_average = green_average/count;
	      blue_average = blue_average/count;
	    }
	  
	  /* Store the new average values into the pixelized square */
	  for (j = 0; (j < pixelizewidth - (row % pixelizewidth)) && ((j + row) < y2); j++) {
              dest = dest_row + j * rowstride + col * channels;
	      for (i = 0; (i < pixelizewidth - (col % pixelizewidth)) && ((i + col) < x2); i++) {
		  *dest++ = red_average;
		  if (channels > 1) {
		      *dest++ = green_average;
		      *dest++ = blue_average;
		  }
	      }
	  }
	}	  
      src_row += rowstride * (pixelizewidth - (row%pixelizewidth));
      dest_row += rowstride * (pixelizewidth - (row%pixelizewidth));
    }
}






