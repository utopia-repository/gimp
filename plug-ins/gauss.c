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
#include <stdio.h>
#include <stdlib.h>
#include "gimp.h"

typedef struct {
  double radius;
  long horizontal;
  long vertical;
} BlurValues;

/* Declare a local function.
 */
static void text_callback (int, void *, void *);
static void toggle_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void gaussian_blur_both (Image, Image, double);
static void gaussian_blur_horizontal (Image, Image, double);
static void gaussian_blur_vertical (Image, Image, double);
static double *make_curve (double, int *);

static char *prog_name;
static int dialog_ID;
static BlurValues vals;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image input, output;
  int group_ID;
  int text_ID;
  int horz_ID;
  int vert_ID;
  int frame_ID;
  int temp_ID;
  char buf[16];
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
	      data = gimp_get_params ();
	      if (data)
		vals = *((BlurValues*) data);
	      else
		{
		  vals.radius = 1.0;
		  vals.horizontal = 1;
		  vals.vertical = 1;
		}

	      dialog_ID = gimp_new_dialog ("Gaussian Blur");
	      gimp_new_label (dialog_ID, DEFAULT, "Options");
	      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
	      
	      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
	      gimp_new_label (dialog_ID, temp_ID, "Pixel Radius:");
	      sprintf (buf, "%0.2f", vals.radius);
	      text_ID = gimp_new_text (dialog_ID, temp_ID, buf);

	      frame_ID = gimp_new_frame (dialog_ID, group_ID, "Direction");
	      temp_ID = gimp_new_row_group (dialog_ID, frame_ID, NORMAL, "");
	      horz_ID = gimp_new_check_button (dialog_ID, temp_ID, "Horizontal");
	      vert_ID = gimp_new_check_button (dialog_ID, temp_ID, "Vertical");
	      gimp_change_item (dialog_ID, horz_ID, sizeof (vals.horizontal), &vals.horizontal);
	      gimp_change_item (dialog_ID, vert_ID, sizeof (vals.vertical), &vals.vertical);
	      
	      gimp_add_callback (dialog_ID, text_ID, text_callback, &vals.radius);
	      gimp_add_callback (dialog_ID, horz_ID, toggle_callback, &vals.horizontal);
	      gimp_add_callback (dialog_ID, vert_ID, toggle_callback, &vals.vertical);
	      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
	      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

	      if (gimp_show_dialog (dialog_ID))
		{
		  gimp_set_params (sizeof (BlurValues), &vals);
		  gimp_init_progress ("gaussian blur");
		  
		  if (vals.horizontal && vals.vertical)
		    gaussian_blur_both (input, output, vals.radius);
		  else if (vals.horizontal)
		    gaussian_blur_horizontal (input, output, vals.radius);
		  else if (vals.vertical)
		    gaussian_blur_vertical (input, output, vals.radius);
		    
		  gimp_update_image (output);
		}
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
text_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  *((double*) client_data) = atof (call_data);
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
gaussian_blur_both (input, output, radius)
     Image input, output;
     double radius;
{
  long width, height;
  long channels, rowstride;
  unsigned char *dest, *dp;
  unsigned char *src, *sp;
  unsigned char *buf;
  int x1, y1, x2, y2;
  int i, row, col, chan;
  int start, end;
  int progress, max_progress;
  double *curve;
  double *sum;
  double val;
  int length;
  
  curve = make_curve (radius, &length);
  sum = malloc (sizeof (double) * (2 * length + 1));
  if (!sum)
    {
      fprintf (stderr, "%s: unable to allocate memory\n", prog_name);
      gimp_quit ();
    }

  sum += length;
  sum[0] = 0.0;
  
  for (i = 1; i <= length; i++)
    {
      sum[i] = curve[i] + sum[i-1];
      sum[-i] = sum[i];
    }

  gimp_image_area (input, &x1, &y1, &x2, &y2);
  
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  buf = malloc (rowstride);
  if (!buf)
    {
      fprintf (stderr, "%s: unable to allocate memory\n", prog_name);
      gimp_quit ();
    }

  progress = 0;
  max_progress = (y2 - y1) + (x2 - x1);
  
  src = gimp_image_data (input);
  src += x1 + y1 * rowstride;
  dest = gimp_image_data (output);
  dest += x1 + y1 * rowstride;

  for (col = x1; col < x2; col++)
    {
      sp = src;
      dp = dest;

      src += channels;
      dest += channels;
      
      for (row = y1; row < y2; row++)
	{
	  for (chan = 0; chan < channels; chan++)
	    {
	      start = (row < length) ? (-row) : (-length);
	      end = (height <= (row + length)) ? (height - row - 1) : (length);
	      
	      val = 0.0;
	      for (i = start; i <= end; i++)
		val += curve[i] * (*(sp + (i * rowstride) + chan));
	      
	      dp[chan] = val / (1.0 + sum[start] + sum[end]);
	    }
	  dp += rowstride;
	  sp += rowstride;
	}
      
      progress++;
      if ((progress % 5) == 0)
        gimp_do_progress (progress, max_progress);
    }

  dest = gimp_image_data (output);
  dest += y1 * rowstride;

  for (row = y1; row < y2; row++)
    {
      dp = dest;
      dest += rowstride;
      
      memcpy (buf, dp - x1, rowstride);
      
      for (col = x1; col < x2; col++)
	{
	  for (chan = 0; chan < channels; chan++)
	    {
	      start = (col < length) ? (-col) : (-length);
	      end = (width <= (col + length)) ? (width - col - 1) : (length);
	      
	      val = 0.0;
	      for (i = start; i <= end; i++)
		val += curve[i] * (buf[(col + i) * channels + chan]);
	      dp[chan] = val / (1.0 + sum[start] + sum[end]);
	    }
	  dp += channels;
	}
      
      progress++;
      if ((progress % 5) == 0)
        gimp_do_progress (progress, max_progress);
    }
}

