/* GIF loading and saving file filter for the Gimp
 *  -Peter Mattis
 *
 * This filter uses code taken from the "giftopnm" and "ppmtogif" programs
 *  which are part of the "netpbm" package.
 */

/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)    | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gimp.h"

#define MAXCOLORMAPSIZE  256

#define TRUE             1
#define FALSE            0

#define CM_RED           0
#define CM_GREEN         1
#define CM_BLUE          2

#define MAX_LWZ_BITS     12

#define INTERLACE          0x40
#define LOCALCOLORMAP      0x80
#define BitSet(byte, bit)  (((byte) & (bit)) == (bit))

#define ReadOK(file,buffer,len) (fread(buffer, len, 1, file) != 0)
#define LM_to_uint(a,b)         (((b)<<8)|(a))

#define GRAYSCALE        1
#define COLOR            2

typedef unsigned char CMap[3][MAXCOLORMAPSIZE];

static struct
{
  unsigned int Width;
  unsigned int Height;
  CMap ColorMap;
  unsigned int BitPixel;
  unsigned int ColorResolution;
  unsigned int Background;
  unsigned int AspectRatio;
  /*
   **
   */
  int GrayScale;
} GifScreen;

static struct
{
  int transparent;
  int delayTime;
  int inputFlag;
  int disposal;
} Gif89 = { -1, -1, -1, 0 };

int verbose;
int showComment;


static void ReadGIF (char *);
static int ReadColorMap (FILE *, int, CMap, int *);
static int DoExtension (FILE *, int);
static int GetDataBlock (FILE *, unsigned char *);
static int GetCode (FILE *, int, int);
static int LWZReadByte (FILE *, int, int);
static void ReadImage (FILE *, int, int, CMap, int, int, int, int);

static void encode (char*);

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
      gimp_install_load_save_handlers (ReadGIF, encode);

      /* Run until something happens. That something could be getting
       *  a 'QUIT' message or getting a load or save message.
       */
      gimp_main_loop ();
    }

  return 0;
}

static void
ReadGIF (name)
     char *name;
{
  FILE *fd;
  unsigned char *temp_buf;
  unsigned char buf[16];
  unsigned char c;
  CMap localColorMap;
  int grayScale;
  int useGlobalColormap;
  int bitPixel;
  int imageCount = 0;
  char version[4];

  temp_buf = malloc (strlen (name) + 11);
  if (!temp_buf)
    gimp_quit ();

  sprintf (temp_buf, "Loading %s:", name);
  gimp_init_progress (temp_buf);
  free (temp_buf);

  filename = name;
  fd = fopen (filename, "rb");
  if (!fd)
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  if (!ReadOK (fd, buf, 6))
    {
      printf ("%s: error reading magic number\n", prog_name);
      gimp_quit ();
    }

  if (strncmp ((char *) buf, "GIF", 3) != 0)
    {
      printf ("%s: not a GIF file\n", prog_name);
      gimp_quit ();
    }

  strncpy (version, (char *) buf + 3, 3);
  version[3] = '\0';

  if ((strcmp (version, "87a") != 0) && (strcmp (version, "89a") != 0))
    {
      printf ("%s: bad version number, not '87a' or '89a'\n", prog_name);
      gimp_quit ();
    }

  if (!ReadOK (fd, buf, 7))
    {
      printf ("%s: failed to read screen descriptor\n", prog_name);
      gimp_quit ();
    }

  GifScreen.Width = LM_to_uint (buf[0], buf[1]);
  GifScreen.Height = LM_to_uint (buf[2], buf[3]);
  GifScreen.BitPixel = 2 << (buf[4] & 0x07);
  GifScreen.ColorResolution = (((buf[4] & 0x70) >> 3) + 1);
  GifScreen.Background = buf[5];
  GifScreen.AspectRatio = buf[6];

  if (BitSet (buf[4], LOCALCOLORMAP))
    {
      /* Global Colormap */
      if (ReadColorMap (fd, GifScreen.BitPixel, GifScreen.ColorMap, &GifScreen.GrayScale))
	{
	  printf ("%s: error reading global colormap\n", prog_name);
	  gimp_quit ();
	}
    }

  if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49)
    {
      printf ("%s: warning - non-square pixels\n", prog_name);
    }

  for (;;)
    {
      if (!ReadOK (fd, &c, 1))
	{
	  printf ("%s: EOF / read error on image data\n", prog_name);
	  gimp_quit ();
	}

      if (c == ';')
	{
	  /* GIF terminator */
	  return;
	}

      if (c == '!')
	{			/* Extension */
	  if (!ReadOK (fd, &c, 1))
	    {
	      printf ("%s: OF / read error on extention function code\n", prog_name);
	      gimp_quit ();
	    }
	  DoExtension (fd, c);
	  continue;
	}

      if (c != ',')
	{
	  /* Not a valid start character */
	  printf ("%s: bogus character 0x%02x, ignoring\n", prog_name, (int) c);
	  continue;
	}

      ++imageCount;

      if (!ReadOK (fd, buf, 9))
	{
	  printf ("%s: couldn't read left/top/width/height\n", prog_name);
	  gimp_quit ();
	}

      useGlobalColormap = !BitSet (buf[8], LOCALCOLORMAP);

      bitPixel = 1 << ((buf[8] & 0x07) + 1);

      if (!useGlobalColormap)
	{
	  if (ReadColorMap (fd, bitPixel, localColorMap, &grayScale))
	    {
	      printf ("%s: error reading local colormap\n", prog_name);
	      gimp_quit ();
	    }
	  ReadImage (fd, LM_to_uint (buf[4], buf[5]),
		     LM_to_uint (buf[6], buf[7]),
		     localColorMap, grayScale, bitPixel,
		     BitSet (buf[8], INTERLACE), imageCount);
	}
      else
	{
	  ReadImage (fd, LM_to_uint (buf[4], buf[5]),
		     LM_to_uint (buf[6], buf[7]),
		     GifScreen.ColorMap, GifScreen.BitPixel, GifScreen.GrayScale,
		     BitSet (buf[8], INTERLACE), imageCount);
	}

    }
}

