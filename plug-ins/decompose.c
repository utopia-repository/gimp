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

/* decompose a color image into its constituent r, g and b channels
 */

#include <stdio.h>
#include "gimp.h"

/* Declare local functions.
 */
static void radio_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void decompose_rgb (Image);
static void decompose_hsv (Image);
static void rgb_to_hsv (int *, int *, int *);

static char *prog_name;
static int dialog_ID;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image src;
  int group_ID;
  int rgb_ID;
  int hsv_ID;
  long rgb, hsv;

  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      src = gimp_get_input_image (0);
      if (src)
	switch (gimp_image_type (src))
	  {
	  case RGB_IMAGE:
	    rgb = 1;
	    hsv = 0;
	    
	    dialog_ID = gimp_new_dialog ("decompose");
	    group_ID = gimp_new_row_group (dialog_ID, DEFAULT, RADIO, "");
	    rgb_ID = gimp_new_radio_button (dialog_ID, group_ID, "RGB decomposition");
	    gimp_change_item (dialog_ID, rgb_ID, sizeof (rgb), &rgb);
	    hsv_ID = gimp_new_radio_button (dialog_ID, group_ID, "HSV decomposition");
	    gimp_change_item (dialog_ID, hsv_ID, sizeof (hsv), &hsv);
	    gimp_add_callback (dialog_ID, rgb_ID, radio_callback, &rgb);
	    gimp_add_callback (dialog_ID, hsv_ID, radio_callback, &hsv);
	    gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
	    gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
	    
	    if (gimp_show_dialog (dialog_ID))
	      {
		if (rgb)
		  decompose_rgb (src);
		else if (hsv)
		  decompose_hsv (src);
	      }
	    break;
	  case GRAY_IMAGE:
	    gimp_message ("decompose: cannot operate on grayscale images");
	    break;
	  case INDEXED_IMAGE:
	    gimp_message ("decompose: cannot operate on indexed color images");
	    break;
	  default:
	    gimp_message ("decompose: cannot operate on unknown image types");
	    break;
	  }

      /* Free the image.
       */
      if (src)
	gimp_free_image (src);

      /* Quit
       */
      gimp_quit ();
    }

  return 0;
}

static void
radio_callback (item_ID, client_data, call_data)
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
decompose_rgb (src)
     Image src;
{
  Image rdest, gdest, bdest;
  char name_buf[256];
  unsigned char *srcp;
  unsigned char *rdestp;
  unsigned char *gdestp;
  unsigned char *bdestp;
  long width, height;
  int i, j;

  width = gimp_image_width (src);
  height = gimp_image_height (src);
  
  sprintf (name_buf, "%s-red", gimp_image_name (src));
  rdest = gimp_new_image (name_buf, width, height, GRAY_IMAGE);

  sprintf (name_buf, "%s-green", gimp_image_name (src));
  gdest = gimp_new_image (name_buf, width, height, GRAY_IMAGE);

  sprintf (name_buf, "%s-blue", gimp_image_name (src));
  bdest = gimp_new_image (name_buf, width, height, GRAY_IMAGE);
  
  srcp = gimp_image_data (src);
  rdestp = gimp_image_data (rdest);
  gdestp = gimp_image_data (gdest);
  bdestp = gimp_image_data (bdest);

  gimp_init_progress ("decompose RGB");
  
  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	{
	  *rdestp++ = *srcp++;
	  *gdestp++ = *srcp++;
	  *bdestp++ = *srcp++;
	}

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  gimp_display_image (rdest);
  gimp_display_image (gdest);
  gimp_display_image (bdest);

  gimp_update_image (rdest);
  gimp_update_image (gdest);
  gimp_update_image (bdest);

  gimp_free_image (rdest);
  gimp_free_image (gdest);
  gimp_free_image (bdest);
}

static void
decompose_hsv (src)
     Image src;
{
  Image hdest, sdest, vdest;
  char name_buf[256];
  unsigned char *srcp;
  unsigned char *hdestp;
  unsigned char *sdestp;
  unsigned char *vdestp;
  long width, height;
  int h, s, v;
  int i, j;

  width = gimp_image_width (src);
  height = gimp_image_height (src);
  
  sprintf (name_buf, "%s-hue", gimp_image_name (src));
  hdest = gimp_new_image (name_buf, width, height, 1);

  sprintf (name_buf, "%s-saturation", gimp_image_name (src));
  sdest = gimp_new_image (name_buf, width, height, 1);

  sprintf (name_buf, "%s-value", gimp_image_name (src));
  vdest = gimp_new_image (name_buf, width, height, 1);
  
  srcp = gimp_image_data (src);
  hdestp = gimp_image_data (hdest);
  sdestp = gimp_image_data (sdest);
  vdestp = gimp_image_data (vdest);

  gimp_init_progress ("decompose HSV");

  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	{
	  h = *srcp++;
	  s = *srcp++;
	  v = *srcp++;
	  
	  rgb_to_hsv (&h, &s, &v);
	  
	  *hdestp++ = h;
	  *sdestp++ = s;
	  *vdestp++ = v;
	}

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  gimp_display_image (hdest);
  gimp_display_image (sdest);
  gimp_display_image (vdest);

  gimp_update_image (hdest);
  gimp_update_image (sdest);
  gimp_update_image (vdest);

  gimp_free_image (hdest);
  gimp_free_image (sdest);
  gimp_free_image (vdest);
}

static void
rgb_to_hsv (r, g, b)
     int *r, *g, *b;
{
  int red, green, blue;
  float h, s, v;
  int min, max;
  int delta;
  
  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;
      
      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;
      
      if (red < blue)
	min = red;
      else
	min = blue;
    }
  
  v = max;
  
  if (max != 0)
    s = ((max - min) * 255) / (float) max;
  else
    s = 0;
  
  if (s == 0)
    h = 0;
  else
    {
      delta = max - min;
      if (red == max)
	h = (green - blue) / (float) delta;
      else if (green == max)
	h = 2 + (blue - red) / (float) delta;
      else if (blue == max)
	h = 4 + (red - green) / (float) delta;
      h *= 42.5;

      if (h < 0)
	h += 255;
      if (h > 255)
	h -= 255;
    }

  *r = h;
  *g = s;
  *b = v;
}
