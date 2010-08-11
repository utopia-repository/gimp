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
#include "gimp.h"

/* Declare local functions.
 */
static void image_menu_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);
static void add (Image, Image);

static char *prog_name;

static int dialog_ID;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image src1, src2;
  int group_ID;
  int image_menu1_ID;
  int image_menu2_ID;
  int src1_ID, src2_ID;
  
  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      src1 = src2 = 0;
      src1_ID = src2_ID = 0;

      /* Create a dialog.
       */
      dialog_ID = gimp_new_dialog ("Add");
      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
      image_menu1_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_RGB | IMAGE_CONSTRAIN_GRAY,
					    "First Image");
      image_menu2_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_RGB | IMAGE_CONSTRAIN_GRAY,
					    "Second Image");
      gimp_add_callback (dialog_ID, image_menu1_ID, image_menu_callback, &src1_ID);
      gimp_add_callback (dialog_ID, image_menu2_ID, image_menu_callback, &src2_ID);
      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
      
      if (gimp_show_dialog (dialog_ID))
	{
	  src1 = gimp_get_input_image (src1_ID);
	  src2 = (src2_ID != src1_ID) ? gimp_get_input_image (src2_ID) : src1;
	  
	  if (src1 && src2)
	    {
	      gimp_init_progress ("add");
	      add (src1, src2);
	    }
	}

      /* Free the images.
       */
      if (src1)
	gimp_free_image (src1);
      if (src2)
	gimp_free_image (src2);

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
add (src1, src2)
     Image src1, src2;
{
  Image dest;
  ImageType dest_type;
  unsigned char *src1p;
  unsigned char *src2p;
  unsigned char *destp;
  long width, height;
  long src1_channels;
  long src2_channels;
  long dest_channels;
  int src1r, src1g, src1b;
  int src2r, src2g, src2b;
  int destr, destg, destb;
  int i, j;

  src1p = gimp_image_data (src1);
  src2p = gimp_image_data (src2);

  width = gimp_image_width (src1);
  height = gimp_image_height (src1);
  
  src1_channels = gimp_image_channels (src1);
  src2_channels = gimp_image_channels (src2);

  if ((src1_channels == 3) || (src2_channels == 3))
    dest_channels = 3;
  else
    dest_channels = 1;

  if ((gimp_image_type (src1) == RGB_IMAGE) ||
      (gimp_image_type (src2) == RGB_IMAGE))
    dest_type = RGB_IMAGE;
  else
    dest_type = GRAY_IMAGE;
  
  dest = gimp_new_image (0, width, height, dest_type);
  destp = gimp_image_data (dest);

  for (i = 0; i < height; i++)
    {
      for (j = 0; j < width; j++)
	{
	  if (src1_channels == 3)
	    {
	      src1r = *src1p++;
	      src1g = *src1p++;
	      src1b = *src1p++;
	    }
	  else
	    {
	      src1r = *src1p;
	      src1g = *src1p;
	      src1b = *src1p++;
	    }

	  if (src2_channels == 3)
	    {
	      src2r = *src2p++;
	      src2g = *src2p++;
	      src2b = *src2p++;
	    }
	  else
	    {
	      src2r = *src2p;
	      src2g = *src2p;
	      src2b = *src2p++;
	    }

	  destr = src1r + src2r;
	  destg = src1g + src2g;
	  destb = src1b + src2b;

	  if (destr < 0) destr = 0;
	  if (destg < 0) destg = 0;
	  if (destb < 0) destb = 0;
	  if (destr > 255) destr = 255;
	  if (destg > 255) destg = 255;
	  if (destb > 255) destb = 255;

	  if (dest_channels == 3)
	    {
	      *destp++ = destr;
	      *destp++ = destg;
	      *destp++ = destb;
	    }
	  else
	    {
	      *destp++ = destr;
	    }
	}

      if ((i % 5) == 0)
	gimp_do_progress (i, height);
    }

  gimp_display_image (dest);
  gimp_update_image (dest);
  gimp_free_image (dest);
}