static int
ReadColorMap (fd, number, buffer, format)
     FILE *fd;
     int number;
     CMap buffer;
     int *format;
{
  int i;
  unsigned char rgb[3];
  int flag;

  flag = TRUE;

  for (i = 0; i < number; ++i)
    {
      if (!ReadOK (fd, rgb, sizeof (rgb)))
	{
	  printf ("%s: bad colormap\n", prog_name);
	  gimp_quit ();
	}

      buffer[CM_RED][i] = rgb[0];
      buffer[CM_GREEN][i] = rgb[1];
      buffer[CM_BLUE][i] = rgb[2];

      flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
    }

  *format = (flag) ? GRAYSCALE : COLOR;

  return FALSE;
}

static int
DoExtension (fd, label)
     FILE *fd;
     int label;
{
  static char buf[256];
  char *str;

  switch (label)
    {
    case 0x01:			/* Plain Text Extension */
      str = "Plain Text Extension";
#ifdef notdef
      if (GetDataBlock (fd, (unsigned char *) buf) == 0)
	;

      lpos = LM_to_uint (buf[0], buf[1]);
      tpos = LM_to_uint (buf[2], buf[3]);
      width = LM_to_uint (buf[4], buf[5]);
      height = LM_to_uint (buf[6], buf[7]);
      cellw = buf[8];
      cellh = buf[9];
      foreground = buf[10];
      background = buf[11];

      while (GetDataBlock (fd, (unsigned char *) buf) != 0)
	{
	  PPM_ASSIGN (image[ypos][xpos],
		      cmap[CM_RED][v],
		      cmap[CM_GREEN][v],
		      cmap[CM_BLUE][v]);
	  ++index;
	}

      return FALSE;
#else
      break;
#endif
    case 0xff:			/* Application Extension */
      str = "Application Extension";
      break;
    case 0xfe:			/* Comment Extension */
      str = "Comment Extension";
      while (GetDataBlock (fd, (unsigned char *) buf) != 0)
	{
	  if (showComment)
	    printf ("%s: gif comment: %s\n", prog_name, buf);
	}
      return FALSE;
    case 0xf9:			/* Graphic Control Extension */
      str = "Graphic Control Extension";
      (void) GetDataBlock (fd, (unsigned char *) buf);
      Gif89.disposal = (buf[0] >> 2) & 0x7;
      Gif89.inputFlag = (buf[0] >> 1) & 0x1;
      Gif89.delayTime = LM_to_uint (buf[1], buf[2]);
      if ((buf[0] & 0x1) != 0)
	Gif89.transparent = buf[3];

      while (GetDataBlock (fd, (unsigned char *) buf) != 0)
	;
      return FALSE;
    default:
      str = buf;
      sprintf (buf, "UNKNOWN (0x%02x)", label);
      break;
    }

  printf ("%s: got a '%s' extension\n", prog_name, str);

  while (GetDataBlock (fd, (unsigned char *) buf) != 0)
    ;

  return FALSE;
}

int ZeroDataBlock = FALSE;

