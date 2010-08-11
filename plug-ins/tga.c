/* Targa loading and saving file filter for the Gimp
 *  -Spencer Kimball
 *
 * This filter uses code taken from the "tgatoppm" and "ppmtotga" programs
 *  which are part of the "netpbm" package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gimp.h"
#include "tga.h"

/* Based on netpbm utilties with the following copyright:  */

/*
**
** Partially based on tga2rast, version 1.0, by Ian MacPhedran.
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

#define MAXCOLORS 16384

static long mapped, rlencoded;

static unsigned char ColorMap[MAXCOLORS * 3];
static int RLE_count = 0, RLE_flag = 0;

static void readtga (FILE *, struct ImageHeader *);
static void get_map_entry (FILE *, unsigned char *, int);
static void get_pixel (FILE *, unsigned char *, int);
static unsigned char getbyte (FILE *);

static void LoadTarga (char *);


static void writetga (FILE *, struct ImageHeader *, char *);
static void put_map_entry (FILE *, unsigned char *);
static void compute_runlengths (int, unsigned char *, int, int *);
static void put_pixel (FILE *, unsigned char *, int, int);
static int  is_equal (unsigned char *, int, int, int);

static void SaveTarga (char *);


static void item_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);


static int dialog_ID;
static int group_ID;
static int rle_ID;

char *prog_name;
char *filename;


int
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
      gimp_install_load_save_handlers (LoadTarga, SaveTarga);

      /* Run until something happens. That something could be getting
       *  a 'QUIT' message or getting a load or save message.
       */
      gimp_main_loop ();
    }

  return 0;
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

/*****************************************************************/
/*****************************************************************/
/*    Loading targa files                                        */
/*****************************************************************/
/*****************************************************************/

