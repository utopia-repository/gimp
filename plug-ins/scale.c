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
 * This filter scales an image into a new image.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gimp.h"

/* Declare a local function.
 */
static void scale (Image, Image);

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

      dialog_ID = gimp_new_dialog ("Scale");
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

	  if (input && output)
	    {
	      gimp_display_image (output);
	      scale (input, output);
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
scale (input, output)
     Image input, output;
{
  unsigned char * src, * s;
  unsigned char * dest, * d;
  float * row, * r;
  long srcwidth, destwidth;
  int src_row, src_col;
  int bytes, b;
  int orig_width, orig_height;
  float x_rat, y_rat;
  float x_cum, y_cum;
  float x_last, y_last;
  float * x_frac, y_frac, tot_frac;
  int i, j;
  int frac;
  int advance_dest;

  orig_width = gimp_image_width (input);
  orig_height = gimp_image_height (input);

  /*  Some calculations...  */
  bytes = gimp_image_channels (input);
  srcwidth = bytes * orig_width;
  destwidth = bytes * new_width;

  /*  the data pointers...  */
  src  = gimp_image_data (input);
  dest = gimp_image_data (output);

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (float) orig_width / (float) new_width;
  y_rat = (float) orig_height / (float) new_height;

  /*  allocate an array to help with the calculations  */
  row    = (float *) malloc (sizeof (float) * new_width * bytes);
  x_frac = (float *) malloc (sizeof (float) * (new_width + orig_width));

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = 0;
  x_cum = (float) src_col;
  x_last = x_cum;

  for (i = 0; i < new_width + orig_width; i++)
    {
      if (x_cum + x_rat <= src_col + 1)
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (float) * new_width * bytes);

  /*  counters...  */
  src_row = 0;
  y_cum = (float) src_row;
  y_last = y_cum;

  /*  Scale the selected region  */
  i = new_height;
  while (i)
    {
      src_col = 0;
      x_cum = (float) src_col;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= src_row + 1)
	{
	  y_cum += y_rat;
	  y_frac = y_cum - y_last;
	  advance_dest = 1;
	}
      else
	{
	  src_row ++;
	  y_frac = src_row - y_last;
	  advance_dest = 0;
	}
      y_last += y_frac;

      s = src;
      r = row;

      frac = 0;
      j = new_width;

      while (j)
	{
	  tot_frac = x_frac[frac++] * y_frac;

	  for (b = 0; b < bytes; b++)
	    r[b] += s[b] * tot_frac;

	  /*  increment the destination  */
	  if (x_cum + x_rat <= src_col + 1)
	    {
	      r += bytes;
	      x_cum += x_rat;
	      j--;
	    }

	  /* increment the source */
	  else
	    {
	      s += bytes;
	      src_col++;
	    }

	}

      if (advance_dest)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;

	  j = new_width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (unsigned char) (*r++ * tot_frac);
	    }

	  dest += destwidth;

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (float) * new_width * bytes);

	  i--;
	}
      else
	src += srcwidth;
      
    }

  /*  free up temporary arrays  */
  free (row);
  free (x_frac);
}
