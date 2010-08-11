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

/*  saves and loads gimp brush files...
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gimp.h"
#include "brush_header.h"

#define MAX_NAME 256

/* Some variables... */
static int dialog_ID;

/* Declare some local functions.
 */
static void load_image (char *);
static void save_image (char *);

static void text_callback (int, void *, void *);
static void ok_callback (int, void *, void *);
static void cancel_callback (int, void *, void *);

char *prog_name;

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
  Image image;
  ImageType image_type;
  FILE * fp;
  unsigned char *dest;
  char * brush_name;
  int bn_size;
  unsigned char buf [sz_BrushHeader];
  BrushHeader header;
  unsigned long * hp;
  int i;

  /*  Open the requested file  */
  if (! (fp = fopen (filename, "r")))
    {
      printf ("%s: can't open \"%s\"\n", prog_name, filename);
      gimp_quit ();
    }

  /*  Read in the header size  */
  fread (buf, sz_BrushHeader, 1, fp);
  
  /*  rearrange the bytes in each unsigned long  */
  hp = (unsigned long *) &header;
  for (i = 0; i < 5; i++)
    hp [i] = (buf [i * 4] << 24) + (buf [i * 4 + 1] << 16) +
             (buf [i * 4 + 2] << 8) + (buf [i * 4 + 3]);

  /*  Get a new image structure  */
  image_type = (header.bytes == 1) ? GRAY_IMAGE : RGB_IMAGE;
  image = gimp_new_image (filename, header.width, header.height, image_type);
  dest = gimp_image_data (image);

  /*  Read in the brush name  */
  if ((bn_size = (header.header_size - sz_BrushHeader)))
    {
      brush_name = (char *) malloc (sizeof (char) * bn_size);
      fread (brush_name, bn_size, 1, fp);
    }

  /*  Read the image data  */
  fread (dest, header.width * header.height * header.bytes, 1, fp);

  /*  Clean up  */
  fclose (fp);
  gimp_display_image (image);
  gimp_update_image (image);
  gimp_free_image (image);

  free (brush_name);
  gimp_quit ();
}


static void
save_image (filename)
     char *filename;
{
  Image image;
  FILE * fp;
  BrushHeader header;
  unsigned char buf [sz_BrushHeader];
  char * brush_name;
  unsigned long * hp;
  int bn_size;
  int i;
  unsigned char *src;
  int group_ID;
  int name_ID;

  dialog_ID = gimp_new_dialog ("GIMP brush");

  group_ID = gimp_new_column_group (dialog_ID, DEFAULT, NORMAL, "");
  brush_name = strdup ("User Defined");
  gimp_new_label (dialog_ID, group_ID, "Brush Name:");
  name_ID = gimp_new_text (dialog_ID, group_ID, brush_name);

  gimp_add_callback (dialog_ID, gimp_ok_item_id (dialog_ID), ok_callback, 0);
  gimp_add_callback (dialog_ID, gimp_cancel_item_id (dialog_ID), cancel_callback, 0);
  gimp_add_callback (dialog_ID, name_ID, text_callback, &brush_name);

  if (!gimp_show_dialog (dialog_ID))
    {
      gimp_quit ();
      return;
    }

  image = gimp_get_input_image (0);

  bn_size = strlen (brush_name) + 1;
  header.header_size = sz_BrushHeader + bn_size;
  header.version = FILE_VERSION;
  header.width = gimp_image_width (image);
  header.height = gimp_image_height (image);
  header.bytes = gimp_image_channels (image);

  /*  rearrange the bytes in each unsigned long  */
  hp = (unsigned long *) &header;
  for (i = 0; i < 5; i++)
    {
      buf [i * 4 + 0] = (unsigned char) ((hp [i] >> 24) & 0xff);
      buf [i * 4 + 1] = (unsigned char) ((hp [i] >> 16) & 0xff);
      buf [i * 4 + 2] = (unsigned char) ((hp [i] >> 8) & 0xff);
      buf [i * 4 + 3] = (unsigned char) ((hp [i] >> 0) & 0xff);
    }

  /*  open the file for writing  */
  if ((fp = fopen (filename, "w")))
    {
      /*  write the header to the open file  */
      fwrite (buf, sz_BrushHeader, 1, fp);

      /*  write the brush name to the open file  */
      fwrite (brush_name, bn_size, 1, fp);

      /*  write the brush data to the file  */
      src = gimp_image_data (image);
      fwrite (src, header.width * header.height * header.bytes, 1, fp);
      
      fclose (fp);
    }

  /*  Clean up  */
  free (brush_name);
  gimp_free_image (image);
  gimp_quit ();
}


static void
text_callback (item_ID, client_data, call_data)
     int item_ID;
     void *client_data;
     void *call_data;
{
  char ** brush_name;

  brush_name = (char **) client_data;

  *brush_name = (char *) realloc (*brush_name, strlen (call_data) + 1);

  strcpy (*brush_name, call_data);
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

