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

/* JPEG loading and saving file filter for the GIMP
 *  -Peter Mattis
 *
 * This filter is heavily based upon the "example.c" file in libjpeg.
 * In fact most of the loading and saving code was simply cut-and-pasted
 *  from that file. The filter, therefore, also uses libjpeg.
 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
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


typedef struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  
  jmp_buf setjmp_buffer;	/* for return to caller */
} *my_error_ptr;


/*
 * Here's the routine that will replace the standard error_exit method:
 */

static void
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp (myerr->setjmp_buffer, 1);
}

static void
load_image (filename)
     char *filename;
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  long row_stride;
  FILE *infile;
  Image image;
  unsigned char *temp;

  temp = malloc (strlen (filename) + 11);
  if (!temp)
    gimp_quit ();

  sprintf (temp, "Loading %s:", filename);
  gimp_init_progress (temp);
  free (temp);

  /* We set up the normal JPEG error routines. */
  cinfo.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  
  if ((infile = fopen (filename, "rb")) == NULL)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  image = NULL;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp (jerr.setjmp_buffer))
    {
      /* If we get here, the JPEG code has signaled an error.
       * We need to clean up the JPEG object, close the input file, and return.
       */
      jpeg_destroy_decompress (&cinfo);
      if (infile)
	fclose (infile);
      if (image)
	gimp_free_image (image);
      gimp_quit ();
    }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress (&cinfo);
  
  /* Step 2: specify data source (eg, a file) */
  
  jpeg_stdio_src (&cinfo, infile);
  
  /* Step 3: read file parameters with jpeg_read_header() */
  
  (void) jpeg_read_header (&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */
  
  /* Step 4: set parameters for decompression */
  
  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */
  
  /* Step 5: Start decompressor */
  
  jpeg_start_decompress (&cinfo);
  
  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;

  /* Create a new image of the proper size and associate the filename with it.
   */
  image = gimp_new_image (filename,
			  cinfo.output_width, 
			  cinfo.output_height, 
			  (cinfo.output_components == 1) ? GRAY_IMAGE : RGB_IMAGE);

  /* Get a pointer to the image data.
   */
  temp = gimp_image_data (image);
  
  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */
  
  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo.output_scanline < cinfo.output_height) 
    {
      jpeg_read_scanlines (&cinfo, (JSAMPARRAY) &temp, 1);
      temp += row_stride;

      if ((cinfo.output_scanline % 5) == 0)
	gimp_do_progress (cinfo.output_scanline, cinfo.output_height);
    }

  /* Step 7: Finish decompression */
  
  jpeg_finish_decompress (&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */
  
  /* Step 8: Release JPEG decompression object */
  
  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress (&cinfo);
  
  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
  fclose (infile);
  
  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.num_warnings is nonzero).
   */
  
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
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *outfile;
  long row_stride;
  Image image;
  unsigned char *temp;
  int group_ID;
  int frame_ID;
  int quality_ID;
  int smoothing_ID;
  long quality;
  long smoothing;

  quality = 75;
  smoothing = 0;

  dialog_ID = gimp_new_dialog ("JPEG Save Options");
  group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
  frame_ID = gimp_new_frame (dialog_ID, group_ID, "Quality");
  quality_ID = gimp_new_scale (dialog_ID, frame_ID, 0, 100, quality, 0);
  frame_ID = gimp_new_frame (dialog_ID, group_ID, "Smoothing");
  smoothing_ID = gimp_new_scale (dialog_ID, frame_ID, 0, 100, smoothing, 0);

  gimp_add_callback (dialog_ID, quality_ID, item_callback, &quality);
  gimp_add_callback (dialog_ID, smoothing_ID, item_callback, &smoothing);
  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

  if (gimp_show_dialog (dialog_ID))
    {    
      temp = malloc (strlen (filename) + 11);
      if (!temp)
	gimp_quit ();
      
      sprintf (temp, "Saving %s:", filename);
      gimp_init_progress (temp);
      free (temp);

      /* Step 1: allocate and initialize JPEG compression object */
      
      /* We have to set up the error handler first, in case the initialization
       * step fails.  (Unlikely, but it could happen if you are out of memory.)
       * This routine fills in the contents of struct jerr, and returns jerr's
       * address which we place into the link field in cinfo.
       */
      cinfo.err = jpeg_std_error (&jerr.pub);
      jerr.pub.error_exit = my_error_exit;
      
      image = NULL;
      outfile = NULL;
      /* Establish the setjmp return context for my_error_exit to use. */
      if (setjmp (jerr.setjmp_buffer))
	{
	  /* If we get here, the JPEG code has signaled an error.
	   * We need to clean up the JPEG object, close the input file, and return.
	   */
	  jpeg_destroy_compress (&cinfo);
	  if (outfile)
	    fclose (outfile);
	  if (image)
	    gimp_free_image (image);
	  gimp_quit ();
	}
      
      /* Now we can initialize the JPEG compression object. */
      jpeg_create_compress (&cinfo);
      
      /* Step 2: specify data destination (eg, a file) */
      /* Note: steps 2 and 3 can be done in either order. */
      
      /* Here we use the library-supplied code to send compressed data to a
       * stdio stream.  You can also write your own code to do something else.
       * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
       * requires it in order to write binary files.
       */
      if ((outfile = fopen (filename, "wb")) == NULL)
	{
	  fprintf (stderr, "can't open %s\n", filename);
	  gimp_quit ();
	}
      jpeg_stdio_dest (&cinfo, outfile);
      
      /* Get the input image and a pointer to its data.
       */
      image = gimp_get_input_image (0);
      switch (gimp_image_type (image))
	{
	case RGB_IMAGE:
	case GRAY_IMAGE:
	  break;
	case INDEXED_IMAGE:
	  gimp_message ("jpeg: cannot operate on indexed color images");
	  gimp_quit ();
	  break;
	default:
	  gimp_message ("jpeg: cannot operate on unknown image types");
	  gimp_quit ();
	  break;
	}
      temp = gimp_image_data (image);
      
      /* Step 3: set parameters for compression */
      
      /* First we supply a description of the input image.
       * Four fields of the cinfo struct must be filled in:
       */
      cinfo.image_width = gimp_image_width (image);      /* image width and height, in pixels */
      cinfo.image_height = gimp_image_height (image);
      cinfo.input_components = gimp_image_channels (image) ;/* # of color components per pixel */
      /* colorspace of input image */
      cinfo.in_color_space = (gimp_image_channels (image) == 3) ? JCS_RGB : JCS_GRAYSCALE;
      /* Now use the library's routine to set default compression parameters.
       * (You must set at least cinfo.in_color_space before calling this,
       * since the defaults depend on the source color space.)
       */
      jpeg_set_defaults (&cinfo);
      /* Now you can set any non-default parameters you wish to.
       * Here we just illustrate the use of quality (quantization table) scaling:
       */
      jpeg_set_quality (&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
      cinfo.smoothing_factor = smoothing;
      
      /* Step 4: Start compressor */
      
      /* TRUE ensures that we will write a complete interchange-JPEG file.
       * Pass TRUE unless you are very sure of what you're doing.
       */
      jpeg_start_compress (&cinfo, TRUE);
      
      /* Step 5: while (scan lines remain to be written) */
      /*           jpeg_write_scanlines(...); */
      
      /* Here we use the library's state variable cinfo.next_scanline as the
       * loop counter, so that we don't have to keep track ourselves.
       * To keep things simple, we pass one scanline per call; you can pass
       * more if you wish, though.
       */
      /* JSAMPLEs per row in image_buffer */
      row_stride = gimp_image_width (image) * cinfo.input_components;
      
      while (cinfo.next_scanline < cinfo.image_height) 
	{
	  jpeg_write_scanlines (&cinfo, (JSAMPARRAY) &temp, 1);
	  temp += row_stride;

	  if ((cinfo.next_scanline % 5) == 0)
	    gimp_do_progress (cinfo.next_scanline, cinfo.image_height);
	}

      /* Step 6: Finish compression */
      jpeg_finish_compress (&cinfo);
      /* After finish_compress, we can close the output file. */
      fclose (outfile);
      
      /* Step 7: release JPEG compression object */
      
      /* This is an important step since it will release a good deal of memory. */
      jpeg_destroy_compress (&cinfo);
      
      /* And we're done! */
      
      gimp_do_progress (1, 1);
      
      /* Free the image.
       */
      gimp_free_image (image);
    }
  
  /* Quit
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