static void
LoadTarga (filename)
     char * filename;
{
  struct ImageHeader tga_head;
  char * name;
  Image image;
  int i;
  unsigned int temp1, temp2;
  unsigned char *dest;
  FILE* ifp;
  int rows, cols, row, col, realrow, truerow, baserow;
  int rowstride, channels;
  int maxval;
  int cur_progress, max_progress;

/*
  unsigned char gimp_cmap[768];
  long rowstride, channels;
  int i, j, 
*/

  ifp = fopen (filename, "rb");
  if (!ifp)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  /* Read the Targa file header. */
  readtga( ifp, &tga_head );

  rows = ( (int) tga_head.Height_lo ) + ( (int) tga_head.Height_hi ) * 256;
  cols = ( (int) tga_head.Width_lo ) + ( (int) tga_head.Width_hi ) * 256;

  switch ( tga_head.ImgType )
    {
    case TGA_Map:
    case TGA_RGB:
    case TGA_Mono:
    case TGA_RLEMap:
    case TGA_RLERGB:
    case TGA_RLEMono:
      break;

    default:
      printf ("unknown Targa image type %d\n", tga_head.ImgType);
    }

  if (tga_head.ImgType == TGA_Map ||
      tga_head.ImgType == TGA_RLEMap ||
      tga_head.ImgType == TGA_CompMap ||
      tga_head.ImgType == TGA_CompMap4 )
    { /* Color-mapped image */
      if ( tga_head.CoMapType != 1 )
	printf ("mapped image (type %d) with color map type != 1\n", tga_head.ImgType);
      mapped = 1;
      /* Figure maxval from CoSize. */
      switch ( tga_head.CoSize )
	{
	case 8:
	case 24:
	case 32:
	  maxval = 255;
	  break;
	  
	case 15:
	case 16:
	  maxval = 31;
	  break;
	  
	default:
	  printf ("unknown colormap pixel size - %d\n", tga_head.CoSize );
	}
    }
  else
    { /* Not colormap, so figure maxval from PixelSize. */
      mapped = 0;
      switch ( tga_head.PixelSize )
	{
	case 8:
	case 24:
	case 32:
	  maxval = 255;
	  break;
	  
	case 15:
	case 16:
	  maxval = 31;
	  break;
	  
	default:
	  printf ("unknown pixel size - %d\n", tga_head.PixelSize);
	}
    }

  if (mapped && tga_head.PixelSize == 8)
    image = gimp_new_image (filename, cols, rows, INDEXED_IMAGE);
  else if (!mapped && tga_head.PixelSize == 8)
    image = gimp_new_image (filename, cols, rows, GRAY_IMAGE);
  else
    image = gimp_new_image (filename, cols, rows, RGB_IMAGE);

  /* If required, read the color map information. */
  if ( tga_head.CoMapType != 0 )
    {
      temp1 = tga_head.Index_lo + tga_head.Index_hi * 256;
      temp2 = tga_head.Length_lo + tga_head.Length_hi * 256;
      if ( ( temp1 + temp2 + 1 ) >= MAXCOLORS )
	printf ("too many colors - %d\n", ( temp1 + temp2 + 1 ));
      for ( i = temp1; i < ( temp1 + temp2 ); ++i )
	get_map_entry( ifp, ColorMap + i*3, (int) tga_head.CoSize );
    }
  
  /* Check run-length encoding. */
  if ( tga_head.ImgType == TGA_RLEMap ||
      tga_head.ImgType == TGA_RLERGB ||
      tga_head.ImgType == TGA_RLEMono )
    rlencoded = 1;
  else
    rlencoded = 0;
  
  /* Read the Targa file body and convert to portable format. */
  
  name = malloc (strlen (filename) + 11);
  if (!name)
    gimp_quit ();

  sprintf (name, "Loading %s:", filename);
  gimp_init_progress (name);
  free (name);

  truerow = 0;
  baserow = 0;
  dest = gimp_image_data (image);
  channels = gimp_image_channels (image);
  rowstride = channels * gimp_image_width (image);
  cur_progress = 0;
  max_progress = rows;

  for ( row = 0; row < rows; ++row )
    {
      realrow = truerow;
      if ( tga_head.OrgBit == 0 )
	realrow = rows - realrow - 1;
      
      for ( col = 0; col < cols; ++col )
	get_pixel( ifp, (dest + realrow*rowstride + col*channels), (int) tga_head.PixelSize );
      if ( tga_head.IntrLve == TGA_IL_Four )
	truerow += 4;
      else if ( tga_head.IntrLve == TGA_IL_Two )
	truerow += 2;
      else
	++truerow;
      if ( truerow >= rows )
	truerow = ++baserow;
      if ((++cur_progress % 5) == 0)
	gimp_do_progress (cur_progress, max_progress);
    }

  fclose (ifp);
  
  gimp_do_progress (1, 1);
  if (mapped && tga_head.PixelSize == 8)
    gimp_set_image_colors (image, ColorMap + temp1*3, (temp1 + temp2));
  gimp_display_image (image);
  gimp_update_image (image);
  gimp_free_image (image);

  gimp_quit ();
}

static void
readtga( ifp, tgaP )
     FILE* ifp;
     struct ImageHeader* tgaP;
{
  unsigned char flags;
  ImageIDField junk;

  tgaP->IDLength = getbyte( ifp );
  tgaP->CoMapType = getbyte( ifp );
  tgaP->ImgType = getbyte( ifp );
  tgaP->Index_lo = getbyte( ifp );
  tgaP->Index_hi = getbyte( ifp );
  tgaP->Length_lo = getbyte( ifp );
  tgaP->Length_hi = getbyte( ifp );
  tgaP->CoSize = getbyte( ifp );
  tgaP->X_org_lo = getbyte( ifp );
  tgaP->X_org_hi = getbyte( ifp );
  tgaP->Y_org_lo = getbyte( ifp );
  tgaP->Y_org_hi = getbyte( ifp );
  tgaP->Width_lo = getbyte( ifp );
  tgaP->Width_hi = getbyte( ifp );
  tgaP->Height_lo = getbyte( ifp );
  tgaP->Height_hi = getbyte( ifp );
  tgaP->PixelSize = getbyte( ifp );
  flags = getbyte( ifp );
  tgaP->AttBits = flags & 0xf;
  tgaP->Rsrvd = ( flags & 0x10 ) >> 4;
  tgaP->OrgBit = ( flags & 0x20 ) >> 5;
  tgaP->IntrLve = ( flags & 0xc0 ) >> 6;
  
  if ( tgaP->IDLength != 0 )
    fread( junk, 1, (int) tgaP->IDLength, ifp );
}