static int
GetDataBlock (fd, buf)
     FILE *fd;
     unsigned char *buf;
{
  unsigned char count;

  if (!ReadOK (fd, &count, 1))
    {
      printf ("%s: error in getting DataBlock size\n", prog_name);
      return -1;
    }

  ZeroDataBlock = count == 0;

  if ((count != 0) && (!ReadOK (fd, buf, count)))
    {
      printf ("%s: error in reading DataBlock\n", prog_name);
      return -1;
    }

  return count;
}

static int
GetCode (fd, code_size, flag)
     FILE *fd;
     int code_size;
     int flag;
{
  static unsigned char buf[280];
  static int curbit, lastbit, done, last_byte;
  int i, j, ret;
  unsigned char count;

  if (flag)
    {
      curbit = 0;
      lastbit = 0;
      done = FALSE;
      return 0;
    }

  if ((curbit + code_size) >= lastbit)
    {
      if (done)
	{
	  if (curbit >= lastbit)
	    {
	      printf ("%s: ran off the end of by bits\n", prog_name);
	      gimp_quit ();
	    }
	  return -1;
	}
      buf[0] = buf[last_byte - 2];
      buf[1] = buf[last_byte - 1];

      if ((count = GetDataBlock (fd, &buf[2])) == 0)
	done = TRUE;

      last_byte = 2 + count;
      curbit = (curbit - lastbit) + 16;
      lastbit = (2 + count) * 8;
    }

  ret = 0;
  for (i = curbit, j = 0; j < code_size; ++i, ++j)
    ret |= ((buf[i / 8] & (1 << (i % 8))) != 0) << j;

  curbit += code_size;

  return ret;
}

static int
LWZReadByte (fd, flag, input_code_size)
     FILE *fd;
     int flag;
     int input_code_size;
{
  static int fresh = FALSE;
  int code, incode;
  static int code_size, set_code_size;
  static int max_code, max_code_size;
  static int firstcode, oldcode;
  static int clear_code, end_code;
  static int table[2][(1 << MAX_LWZ_BITS)];
  static int stack[(1 << (MAX_LWZ_BITS)) * 2], *sp;
  register int i;

  if (flag)
    {
      set_code_size = input_code_size;
      code_size = set_code_size + 1;
      clear_code = 1 << set_code_size;
      end_code = clear_code + 1;
      max_code_size = 2 * clear_code;
      max_code = clear_code + 2;

      GetCode (fd, 0, TRUE);

      fresh = TRUE;

      for (i = 0; i < clear_code; ++i)
	{
	  table[0][i] = 0;
	  table[1][i] = i;
	}
      for (; i < (1 << MAX_LWZ_BITS); ++i)
	table[0][i] = table[1][0] = 0;

      sp = stack;

      return 0;
    }
  else if (fresh)
    {
      fresh = FALSE;
      do
	{
	  firstcode = oldcode =
	    GetCode (fd, code_size, FALSE);
	}
      while (firstcode == clear_code);
      return firstcode;
    }

  if (sp > stack)
    return *--sp;

  while ((code = GetCode (fd, code_size, FALSE)) >= 0)
    {
      if (code == clear_code)
	{
	  for (i = 0; i < clear_code; ++i)
	    {
	      table[0][i] = 0;
	      table[1][i] = i;
	    }
	  for (; i < (1 << MAX_LWZ_BITS); ++i)
	    table[0][i] = table[1][i] = 0;
	  code_size = set_code_size + 1;
	  max_code_size = 2 * clear_code;
	  max_code = clear_code + 2;
	  sp = stack;
	  firstcode = oldcode =
	    GetCode (fd, code_size, FALSE);
	  return firstcode;
	}
      else if (code == end_code)
	{
	  int count;
	  unsigned char buf[260];

	  if (ZeroDataBlock)
	    return -2;

	  while ((count = GetDataBlock (fd, buf)) > 0)
	    ;

	  if (count != 0)
	    printf ("%s: missing EOD in data stream (common occurence)", prog_name);
	  return -2;
	}

      incode = code;

      if (code >= max_code)
	{
	  *sp++ = firstcode;
	  code = oldcode;
	}

      while (code >= clear_code)
	{
	  *sp++ = table[1][code];
	  if (code == table[0][code])
	    {
	      printf ("%s: circular table entry BIG ERROR\n", prog_name);
	      gimp_quit ();
	    }
	  code = table[0][code];
	}

      *sp++ = firstcode = table[1][code];

      if ((code = max_code) < (1 << MAX_LWZ_BITS))
	{
	  table[0][code] = oldcode;
	  table[1][code] = firstcode;
	  ++max_code;
	  if ((max_code >= max_code_size) &&
	      (max_code_size < (1 << MAX_LWZ_BITS)))
	    {
	      max_code_size *= 2;
	      ++code_size;
	    }
	}

      oldcode = incode;

      if (sp > stack)
	return *--sp;
    }
  return code;
}

