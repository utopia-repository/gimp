/* edge filter for the GIMP
 *  -Peter Mattis
 *
 * This filter performs edge detection on the input image.
 *  The code for this filter is based on "pgmedge", a program
 *  that is part of the netpbm package.
 */

/* pgmedge.c - edge-detect a portable graymap
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <math.h>
#include "gimp.h"

/* Declare local functions.
 */
static void scale_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void edge (Image, Image);
static long long_sqrt (long);

static char *prog_name;

/* edge detection scale amount  */
static long amount;
static int dialog_ID;

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
	      amount = *((long*) data);
	    else
	      amount = 20;
	    
	    /* Create a dialog.
	     */
	    dialog_ID = gimp_new_dialog ("Edge Detection");
	    scale_ID = gimp_new_scale (dialog_ID, DEFAULT, 10, 100, amount, 1);
	    gimp_new_label (dialog_ID, DEFAULT, "Amount");
	    gimp_add_callback (dialog_ID, scale_ID, scale_callback, &amount);
	    gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
	    gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
	    
	    if (gimp_show_dialog (dialog_ID))
	      {
		gimp_set_params (sizeof (amount), &amount);
		
		output = gimp_get_output_image (0);
		if (output)
		  {
		    edge (input, output);
		    gimp_update_image (output);
		  }
	      }
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("edge: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("edge: cannot operate on unknown image types");
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
edge (input, output)
     Image input, output;
{
  unsigned char *dest;
  unsigned char *prev_row, *cur_row, *next_row;
  long width, height;
  long channels, rowstride;
  int row, col;
  int x1, y1, x2, y2;
  int left, right;
  int top, bottom;
  long sum1, sum2;
  long sum, scale;
  int maxval;
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
  next_row = cur_row + rowstride;

  dest = gimp_image_data (output);

  maxval = 255;
  scale = (10 << 16) / amount;

  if (top)
    {
      for (col = 0; col < rowstride; col++)
        *dest++ = 0;
    }
  else
    {
      dest += y1 * rowstride;
    }

  cur_progress = 0;
  max_progress = y2 - y1;

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
	  sum1 = ((long) prev_row[col + channels] - (long) prev_row[col - channels] +
		  2 * ((long) cur_row[col + channels] - (long) cur_row[col - channels]) +
		  (long) next_row[col + channels] - (long) cur_row[col - channels]);
	  sum2 = (((long) next_row[col - channels] + 
		   2 * (long) next_row[col] +
		   (long) next_row[col + channels]) -
		  ((long) prev_row[col - channels] +
		   2 * (long) prev_row[col] +
		   (long) prev_row[col + channels]));
	  sum = long_sqrt (sum1 * sum1 + sum2 * sum2);
	  sum = (sum * scale) >> 16;    /* arbitrary scaling factor */
	  if (sum > maxval) sum = maxval;
	  *dest++ = sum;
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
      cur_row = next_row;
      next_row += rowstride;

      gimp_do_progress (++cur_progress, max_progress);
    }

  /* And write the last row, zeros again. */
  if (bottom)
    for (col = 0; col < rowstride; col++)
      *dest++ = 0;
}

static long
long_sqrt (n)
     long n;
{
#define lsqrt_max4pow (1UL << 30)
  /* lsqrt_max4pow is the (machine-specific) largest power of 4 that can
   * be represented in an unsigned long.
   *
   * Compute the integer square root of the integer argument n
   * Method is to divide n by x computing the quotient x and remainder r
   * Notice that the divisor x is changing as the quotient x changes
   * 
   * Instead of shifting the dividend/remainder left, we shift the
   * quotient/divisor right. The binary point starts at the extreme
   * left, and shifts two bits at a time to the extreme right.
   * 
   * The residue contains n-x^2. (Within these comments, the ^ operator
   * signifies exponentiation rather than exclusive or. Also, the /
   * operator returns fractions, rather than truncating, so 1/4 means
   * one fourth, not zero.)
   * 
   * Since (x + 1/2)^2 == x^2 + x + 1/4,
   * n - (x + 1/2)^2 == (n - x^2) - (x + 1/4)
   * Thus, we can increase x by 1/2 if we decrease (n-x^2) by (x+1/4)
   */

  unsigned long residue;        /* n - x^2  */
  unsigned long root;           /* x + 1/4  */
  unsigned long half;           /* 1/2      */

  residue = n;                  /* n - (x = 0)^2, with suitable alignment */

  /*
   * if the correct answer fits in two bits, pull it out of a magic hat
   */
  if (residue <= 12)
    return (0x03FFEA94 >> (residue *= 2)) & 3;

  root = lsqrt_max4pow;         /* x + 1/4, shifted all the way left */
  /* half = root + root; 1/2, shifted likewise */

  /* 
   * Unwind iterations corresponding to leading zero bits 
   */
  while (root > residue)
    root >>= 2;

  /*
   * Unwind the iteration corresponding to the first one bit
   * Operations have been rearranged and combined for efficiency
   * Initialization of half is folded into this iteration
   */
  residue -= root;              /* Decrease (n-x^2) by (0+1/4)             */
  half = root >> 2;             /* 1/4, with binary point shifted right 2  */
  root += half;                 /* x=1. (root is now (x=1)+1/4.)           */
  half += half;                 /* 1/2, properly aligned                   */

  /*
   * Normal loop (there is at least one iteration remaining)
   */
  do
    {
      if (root <= residue)      /* Whenever we can,                          */
        {
          residue -= root;      /* decrease (n-x^2) by (x+1/4)               */
          root += half;         /* increase x by 1/2                         */
        }
      half >>= 2;               /* Shift binary point 2 places right          */
      root -= half;             /* x{ +1/2 } +1/4 - 1/8 == x { +1/2 } 1/8     */
      root >>= 1;               /* 2x{ +1 } +1/4, shifted right 2 places      */
    }
  while (half);                 /* When 1/2 == 0, bin. point is at far right  */

  /* 
   * round up if (x+1/2)^2 < n
   */
  if (root < residue)
    ++root;

  /* 
   * Guaranteed to be correctly rounded (or truncated)
   */
  return root;
}