static void
gaussian_blur_horizontal (input, output, radius)
     Image input, output;
     double radius;
{
  long width, height;
  long channels, rowstride;
  unsigned char *dest, *dp;
  unsigned char *src, *sp;
  int x1, y1, x2, y2;
  int i, row, col, chan;
  int start, end;
  int progress, max_progress;
  double *curve;
  double *sum;
  double val;
  int length;

  curve = make_curve (radius, &length);
  sum = malloc (sizeof (double) * (2 * length + 1));
  if (!sum)
    {
      fprintf (stderr, "%s: unable to allocate memory\n", prog_name);
      gimp_quit ();
    }

  sum += length;
  sum[0] = 0.0;
  
  for (i = 1; i <= length; i++)
    {
      sum[i] = curve[i] + sum[i-1];
      sum[-i] = sum[i];
    }

  gimp_image_area (input, &x1, &y1, &x2, &y2);
  
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  progress = 0;
  max_progress = (y2 - y1);
  
  src = gimp_image_data (input);
  src += y1 * rowstride;
  dest = gimp_image_data (output);
  dest += y1 * rowstride;

  for (row = y1; row < y2; row++)
    {
      sp = src;
      dp = dest;

      src += rowstride;
      dest += rowstride;
      
      for (col = x1; col < x2; col++)
	{
	  for (chan = 0; chan < channels; chan++)
	    {
	      start = (col < length) ? (-col) : (-length);
	      end = (width <= (col + length)) ? (width - col - 1) : (length);
	      
	      val = 0.0;
	      for (i = start; i <= end; i++)
		val += curve[i] * (sp[(col + i) * channels + chan]);
	      
	      dp[chan] = val / (1.0 + sum[start] + sum[end]);
	    }
	  dp += channels;
	}
      
      progress++;
      if ((progress % 5) == 0)
        gimp_do_progress (progress, max_progress);
    }
}

static void
gaussian_blur_vertical (input, output, radius)
     Image input, output;
     double radius;
{
  long width, height;
  long channels, rowstride;
  unsigned char *dest, *dp;
  unsigned char *src, *sp;
  int x1, y1, x2, y2;
  int i, row, col, chan;
  int start, end;
  int progress, max_progress;
  double *curve;
  double *sum;
  double val;
  int length;
  
  curve = make_curve (radius, &length);
  sum = malloc (sizeof (double) * (2 * length + 1));
  if (!sum)
    {
      fprintf (stderr, "%s: unable to allocate memory\n", prog_name);
      gimp_quit ();
    }

  sum += length;
  sum[0] = 0.0;
  
  for (i = 1; i <= length; i++)
    {
      sum[i] = curve[i] + sum[i-1];
      sum[-i] = sum[i];
    }

  gimp_image_area (input, &x1, &y1, &x2, &y2);
  
  width = gimp_image_width (input);
  height = gimp_image_height (input);
  channels = gimp_image_channels (input);
  rowstride = width * channels;

  progress = 0;
  max_progress = (x2 - x1);
  
  src = gimp_image_data (input);
  src += x1 + y1 * rowstride;
  dest = gimp_image_data (output);
  dest += x1 + y1 * rowstride;

  for (col = x1; col < x2; col++)
    {
      sp = src;
      dp = dest;

      src += channels;
      dest += channels;
      
      for (row = y1; row < y2; row++)
	{
	  for (chan = 0; chan < channels; chan++)
	    {
	      start = (row < length) ? (-row) : (-length);
	      end = (height <= (row + length)) ? (height - row - 1) : (length);
	      
	      val = 0.0;
	      for (i = start; i <= end; i++)
		val += curve[i] * (*(sp + (i * rowstride) + chan));
	      
	      dp[chan] = val / (1.0 + sum[start] + sum[end]);
	    }
	  dp += rowstride;
	  sp += rowstride;
	}
      
      progress++;
      if ((progress % 5) == 0)
        gimp_do_progress (progress, max_progress);
    }
}

/*
 * The equations: g(r) = exp (- r^2 / (2 * sigma^2))
 *                   r = sqrt (x^2 + y ^2)
 */

static double*
make_curve (sigma, length)
     double sigma;
     int *length;
{
  double *curve;
  double sigma2;
  double temp;
  int i, n;

  sigma2 = 2 * sigma * sigma;
  temp = sqrt (-sigma2 * log (0.001));

  n = ceil (temp) * 2;
  if ((n % 2) == 0)
    n += 1;

  curve = malloc (sizeof (double) * n);
  if (!curve)
    {
      fprintf (stderr, "%s: could not allocate memory\n", prog_name);
      gimp_quit ();
    }

  *length = n / 2;
  curve += *length;
  curve[0] = 1.0;

  for (i = 1; i <= *length; i++)
    {
      temp = exp (- (i * i) / sigma2);
      curve[-i] = temp;
      curve[i] = temp;
    }

  return curve;
}