static void
ReadImage (fd, len, height, cmap, ncols, format, interlace, number)
     FILE *fd;
     int len, height;
     CMap cmap;
     int ncols;
     int format, interlace, number;
{
  char *name_buf;
  unsigned char c;
  int v;
  int xpos = 0, ypos = 0, pass = 0;
  Image image;
  unsigned char *dest, *temp;
  unsigned char gimp_cmap[768];
  long rowstride, channels;
  int i, j, cur_progress, max_progress;

  /*
   **  Initialize the Compression routines
   */
  if (!ReadOK (fd, &c, 1))
    {
      printf ("%s: EOF / read error on image data\n", prog_name);
      gimp_quit ();
    }

  if (LWZReadByte (fd, TRUE, c) < 0)
    {
      printf ("%s: error reading image\n", prog_name);
      gimp_quit ();
    }

  name_buf = malloc (strlen (filename) + 10);
  if (!name_buf)
    gimp_quit ();

  if (number > 1)
    sprintf (name_buf, "%s-%d", filename, number);
  else
    sprintf (name_buf, "%s", filename);

  image = gimp_new_image (name_buf, len, height, INDEXED_IMAGE);

  /*  free up the allocated buffer  */
  free (name_buf);

  dest = gimp_image_data (image);
  channels = gimp_image_channels (image);
  rowstride = gimp_image_width (image) * channels;
  
  cur_progress = 0;
  max_progress = height;

  if (verbose)
    printf ("%s: reading %d by %d%s GIF image\n", prog_name,
	    len, height, interlace ? " interlaced" : "");

  while ((v = LWZReadByte (fd, FALSE, c)) >= 0)
    {
      temp = dest + (ypos * rowstride) + (xpos * channels);
      *temp = v;

      ++xpos;
      if (xpos == len)
	{
	  xpos = 0;
	  if (interlace)
	    {
	      switch (pass)
		{
		case 0:
		case 1:
		  ypos += 8;
		  break;
		case 2:
		  ypos += 4;
		  break;
		case 3:
		  ypos += 2;
		  break;
		}

	      if (ypos >= height)
		{
		  ++pass;
		  switch (pass)
		    {
		    case 1:
		      ypos = 4;
		      break;
		    case 2:
		      ypos = 2;
		      break;
		    case 3:
		      ypos = 1;
		      break;
		    default:
		      goto fini;
		    }
		}
	    }
	  else
	    {
	      ++ypos;
	    }

	  if ((++cur_progress % 5) == 0)
	    gimp_do_progress (cur_progress, max_progress);
	}
      if (ypos >= height)
	break;
    }

fini:
  if (LWZReadByte (fd, FALSE, c) >= 0)
    printf ("%s: too much input data, ignoring extra...\n", prog_name);

  for (i = 0, j = 0; i < ncols; i++)
    {
      gimp_cmap[j++] = cmap[0][i];
      gimp_cmap[j++] = cmap[1][i];
      gimp_cmap[j++] = cmap[2][i];
    }

  gimp_do_progress (1, 1);
  gimp_set_image_colors (image, gimp_cmap, ncols);
  gimp_display_image (image);
  gimp_update_image (image);
  gimp_free_image (image);

  gimp_quit ();
}



/* ppmtogif.c - read a portable pixmap and produce a GIF file
**
** Based on GIFENCOD by David Rowley <mgardi@watdscu.waterloo.edu>.A
** Lempel-Zim compression based on "compress".
**
** Modified by Marcel Wijkstra <wijkstra@fwi.uva.nl>
**
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** The Graphics Interchange Format(c) is the Copyright property of
** CompuServe Incorporated.  GIF(sm) is a Service Mark property of
** CompuServe Incorporated.
*/

#define MAXCOLORS 256

/*
 * Pointer to function returning an int
 */
typedef int (*ifunptr) (int, int);

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
typedef int code_int;

#ifdef SIGNED_COMPARE_SLOW
typedef unsigned long int count_int;
typedef unsigned short int count_short;
#else /*SIGNED_COMPARE_SLOW */
typedef long int count_int;
#endif /*SIGNED_COMPARE_SLOW */

static int colorstobpp (int);
static int GetPixel (int, int);
static void BumpPixel (void);
static int GIFNextPixel (ifunptr);
static void GIFEncode (FILE *, int, int, int, int, int, int, int *, int *, int *, ifunptr);

long rowstride;
unsigned char *pixels;
int cur_progress;
int max_progress;

