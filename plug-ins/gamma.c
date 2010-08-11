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
#include <math.h>
#include "gimp.h"

/* Declare local functions.
 */
static void scale_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void gamma_correct (Image, Image);

static char *prog_name;

static long amount[3] = { 10, 10, 10 };
static int dialog_ID;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input, output;
  void *data;
  int group_ID;
  int scaler_ID;
  int scaleg_ID;
  int scaleb_ID;

  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      input = output = 0;

      /* This is a regular filter. What that means is that it operates
       *  on the input image. Output is put into the ouput image. The
       *  filter should not worry, or even care where these images come
       *  from. The only guarantee is that they are the same size and
       *  depth.
       */
      input = gimp_get_input_image (0);

      /* If input image is available, then do some work. (Edge Detect). 
       *  Then update the output image.
       */
      if (input)
	switch (gimp_image_type (input))
	  {
	  case RGB_IMAGE:
	  case GRAY_IMAGE:
	    data = gimp_get_params ();
	    if (data)
	      {
		amount[0] = ((long*) data)[0];
		amount[1] = ((long*) data)[1];
		amount[2] = ((long*) data)[2];
	      }
	    
	    /* Create a dialog.
	     */
	    dialog_ID = gimp_new_dialog ("Gamma Correction");
	    group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
	    if (gimp_image_type (input) == GRAY_IMAGE)
	      {
		scaler_ID = gimp_new_scale (dialog_ID, group_ID, 1, 100, amount[0], 1);
		gimp_add_callback (dialog_ID, scaler_ID, scale_callback, &amount[0]);
	      }
	    else
	      {
		scaler_ID = gimp_new_scale (dialog_ID, group_ID, 1, 100, amount[0], 1);
		scaleg_ID = gimp_new_scale (dialog_ID, group_ID, 1, 100, amount[1], 1);
		scaleb_ID = gimp_new_scale (dialog_ID, group_ID, 1, 100, amount[2], 1);
		gimp_add_callback (dialog_ID, scaler_ID, scale_callback, &amount[0]);
		gimp_add_callback (dialog_ID, scaleg_ID, scale_callback, &amount[1]);
		gimp_add_callback (dialog_ID, scaleb_ID, scale_callback, &amount[2]);
	      }
	    gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
	    gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
	    
	    if (gimp_show_dialog (dialog_ID))
	      {
		gimp_set_params (sizeof (long) * 3, amount);
		
		output = gimp_get_output_image (0);
		if (output)
		  {
		    gamma_correct (input, output);
		    gimp_update_image (output);
		  }
	      }
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("gamma: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("gamma: cannot operate on unknown image types");
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
gamma_correct (input, output)
     Image input, output;
{
  unsigned char gamma[3][256];
  unsigned char *src_row, *dest_row;
  unsigned char *src, *dest;
  long width, height;
  long channels, rowstride;
  int row, col;
  int x1, y1, x2, y2;
  int i, j, v;
  double one_over_gamma, ind, q;

  for (i = 0; i < gimp_image_channels (input); i++)
    {
      one_over_gamma = 10.0 / amount[i];
      q = (double) 256;
      
      for (j = 0; j < 256; j++)
	{
	  ind = (double) j / q;
	  v = (q * pow (ind, one_over_gamma)) + 0.5;
	  if (v > 256)
	    v = 256;
	  gamma[i][j] = v;
	}
    }
  
  gimp_image_area (input, &x1, &y1, &x2, &y2);
  
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;
  
  src_row = gimp_image_data (input);
  dest_row = gimp_image_data (output);
  
  src_row += rowstride * y1 + (x1 * channels);
  dest_row += rowstride * y1 + (x1 * channels);
  
  for (row = y1; row < y2; ++row)
    {
      src = src_row;
      dest = dest_row;
      
      for (col = x1; col < x2; col++)
	{
	  for (i = 0; i < channels; i++)
	    *dest++ = gamma[i][*src++];
	}
      
      src_row += rowstride;
      dest_row += rowstride;
    }
}

