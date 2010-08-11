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
 * Allows netpbm filters to be run from within the GIMP
 * GIMP images are used as input and output.  To run the filters,
 * the GIMP images are converted to either pgm or ppm and
 * saved to disk and supplied as inputs to the specified netpbm
 * filter.  The called filter's output is then converted to GIMP format
 *   Following is Jef Poskanzer's copyright for netpbm...
 */

/* netpbm utilities
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gimp.h"

/* Magic constants. */

#define PGM_MAGIC1 'P'
#define PGM_MAGIC2 '2'
#define RPGM_MAGIC2 '5'
#define PGM_FORMAT (PGM_MAGIC1 * 256 + PGM_MAGIC2)
#define RPGM_FORMAT (PGM_MAGIC1 * 256 + RPGM_MAGIC2)

#define PPM_MAGIC1 'P'
#define PPM_MAGIC2 '3'
#define RPPM_MAGIC2 '6'
#define PPM_FORMAT (PPM_MAGIC1 * 256 + PPM_MAGIC2)
#define RPPM_FORMAT (PPM_MAGIC1 * 256 + RPPM_MAGIC2)

#define MAXBUF 4096

/* Declare local functions.
 */
static void    image_menu_callback (int, void *, void *);
static void    text_callback (int, void *, void *);
static void    toggle_callback (int, void *, void *);
static void    ok_callback (int, void *, void *);
static void    cancel_callback (int, void *, void *);
static int     execute (Image, Image, Image, char *, char *);
static int     save_pgm (Image, char *);
static int     save_ppm (Image, char *);
static int     convert_to_netpbm (Image, char *);
static int     load_pgm (Image, FILE *, int);
static int     load_ppm (Image, FILE *, int);
static char    pbm_getc (FILE *);
static int     pbm_getint (FILE *);
static int     pbm_readmagicnumber (FILE *);
static Image   convert_from_netpbm (FILE *);
static void    remove_netpbm (char *);
static char *  get_temp_name (int);

static char *prog_name;

static int dialog_ID;

