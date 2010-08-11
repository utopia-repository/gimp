/* tiff loading and saving for the GIMP
 *  -Peter Mattis
 *
 * The code for this filter is based on "tifftopnm" and "pnmtotiff",
 *  2 programs that are a part of the netpbm package.
 */

/*
** tifftopnm.c - converts a Tagged Image File to a portable anymap
**
** Derived by Jef Poskanzer from tif2ras.c, which is:
**
** Copyright (c) 1990 by Sun Microsystems, Inc.
**
** Author: Patrick J. Naughton
** naughton@wind.sun.com
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation.
**
** This file is provided AS IS with no warranties of any kind.  The author
** shall have no liability with respect to the infringement of copyrights,
** trade secrets or any patents by this file or any part thereof.  In no
** event will the author be liable for any lost revenue or profits or
** other special, indirect and consequential damages.
*/

#include <stdlib.h>
#include <tiffio.h>
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

static void
load_image (filename)
     char *filename;
{
  register TIFF *tif;
  register int col;
  register unsigned char *inP;
  register unsigned char sample;
  register int bitsleft;
  int cols, rows, grayscale;
  int numcolors;
  int row, i;
  unsigned char *buf;
  int maxval;
  unsigned short bps, spp, photomet;
  unsigned short *redcolormap;
  unsigned short *greencolormap;
  unsigned short *bluecolormap;
  Image image;
  unsigned char *dest;

  buf = malloc (strlen (filename) + 11);
  if (!buf)
    gimp_quit ();

  sprintf (buf, "Loading %s:", filename);
  gimp_init_progress (buf);
  free (buf);

  tif = TIFFOpen (filename, "r");
  if (!tif)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  if (!TIFFGetField (tif, TIFFTAG_BITSPERSAMPLE, &bps))
    bps = 1;
  if (!TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &spp))
    spp = 1;
  if (!TIFFGetField (tif, TIFFTAG_PHOTOMETRIC, &photomet))
    {
      printf ("%s: error getting photometric\n", prog_name);
      gimp_quit ();
    }

  switch (spp)
    {
    case 1:
    case 3:
    case 4:
      break;
    default:
      printf ("%s: %d samples per pixel\n", prog_name, spp);
      printf ("%s: can only handle 1-channel gray scale or 1- or 3-channel color\n", prog_name);
      gimp_quit ();
    }

  TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &cols);
  TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &rows);

  maxval = (1 << bps) - 1;
  if (maxval == 1 && spp == 1)
    {
      grayscale = 1;
    }
  else
    {
      switch (photomet)
	{
	case PHOTOMETRIC_MINISBLACK:
	  grayscale = 1;
	  break;

	case PHOTOMETRIC_MINISWHITE:
	  grayscale = 1;
	  break;

	case PHOTOMETRIC_PALETTE:
	  if (!TIFFGetField (tif, TIFFTAG_COLORMAP, &redcolormap, &greencolormap, &bluecolormap))
	    {
	      printf ("%s: error getting colormaps\n", prog_name);
	      gimp_quit ();
	    }
	  numcolors = maxval + 1;
	  maxval = 255;
	  grayscale = 0;

	  for (i = 0; i < numcolors; i++)
	    {
	      redcolormap[i] >>= 8;
	      greencolormap[i] >>= 8;
	      bluecolormap[i] >>= 8;
	    }
	  break;

	case PHOTOMETRIC_RGB:
	  grayscale = 0;
	  break;

	case PHOTOMETRIC_MASK:
	  printf ("%s: don't know how to handle PHOTOMETRIC_MASK\n", prog_name);
	  gimp_quit ();

	default:
	  printf ("%s: unknown photometric: %d\n", prog_name, photomet);
	  gimp_quit ();
	}
    }

  buf = (unsigned char *) malloc (TIFFScanlineSize (tif));
  if (!buf)
    {
      printf ("%s: can't allocate memory for scanline buffer\n", prog_name);
      gimp_quit ();
    }

  image = gimp_new_image (filename, cols, rows, (grayscale) ? GRAY_IMAGE : RGB_IMAGE);
  dest = gimp_image_data (image);

