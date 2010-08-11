/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * HRZ reading and writing code Copyright (C) 1996 Albert Cahalan
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: hrz.c,v 1.2.2.1 1998/09/09 09:45:04 yosh Exp $ */

/*
 * Albert Cahalan <acahalan at cs.uml.edu>, 1997  -  Initial HRZ support.
 * Based on PNM code by Erik Nygren (nygren@mit.edu)
 *
 * Bug reports are wanted. I'd like to remove useless code.
 *
 * The HRZ file is always 256x240 with RGB values from 0 to 63.
 * No compression, no header, just the raw RGB data.
 * It is (was?) used for amatuer radio slow-scan TV.
 * That makes the size 256*240*3 = 184320 bytes.
 */


#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


/* Declare local data types
 */


typedef struct
{
  gint  run;  /*  run  */
} HRZSaveInterface;


/* Declare some local functions.
 */
static void   query      (void);
static void   run        (char    *name,
                          int      nparams,
                          GParam  *param,
                          int     *nreturn_vals,
                          GParam **return_vals);
static gint32 load_image (char   *filename);
static gint   save_image (char   *filename,
			  gint32  image_ID,
			  gint32  drawable_ID);

static gint   save_dialog ();

static void   save_close_callback      (GtkWidget *widget, gpointer data);
static void   save_ok_callback         (GtkWidget *widget,
					gpointer   data);
/*
static void   save_toggle_update       (GtkWidget *widget,
					gpointer   data);
*/


#define hrzscanner_eof(s) ((s)->eof)
#define hrzscanner_fp(s) ((s)->fp)

/* Checks for a fatal error */
#define CHECK_FOR_ERROR(predicate, jmpbuf, errmsg) \
        if ((predicate)) \
        { /*gimp_message((errmsg));*/ longjmp((jmpbuf),1); }

GPlugInInfo PLUG_IN_INFO =
{
  NULL,    /* init_proc */
  NULL,    /* quit_proc */
  query,   /* query_proc */
  run,     /* run_proc */
};

static HRZSaveInterface psint =
{
  FALSE     /* run */
};


MAIN ()

static void
query ()
{
  static GParamDef load_args[] =
  {
    { PARAM_INT32, "run_mode", "Interactive, non-interactive" },
    { PARAM_STRING, "filename", "The name of the file to load" },
    { PARAM_STRING, "raw_filename", "The name of the file to load" },
  };
  static GParamDef load_return_vals[] =
  {
    { PARAM_IMAGE, "image", "Output image" },
  };
  static int nload_args = sizeof (load_args) / sizeof (load_args[0]);
  static int nload_return_vals = sizeof (load_return_vals) / sizeof (load_return_vals[0]);

  static GParamDef save_args[] =
  {
    { PARAM_INT32, "run_mode", "Interactive, non-interactive" },
    { PARAM_IMAGE, "image", "Input image" },
    { PARAM_DRAWABLE, "drawable", "Drawable to save" },
    { PARAM_STRING, "filename", "The name of the file to save the image in" },
    { PARAM_STRING, "raw_filename", "The name of the file to save the image in" }
  };
  static int nsave_args = sizeof (save_args) / sizeof (save_args[0]);

  gimp_install_procedure ("file_hrz_load",
                          "loads files of the hrz file format",
                          "FIXME: write help for hrz_load",
                          "Albert Cahalan",
                          "Albert Cahalan",
                          "1997",
                          "<Load>/HRZ",
			  NULL,
                          PROC_PLUG_IN,
                          nload_args, nload_return_vals,
                          load_args, load_return_vals);

  gimp_install_procedure ("file_hrz_save",
                          "saves files in the hrz file format",
                          "HRZ saving handles all image types except those with alpha channels.",
                          "Albert Cahalan",
                          "Albert Cahalan",
                          "1997",
                          "<Save>/HRZ",
			  "RGB*, GRAY*, INDEXED*",
                          PROC_PLUG_IN,
                          nsave_args, 0,
                          save_args, NULL);

  gimp_register_magic_load_handler ("file_hrz_load", "hrz", "", "0,size,184320");
  gimp_register_save_handler ("file_hrz_save", "hrz", "");
}