static void Putword (int, FILE *);
static void compress (int, FILE *, ifunptr);
static void output (code_int);
static void cl_block (void);
static void cl_hash (count_int);
static void writeerr (void);
static void char_init (void);
static void char_out (int);
static void flush_char (void);

static void item_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);

static int dialog_ID;
static int group_ID;
static int interlace_ID;
static int transparency_ID;

void
encode (filename)
     char *filename;
{
  FILE *outfile;
  Image image;
  int Red[MAXCOLORS];
  int Green[MAXCOLORS];
  int Blue[MAXCOLORS];
  unsigned char *cmap;
  int rows, cols;
  long BitsPerPixel;
  long colors;
  long interlace;
  long transparent;
  char *temp_buf;
  int i;

  image = gimp_get_input_image (0);
  switch (gimp_image_type (image))
    {
    case RGB_IMAGE:
      gimp_message ("gif: cannot operate on RGB images");
      gimp_quit ();
      break;
    case GRAY_IMAGE:
    case INDEXED_IMAGE:
      break;
    default:
      gimp_message ("gif: cannot operate on unknown image types");
      gimp_quit ();
      break;
    }

  interlace = 0;
  transparent = 0;

  dialog_ID = gimp_new_dialog ("Gif Save Options");
  group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");
  interlace_ID = gimp_new_check_button (dialog_ID, group_ID, "Interlace");
  transparency_ID = gimp_new_check_button (dialog_ID, group_ID, "Transparency");
  gimp_add_callback (dialog_ID, interlace_ID, item_callback, &interlace);
  gimp_add_callback (dialog_ID, transparency_ID, item_callback, &transparent);
  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
  
  if (!gimp_show_dialog (dialog_ID))
    gimp_quit ();

  temp_buf = malloc (strlen (filename) + 11);
  if (!temp_buf)
    gimp_quit ();

  sprintf (temp_buf, "Saving %s:", filename);
  gimp_init_progress (temp_buf);
  free (temp_buf);

  cur_progress = 0;
  max_progress = gimp_image_height (image);

  if (!transparent)
    transparent = -1;
  
  switch (gimp_image_type (image))
    {
    case GRAY_IMAGE:
      colors = 256;
      for (i = 0; i < colors; i++)
	{
	  Red[i] = i;
	  Green[i] = i;
	  Blue[i] = i;
	}
      break;
    case INDEXED_IMAGE:
      cmap = gimp_image_cmap (image);
      colors = gimp_image_colors (image);

      for (i = 0; i < colors; i++)
	{
	  Red[i] = *cmap++;
	  Green[i] = *cmap++;
	  Blue[i] = *cmap++;
	}
      for ( ; i < 256; i++)
	{
	  Red[i] = 0;
	  Green[i] = 0;
	  Blue[i] = 0;
	}
      break;
    default:
      fprintf (stderr, "%s: you should not receive this error for any reason\n", prog_name);
      break;
    }

  outfile = fopen (filename, "wb");
  if (!outfile)
    {
      fprintf (stderr, "can't open %s\n", filename);
      gimp_quit ();
    }
  
  pixels = gimp_image_data (image);
  cols = gimp_image_width (image);
  rows = gimp_image_height (image);
  rowstride = gimp_image_width (image);

  BitsPerPixel = colorstobpp (colors);
  BitsPerPixel = 8;

  GIFEncode (outfile, cols, rows, interlace, 0, transparent, 
	     BitsPerPixel, Red, Green, Blue, GetPixel);

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

static int
colorstobpp (colors)
     int colors;
{
  int bpp;

  if (colors <= 2)
    bpp = 1;
  else if (colors <= 4)
    bpp = 2;
  else if (colors <= 8)
    bpp = 3;
  else if (colors <= 16)
    bpp = 4;
  else if (colors <= 32)
    bpp = 5;
  else if (colors <= 64)
    bpp = 6;
  else if (colors <= 128)
    bpp = 7;
  else if (colors <= 256)
    bpp = 8;
  else
    {
      printf ("%s: too many colors: %d\n", prog_name, colors);
      gimp_quit ();
    }

  return bpp;
}


static int
GetPixel (x, y)
     int x, y;
{
  return *(pixels + (rowstride * (long) y) + (long) x);
}


/*****************************************************************************
 *
 * GIFENCODE.C    - GIF Image compression interface
 *
 * GIFEncode( FName, GHeight, GWidth, GInterlace, Background, Transparent,
 *            BitsPerPixel, Red, Green, Blue, GetPixel )
 *
 *****************************************************************************/

#define TRUE 1
#define FALSE 0

static int Width, Height;
static int curx, cury;
static long CountDown;
static int Pass = 0;
static int Interlace;

/*
 * Bump the 'curx' and 'cury' to point to the next pixel
 */
static void
BumpPixel ()
{
  /*
   * Bump the current X position
   */
  ++curx;

  /*
   * If we are at the end of a scan line, set curx back to the beginning
   * If we are interlaced, bump the cury to the appropriate spot,
   * otherwise, just increment it.
   */
  if (curx == Width)
    {
      if ((++cur_progress % 5) == 0)
	gimp_do_progress (cur_progress, max_progress);
      
      curx = 0;

      if (!Interlace)
	++cury;
      else
	{
	  switch (Pass)
	    {

	    case 0:
	      cury += 8;
	      if (cury >= Height)
		{
		  ++Pass;
		  cury = 4;
		}
	      break;

	    case 1:
	      cury += 8;
	      if (cury >= Height)
		{
		  ++Pass;
		  cury = 2;
		}
	      break;

	    case 2:
	      cury += 4;
	      if (cury >= Height)
		{
		  ++Pass;
		  cury = 1;
		}
	      break;

	    case 3:
	      cury += 2;
	      break;
	    }
	}
    }
}

/*
 * Return the next pixel from the image
 */
static int
GIFNextPixel (getpixel)
     ifunptr getpixel;
{
  int r;

  if (CountDown == 0)
    return EOF;

  --CountDown;

  r = (*getpixel) (curx, cury);

  BumpPixel ();

  return r;
}

/* public */

static void
GIFEncode (fp, GWidth, GHeight, GInterlace, Background, Transparent,
	   BitsPerPixel, Red, Green, Blue, GetPixel)
     FILE *fp;
     int GWidth, GHeight;
     int GInterlace;
     int Background;
     int Transparent;
     int BitsPerPixel;
     int Red[], Green[], Blue[];
     ifunptr GetPixel;
{
  int B;
  int RWidth, RHeight;
  int LeftOfs, TopOfs;
  int Resolution;
  int ColorMapSize;
  int InitCodeSize;
  int i;

  Interlace = GInterlace;

  ColorMapSize = 1 << BitsPerPixel;

  RWidth = Width = GWidth;
  RHeight = Height = GHeight;
  LeftOfs = TopOfs = 0;

  Resolution = BitsPerPixel;

  /*
   * Calculate number of bits we are expecting
   */
  CountDown = (long) Width *(long) Height;

  /*
   * Indicate which pass we are on (if interlace)
   */
  Pass = 0;

  /*
   * The initial code size
   */
  if (BitsPerPixel <= 1)
    InitCodeSize = 2;
  else
    InitCodeSize = BitsPerPixel;

  /*
   * Set up the current x and y position
   */
  curx = cury = 0;

  /*
   * Write the Magic header
   */
  fwrite (Transparent < 0 ? "GIF87a" : "GIF89a", 1, 6, fp);

  /*
   * Write out the screen width and height
   */
  Putword (RWidth, fp);
  Putword (RHeight, fp);

  /*
   * Indicate that there is a global colour map
   */
  B = 0x80;			/* Yes, there is a color map */

  /*
   * OR in the resolution
   */
  B |= (Resolution - 1) << 5;

  /*
   * OR in the Bits per Pixel
   */
  B |= (BitsPerPixel - 1);

  /*
   * Write it out
   */
  fputc (B, fp);

  /*
   * Write out the Background colour
   */
  fputc (Background, fp);

  /*
   * Byte of 0's (future expansion)
   */
  fputc (0, fp);

  /*
   * Write out the Global Colour Map
   */
  for (i = 0; i < ColorMapSize; ++i)
    {
      fputc (Red[i], fp);
      fputc (Green[i], fp);
      fputc (Blue[i], fp);
    }

  /*
   * Write out extension for transparent colour index, if necessary.
   */
  if (Transparent >= 0)
    {
      fputc ('!', fp);
      fputc (0xf9, fp);
      fputc (4, fp);
      fputc (1, fp);
      fputc (0, fp);
      fputc (0, fp);
      fputc (Transparent, fp);
      fputc (0, fp);
    }

  /*
   * Write an Image separator
   */
  fputc (',', fp);

  /*
   * Write the Image header
   */

  Putword (LeftOfs, fp);
  Putword (TopOfs, fp);
  Putword (Width, fp);
  Putword (Height, fp);

  /*
   * Write out whether or not the image is interlaced
   */
  if (Interlace)
    fputc (0x40, fp);
  else
    fputc (0x00, fp);

  /*
   * Write out the initial code size
   */
  fputc (InitCodeSize, fp);

  /*
   * Go and actually compress the data
   */
  compress (InitCodeSize + 1, fp, GetPixel);

  /*
   * Write out a Zero-length packet (to end the series)
   */
  fputc (0, fp);

  /*
   * Write the GIF file terminator
   */
  fputc (';', fp);

  /*
   * And close the file
   */
  fclose (fp);
}

/*
 * Write out a word to the GIF file
 */
static void
Putword (w, fp)
     int w;
     FILE *fp;
{
  fputc (w & 0xff, fp);
  fputc ((w / 256) & 0xff, fp);
}


/***************************************************************************
 *
 *  GIFCOMPR.C       - GIF Image compression routines
 *
 *  Lempel-Ziv compression based on 'compress'.  GIF modifications by
 *  David Rowley (mgardi@watdcsu.waterloo.edu)
 *
 ***************************************************************************/

/*
 * General DEFINEs
 */

#define BITS    12

#define HSIZE  5003		/* 80% occupancy */

#ifdef NO_UCHAR
typedef char char_type;
#else /*NO_UCHAR */
typedef unsigned char char_type;
#endif /*NO_UCHAR */

/*

 * GIF Image compression - modified 'compress'
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 * By Authors:  Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */
#include <ctype.h>

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

static int n_bits;		/* number of bits/code */
static int maxbits = BITS;	/* user settable max # bits/code */
static code_int maxcode;	/* maximum code, given n_bits */
static code_int maxmaxcode = (code_int) 1 << BITS;	/* should NEVER generate this code */
#ifdef COMPATIBLE		/* But wrong! */
#define MAXCODE(n_bits)        ((code_int) 1 << (n_bits) - 1)
#else /*COMPATIBLE */
#define MAXCODE(n_bits)        (((code_int) 1 << (n_bits)) - 1)
#endif /*COMPATIBLE */

static count_int htab[HSIZE];
static unsigned short codetab[HSIZE];
#define HashTabOf(i)       htab[i]
#define CodeTabOf(i)    codetab[i]

static code_int hsize = HSIZE;	/* for dynamic table sizing */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i) CodeTabOf(i)
#define tab_suffixof(i)        ((char_type*)(htab))[i]
#define de_stack               ((char_type*)&tab_suffixof((code_int)1<<BITS))