#define NEXTSAMPLE                            \
  {                                           \
      if ( bitsleft == 0 )                    \
      {                                       \
	  ++inP;                              \
	  bitsleft = 8;                       \
      }                                       \
      bitsleft -= bps;                        \
      sample = ( *inP >> bitsleft ) & maxval; \
  }

  for (row = 0; row < rows; ++row)
    {
      if (TIFFReadScanline (tif, buf, row, 0) < 0)
	{
	  printf ("%s: bad data read on line %d\n", prog_name, row);
	  gimp_quit ();
	}

      inP = buf;
      bitsleft = 8;

      switch (photomet)
	{
	case PHOTOMETRIC_MINISBLACK:
	  for (col = 0; col < cols; col++)
	    {
	      NEXTSAMPLE;
	      *dest++ = sample;
	    }
	  break;

	case PHOTOMETRIC_MINISWHITE:
	  for (col = 0; col < cols; col++)
	    {
	      NEXTSAMPLE;
	      *dest++ = maxval - sample;
	    }
	  break;

	case PHOTOMETRIC_PALETTE:
	  for (col = 0; col < cols; col++)
	    {
	      NEXTSAMPLE;
	      *dest++ = redcolormap[sample];
	      *dest++ = greencolormap[sample];
	      *dest++ = bluecolormap[sample];
	    }
	  break;

	case PHOTOMETRIC_RGB:
	  for (col = 0; col < cols; col++)
	    {
	      NEXTSAMPLE;
	      *dest++ = sample;
	      NEXTSAMPLE;
	      *dest++ = sample;
	      NEXTSAMPLE;
	      *dest++ = sample;
	      if (spp == 4)
		NEXTSAMPLE;
	    }
	  break;

	default:
	  printf ("%s: unknown photometric: %d\n", prog_name, photomet);
	  gimp_quit ();
	}
      
      if ((row % 5) == 0)
	gimp_do_progress (row, rows);
    }

  gimp_do_progress (1, 1);
  gimp_display_image (image);
  gimp_update_image (image);
  gimp_free_image (image);
  gimp_quit ();
}


/*
** pnmtotiff.c - converts a portable anymap to a Tagged Image File
**
** Derived by Jef Poskanzer from ras2tif.c, which is:
**
** Copyright (c) 1990 by Sun Microsystems, Inc.
**
** Author: Patrick J. Naughton
** naughton@wind.sun.com
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation.
**
** This file is provided AS IS with no warranties of any kind.  The author
** shall have no liability with respect to the infringement of copyrights,
** trade secrets or any patents by this file or any part thereof.  In no
** event will the author be liable for any lost revenue or profits or
** other special, indirect and consequential damages.
*/

static int dialog_ID;

