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
 * This filter tiles an image to arbitrary width and height
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gimp.h"

/* Declare a local function.
 */
static void tile (Image, Image);

static void text_callback (int, void *, void *);
static void toggle_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);

static char *prog_name;

static long orig_width;
static long orig_height;
static long new_width;
static long new_height;
static long constrain;
static int dialog_ID;
static int text_width_ID;
static int text_height_ID;
static int constrain_ID;


int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input, output;
  char buf[64];
  int group_ID;
  int temp_ID;
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
      input = 0;
      output = 0;

      /* This is a regular filter. What that means is that it operates
       *  on the input image. Output is put into the ouput image. The
       *  filter should not worry, or even care where these images come
       *  from. The only guarantee is that they are the same size and
       *  depth.
       */
      input = gimp_get_input_image (0);

      orig_width = new_width = gimp_image_width (input);
      orig_height = new_height = gimp_image_height (input);
      data = gimp_get_params ();
      constrain = (data) ? (*((long*) data)) : 0;

      dialog_ID = gimp_new_dialog ("Tile");
      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "Width:");
      sprintf (buf, "%d", (int) new_width);
      text_width_ID = gimp_new_text (dialog_ID, temp_ID, buf);

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "Height:");
      sprintf (buf, "%d", (int) new_height);
      text_height_ID = gimp_new_text (dialog_ID, temp_ID, buf);

      constrain_ID = gimp_new_check_button (dialog_ID, group_ID, "Constrain Ratio");
      gimp_change_item (dialog_ID, constrain_ID, sizeof (constrain), &constrain);

      gimp_add_callback (dialog_ID, text_width_ID, text_callback, &new_width);
      gimp_add_callback (dialog_ID, text_height_ID, text_callback, &new_height);
      gimp_add_callback (dialog_ID, constrain_ID, toggle_callback, &constrain);
      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

      if (gimp_show_dialog (dialog_ID))
	{
	  gimp_set_params (sizeof (constrain), &constrain);
	  
	  output = gimp_new_image (0,
				   new_width,
				   new_height,
				   gimp_image_type (input));

	  if (gimp_image_type (input) == INDEXED_IMAGE)
	    gimp_set_image_colors (output, gimp_image_cmap (input), gimp_image_colors (input));
	  
	  if (input && output)
	    {
	      gimp_display_image (output);
	      tile (input, output);
	      gimp_update_image (output);
	    }
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
text_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  static int ignore = 0;
  static char buf[32];

  *((long*) client_data) = atoi (call_data);

  if (constrain && !ignore)
    {
      ignore = 1;

      if ((orig_width != 0) && (orig_height != 0))
	{
	  if (item_ID == text_width_ID)
	    {
	      item_ID = text_height_ID;
	      sprintf (buf, "%d", (int) ((new_width * orig_height) / orig_width));
	    }
	  else if (item_ID == text_height_ID)
	    {
	      item_ID = text_width_ID;
	      sprintf (buf, "%d", (int) ((new_height * orig_width) / orig_height));
	    }
	  
	  gimp_change_item (dialog_ID, item_ID, strlen (buf) + 1, buf);
	}
    }

  ignore = 0;
}

static void
toggle_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  *((long*) client_data) = *((long*) call_data);

  if (item_ID == constrain_ID)
    {
      orig_width = new_width;
      orig_height = new_height;
    }
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
tile (input, output)
     Image input, output;
{
  unsigned char * src;
  unsigned char * dest;
  long srcwidth, destwidth;
  int orig_width, orig_height;
  int length;
  int bytes;
  int i, j;

  orig_width = gimp_image_width (input);
  orig_height = gimp_image_height (input);

  /*  Some calculations...  */
  bytes = gimp_image_channels (input);
  srcwidth = bytes * orig_width;
  destwidth = bytes * new_width;

  /*  the data pointers...  */
  dest = gimp_image_data (output);

  for (i = 0; i < new_height; i ++)
    {
      src = gimp_image_data (input);
      src += (i % orig_height) * srcwidth;
      for (j = 0; j < new_width; j += orig_width)
	{
	  length = orig_width;
	  if (length + j > new_width)
	    length = new_width - j;
	  
	  length *= bytes;

	  memcpy (dest, src, length);
	  
	  dest += length;
	}
    }

}