static void
get_map_entry( ifp, color, Size )
     FILE* ifp;
     unsigned char * color;
     int Size;
{
  unsigned char j, k, r, g, b;
  
  /* Read appropriate number of bytes, break into rgb & put in map. */
  switch ( Size )
    {
    case 8:				/* Grey scale, read and triplicate. */
      r = g = b = getbyte( ifp );
      break;
      
    case 16:			/* 5 bits each of red green and blue. */
    case 15:			/* Watch for byte order. */
      j = getbyte( ifp );
      k = getbyte( ifp );
      r = ( k & 0x7C ) >> 2;
      g = ( ( k & 0x03 ) << 3 ) + ( ( j & 0xE0 ) >> 5 );
      b = j & 0x1F;
      break;
      
    case 32:
    case 24:			/* 8 bits each of blue green and red. */
      b = getbyte( ifp );
      g = getbyte( ifp );
      r = getbyte( ifp );
      if ( Size == 32 )
	(void) getbyte( ifp );	/* Read alpha byte & throw away. */
      break;
      
    default:
      printf ("unknown colormap pixel size (#2) - %d\n", Size);
    }

  /*  Set the colormap values  */
  color[0] = r;
  color[1] = g;
  color[2] = b;
}

static void
get_pixel( ifp, dest, Size )
    FILE* ifp;
    unsigned char * dest;
    int Size;
{
  static unsigned char Red, Grn, Blu;
  static unsigned int l;
  unsigned char j, k;

  /* Check if run length encoded. */
  if ( rlencoded )
    {
      if ( RLE_count == 0 )
	{ /* Have to restart run. */
	  unsigned char i;
	  i = getbyte( ifp );
	  RLE_flag = ( i & 0x80 );
	  if ( RLE_flag == 0 )
	    /* Stream of unencoded pixels. */
	    RLE_count = i + 1;
	  else
	    /* Single pixel replicated. */
	    RLE_count = i - 127;
	  /* Decrement count & get pixel. */
	  --RLE_count;
	}
      else
	{ /* Have already read count & (at least) first pixel. */
	  --RLE_count;
	  if ( RLE_flag != 0 )
	    /* Replicated pixels. */
	    goto PixEncode;
	}
    }
  /* Read appropriate number of bytes, break into RGB. */
  switch ( Size )
    {
    case 8:				/* Grey scale, read and triplicate. */
      Red = Grn = Blu = l = getbyte( ifp );
      break;
      
    case 16:			/* 5 bits each of red green and blue. */
    case 15:			/* Watch byte order. */
      j = getbyte( ifp );
      k = getbyte( ifp );
      l = ( (unsigned int) k << 8 ) + j;
      if (mapped)
	{
	  Red = ColorMap [l*3 + 0];
	  Grn = ColorMap [l*3 + 1];
	  Blu = ColorMap [l*3 + 2];
	}
      else
	{
	  Red = ( k & 0x7C ) >> 2;
	  Grn = ( ( k & 0x03 ) << 3 ) + ( ( j & 0xE0 ) >> 5 );
	  Blu = j & 0x1F;
	}
      break;
      
    case 32:
    case 24:			/* 8 bits each of blue green and red. */
      Blu = getbyte( ifp );
      Grn = getbyte( ifp );
      Red = getbyte( ifp );
      if ( Size == 32 )
	(void) getbyte( ifp );	/* Read alpha byte & throw away. */
      l = 0;
      break;
      
    default:
      printf ("unknown pixel size (#2) - %d\n", Size);
    }
  
 PixEncode:
  switch (Size)
    {
    case 8 :
      dest[0] = l;
      break;
    case 16 : case 15 : case 32 : case 24 :
      dest[0] = Red;
      dest[1] = Grn;
      dest[2] = Blu;
      break;
    default:
      printf ("unknown pixel size (#2) - %d\n", Size);
    }
}

static unsigned char
getbyte( ifp )
     FILE* ifp;
{
  unsigned char c;
  
  if ( fread( (char*) &c, 1, 1, ifp ) != 1 )
    printf ("EOF / read error\n");
  
  return c;
}


/*****************************************************************/
/*****************************************************************/
/*    Saving targa files                                         */
/*****************************************************************/
/*****************************************************************/