static void
run (char    *name,
     int      nparams,
     GParam  *param,
     int     *nreturn_vals,
     GParam **return_vals)
{
  static GParam values[2];
  GRunModeType run_mode;
  GStatusType status = STATUS_SUCCESS;
  gint32 image_ID;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = PARAM_STATUS;
  values[0].data.d_status = STATUS_CALLING_ERROR;

  if (strcmp (name, "file_hrz_load") == 0)
    {
      image_ID = load_image (param[1].data.d_string);

      if (image_ID != -1)
	{
	  *nreturn_vals = 2;
	  values[0].data.d_status = STATUS_SUCCESS;
	  values[1].type = PARAM_IMAGE;
	  values[1].data.d_image = image_ID;
	}
      else
	{
	  values[0].data.d_status = STATUS_EXECUTION_ERROR;
	}
    }
  else if (strcmp (name, "file_hrz_save") == 0)
    {
      switch (run_mode)
	{
	case RUN_INTERACTIVE:

	  /*  First acquire information with a dialog  */
	  if (! save_dialog ())
	    return;
	  break;

	case RUN_NONINTERACTIVE:
	  /*  Make sure all the arguments are there!  */
	  if (nparams != 4)
	    status = STATUS_CALLING_ERROR;

	case RUN_WITH_LAST_VALS:
	  break;

	default:
	  break;
	}

      *nreturn_vals = 1;
      if (save_image (param[3].data.d_string, param[1].data.d_int32, param[2].data.d_int32))
	{
	  values[0].data.d_status = STATUS_SUCCESS;
	}
      else
	values[0].data.d_status = STATUS_EXECUTION_ERROR;
    }
}

/************ load HRZ image row *********************/
void
do_hrz_load(void *mapped, GPixelRgn *pixel_rgn)
{
  unsigned char *data, *d;
  int            x, y;
  int            start, end, scanlines;

  data = g_malloc (gimp_tile_height () * 256 * 3);

  for (y = 0; y < 240; )
    {
      start = y;
      end = y + gimp_tile_height ();
      end = MIN (end, 240);
      scanlines = end - start;
      d = data;

      memcpy(d, ((unsigned char *) mapped)+256*3*y, 256*3*scanlines); /* this is gross */
      /* scale 0..63 into 0..255 properly */
      for (x=0; x<256*3*scanlines; x++)  d[x] = (d[x]>>4) | (d[x]<<2);
      d += 256*3*y;

      gimp_progress_update ((double) y / 240.0);
      gimp_pixel_rgn_set_rect (pixel_rgn, data, 0, y, 256, scanlines);
      y += scanlines;
    }

  g_free (data);
}

/********************* Load HRZ image **********************/
static gint32
load_image (char *filename)
{
  GPixelRgn pixel_rgn;
  gint32 image_ID;
  gint32 layer_ID;
  GDrawable *drawable;
  int filedes;
  char *temp;
  void *mapped;  /* memory mapped file data */
  struct stat statbuf;  /* must check file size */

  temp = g_malloc (strlen (filename) + 11);
  sprintf (temp, "Loading %s:", filename);
  gimp_progress_init (temp);
  g_free (temp);

  /* open the file */
  filedes = open(filename, O_RDONLY);
  if (filedes == -1)
    {
      /* errno is set to indicate the error, but the user won't know :-( */
      /*gimp_message("hrz filter: can't open file\n");*/
      return -1;
    }
  /* stat the file to see if it is the right size */
  fstat(filedes, &statbuf);
  if(statbuf.st_size != 256*240*3)
    {
      fprintf(stderr, "hrz filter: file is not HRZ type\n");
      return -1;
    }
  mapped = mmap(NULL, 256*240*3, PROT_READ, MAP_PRIVATE, filedes, 0);
  if(mapped == (void *)(-1))
    {
      fprintf(stderr, "hrz filter: could not map file\n");
      return -1;
    }
  close (filedes);  /* not needed anymore, data is memory mapped */

  /* Create new image of proper size; associate filename */
  image_ID = gimp_image_new (256, 240, RGB);
  gimp_image_set_filename (image_ID, filename);

  layer_ID = gimp_layer_new (image_ID, "Background",
			     256, 240,
			     RGB_IMAGE, 100, NORMAL_MODE);
  gimp_image_add_layer (image_ID, layer_ID, 0);

  drawable = gimp_drawable_get (layer_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, TRUE, FALSE);


  do_hrz_load(mapped, &pixel_rgn);

  /* close the file */
#ifndef NeXT /* @#%@! NeXTStep */
  munmap(mapped, 256*240*3);
#endif

  /* Tell the GIMP to display the image.
   */
  gimp_drawable_flush (drawable);

  return image_ID;
}


