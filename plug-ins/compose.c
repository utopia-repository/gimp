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

/* compose 3 grayscale images into an rgb image
 */

#include "gimp.h"

/* Declare local functions.
 */
static void image_menu_callback (int, void *, void *);
static void radio_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void compose_rgb (Image, Image, Image);
static void compose_hsv (Image, Image, Image);
static void hsv_to_rgb (int *, int *, int *);

static char *prog_name;
static int dialog_ID;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image src1, src2, src3;
  int group_ID;
  int image_menu1_ID;
  int image_menu2_ID;
  int image_menu3_ID;
  long src1_ID, src2_ID, src3_ID;
  long rgb_ID, hsv_ID;
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
      src1 = src2 = src3 = 0;
      src1_ID = src2_ID = src3_ID = 0;
      
      rgb = 1;
      hsv = 0;
      
      dialog_ID = gimp_new_dialog ("Composite");
      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, RADIO, "");
      image_menu1_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_GRAY,
					    "R/H Image");
      image_menu2_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_GRAY,
					    "G/S Image");
      image_menu3_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_GRAY,
					    "B/V Image");
      rgb_ID = gimp_new_radio_button (dialog_ID, group_ID, "RGB composition");
      gimp_change_item (dialog_ID, rgb_ID, sizeof (rgb), &rgb);
      hsv_ID = gimp_new_radio_button (dialog_ID, group_ID, "HSV composition");
      gimp_change_item (dialog_ID, hsv_ID, sizeof (hsv), &hsv);
      gimp_add_callback (dialog_ID, image_menu1_ID, image_menu_callback, &src1_ID);
      gimp_add_callback (dialog_ID, image_menu2_ID, image_menu_callback, &src2_ID);
      gimp_add_callback (dialog_ID, image_menu3_ID, image_menu_callback, &src3_ID);
      gimp_add_callback (dialog_ID, rgb_ID, radio_callback, &rgb);
      gimp_add_callback (dialog_ID, hsv_ID, radio_callback, &hsv);
      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

      if (gimp_show_dialog (dialog_ID))
	{
	  src1 = gimp_get_input_image (src1_ID);
	  
	  if (src2_ID == src1_ID)
	    src2 = src1;
	  else
	    src2 = gimp_get_input_image (src2_ID);
	  
	  if ((src3_ID == src1_ID) || (src3_ID == src2_ID))
	    src3 = (src3_ID == src1_ID) ? src1 : src2;
	  else
	    src3 = gimp_get_input_image (src3_ID);
	  
	  if (src1 && src2 && src3)
	    {
	      if (rgb)
		compose_rgb (src1, src2, src3);
	      else if (hsv)
		compose_hsv (src1, src2, src3);
	    }
	}

      /* Free the images.
       */
      if (src1)
	gimp_free_image (src1);
      if (src2)
	gimp_free_image (src2);
      if (src3)
	gimp_free_image (src3);

      /* Quit
       */
      gimp_quit ();
    }

  return 0;
}

static void
image_menu_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  *((long*) client_data) = *((long*) call_data);
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
compose_rgb (rsrc, gsrc, bsrc)
     Image rsrc, gsrc, bsrc;
{
  Image dest;
  long width;
  long height;
  unsigned char *destp;
  unsigned char *rsrcp;
  unsigned char *gsrcp;
  unsigned char *bsrcp;
  int i, j;

  width = gimp_image_width (rsrc);
  height = gimp_image_height (rsrc);

  dest = gimp_new_image (0, width, height, RGB_IMAGE);

  destp = gimp_image_data (dest);
  rsrcp = gimp_image_data (rsrc);
  gsrcp = gimp_image_data (gsrc);
  bsrcp = gimp_image_data (bsrc);
  
  gimp_init_progress ("compose RGB");

  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	{
	  *destp++ = *rsrcp++;
	  *destp++ = *gsrcp++;
	  *destp++ = *bsrcp++;
	}

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  gimp_display_image (dest);
  gimp_update_image (dest);
  gimp_free_image (dest);
}

static void
compose_hsv (hsrc, ssrc, vsrc)
     Image hsrc, ssrc, vsrc;
{
  Image dest;
  long width;
  long height;
  unsigned char *destp;
  unsigned char *hsrcp;
  unsigned char *ssrcp;
  unsigned char *vsrcp;
  int r, g, b;
  int i, j;

  width = gimp_image_width (hsrc);
  height = gimp_image_height (hsrc);

  dest = gimp_new_image (0, width, height, RGB_IMAGE);

  destp = gimp_image_data (dest);
  hsrcp = gimp_image_data (hsrc);
  ssrcp = gimp_image_data (ssrc);
  vsrcp = gimp_image_data (vsrc);

  gimp_init_progress ("compose HSV");

  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	{
	  r = *hsrcp++;
	  g = *ssrcp++;
	  b = *vsrcp++;

	  hsv_to_rgb (&r, &g, &b);
	  
	  *destp++ = r;
	  *destp++ = g;
	  *destp++ = b;
	}

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  gimp_display_image (dest);
  gimp_update_image (dest);
  gimp_free_image (dest);
}

static void
hsv_to_rgb (h, s, v)
     int *h, *s, *v;
{
  float hue, saturation, value;
  float f, p, q, t;

  if (*s == 0)
    {
      *h = *v;
      *s = *v;
      *v = *v;
    }
  else
    {
      hue = *h * 6.0 / 255.0;
      saturation = *s / 255.0;
      value = *v / 255.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - (saturation * f));
      t = value * (1.0 - (saturation * (1.0 - f)));
      
      switch ((int) hue)
	{
	case 0:
	  *h = value * 255;
	  *s = t * 255;
	  *v = p * 255;
	  break;
	case 1:
	  *h = q * 255;
	  *s = value * 255;
	  *v = p * 255;
	  break;
	case 2:
	  *h = p * 255;
	  *s = value * 255;
	  *v = t * 255;
	  break;
	case 3:
	  *h = p * 255;
	  *s = q * 255;
	  *v = value * 255;
	  break;
	case 4:
	  *h = t * 255;
	  *s = p * 255;
	  *v = value * 255;
	  break;
	case 5:
	  *h = value * 255;
	  *s = p * 255;
	  *v = q * 255;
	  break;
	}
    }
}