static void
SaveTarga (filename)
     char *filename;
{
  FILE* ofp;
  Image image;
  unsigned char * pixels, * pP;
  unsigned char * cmap;
  int rows, cols, channels;
  int ncolors, row, col, i, realrow;
  int rowstride;
  int* runlength;
  struct ImageHeader tgaHeader;

  rlencoded = 0;

  image = gimp_get_input_image (0);
  rows = gimp_image_height (image);
  cols = gimp_image_width (image);
  channels = gimp_image_channels (image);
  pixels = gimp_image_data (image);
  rowstride = channels * cols;
  
  switch (gimp_image_type (image))
    {
    case INDEXED_IMAGE:
      tgaHeader.ImgType = TGA_Map;
      break;
    case GRAY_IMAGE:    case RGB_IMAGE:
      tgaHeader.ImgType = TGA_RGB;
      break;
    default:
      gimp_message ("gif: cannot operate on unknown image types");
      gimp_quit ();
      break;
    }

  dialog_ID = gimp_new_dialog ("Targa Save Options");
  group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
  rle_ID = gimp_new_check_button (dialog_ID, group_ID, "Run-length Encoding");
  gimp_add_callback (dialog_ID, rle_ID, item_callback, &rlencoded);
  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
  
  if (!gimp_show_dialog (dialog_ID))
    gimp_quit ();

  /* Open the input file. */
  ofp = fopen (filename, "wb");
  if (!ofp)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  if ( rlencoded )
    {
      switch ( tgaHeader.ImgType )
	{
	case TGA_Map:
	  tgaHeader.ImgType = TGA_RLEMap;
	  break;
	case TGA_RGB:
	  tgaHeader.ImgType = TGA_RLERGB;
	  break;
	default:
	  break;
	}
      runlength = (int*) malloc ( sizeof(int) * cols );
    }
    
  tgaHeader.IDLength = 0;
  tgaHeader.Index_lo = 0;
  tgaHeader.Index_hi = 0;
  if ( tgaHeader.ImgType == TGA_Map || tgaHeader.ImgType == TGA_RLEMap )
    {
      cmap = gimp_image_cmap (image);
      ncolors = gimp_image_colors (image);

      tgaHeader.CoMapType = 1;
      tgaHeader.Length_lo = ncolors % 256;
      tgaHeader.Length_hi = ncolors / 256;
      tgaHeader.CoSize = 24;
    }
  else
    {
      tgaHeader.CoMapType = 0;
      tgaHeader.Length_lo = 0;
      tgaHeader.Length_hi = 0;
      tgaHeader.CoSize = 0;
    }

  if ((tgaHeader.ImgType == TGA_RGB || tgaHeader.ImgType == TGA_RLERGB) && channels == 3)
    tgaHeader.PixelSize = 24;
  else
    tgaHeader.PixelSize = 8;

  tgaHeader.X_org_lo = tgaHeader.X_org_hi = 0;
  tgaHeader.Y_org_lo = tgaHeader.Y_org_hi = 0;
  tgaHeader.Width_lo = cols % 256;
  tgaHeader.Width_hi = cols / 256;
  tgaHeader.Height_lo = rows % 256;
  tgaHeader.Height_hi = rows / 256;
  tgaHeader.AttBits = 0;
  tgaHeader.Rsrvd = 0;
  tgaHeader.IntrLve = 0;
  tgaHeader.OrgBit = 0;

  /* Write out the Targa header. */
  writetga(ofp, &tgaHeader, (char*) 0);

  if ( tgaHeader.ImgType == TGA_Map || tgaHeader.ImgType == TGA_RLEMap )
    {
      /* Write out the Targa colormap. */
      for ( i = 0; i < ncolors; ++i )
	put_map_entry(ofp, cmap + i*3);
    }

  /* Write out the pixels */
  for ( row = 0; row < rows; ++row )
    {
      realrow = row;
      if ( tgaHeader.OrgBit == 0 )
	realrow = rows - realrow - 1;
      if ( rlencoded )
	{
	  compute_runlengths( cols, (pixels + realrow*rowstride), channels, runlength);
	  for ( col = 0; col < cols; )
	    {
	      if ( runlength[col] > 0 )
		{
		  fputc ( 0x80 + runlength[col] - 1, ofp );
		  put_pixel(ofp, (pixels + realrow*rowstride + col*channels),
			    tgaHeader.ImgType, channels);
		  col += runlength[col];
		}
	      else if ( runlength[col] < 0 )
		{
		  fputc ( -runlength[col] - 1, ofp );
		  for ( i = 0; i < -runlength[col]; ++i )
		    put_pixel(ofp, (pixels + realrow*rowstride + (col + i)*channels),
			      tgaHeader.ImgType, channels);
		  col += -runlength[col];
		}
	      else
		printf ("error in inner targa save loop\n" );
	    }
	}
      else
	{
	  for ( col = 0, pP = pixels + realrow*rowstride; col < cols; ++col, pP+=channels )
	    put_pixel (ofp, pP, tgaHeader.ImgType, channels);
	}
    }

  fclose (ofp);
  gimp_free_image (image);
  gimp_quit ();
}

