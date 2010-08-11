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
 * Duplicates and offsets the input image.
 */

#include <stdio.h>
#include <stdlib.h>
#include "gimp.h"

typedef struct {
  long x_offset;
  long y_offset;
  long wraparound;
} OffsetValues;

/* Declare local functions.
 */
static void offset_callback (int, void *, void *);
static void toggle_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void offset (Image);

static char *prog_name;
static int dialog_ID;
static OffsetValues vals;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input;
  char buf[16];
  int group_ID;
  int temp_ID;
  int offsetx_ID;
  int offsety_ID;
  int wrap_ID;
  void *data;
  
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

      data = gimp_get_params ();
      if (data)
	vals = *((OffsetValues*) data);
      else
	{
	  vals.x_offset = 0;
	  vals.y_offset = 0;
	  vals.wraparound = 1;
	}
      
      dialog_ID = gimp_new_dialog ("Offset");
      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "X:");
      sprintf (buf, "%d", (int) vals.x_offset);
      offsetx_ID = gimp_new_text (dialog_ID, temp_ID, buf);
     
      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "Y:");
      sprintf (buf, "%d", (int) vals.y_offset);
      offsety_ID = gimp_new_text (dialog_ID, temp_ID, buf);

      switch (gimp_image_type (input))
	{
	case RGB_IMAGE:
	case GRAY_IMAGE:
	  wrap_ID = gimp_new_check_button (dialog_ID, group_ID, "Wrap Around");
	  gimp_change_item (dialog_ID, wrap_ID, sizeof (vals.wraparound), &vals.wraparound);
	  gimp_add_callback (dialog_ID, wrap_ID, toggle_callback, &vals.wraparound);
	  break;
	default:
	  vals.wraparound = 1;
	  break;
	}
     
      gimp_add_callback (dialog_ID, offsetx_ID, offset_callback, &vals.x_offset);
      gimp_add_callback (dialog_ID, offsety_ID, offset_callback, &vals.y_offset);
      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

      if (gimp_show_dialog (dialog_ID))
	{
	  gimp_set_params (sizeof (OffsetValues), &vals);
	  
	  if (input)
	    {
	      gimp_init_progress ("offset");
	      offset (input);
	    }
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
offset_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  *((long*) client_data) = atoi (call_data);
}

static void
toggle_callback (item_ID, client_data, call_data)
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
offset (input)
     Image input;
{
  Image output;
  unsigned char *src, *dest;
  unsigned char r, g, b;
  unsigned char c[3];
  long width, height;
  long channels;
  long rowstride;
  int srcx, srcy;
  int xoff, yoff;
  int i, j, k;

  output = gimp_new_image (0, 
			   gimp_image_width (input), 
			   gimp_image_height (input),
			   gimp_image_type (input));

  src = gimp_image_data (input);
  dest = gimp_image_data (output);

  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * gimp_image_channels (input);

  xoff = vals.x_offset;
  yoff = vals.y_offset;

  if (vals.wraparound)
    {
      for (i = 0; i < height; i++)
	{
	  for (j = 0; j < width; j++)
	    {
	      srcx = j - xoff;
	      srcy = i - yoff;
	      
	      while (srcx < 0) 
		srcx += width;
	      while (srcx >= width) 
		srcx -= width;
	      while (srcy < 0) 
		srcy += height;
	      while (srcy >= height) 
		srcy -= height;
	      
	      for (k = 0; k < channels; k++)
		*dest++ = *(src + (srcy * rowstride) + (srcx * channels) + k);
	    }

	  if ((i % 5) == 0)
	    gimp_do_progress (i, height);
	}
    }
  else
    {
      gimp_background_color (&r, &g, &b);
      switch (gimp_image_channels (input))
	{
	case 1:
	  c[0] = ((int) r + (int) g + (int) b) / 3;
	  c[1] = c[0];
	  c[2] = c[0];
	  break;
	case 3:
	  c[0] = r;
	  c[1] = g;
	  c[2] = b;
	  break;
	}
      
      for (i = 0; i < height; i++)
	{
	  for (j = 0; j < width; j++)
	    {
	      srcx = j - xoff;
	      srcy = i - yoff;
	      
	      if ((srcx < 0) || (srcx >= width) || (srcy < 0) || (srcy >= height))
		{
		  for (k = 0; k < channels; k++)
		    *dest++ = c[k];
		}
	      else
		{
		  for (k = 0; k < channels; k++)
		    *dest++ = *(src + (srcy * rowstride) + (srcx * channels) + k);
		}
	    }

	  if ((i % 5) == 0)
	    gimp_do_progress (i, height);
	}
    }

  if (gimp_image_type (input) == INDEXED_IMAGE)
    gimp_set_image_colors (output, gimp_image_cmap (input), gimp_image_colors (input));

  gimp_display_image (output);
  gimp_update_image (output);
  gimp_free_image (output);
}