/************** Writes out RGB raw rows ************/
static void
saverow (FILE *fp, unsigned char *data)
{
  int loop = 256*3;
  unsigned char *walk = data;
  while(loop--)
    {
      *walk = (*walk >> 2);
      walk++;
    }
  fwrite(data, 1, 256*3, fp);
}

/********************* save image *********************/
static gint
save_image (char   *filename,
	    gint32  image_ID,
	    gint32  drawable_ID)
{
  GPixelRgn pixel_rgn;
  GDrawable *drawable;
  GDrawableType drawable_type;
  unsigned char *data;
  unsigned char *d;                       /* FIX */
  unsigned char *rowbuf;
  char *temp;
  int np = 3;
  int xres, yres;
  int ypos, yend;
  FILE *fp;

  /* initialize */

  d = NULL;

  drawable = gimp_drawable_get (drawable_ID);
  drawable_type = gimp_drawable_type (drawable_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, FALSE, FALSE);

  /*  Make sure we're not saving an image with an alpha channel  */
  if (gimp_drawable_has_alpha (drawable_ID))
    {
      /* gimp_message ("HRZ save cannot handle images with alpha channels.");  */
      return FALSE;
    }

  /* open the file */
  fp = fopen(filename, "wb");
  if (fp == NULL)
    {
      /* Ought to pass errno back... */
      fprintf (stderr, "hrz: can't open \"%s\"\n", filename);
      return FALSE;
    }

  xres = drawable->width;
  yres = drawable->height;

  if ((xres != 256) || (yres != 240))
    {
      fprintf (stderr, "hrz: Image must be 256x240 for HRZ format.\n");
      return FALSE;
    }
  if (drawable_type == INDEXED_IMAGE)
    {
      fprintf (stderr, "hrz: Image must be RGB for HRZ format.\n");
      return FALSE;
    }

  temp = g_malloc (strlen (filename) + 11);
  sprintf (temp, "Saving %s:", filename);
  gimp_progress_init (temp);
  g_free (temp);


  /* allocate a buffer for retrieving information from the pixel region  */
  data = (unsigned char *) g_malloc (gimp_tile_height () * drawable->width * drawable->bpp);

  rowbuf = g_malloc(256*3);

  /* Write the body out */
  for (ypos = 0; ypos < yres; ypos++)
    {
      if ((ypos % gimp_tile_height ()) == 0)
	{
	  yend = ypos + gimp_tile_height ();
	  yend = MIN (yend, yres);
	  gimp_pixel_rgn_get_rect (&pixel_rgn, data, 0, ypos, xres, (yend - ypos));
	  d = data;
	}

      saverow(fp, d);
      d += xres*np;

      if (!(ypos & 0x0f))
	gimp_progress_update( (double)ypos / 240.0 );
    }

  /* close the file */
  fclose (fp);

  g_free(rowbuf);
  g_free(data);

  gimp_drawable_detach (drawable);

  return TRUE;
}

/*********** Save dialog ************/
static gint
save_dialog ()
{
  GtkWidget *dlg;
  GtkWidget *button;
  gchar **argv;
  gint argc;

  argc = 1;
  argv = g_new (gchar *, 1);
  argv[0] = g_strdup ("save");

  gtk_init (&argc, &argv);
  gtk_rc_parse (gimp_gtkrc ());

  dlg = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dlg), "Save as HRZ");
  gtk_window_position (GTK_WINDOW (dlg), GTK_WIN_POS_MOUSE);
  gtk_signal_connect (GTK_OBJECT (dlg), "destroy",
		      (GtkSignalFunc) save_close_callback,
		      NULL);

  /*  Action area  */
  button = gtk_button_new_with_label ("OK");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) save_ok_callback,
                      dlg);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->action_area), button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Cancel");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) gtk_widget_destroy,
			     GTK_OBJECT (dlg));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->action_area), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  gtk_widget_show (dlg);

  gtk_main ();
  gdk_flush ();

  return psint.run;
}


/**********  Save interface functions  **********/

static void
save_close_callback (GtkWidget *widget,
		     gpointer   data)
{
  gtk_main_quit ();
}

static void
save_ok_callback (GtkWidget *widget,
		  gpointer   data)
{
  psint.run = TRUE;
  gtk_widget_destroy (GTK_WIDGET (data));
}

/*
static void
save_toggle_update (GtkWidget *widget,
		    gpointer   data)
{
  int *toggle_val;

  toggle_val = (int *) data;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    *toggle_val = TRUE;
  else
    *toggle_val = FALSE;
}
*/