static void
writetga(ofp, tgaP, id)
     FILE * ofp;
     struct ImageHeader* tgaP;
     char* id;
{
  unsigned char flags;

  fputc ( tgaP->IDLength, ofp );
  fputc ( tgaP->CoMapType, ofp );
  fputc( tgaP->ImgType, ofp );
  fputc( tgaP->Index_lo, ofp );
  fputc( tgaP->Index_hi, ofp );
  fputc( tgaP->Length_lo, ofp );
  fputc( tgaP->Length_hi, ofp );
  fputc( tgaP->CoSize, ofp );
  fputc( tgaP->X_org_lo, ofp );
  fputc( tgaP->X_org_hi, ofp );
  fputc( tgaP->Y_org_lo, ofp );
  fputc( tgaP->Y_org_hi, ofp );
  fputc( tgaP->Width_lo, ofp );
  fputc( tgaP->Width_hi, ofp );
  fputc( tgaP->Height_lo, ofp );
  fputc( tgaP->Height_hi, ofp );
  fputc( tgaP->PixelSize, ofp );
  flags = ( tgaP->AttBits & 0xf ) | ( ( tgaP->Rsrvd & 0x1 ) << 4 ) |
    ( ( tgaP->OrgBit & 0x1 ) << 5 ) | ( ( tgaP->OrgBit & 0x3 ) << 6 );
  fputc( flags, ofp );
  if ( tgaP->IDLength )
    fwrite( id, 1, (int) tgaP->IDLength, ofp );
}
    
static void
put_map_entry( ofp, valueP )
     FILE * ofp;
     unsigned char * valueP;
{
  fputc (valueP[2], ofp);  /* blue */
  fputc (valueP[1], ofp);  /* green */
  fputc (valueP[0], ofp);  /* red */
}

static void
compute_runlengths( cols, pixelrow, channels, runlength )
     int cols;
     unsigned char * pixelrow;
     int channels;
     int * runlength;
{
  int col, start;

  /* Initialize all run lengths to 0.  (This is just an error check.) */
  for ( col = 0; col < cols; ++col )
    runlength[col] = 0;
    
  /* Find runs of identical pixels. */
  for ( col = 0; col < cols; )
    {
      start = col;
      do 
	{
	  ++col;
	}
      while ((col < cols) && (col - start < 128) && is_equal (pixelrow, col, start, channels));

      runlength[start] = col - start;
    }
  
  /* Now look for runs of length-1 runs, and turn them into negative runs. */
  for ( col = 0; col < cols; )
    {
      if ( runlength[col] == 1 )
	{
	  start = col;
	  while ( col < cols &&
		 col - start < 128 &&
		 runlength[col] == 1 )
	    {
	      runlength[col] = 0;
	      ++col;
	    }
	  runlength[start] = - ( col - start );
	}
      else
	col += runlength[col];
    }
}

static void
put_pixel(ofp, pP, imgtype, channels)
     FILE * ofp;
     unsigned char * pP;
     int imgtype;
     int channels;
{
  int i;

  switch ( imgtype )
    {
    case TGA_Map:
    case TGA_RLEMap:
      fputc (pP[0], ofp);
      break;
    case TGA_RGB:
    case TGA_RLERGB:
      for (i = channels; i > 0; i--)
	fputc (pP[i-1], ofp);
      break;
    default:
      break;
    }
}

static int
is_equal (pP, r1, r2, b)
     unsigned char * pP;
     int r1, r2;
     int b;
{
  r1 *= b;
  r2 *= b;

  while (b--)
    if (pP[r1 + b] != pP[r2 + b]) return 0;

  return 1;
}


/* The End */