static void
save_image (filename)
     char *filename;
{
  TIFF *tif;
  unsigned short red[256];
  unsigned short grn[256];
  unsigned short blu[256];
  int cols, rows, row, i;
  long g3options;
  long rowsperstrip;
  unsigned short compression;
  unsigned short fillorder;
  short predictor;
  short photometric;
  short samplesperpixel;
  short bitspersample;
  int bytesperrow;
  Image image;
  unsigned char *src;
  unsigned char *cmap;
  long colors;
  int group_ID;
  int radio_ID;
  int frame_ID;
  int none_ID;
  int lzw_ID;
  int packbits_ID;
  int msb2lsb_ID;
  int lsb2msb_ID;
  long none, lzw, packbits;
  long msb2lsb, lsb2msb;

  none = 0;
  lzw = 1;
  packbits = 0;
  msb2lsb = 1;
  lsb2msb = 0;

  image = gimp_get_input_image (0);

  dialog_ID = gimp_new_dialog ("tiff");
  gimp_new_label (dialog_ID, DEFAULT, "Save Options");
  group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");

  frame_ID = gimp_new_frame (dialog_ID, group_ID, "Compression");
  radio_ID = gimp_new_row_group (dialog_ID, frame_ID, RADIO, "");
  none_ID = gimp_new_radio_button (dialog_ID, radio_ID, "None");
  gimp_change_item (dialog_ID, none_ID, sizeof (none), &none);
  lzw_ID = gimp_new_radio_button (dialog_ID, radio_ID, "LZW");
  gimp_change_item (dialog_ID, lzw_ID, sizeof (lzw), &lzw);
  packbits_ID = gimp_new_radio_button (dialog_ID, radio_ID, "Pack Bits");
  gimp_change_item (dialog_ID, packbits_ID, sizeof (packbits), &packbits);

  frame_ID = gimp_new_frame (dialog_ID, group_ID, "Fill Order");
  radio_ID = gimp_new_row_group (dialog_ID, frame_ID, RADIO, "");
  msb2lsb_ID = gimp_new_radio_button (dialog_ID, radio_ID, "MSB to LSB");
  gimp_change_item (dialog_ID, msb2lsb_ID, sizeof (msb2lsb), &msb2lsb);
  lsb2msb_ID = gimp_new_radio_button (dialog_ID, radio_ID, "LSB to MSB");
  gimp_change_item (dialog_ID, lsb2msb_ID, sizeof (lsb2msb), &lsb2msb);

  gimp_add_callback (dialog_ID, none_ID, item_callback, &none);
  gimp_add_callback (dialog_ID, lzw_ID, item_callback, &lzw);
  gimp_add_callback (dialog_ID, packbits_ID, item_callback, &packbits);
  gimp_add_callback (dialog_ID, msb2lsb_ID, item_callback, &msb2lsb);
  gimp_add_callback (dialog_ID, lsb2msb_ID, item_callback, &lsb2msb);
  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);

  if (!gimp_show_dialog (dialog_ID))
    gimp_quit ();

  src = gimp_image_data (image);
  cols = gimp_image_width (image);
  rows = gimp_image_height (image);

  if (lzw)
    compression = COMPRESSION_LZW;
  else if (packbits)
    compression = COMPRESSION_PACKBITS;
  else
    compression = COMPRESSION_NONE;
  
  if (lsb2msb)
    fillorder = FILLORDER_LSB2MSB;
  else
    fillorder = FILLORDER_MSB2LSB;

  g3options = 0;
  predictor = 0;
  rowsperstrip = 0;

  tif = TIFFOpen (filename, "w");
  if (!tif)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  switch (gimp_image_type (image))
    {
    case RGB_IMAGE:
      samplesperpixel = 3;
      bitspersample = 8;
      photometric = PHOTOMETRIC_RGB;
      bytesperrow = cols * 3;
      break;
    case GRAY_IMAGE:
      samplesperpixel = 1;
      bitspersample = 8;
      photometric = PHOTOMETRIC_MINISBLACK;
      bytesperrow = cols;
      break;
    case INDEXED_IMAGE:
      samplesperpixel = 1;
      bitspersample = 8;
      photometric = PHOTOMETRIC_PALETTE;
      bytesperrow = cols;
      
      cmap = gimp_image_cmap (image);
      colors = gimp_image_colors (image);
      
      for (i = 0; i < colors; i++)
	{
	  red[i] = *cmap++ << 8;
	  grn[i] = *cmap++ << 8;
	  blu[i] = *cmap++ << 8;
	}
      break;
    default:
      break;
    }

  if (rowsperstrip == 0)
    rowsperstrip = (8 * 1024) / bytesperrow;

  /* Set TIFF parameters. */
  TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, cols);
  TIFFSetField (tif, TIFFTAG_IMAGELENGTH, rows);
  TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, bitspersample);
  TIFFSetField (tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField (tif, TIFFTAG_COMPRESSION, compression);
  if ((compression == COMPRESSION_LZW) && (predictor != 0))
    TIFFSetField (tif, TIFFTAG_PREDICTOR, predictor);
  TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, photometric);
  TIFFSetField (tif, TIFFTAG_FILLORDER, fillorder);
  TIFFSetField (tif, TIFFTAG_DOCUMENTNAME, filename);
  TIFFSetField (tif, TIFFTAG_IMAGEDESCRIPTION, "the GIMP was here");
  TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
  TIFFSetField (tif, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
  /* TIFFSetField( tif, TIFFTAG_STRIPBYTECOUNTS, rows / rowsperstrip ); */
  TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  if (gimp_image_type (image) == INDEXED_IMAGE)
    TIFFSetField (tif, TIFFTAG_COLORMAP, red, grn, blu);

  /* Now write the TIFF data. */
  for (row = 0; row < rows; ++row)
    {
      if (TIFFWriteScanline (tif, src, row, 0) < 0)
	{
	  printf ("%s: failed a scanline write on row %d\n", prog_name, row);
	  gimp_quit ();
	}

      src += bytesperrow;
    }

  TIFFFlushData (tif);
  TIFFClose (tif);

  gimp_free_image (image);
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
