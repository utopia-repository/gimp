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
 * This filter is heavily based upon the "example.c" file in libpng.
 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include "gimp.h"

/* Declare some local functions.
 */
static void load_image (char *);
static void save_image (char *);

static void item_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);

static char *prog_name;
static int dialog_ID;
static int group_ID;

void
main (argc, argv)
     int argc;
     char **argv;
{
  /* Save the program name so we can use it later in reporting errors
   */
  prog_name = argv[0];

  /* Call 'gimp_init' to initialize this filter.
   * 'gimp_init' makes sure that the filter was properly called and
   *  it opens pipes for reading and writing.
   */
  if (gimp_init (argc, argv))
    {
      /* This is a file filter so all it needs to know about is loading
       *  and saving images. So we'll install handlers for those two
       *  messages.
       */
      gimp_install_load_save_handlers (load_image, save_image);

      /* Run until something happens. That something could be getting
       *  a 'QUIT' message or getting a load or save message.
       */
      gimp_main_loop ();
    }
}

static void
load_image (filename)
     char *filename;
{
  FILE *fp;
  png_struct *png_ptr;
  png_info *info_ptr;
  png_color_16 my_background;
  Image image;
  unsigned char *temp;
  long row_stride;
  short pass, number_passes, y;
  int cur_progress;
  int max_progress;

  temp = malloc (strlen (filename) + 11);
  if (!temp)
    gimp_quit ();

  sprintf (temp, "Loading %s:", filename);
  gimp_init_progress (temp);
  free (temp);
  
  /* open the file */
  fp = fopen (filename, "rb");
  if (!fp)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  /* allocate the necessary structures */
  png_ptr = malloc (sizeof (png_struct));
  if (!png_ptr)
    {
      fclose (fp);
      gimp_quit ();
    }
  
  info_ptr = malloc(sizeof (png_info));
  if (!info_ptr)
    {
      fclose (fp);
      free (png_ptr);
      gimp_quit ();
    }

  image = NULL;
  /* set error handling */
  if (setjmp (png_ptr->jmpbuf))
    {
      /* If we get here, we had a problem reading the file */
      png_read_destroy (png_ptr, info_ptr, (png_info*) 0);
      fclose (fp);
      free (png_ptr);
      free (info_ptr);
      if (image)
	gimp_free_image (image);
      gimp_quit ();
    }
  
  /* initialize the structures, info first for error handling */
  png_info_init (info_ptr);
  png_read_init (png_ptr);
  
  /* set up the input control */
  png_init_io (png_ptr, fp);
  
  /* read the file information */
  png_read_info (png_ptr, info_ptr);
  
  /* allocate the memory to hold the image using the fields
     of png_info. */
  
  /* set up the transformations you want.  Note that these are
     all optional.  Only call them if you want them */
    
  /* expand paletted colors into true rgb */
  if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
    {
      png_set_expand (png_ptr);
      info_ptr->channels = 3;
    }
  
  /* expand grayscale images to the full 8 bits */
  if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY &&
      info_ptr->bit_depth < 8)
    png_set_expand (png_ptr);
  
  /* expand images with transparency to full alpha channels */
  if (info_ptr->valid & PNG_INFO_tRNS)
    png_set_expand (png_ptr);

  /* Set the background color to draw transparent and alpha
     images over */

  if (info_ptr->valid & PNG_INFO_bKGD)
    png_set_background (png_ptr, &(info_ptr->background),
			PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
  else
    {
      my_background.red = 0;
      my_background.green = 0;
      my_background.blue = 0;
      my_background.gray = 0;
      png_set_background (png_ptr, &my_background,
			  PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
    }
  
  /* tell libpng to handle the gamma conversion for you */
  if (info_ptr->valid & PNG_INFO_gAMA)
    png_set_gamma (png_ptr, 2.22, info_ptr->gamma);
  else
    png_set_gamma (png_ptr, 2.22, 0.45);

  /* tell libpng to strip 16 bit depth files down to 8 bits */
  if (info_ptr->bit_depth == 16)
    png_set_strip_16 (png_ptr);
  
  /* shift the pixels down to their true bit depth */
/*
  if (info_ptr->valid & PNG_INFO_sBIT &&
      info_ptr->bit_depth > info_ptr->sig_bit)
    png_set_shift (png_ptr, &(info_ptr->sig_bit));
*/
  
  /* turn on interlace handling */
  if (info_ptr->interlace_type)
    number_passes = png_set_interlace_handling (png_ptr);
  else
    number_passes = 1;

  /* optional call to update palette with transformations */
  png_start_read_image (png_ptr);

  /* Create a new image of the proper size and associate the filename with it.
   */
  image = gimp_new_image (filename,
			  info_ptr->width,
			  info_ptr->height,
			  (info_ptr->channels >= 3) ? RGB_IMAGE : GRAY_IMAGE);

  cur_progress = 0;
  max_progress = info_ptr->height * number_passes;

  row_stride = gimp_image_width (image) * gimp_image_channels (image);
  for (pass = 0; pass < number_passes; pass++)
    {
      temp = gimp_image_data (image);

      /* If you are only reading on row at a time, this works */
      for (y = 0; y < info_ptr->height; y++)
	{
	  png_read_rows (png_ptr, &temp, NULL, 1);
	  temp += row_stride;

	  if ((++cur_progress % 5) == 0)
	    gimp_do_progress (cur_progress, max_progress);
	}
      
      /* if you want to display the image after every pass, do
         so here */
    }
  
  /* read the rest of the file, getting any additional chunks
     in info_ptr */
  png_read_end (png_ptr, info_ptr);
  
  /* clean up after the read, and free any memory allocated */
  png_read_destroy (png_ptr, info_ptr, (png_info *)0);
  
  /* free the structures */
  free (png_ptr);
  free (info_ptr);
  
  /* close the file */
  fclose (fp);

  gimp_do_progress (1, 1);

  /* Tell the GIMP to display the image.
   */
  gimp_display_image (image);

  /* Tell the GIMP to update the image. (ie Redraw it).
   */
  gimp_update_image (image);
  
  /* Free the image. (This involves detaching the shared memory segment,
   *  which is a good thing.)
   */
  gimp_free_image (image);

  /* Quit.
   */
  gimp_quit ();
}

static void
save_image (filename)
     char *filename;
{
  FILE *fp;
  png_struct *png_ptr;
  png_info *info_ptr;
  Image image;
  unsigned char *temp;
  long row_stride;
  short pass, number_passes, y;
  unsigned char *cmap;
  int interlace_ID;
  long interlace;
  int i, cur_progress, max_progress;
  int paletted, colors;

  /* Get the input image.
   */
  image = gimp_get_input_image (0);

  interlace = 0;
  paletted = 0;
  colors = 256;
  
  dialog_ID = gimp_new_dialog ("PNG Save Options");
  gimp_new_label (dialog_ID, DEFAULT, "Options");
  group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
  interlace_ID = gimp_new_check_button (dialog_ID, group_ID, "Interlace");
  gimp_add_callback (dialog_ID, interlace_ID, item_callback, &interlace);
  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
  
  if (!gimp_show_dialog (dialog_ID))
    gimp_quit ();

  temp = malloc (strlen (filename) + 11);
  if (!temp)
    gimp_quit ();

  sprintf (temp, "Saving %s:", filename);
  gimp_init_progress (temp);
  free (temp);

  paletted = (gimp_image_type (image) == INDEXED_IMAGE);
  if (paletted)
    {
      cmap = gimp_image_cmap (image);
      colors = gimp_image_colors (image);
    }

  /* open the file */
  fp = fopen (filename, "wb");
  if (!fp)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }
  
  /* allocate the necessary structures */
  png_ptr = malloc (sizeof (png_struct));
  if (!png_ptr)
    {
      fclose (fp);
      gimp_quit ();
    }
  
  info_ptr = malloc (sizeof (png_info));
  if (!info_ptr)
    {
      fclose (fp);
      free (png_ptr);
      gimp_quit ();
    }
  
  /* set error handling */
  if (setjmp (png_ptr->jmpbuf))
    {
      /* If we get here, we had a problem reading the file */
      png_write_destroy (png_ptr);
      fclose (fp);
      free (png_ptr);
      free (info_ptr);
      gimp_free_image (image);
      gimp_quit ();
    }
  
  /* initialize the structures */
  png_info_init (info_ptr);
  png_write_init (png_ptr);
  
  /* set up the output control */
  png_init_io (png_ptr, fp);
  
  /* set the file information here */
  info_ptr->width = gimp_image_width (image);
  info_ptr->height = gimp_image_height (image);
  info_ptr->bit_depth = 8;
  info_ptr->color_type = ((gimp_image_channels (image) == 3) ? 
			  PNG_COLOR_TYPE_RGB : 
			  PNG_COLOR_TYPE_GRAY);
  info_ptr->compression_type = 0;
  info_ptr->filter_type = 0;
  info_ptr->interlace_type = interlace;
  info_ptr->valid = 0;

  if (paletted)
    {
      info_ptr->color_type = PNG_COLOR_TYPE_PALETTE;
      info_ptr->valid |= PNG_INFO_PLTE;
      
      info_ptr->palette = malloc (sizeof (png_color) * colors);
      if (!info_ptr->palette)
	gimp_quit ();

      info_ptr->num_palette = colors;
      for (i = 0; i < colors; i++)
	{
	  info_ptr->palette[i].red = *cmap++;
	  info_ptr->palette[i].green = *cmap++;
	  info_ptr->palette[i].blue = *cmap++;
	}
    }
    
  /* other optional chunks */
  
  /* write the file information */
  png_write_info (png_ptr, info_ptr);
  
  /* set up the transformations you want.  Note that these are
     all optional.  Only call them if you want them */
  
  /* pack pixels into bytes */
  png_set_packing (png_ptr);
  
  /* the other way to write the image - deal with interlacing */
  
  /* turn on interlace handling */
  if (interlace)
    number_passes = png_set_interlace_handling (png_ptr);
  else
    number_passes = 1;

  cur_progress = 0;
  max_progress = info_ptr->height * number_passes;

  row_stride = gimp_image_width (image) * gimp_image_channels (image);
  for (pass = 0; pass < number_passes; pass++)
    {
      temp = gimp_image_data (image);
      /* If you are only writing one row at a time, this works */
      for (y = 0; y < info_ptr->height; y++)
	{
	  png_write_rows (png_ptr, &temp, 1);
	  temp += row_stride;

	  if ((++cur_progress % 5) == 0)
	    gimp_do_progress (cur_progress, max_progress);
	}
    }
  
  /* write the rest of the file */
  png_write_end (png_ptr, info_ptr);
  
  /* clean up after the write, and free any memory allocated */
  png_write_destroy (png_ptr);
  
  /* if you malloced the palette, free it here */
  if (info_ptr->palette)
    free (info_ptr->palette);
  
  /* free the structures */
  free (png_ptr);
  free (info_ptr);
  
  /* close the file */
  fclose (fp);

  gimp_do_progress (1, 1);
  
  /* Free the image. (This involves detaching the shared memory segment,
   *  which is a good thing.)
   */
  gimp_free_image (image);

  /* Quit.
   */
  gimp_quit ();  
}

static void
item_callback (item_ID, client_data, call_data)
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