int
main (argc, argv)
     int argc;
     char **argv;
{
  Image src1, src2, src3;
  char netpbm_buf [MAXBUF];
  char param_buf [MAXBUF];
  char * cached_params;
  int group_ID, frame_ID, temp_ID;
  long use_src2;
  long use_src3;
  int use_src2_ID;
  int use_src3_ID;
  int image_menu1_ID;
  int image_menu2_ID;
  int image_menu3_ID;
  int src1_ID, src2_ID, src3_ID;
  int netpbm_buf_ID;
  int param_buf_ID;
  
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

      cached_params = gimp_get_params ();
      if (cached_params)
	{
	  sprintf (netpbm_buf, "%s", cached_params);
	  sprintf (param_buf, "%s", cached_params + strlen (netpbm_buf) + 1);
	  use_src2 = *(cached_params + strlen (netpbm_buf) + strlen (param_buf) + 2);
	  use_src3 = *(cached_params + strlen (netpbm_buf) + strlen (param_buf) + 3);
	}
      else
	{
	  netpbm_buf[0] = '\0';
	  param_buf[0] = '\0';
	  use_src2 = use_src3 = 0;
	}

      /* Create a dialog.
       */
      dialog_ID = gimp_new_dialog ("Netpbm");
      group_ID = gimp_new_row_group (dialog_ID, DEFAULT, NORMAL, "");

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "Netpbm filter: ");
      netpbm_buf_ID = gimp_new_text (dialog_ID, temp_ID, netpbm_buf);

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      gimp_new_label (dialog_ID, temp_ID, "Filter parameters: ");
      param_buf_ID = gimp_new_text (dialog_ID, temp_ID, param_buf);

      frame_ID = gimp_new_frame (dialog_ID, group_ID, "Source Images");
      group_ID = gimp_new_row_group (dialog_ID, frame_ID, NORMAL, "");
      image_menu1_ID = gimp_new_image_menu (dialog_ID, group_ID, 
					    IMAGE_CONSTRAIN_ALL,
					    "First Image");
      
      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      use_src2_ID = gimp_new_check_button (dialog_ID, temp_ID, "Use second source");
      gimp_change_item (dialog_ID, use_src2_ID, sizeof (long), &use_src2);
      image_menu2_ID = gimp_new_image_menu (dialog_ID, temp_ID, 
					    IMAGE_CONSTRAIN_ALL,
					    "");

      temp_ID = gimp_new_column_group (dialog_ID, group_ID, NORMAL, "");
      use_src3_ID = gimp_new_check_button (dialog_ID, temp_ID, "Use third source");
      gimp_change_item (dialog_ID, use_src3_ID, sizeof (long), &use_src3);
      image_menu3_ID = gimp_new_image_menu (dialog_ID, temp_ID, 
					    IMAGE_CONSTRAIN_ALL,
					    "");

      gimp_add_callback (dialog_ID, netpbm_buf_ID, text_callback, &netpbm_buf);
      gimp_add_callback (dialog_ID, param_buf_ID, text_callback, &param_buf);
      gimp_add_callback (dialog_ID, image_menu1_ID, image_menu_callback, &src1_ID);
      gimp_add_callback (dialog_ID, image_menu2_ID, image_menu_callback, &src2_ID);
      gimp_add_callback (dialog_ID, image_menu3_ID, image_menu_callback, &src3_ID);
      gimp_add_callback (dialog_ID, use_src2_ID, toggle_callback, &use_src2);
      gimp_add_callback (dialog_ID, use_src3_ID, toggle_callback, &use_src3);
      gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
      gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
      
      if (gimp_show_dialog (dialog_ID))
	{
	  src1 = gimp_get_input_image (src1_ID);
	  src2 = (use_src2) ? gimp_get_input_image (src2_ID) : NULL;
	  src3 = (use_src3) ? gimp_get_input_image (src3_ID) : NULL;
	  
	  if (src1)
	    {
	      if ((gimp_image_type (src1) == INDEXED_IMAGE) ||
		  (gimp_image_type (src2) == INDEXED_IMAGE) ||
		  (gimp_image_type (src3) == INDEXED_IMAGE))
		gimp_message ("netpbm: cannot operate on indexed color images");
	      else
		execute (src1, src2, src3, netpbm_buf, param_buf);
	    }

	  /* setup cache data */
	  sprintf ((netpbm_buf + strlen (netpbm_buf) + 1), "%s%c%c%c",
		   param_buf, 0, (char) use_src2, (char) use_src3);
	  gimp_set_params (strlen (netpbm_buf) + strlen (param_buf) + 4, netpbm_buf);
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
text_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  sprintf ((char *) client_data, "%s", (char *) call_data);
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


static int
save_pgm (image, filename)
     Image image;
     char * filename;
{
  FILE * fp;
  int cols, rows;
  int col, row;
  unsigned char * data;

  fp = fopen (filename, "wb");
  if (fp == NULL)
    return 0;

  cols = gimp_image_width (image);
  rows = gimp_image_height (image);
  data = gimp_image_data (image);

  /*  Write the header information  */
  fprintf (fp, "%c%c\n%d %d\n%d\n", PGM_MAGIC1, RPGM_MAGIC2, cols, rows, 255);

  for (row = 0; row < rows; row++)
    {
      col = cols;
      while (col--)
	fputc (*data++, fp);
    }

  fclose (fp);
  return 1;
}


static int
save_ppm (image, filename)
     Image image;
     char * filename;
{
  FILE * fp;
  int cols, rows;
  int col, row;
  unsigned char * data;

  fp = fopen (filename, "wb");
  if (fp == NULL)
    return 0;

  cols = gimp_image_width (image);
  rows = gimp_image_height (image);
  data = gimp_image_data (image);

  /*  Write the header information  */
  fprintf (fp, "%c%c\n%d %d\n%d\n", PPM_MAGIC1, RPPM_MAGIC2, cols, rows, 255);

  for (row = 0; row < rows; row++)
    {
      col = cols;
      while (col--)
	{
	  fputc (*data++, fp);  /* red */
	  fputc (*data++, fp);  /* green */
	  fputc (*data++, fp);  /* blue */
	}
    }

  fclose (fp);
  return 1;
}


static int
convert_to_netpbm (image, filename)
     Image image;
     char * filename;
{
  int type;

  type = gimp_image_type (image);

  switch (type)
    {
    case GRAY_IMAGE :
      return save_pgm (image, filename);
      break;
    case RGB_IMAGE :
      return save_ppm (image, filename);
      break;
    default :
      return 0;
      break;
    }
}

static int
load_pgm (image, fp, raw)
     Image image;
     FILE * fp;
     int raw;
{
  int i, j;
  int rows, cols;
  int val;
  unsigned char * data;

  rows = gimp_image_height (image);
  cols = gimp_image_width (image);
  data = gimp_image_data (image);

  for (i = 0; i < rows; i++)
    for (j = 0; j < cols; j++)
      {
	/* gray value */
	val = (raw) ? fgetc (fp) : pbm_getint (fp);
	if (val == EOF)
	  return 0;
	else
	  *data ++ = (unsigned char) val;
      }

  return 1;
}

static int
load_ppm (image, fp, raw)
     Image image;
     FILE * fp;
     int raw;
{
  int i, j;
  int rows, cols;
  unsigned char * data;
  int val;

  rows = gimp_image_height (image);
  cols = gimp_image_width (image);
  data = gimp_image_data (image);

  for (i = 0; i < rows; i++)
    for (j = 0; j < cols; j++)
      {
	/* red value */
	val = (raw) ? fgetc (fp) : pbm_getint (fp);
	if (val == EOF)
	  return 0;
	else
	  *data ++ = (unsigned char) val;
	/* green value */
	val = (raw) ? fgetc (fp) : pbm_getint (fp);
	if (val == EOF)
	  return 0;
	else
	  *data ++ = (unsigned char) val;
	/* blue value */
	val = (raw) ? fgetc (fp) : pbm_getint (fp);
	if (val == EOF)
	  return 0;
	else
	  *data ++ = (unsigned char) val;
      }

  return 1;
}

static int
pbm_readmagicnumber (file)
     FILE* file;
{
  int ich1, ich2;

  ich1 = getc( file );
  if ( ich1 == EOF )
    return -1;
  ich2 = getc( file );
  if ( ich2 == EOF )
    return -1;
  return ich1 * 256 + ich2;
}


static char
pbm_getc (file)
     FILE* file;
{
  int ich;
  char ch;

  ich = getc( file );
  ch = (char) ich;
    
  if ( ch == '#' )
    {
      do
	{
	  ich = getc( file );
	  ch = (char) ich;
	}
      while ( ch != '\n' && ch != '\r' );
    }

  return ch;
}


static int
pbm_getint (file)
     FILE* file;
{
  char ch;
  int i;

  do
    {
      ch = pbm_getc( file );
    }
  while ( ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' );
  
  if ( ch < '0' || ch > '9' )
    return -1;
  
  i = 0;
  do
    {
      i = i * 10 + ch - '0';
      ch = pbm_getc( file );
    }
  while ( ch >= '0' && ch <= '9' );
  
  return i;
}


static Image
convert_from_netpbm (fp)
     FILE * fp;
{
  Image image;
  int magic_num;
  int cols, rows;
  int ret_val;
  
  magic_num = pbm_readmagicnumber (fp);
  if (magic_num < 0)
    return NULL;

  cols = pbm_getint (fp);
  rows = pbm_getint (fp);

  switch (magic_num)
    {
      case PPM_FORMAT :
	image = gimp_new_image (0, cols, rows, RGB_IMAGE);
	ret_val = load_ppm (image, fp, 0);
	break;
      case RPPM_FORMAT :
	image = gimp_new_image (0, cols, rows, RGB_IMAGE);
	ret_val = load_ppm (image, fp, 1);
	break;
      case PGM_FORMAT :
	image = gimp_new_image (0, cols, rows, GRAY_IMAGE);
	ret_val = load_pgm (image, fp, 0);
	break;
      case RPGM_FORMAT :
	image = gimp_new_image (0, cols, rows, GRAY_IMAGE);
	ret_val = load_pgm (image, fp, 1);
	break;
      default :
	gimp_message ("cannot convert netpbm output to gimp format");
	image = NULL;
	ret_val = 0;
      }

  /*  If the loading failed, destroy the image  */
  if (! ret_val && image)
    {
      /* gimp_destroy_image (image); */
      image = NULL;
    }

  return image;
}

static void
remove_netpbm (name)
     char * name;
{
  char command_buf [MAXBUF];

  /* Delete file "name" */
  sprintf (command_buf, "rm -f %s", name);
  system (command_buf);
}


static char *
get_temp_name (index)
     int index;
{
  char * name;
  pid_t pid;
  
  pid = getpid ();
  name = (char *) malloc (sizeof (char) * 32);

  sprintf (name, "/tmp/.netpbm_temp%d.%d", index, pid);

  return name;
}


static int
execute (src1, src2, src3, netpbm_name, parameters)
     Image src1, src2, src3;
     char * netpbm_name;
     char * parameters;
{
  Image dest;
  char *src1_name;
  char *src2_name;
  char *src3_name;
  char error_buf [MAXBUF];
  char command_buf [MAXBUF];
  FILE *output;
  int return_val = 1;
 
  /*  get the temporary filenames  */
  src1_name = get_temp_name (1);
  src2_name = (src2) ? get_temp_name (2) : NULL;
  src3_name = (src3) ? get_temp_name (3) : NULL;

  if (src1 && ! convert_to_netpbm (src1, src1_name))
    {
      gimp_message ("netpbm: cannot convert src1 to netpbm format");
      return_val = 0;
    }
  if (src2 && ! convert_to_netpbm (src2, src2_name))
    {
      gimp_message ("netpbm: cannot convert src2 to netpbm format");
      return_val = 0;
    }
  if (src3 && ! convert_to_netpbm (src3, src3_name))
    {
      gimp_message ("netpbm: cannot convert src3 to netpbm format");
      return_val = 0;
    }

  /*  setup the command line  */
  sprintf (command_buf, "%s %s %s", netpbm_name, parameters, src1_name);

  /*  If a second source was specified add it now  */
  if (src2)
    sprintf (command_buf + strlen (command_buf), " %s", src2_name);

  /*  If a third source was specified add it now  */
  if (src3)
    sprintf (command_buf + strlen (command_buf), " %s", src3_name);

  /*  Run the netpbm filter and send the output to the specified pipe  */
  if (return_val)
    {
      output = popen (command_buf, "r");
      if (output <= 0)
	{
	  sprintf (error_buf, "Error running: %s", command_buf);
	  gimp_message (error_buf);
	  return_val = 0;
	}

      /* convert the output of the netpbm filter to a gimp image */
      dest = convert_from_netpbm (output);

      /* if the output is erroneous, signal an error */
      if (!dest)
	{
	  sprintf (error_buf, "Error running: %s", command_buf);
	  gimp_message (error_buf);
	  return_val = 0;
	}
    }

  /* cleanup */
  pclose (output);
  remove_netpbm (src1_name);
  if (src2)
    remove_netpbm (src2_name);
  if (src3)
    remove_netpbm (src3_name);
  if (src1_name)
    free (src1_name);
  if (src2_name)
    free (src2_name);
  if (src3_name)
    free (src3_name);

  if (dest)
    {
      gimp_display_image (dest);
      gimp_update_image (dest);
      gimp_free_image (dest);
    }

  return return_val;
}
