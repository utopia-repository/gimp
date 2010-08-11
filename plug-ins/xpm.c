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
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include "gimp.h"

/* Declare some local functions.
 */
static void load_image (char *);
static void save_image (char *);

static char *prog_name;

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
  Display *display;
  XpmImage xpm_image;
  XpmColor *xpm_color;
  unsigned char *cmap;
  Colormap colormap;
  XColor xcolor;
  Image image;
  unsigned int *src;
  unsigned char *dest;
  char *temp;
  int screen;
  int val;
  int i, j;

  temp = malloc (strlen (filename) + 11);
  if (!temp)
    gimp_quit ();

  sprintf (temp, "Loading %s:", filename);
  gimp_init_progress (temp);
  free (temp);

  display = XOpenDisplay (NULL);
  screen = DefaultScreen (display);
  colormap = DefaultColormap (display, screen);
  
  XpmReadFileToXpmImage (filename, &xpm_image, NULL);

  cmap = malloc (sizeof (unsigned char) * 3 * xpm_image.ncolors);
  
  for (i = 0, j = 0; i < xpm_image.ncolors; i++)
    {
      xpm_color = &xpm_image.colorTable[i];
      if (xpm_color->c_color)
	{
	  XParseColor (display, colormap, xpm_color->c_color, &xcolor);
	}
      else if (xpm_color->g_color)
	{
	  XParseColor (display, colormap, xpm_color->g_color, &xcolor);
	}
      else if (xpm_color->g4_color)
	{
	  XParseColor (display, colormap, xpm_color->g4_color, &xcolor);
	}
      else if (xpm_color->m_color)
	{
	  XParseColor (display, colormap, xpm_color->m_color, &xcolor);
	}
      
      cmap[j++] = xcolor.red >> 8;
      cmap[j++] = xcolor.green >> 8;
      cmap[j++] = xcolor.blue >> 8;
    }

  XCloseDisplay (display);

  if (xpm_image.ncolors > 256)
    {
      image = gimp_new_image (filename, xpm_image.width, xpm_image.height, RGB_IMAGE);

      src = xpm_image.data;
      dest = gimp_image_data (image);

      for (i = 0; i < xpm_image.height; i++)
	{
	  for (j = 0; j < xpm_image.width; j++)
	    {
	      val = *src++ * 3;
	      *dest++ = cmap[val+0];
	      *dest++ = cmap[val+1];
	      *dest++ = cmap[val+2];
	    }

	  if ((i % 5) == 0)
	    gimp_do_progress (i, xpm_image.height);
	}
    }
  else
    {
      image = gimp_new_image (filename, xpm_image.width, xpm_image.height, INDEXED_IMAGE);
      gimp_set_image_colors (image, cmap, xpm_image.ncolors);
      
      src = xpm_image.data;
      dest = gimp_image_data (image);
      
      for (i = 0; i < xpm_image.height; i++)
	{
	  for (j = 0; j < xpm_image.width; j++)
	    *dest++ = *src++;

	  if ((i % 5) == 0)
	    gimp_do_progress (i, xpm_image.height);
	}
    }
  
  gimp_display_image (image);
  gimp_update_image (image);
  gimp_free_image (image);
  gimp_quit ();
}

static void
save_image (filename)
     char *filename;
{
  fprintf (stderr, "%s: save not yet implemented\n", prog_name);
  gimp_quit ();
}