static code_int free_ent = 0;	/* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int clear_flg = 0;

static int offset;
static long int in_count = 1;	/* length of input */
static long int out_count = 0;	/* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static FILE *g_outfile;

static int ClearCode;
static int EOFCode;

static void
compress (init_bits, outfile, ReadValue)
     int init_bits;
     FILE *outfile;
     ifunptr ReadValue;
{
  register long fcode;
  register code_int i /* = 0 */ ;
  register int c;
  register code_int ent;
  register code_int disp;
  register code_int hsize_reg;
  register int hshift;

  /*
   * Set up the globals:  g_init_bits - initial number of bits
   *                      g_outfile   - pointer to output file
   */
  g_init_bits = init_bits;
  g_outfile = outfile;

  /*
   * Set up the necessary values
   */
  offset = 0;
  out_count = 0;
  clear_flg = 0;
  in_count = 1;
  maxcode = MAXCODE (n_bits = g_init_bits);

  ClearCode = (1 << (init_bits - 1));
  EOFCode = ClearCode + 1;
  free_ent = ClearCode + 2;

  char_init ();

  ent = GIFNextPixel (ReadValue);

  hshift = 0;
  for (fcode = (long) hsize; fcode < 65536L; fcode *= 2L)
    ++hshift;
  hshift = 8 - hshift;		/* set hash code range bound */

  hsize_reg = hsize;
  cl_hash ((count_int) hsize_reg);	/* clear hash table */

  output ((code_int) ClearCode);

#ifdef SIGNED_COMPARE_SLOW
  while ((c = GIFNextPixel (ReadValue)) != (unsigned) EOF)
    {
#else /*SIGNED_COMPARE_SLOW */
  while ((c = GIFNextPixel (ReadValue)) != EOF)
    {				/* } */
#endif /*SIGNED_COMPARE_SLOW */

      ++in_count;

      fcode = (long) (((long) c << maxbits) + ent);
      i = (((code_int) c << hshift) ^ ent);	/* xor hashing */

      if (HashTabOf (i) == fcode)
	{
	  ent = CodeTabOf (i);
	  continue;
	}
      else if ((long) HashTabOf (i) < 0)	/* empty slot */
	goto nomatch;
      disp = hsize_reg - i;	/* secondary hash (after G. Knott) */
      if (i == 0)
	disp = 1;
    probe:
      if ((i -= disp) < 0)
	i += hsize_reg;

      if (HashTabOf (i) == fcode)
	{
	  ent = CodeTabOf (i);
	  continue;
	}
      if ((long) HashTabOf (i) > 0)
	goto probe;
    nomatch:
      output ((code_int) ent);
      ++out_count;
      ent = c;
#ifdef SIGNED_COMPARE_SLOW
      if ((unsigned) free_ent < (unsigned) maxmaxcode)
	{
#else /*SIGNED_COMPARE_SLOW */
      if (free_ent < maxmaxcode)
	{			/* } */
#endif /*SIGNED_COMPARE_SLOW */
	  CodeTabOf (i) = free_ent++;	/* code -> hashtable */
	  HashTabOf (i) = fcode;
	}
      else
	cl_block ();
    }
  /*
   * Put out the final code.
   */
  output ((code_int) ent);
  ++out_count;
  output ((code_int) EOFCode);
}

/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static unsigned long cur_accum = 0;
static int cur_bits = 0;

static unsigned long masks[] =
{0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
 0x001F, 0x003F, 0x007F, 0x00FF,
 0x01FF, 0x03FF, 0x07FF, 0x0FFF,
 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF};

static void
output (code)
     code_int code;
{
  cur_accum &= masks[cur_bits];

  if (cur_bits > 0)
    cur_accum |= ((long) code << cur_bits);
  else
    cur_accum = code;

  cur_bits += n_bits;

  while (cur_bits >= 8)
    {
      char_out ((unsigned int) (cur_accum & 0xff));
      cur_accum >>= 8;
      cur_bits -= 8;
    }

  /*
   * If the next entry is going to be too big for the code size,
   * then increase it, if possible.
   */
  if (free_ent > maxcode || clear_flg)
    {

      if (clear_flg)
	{

	  maxcode = MAXCODE (n_bits = g_init_bits);
	  clear_flg = 0;

	}
      else
	{

	  ++n_bits;
	  if (n_bits == maxbits)
	    maxcode = maxmaxcode;
	  else
	    maxcode = MAXCODE (n_bits);
	}
    }

  if (code == EOFCode)
    {
      /*
       * At EOF, write the rest of the buffer.
       */
      while (cur_bits > 0)
	{
	  char_out ((unsigned int) (cur_accum & 0xff));
	  cur_accum >>= 8;
	  cur_bits -= 8;
	}

      flush_char ();

      fflush (g_outfile);

      if (ferror (g_outfile))
	writeerr ();
    }
}

/*
 * Clear out the hash table
 */
static void
cl_block ()			/* table clear for block compress */
{

  cl_hash ((count_int) hsize);
  free_ent = ClearCode + 2;
  clear_flg = 1;

  output ((code_int) ClearCode);
}

static void
cl_hash (hsize)			/* reset code table */
     register count_int hsize;
{

  register count_int *htab_p = htab + hsize;

  register long i;
  register long m1 = -1;

  i = hsize - 16;
  do
    {				/* might use Sys V memset(3) here */
      *(htab_p - 16) = m1;
      *(htab_p - 15) = m1;
      *(htab_p - 14) = m1;
      *(htab_p - 13) = m1;
      *(htab_p - 12) = m1;
      *(htab_p - 11) = m1;
      *(htab_p - 10) = m1;
      *(htab_p - 9) = m1;
      *(htab_p - 8) = m1;
      *(htab_p - 7) = m1;
      *(htab_p - 6) = m1;
      *(htab_p - 5) = m1;
      *(htab_p - 4) = m1;
      *(htab_p - 3) = m1;
      *(htab_p - 2) = m1;
      *(htab_p - 1) = m1;
      htab_p -= 16;
    }
  while ((i -= 16) >= 0);

  for (i += 16; i > 0; --i)
    *--htab_p = m1;
}

static void
writeerr ()
{
  printf ("%s: error writing output file\n", prog_name);
  gimp_quit ();
}

/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

/*
 * Number of characters so far in this 'packet'
 */
static int a_count;

/*
 * Set up the 'byte output' routine
 */
static void
char_init ()
{
  a_count = 0;
}

/*
 * Define the storage for the packet accumulator
 */
static char accum[256];

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void
char_out (c)
     int c;
{
  accum[a_count++] = c;
  if (a_count >= 254)
    flush_char ();
}

/*
 * Flush the packet to disk, and reset the accumulator
 */
static void
flush_char ()
{
  if (a_count > 0)
    {
      fputc (a_count, g_outfile);
      fwrite (accum, 1, a_count, g_outfile);
      a_count = 0;
    }
}

/* The End */
