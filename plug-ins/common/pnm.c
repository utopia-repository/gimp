/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * PNM reading and writing code Copyright (C) 1996 Erik Nygren
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

/* $Id: pnm.c,v 1.52 2004/12/12 17:18:03 weskaggs Exp $ */

/*
 * The pnm reading and writing code was written from scratch by Erik Nygren
 * (nygren@mit.edu) based on the specifications in the man pages and
 * does not contain any code from the netpbm or pbmplus distributions.
 */

#include "config.h"

#include <errno.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

/* Declare local data types
 */

typedef struct _PNMScanner
{
  gint    fd;		      /* The file descriptor of the file being read */
  gchar   cur;		      /* The current character in the input stream */
  gint    eof;		      /* Have we reached end of file? */
  gchar  *inbuf;	      /* Input buffer - initially 0 */
  gint    inbufsize;	      /* Size of input buffer */
  gint    inbufvalidsize;     /* Size of input buffer with valid data */
  gint    inbufpos;           /* Position in input buffer */
} PNMScanner;

typedef struct _PNMInfo
{
  gint       xres, yres;	/* The size of the image */
  gint       maxval;		/* For ascii image files, the max value
				 * which we need to normalize to */
  gint       np;		/* Number of image planes (0 for pbm) */
  gint       asciibody;		/* 1 if ascii body, 0 if raw body */
  jmp_buf    jmpbuf;		/* Where to jump to on an error loading */
  /* Routine to use to load the pnm body */
  void    (* loader) (PNMScanner *, struct _PNMInfo *, GimpPixelRgn *);
} PNMInfo;

/* Contains the information needed to write out PNM rows */
typedef struct _PNMRowInfo
{
  gint    fd;		/* File descriptor */
  gchar  *rowbuf;	/* Buffer for writing out rows */
  gint    xres;		/* X resolution */
  gint    np;		/* Number of planes */
  guchar *red;		/* Colormap red */
  guchar *grn;		/* Colormap green */
  guchar *blu;		/* Colormap blue */
} PNMRowInfo;

/* Save info  */
typedef struct
{
  gint  raw;  /*  raw or ascii  */
} PNMSaveVals;

#define BUFLEN 512		/* The input buffer size for data returned
				 * from the scanner.  Note that lines
				 * aren't allowed to be over 256 characters
				 * by the spec anyways so this shouldn't
				 * be an issue. */

#define SAVE_COMMENT_STRING "# CREATOR: The GIMP's PNM Filter Version 1.0\n"

/* Declare some local functions.
 */
static void   query      (void);
static void   run        (const gchar      *name,
                          gint              nparams,
                          const GimpParam  *param,
                          gint             *nreturn_vals,
                          GimpParam       **return_vals);
static gint32 load_image (const gchar      *filename);
static gint   save_image (const gchar      *filename,
			  gint32            image_ID,
			  gint32            drawable_ID);

static gint   save_dialog              (void);

static void   pnm_load_ascii           (PNMScanner *scan,
					PNMInfo    *info,
					GimpPixelRgn  *pixel_rgn);
static void   pnm_load_raw             (PNMScanner *scan,
					PNMInfo    *info,
					GimpPixelRgn  *pixel_rgn);
static void   pnm_load_rawpbm          (PNMScanner *scan,
					PNMInfo    *info,
					GimpPixelRgn  *pixel_rgn);

static void   pnmsaverow_ascii         (PNMRowInfo *ri,
					guchar     *data);
static void   pnmsaverow_raw           (PNMRowInfo *ri,
					guchar     *data);
static void   pnmsaverow_ascii_indexed (PNMRowInfo *ri,
					guchar     *data);
static void   pnmsaverow_raw_indexed   (PNMRowInfo *ri,
					guchar     *data);

static void   pnmscanner_destroy       (PNMScanner *s);
static void   pnmscanner_createbuffer  (PNMScanner *s,
					gint        bufsize);
static void   pnmscanner_getchar       (PNMScanner *s);
static void   pnmscanner_eatwhitespace (PNMScanner *s);
static void   pnmscanner_gettoken      (PNMScanner *s,
					gchar      *buf,
					gint        bufsize);
static void   pnmscanner_getsmalltoken (PNMScanner *s,
					gchar      *buf);

static PNMScanner * pnmscanner_create  (gint        fd);


#define pnmscanner_eof(s) ((s)->eof)
#define pnmscanner_fd(s) ((s)->fd)

/* Checks for a fatal error */
#define CHECK_FOR_ERROR(predicate, jmpbuf, errmsg) \
        if ((predicate)) \
        { g_message ((errmsg)); longjmp((jmpbuf),1); }

static struct struct_pnm_types
{
  gchar   name;
  gint    np;
  gint    asciibody;
  gint    maxval;
  void (* loader) (PNMScanner *, struct _PNMInfo *, GimpPixelRgn *pixel_rgn);
} pnm_types[] =
{
  { '1', 0, 1,   1, pnm_load_ascii },  /* ASCII PBM */
  { '2', 1, 1, 255, pnm_load_ascii },  /* ASCII PGM */
  { '3', 3, 1, 255, pnm_load_ascii },  /* ASCII PPM */
  { '4', 0, 0,   1, pnm_load_rawpbm }, /* RAW   PBM */
  { '5', 1, 0, 255, pnm_load_raw },    /* RAW   PGM */
  { '6', 3, 0, 255, pnm_load_raw },    /* RAW   PPM */
  {  0 , 0, 0,   0, NULL}
};

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static PNMSaveVals psvals =
{
  TRUE     /* raw? or ascii */
};


MAIN ()

static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "filename", "The name of the file to load" },
    { GIMP_PDB_STRING, "raw_filename", "The name of the file to load" }
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "image", "Output image" }
  };

  static GimpParamDef save_args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",     "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",        "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",     "Drawable to save" },
    { GIMP_PDB_STRING,   "filename",     "The name of the file to save the image in" },
    { GIMP_PDB_STRING,   "raw_filename", "The name of the file to save the image in" },
    { GIMP_PDB_INT32,    "raw",          "Specify non-zero for raw output, zero for ascii output" }
  };

  gimp_install_procedure ("file_pnm_load",
                          "loads files of the pnm file format",
                          "FIXME: write help for pnm_load",
                          "Erik Nygren",
                          "Erik Nygren",
                          "1996",
                          N_("PNM Image"),
			  NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (load_args),
                          G_N_ELEMENTS (load_return_vals),
                          load_args, load_return_vals);

  gimp_register_file_handler_mime ("file_pnm_load", "image/x-portable-anymap");
  gimp_register_magic_load_handler ("file_pnm_load",
                                    "pnm,ppm,pgm,pbm",
				    "",
				    "0,string,P1,0,string,P2,0,string,P3,0,"
				    "string,P4,0,string,P5,0,string,P6");

  gimp_install_procedure ("file_pnm_save",
                          "saves files in the pnm file format",
                          "PNM saving handles all image types without transparency.",
                          "Erik Nygren",
                          "Erik Nygren",
                          "1996",
                          N_("PNM image"),
			  "RGB, GRAY, INDEXED",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_install_procedure ("file_pgm_save",
                          "saves files in the pnm file format",
                          "PGM saving produces grayscale images without transparency.",
                          "Erik Nygren",
                          "Erik Nygren",
                          "1996",
                          N_("PGM image"),
			  "RGB, GRAY, INDEXED",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_install_procedure ("file_ppm_save",
                          "saves files in the pnm file format",
                          "PPM saving handles RGB images without transparency.",
                          "Erik Nygren",
                          "Erik Nygren",
                          "1996",
                          N_("PPM image"),
			  "RGB, GRAY, INDEXED",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_register_file_handler_mime ("file_pgm_save", "image/x-portable-graymap");
  gimp_register_file_handler_mime ("file_ppm_save", "image/x-portable-pixmap");
  gimp_register_save_handler ("file_pgm_save", "pgm", "");
  gimp_register_save_handler ("file_ppm_save", "ppm", "");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam  values[2];
  GimpRunMode       run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32            image_ID;
  gint32            drawable_ID;
  GimpExportReturn  export = GIMP_EXPORT_CANCEL;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;
  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  INIT_I18N ();

  if (strcmp (name, "file_pnm_load") == 0)
    {
      image_ID = load_image (param[1].data.d_string);

      if (image_ID != -1)
	{
	  *nreturn_vals = 2;
	  values[1].type         = GIMP_PDB_IMAGE;
	  values[1].data.d_image = image_ID;
	}
      else
	{
	  status = GIMP_PDB_EXECUTION_ERROR;
	}
    }
  else if (strcmp (name, "file_pnm_save") == 0
           || strcmp (name, "file_pgm_save") == 0
           || strcmp (name, "file_ppm_save") == 0 )
    {
      image_ID      = param[1].data.d_int32;
      drawable_ID   = param[2].data.d_int32;

      /*  eventually export the image */
      switch (run_mode)
	{
	case GIMP_RUN_INTERACTIVE:
	case GIMP_RUN_WITH_LAST_VALS:
	  gimp_ui_init ("pnm", FALSE);
          if (strcmp (name, "file_pnm_save") == 0)
            export = gimp_export_image (&image_ID, &drawable_ID, "PNM",
                                        (GIMP_EXPORT_CAN_HANDLE_RGB |
                                         GIMP_EXPORT_CAN_HANDLE_GRAY |
                                         GIMP_EXPORT_CAN_HANDLE_INDEXED));
          else if (strcmp (name, "file_pgm_save") == 0)
            export = gimp_export_image (&image_ID, &drawable_ID, "PGM",
                                        (GIMP_EXPORT_CAN_HANDLE_GRAY));
          else
            export = gimp_export_image (&image_ID, &drawable_ID, "PPM",
                                        (GIMP_EXPORT_CAN_HANDLE_RGB |
                                         GIMP_EXPORT_CAN_HANDLE_INDEXED));

	  if (export == GIMP_EXPORT_CANCEL)
	    {
	      values[0].data.d_status = GIMP_PDB_CANCEL;
	      return;
	    }
	break;
	default:
	  break;
	}

      switch (run_mode)
	{
	case GIMP_RUN_INTERACTIVE:
	  /*  Possibly retrieve data  */
	  gimp_get_data (name, &psvals);

	  /*  First acquire information with a dialog  */
	  if (! save_dialog ())
	    status = GIMP_PDB_CANCEL;
	  break;

	case GIMP_RUN_NONINTERACTIVE:
	  /*  Make sure all the arguments are there!  */
	  if (nparams != 6)
	    {
	      status = GIMP_PDB_CALLING_ERROR;
	    }
	  else
	    {
	      psvals.raw = (param[5].data.d_int32) ? TRUE : FALSE;
	    }

	case GIMP_RUN_WITH_LAST_VALS:
	  /*  Possibly retrieve data  */
	  gimp_get_data (name, &psvals);
	  break;

	default:
	  break;
	}

      if (status == GIMP_PDB_SUCCESS)
	{
	  if (save_image (param[3].data.d_string, image_ID, drawable_ID))
	    {
	      /*  Store psvals data  */
	      gimp_set_data (name, &psvals, sizeof (PNMSaveVals));
	    }
	  else
	    {
	      status = GIMP_PDB_EXECUTION_ERROR;
	    }
	}

      if (export == GIMP_EXPORT_EXPORT)
	gimp_image_delete (image_ID);
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  values[0].data.d_status = status;
}

static gint32
load_image (const gchar *filename)
{
  GimpPixelRgn pixel_rgn;
  gint32 volatile image_ID = -1;
  gint32 layer_ID;
  GimpDrawable *drawable;
  int fd;			/* File descriptor */
  char *temp;
  char buf[BUFLEN];		/* buffer for random things like scanning */
  PNMInfo *pnminfo;
  PNMScanner * volatile scan;
  int ctr;

  /* open the file */
  fd = open (filename, O_RDONLY | _O_BINARY);

  if (fd == -1)
    {
      g_message (_("Could not open '%s' for reading: %s"),
                 gimp_filename_to_utf8 (filename), g_strerror (errno));
      return -1;
    }

  temp = g_strdup_printf (_("Opening '%s'..."),
                          gimp_filename_to_utf8 (filename));
  gimp_progress_init (temp);
  g_free (temp);

  /* allocate the necessary structures */
  pnminfo = g_new (PNMInfo, 1);

  scan = NULL;
  /* set error handling */
  if (setjmp (pnminfo->jmpbuf))
    {
      /* If we get here, we had a problem reading the file */
      if (scan)
	pnmscanner_destroy (scan);
      close (fd);
      g_free (pnminfo);
      if (image_ID != -1)
	gimp_image_delete (image_ID);
      return -1;
    }

  if (!(scan = pnmscanner_create(fd)))
    longjmp(pnminfo->jmpbuf,1);

  /* Get magic number */
  pnmscanner_gettoken (scan, buf, BUFLEN);
  CHECK_FOR_ERROR(pnmscanner_eof(scan), pnminfo->jmpbuf,
		  _("PNM: Premature end of file."));
  CHECK_FOR_ERROR((buf[0] != 'P' || buf[2]), pnminfo->jmpbuf,
		  _("PNM: Invalid file."));

  /* Look up magic number to see what type of PNM this is */
  for (ctr=0; pnm_types[ctr].name; ctr++)
    if (buf[1] == pnm_types[ctr].name)
      {
	pnminfo->np        = pnm_types[ctr].np;
	pnminfo->asciibody = pnm_types[ctr].asciibody;
	pnminfo->maxval    = pnm_types[ctr].maxval;
	pnminfo->loader    = pnm_types[ctr].loader;
      }
  if (!pnminfo->loader)
    {
      g_message (_("File not in a supported format."));
      longjmp(pnminfo->jmpbuf,1);
    }

  pnmscanner_gettoken(scan, buf, BUFLEN);
  CHECK_FOR_ERROR(pnmscanner_eof(scan), pnminfo->jmpbuf,
		  _("PNM: Premature end of file."));
  pnminfo->xres = g_ascii_isdigit(*buf)?atoi(buf):0;
  CHECK_FOR_ERROR(pnminfo->xres<=0, pnminfo->jmpbuf,
		  _("PNM: Invalid X resolution."));

  pnmscanner_gettoken(scan, buf, BUFLEN);
  CHECK_FOR_ERROR(pnmscanner_eof(scan), pnminfo->jmpbuf,
		  _("PNM: Premature end of file."));
  pnminfo->yres = g_ascii_isdigit(*buf)?atoi(buf):0;
  CHECK_FOR_ERROR(pnminfo->yres<=0, pnminfo->jmpbuf,
		  _("PNM: Invalid Y resolution."));

  if (pnminfo->np != 0)		/* pbm's don't have a maxval field */
    {
      pnmscanner_gettoken(scan, buf, BUFLEN);
      CHECK_FOR_ERROR(pnmscanner_eof(scan), pnminfo->jmpbuf,
		      _("PNM: Premature end of file."));

      pnminfo->maxval = g_ascii_isdigit(*buf)?atoi(buf):0;
      CHECK_FOR_ERROR(((pnminfo->maxval<=0)
		       || (pnminfo->maxval>255 && !pnminfo->asciibody)),
		      pnminfo->jmpbuf,
		      _("PNM: Invalid maximum value."));
    }

  /* Create a new image of the proper size and associate the filename with it.
   */
  image_ID = gimp_image_new (pnminfo->xres, pnminfo->yres,
			     (pnminfo->np >= 3) ? GIMP_RGB : GIMP_GRAY);
  gimp_image_set_filename (image_ID, filename);

  layer_ID = gimp_layer_new (image_ID, _("Background"),
			     pnminfo->xres, pnminfo->yres,
			     (pnminfo->np >= 3) ? GIMP_RGB_IMAGE : GIMP_GRAY_IMAGE,
			     100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_ID, layer_ID, 0);

  drawable = gimp_drawable_get (layer_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable,
		       0, 0, drawable->width, drawable->height, TRUE, FALSE);

  pnminfo->loader (scan, pnminfo, &pixel_rgn);

  /* Destroy the scanner */
  pnmscanner_destroy (scan);

  /* free the structures */
  g_free (pnminfo);

  /* close the file */
  close (fd);

  /* Tell the GIMP to display the image.
   */
  gimp_drawable_flush (drawable);

  return image_ID;
}

static void
pnm_load_ascii (PNMScanner *scan,
		PNMInfo    *info,
		GimpPixelRgn  *pixel_rgn)
{
  unsigned char *data, *d;
  int            x, y, i, b;
  int            start, end, scanlines;
  int            np;
  char           buf[BUFLEN];

  np = (info->np)?(info->np):1;
  data = g_malloc (gimp_tile_height () * info->xres * np);

  /* Buffer reads to increase performance */
  pnmscanner_createbuffer(scan, 4096);

  for (y = 0; y < info->yres; )
    {
      start = y;
      end = y + gimp_tile_height ();
      end = MIN (end, info->yres);
      scanlines = end - start;
      d = data;

      for (i = 0; i < scanlines; i++)
	for (x = 0; x < info->xres; x++)
	  {
	    for (b = 0; b < np; b++)
	      {
		/* Truncated files will just have all 0's at the end of the images */
		CHECK_FOR_ERROR(pnmscanner_eof(scan), info->jmpbuf,
				_("PNM: Premature end of file."));
		if (info->np)
		  pnmscanner_gettoken(scan, buf, BUFLEN);
		else
		  pnmscanner_getsmalltoken(scan, buf);
		switch (info->maxval)
		  {
		  case 255:
		    d[b] = g_ascii_isdigit (*buf) ? atoi (buf) : 0;
		    break;
		  case 1:
		    d[b] = (*buf=='0')?0xff:0x00;
		    break;
		  default:
		    d[b] = (255.0 * (((gdouble)(g_ascii_isdigit (*buf) ? atoi (buf) : 0))
                                     / (gdouble)(info->maxval)));
		  }
	      }

	    d += np;
	  }

      gimp_progress_update ((double) y / (double) info->yres);
      gimp_pixel_rgn_set_rect (pixel_rgn, data, 0, y, info->xres, scanlines);
      y += scanlines;
    }

  g_free (data);
}

static void
pnm_load_raw (PNMScanner *scan,
	      PNMInfo    *info,
	      GimpPixelRgn  *pixel_rgn)
{
  guchar *data, *d;
  gint    x, y, i;
  gint    start, end, scanlines;
  gint    fd;

  data = g_malloc (gimp_tile_height () * info->xres * info->np);
  fd = pnmscanner_fd(scan);

  for (y = 0; y < info->yres; )
    {
      start = y;
      end = y + gimp_tile_height ();
      end = MIN (end, info->yres);
      scanlines = end - start;
      d = data;

      for (i = 0; i < scanlines; i++)
	{
	  CHECK_FOR_ERROR((info->xres*info->np
			   != read(fd, d, info->xres*info->np)),
			  info->jmpbuf,
			  _("PNM: Premature end of file."));

	  if (info->maxval != 255)	/* Normalize if needed */
	    {
	      for (x = 0; x < info->xres * info->np; x++)
		{
                  d[x] = MIN (d[x], info->maxval); /* guard against overflow */
                  d[x] = 255.0 * (gdouble) d[x] / (gdouble) info->maxval;
                }
            }

	  d += info->xres * info->np;
	}

      gimp_progress_update ((double) y / (double) info->yres);
      gimp_pixel_rgn_set_rect (pixel_rgn, data, 0, y, info->xres, scanlines);
      y += scanlines;
    }

  g_free (data);
}

static void
pnm_load_rawpbm (PNMScanner *scan,
		 PNMInfo    *info,
		 GimpPixelRgn  *pixel_rgn)
{
  unsigned char *buf;
  unsigned char  curbyte;
  unsigned char *data, *d;
  int            x, y, i;
  int            start, end, scanlines;
  int            fd;
  int            rowlen, bufpos;

  fd = pnmscanner_fd(scan);
  rowlen = (int)ceil((double)(info->xres)/8.0);
  data = g_malloc (gimp_tile_height () * info->xres);
  buf = g_new (unsigned char, rowlen);

  for (y = 0; y < info->yres; )
    {
      start = y;
      end = y + gimp_tile_height ();
      end = MIN (end, info->yres);
      scanlines = end - start;
      d = data;

      for (i = 0; i < scanlines; i++)
	{
	  CHECK_FOR_ERROR((rowlen != read(fd, buf, rowlen)),
			  info->jmpbuf, _("PNM: Error reading file."));
	  bufpos = 0;
	  curbyte = buf[0];

	  for (x = 0; x < info->xres; x++)
	    {
	      if ((x % 8) == 0)
		curbyte = buf[bufpos++];
	      d[x] = (curbyte&0x80) ? 0x00 : 0xff;
	      curbyte <<= 1;
	    }

	  d += info->xres;
	}

      gimp_progress_update ((double) y / (double) info->yres);
      gimp_pixel_rgn_set_rect (pixel_rgn, data, 0, y, info->xres, scanlines);
      y += scanlines;
    }

  g_free(buf);
  g_free (data);
}

/* Writes out RGB and greyscale raw rows */
static void
pnmsaverow_raw (PNMRowInfo    *ri,
		unsigned char *data)
{
  write(ri->fd, data, ri->xres*ri->np);
}

/* Writes out indexed raw rows */
static void
pnmsaverow_raw_indexed (PNMRowInfo    *ri,
			unsigned char *data)
{
  int i;
  char *rbcur = ri->rowbuf;

  for (i = 0; i < ri->xres; i++)
    {
      *(rbcur++) = ri->red[*data];
      *(rbcur++) = ri->grn[*data];
      *(rbcur++) = ri->blu[*(data++)];
    }
  write(ri->fd, ri->rowbuf, ri->xres*3);
}

/* Writes out RGB and greyscale ascii rows */
static void
pnmsaverow_ascii (PNMRowInfo    *ri,
		  unsigned char *data)
{
  int i;
  char *rbcur = ri->rowbuf;

  for (i = 0; i < ri->xres*ri->np; i++)
    {
      sprintf((char *) rbcur,"%d\n", 0xff & *(data++));
      rbcur += strlen(rbcur);
    }
  write(ri->fd, ri->rowbuf, strlen((char *) ri->rowbuf));
}

/* Writes out RGB and greyscale ascii rows */
static void
pnmsaverow_ascii_indexed (PNMRowInfo    *ri,
			  unsigned char *data)
{
  int i;
  char *rbcur = ri->rowbuf;

  for (i = 0; i < ri->xres; i++)
    {
      sprintf((char *) rbcur,"%d\n", 0xff & ri->red[*(data)]);
      rbcur += strlen(rbcur);
      sprintf((char *) rbcur,"%d\n", 0xff & ri->grn[*(data)]);
      rbcur += strlen(rbcur);
      sprintf((char *) rbcur,"%d\n", 0xff & ri->blu[*(data++)]);
      rbcur += strlen(rbcur);
    }
  write(ri->fd, ri->rowbuf, strlen((char *) ri->rowbuf));
}

static gint
save_image (const gchar *filename,
	    gint32       image_ID,
	    gint32       drawable_ID)
{
  GimpPixelRgn pixel_rgn;
  GimpDrawable *drawable;
  GimpImageType drawable_type;
  PNMRowInfo rowinfo;
  void (*saverow) (PNMRowInfo *, unsigned char *) = NULL;
  unsigned char red[256];
  unsigned char grn[256];
  unsigned char blu[256];
  unsigned char *data, *d;
  char *rowbuf;
  char buf[BUFLEN];
  char *temp;
  int np = 0;
  int xres, yres;
  int ypos, yend;
  int rowbufsize = 0;
  int fd;

  drawable = gimp_drawable_get (drawable_ID);
  drawable_type = gimp_drawable_type (drawable_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable,
		       0, 0, drawable->width, drawable->height, FALSE, FALSE);

  /*  Make sure we're not saving an image with an alpha channel  */
  if (gimp_drawable_has_alpha (drawable_ID))
    {
      g_message (_("PNM save cannot handle images with alpha channels."));
      return FALSE;
    }

  /* open the file */
  fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC | _O_BINARY, 0644);

  if (fd == -1)
    {
      g_message (_("Could not open '%s' for writing: %s"),
                 gimp_filename_to_utf8 (filename), g_strerror (errno));
      return FALSE;
    }

  temp = g_strdup_printf (_("Saving '%s'..."),
                          gimp_filename_to_utf8 (filename));
  gimp_progress_init (temp);
  g_free (temp);

  xres = drawable->width;
  yres = drawable->height;

  /* write out magic number */
  if (psvals.raw == FALSE)
    switch (drawable_type)
      {
      case GIMP_GRAY_IMAGE:
	write(fd, "P2\n", 3);
	np = 1;
	rowbufsize = xres * 4;
	saverow = pnmsaverow_ascii;
	break;
      case GIMP_RGB_IMAGE:
	write(fd, "P3\n", 3);
	np = 3;
	rowbufsize = xres * 12;
	saverow = pnmsaverow_ascii;
	break;
      case GIMP_INDEXED_IMAGE:
	write(fd, "P3\n", 3);
	np = 1;
	rowbufsize = xres * 12;
	saverow = pnmsaverow_ascii_indexed;
	break;
      default:
	g_warning ("pnm: unknown drawable_type\n");
	return FALSE;
      }
  else if (psvals.raw == TRUE)
    switch (drawable_type)
      {
      case GIMP_GRAY_IMAGE:
	write(fd, "P5\n", 3);
	np = 1;
	rowbufsize = xres;
	saverow = pnmsaverow_raw;
	break;
      case GIMP_RGB_IMAGE:
	write(fd, "P6\n", 3);
	np = 3;
	rowbufsize = xres * 3;
	saverow = pnmsaverow_raw;
	break;
      case GIMP_INDEXED_IMAGE:
	write(fd, "P6\n", 3);
	np = 1;
	rowbufsize = xres * 3;
	saverow = pnmsaverow_raw_indexed;
	break;
      default:
	g_warning ("pnm: unknown drawable_type\n");
	return FALSE;
    }

  if (drawable_type == GIMP_INDEXED_IMAGE)
    {
      gint    i;
      guchar *cmap;
      gint    colors;

      cmap = gimp_image_get_colormap (image_ID, &colors);

      for (i = 0; i < colors; i++)
	{
	  red[i] = *cmap++;
	  grn[i] = *cmap++;
	  blu[i] = *cmap++;
	}

      rowinfo.red = red;
      rowinfo.grn = grn;
      rowinfo.blu = blu;
    }

  /* allocate a buffer for retrieving information from the pixel region  */
  data = g_new (guchar, gimp_tile_height () * drawable->width * drawable->bpp);

  /* write out comment string */
  write (fd, SAVE_COMMENT_STRING, strlen (SAVE_COMMENT_STRING));

  /* write out resolution and maxval */
  sprintf (buf, "%d %d\n255\n", xres, yres);
  write (fd, buf, strlen(buf));

  rowbuf = g_malloc (rowbufsize + 1);
  rowinfo.fd = fd;
  rowinfo.rowbuf = rowbuf;
  rowinfo.xres = xres;
  rowinfo.np = np;

  d = NULL; /* only to please the compiler */

  /* Write the body out */
  for (ypos = 0; ypos < yres; ypos++)
    {
      if ((ypos % gimp_tile_height ()) == 0)
	{
	  yend = ypos + gimp_tile_height ();
	  yend = MIN (yend, yres);
	  gimp_pixel_rgn_get_rect (&pixel_rgn, data,
				   0, ypos, xres, (yend - ypos));
	  d = data;
	}

      (*saverow)(&rowinfo, d);
      d += xres*np;

      if ((ypos % 20) == 0)
	gimp_progress_update ((double) ypos / (double) yres);
    }

  /* close the file */
  close (fd);

  g_free (rowbuf);
  g_free (data);

  gimp_drawable_detach (drawable);

  return TRUE;
}

static gint
save_dialog (void)
{
  GtkWidget *dlg;
  GtkWidget *frame;
  gboolean   run;

  dlg = gimp_dialog_new (_("Save as PNM"), "pnm",
                         NULL, 0,
			 gimp_standard_help_func, "file-pnm-save",

			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK,     GTK_RESPONSE_OK,

			 NULL);

  /*  file save type  */
  frame = gimp_int_radio_group_new (TRUE, _("Data formatting"),
				    G_CALLBACK (gimp_radio_button_update),
				    &psvals.raw, psvals.raw,

				    _("Raw"),   TRUE,  NULL,
				    _("Ascii"), FALSE, NULL,

				    NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), frame, FALSE, TRUE, 0);

  gtk_widget_show (frame);

  gtk_widget_show (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dlg);

  return run;
}


/**************** FILE SCANNER UTILITIES **************/

/* pnmscanner_create ---
 *    Creates a new scanner based on a file descriptor.  The
 *    look ahead buffer is one character initially.
 */
static PNMScanner *
pnmscanner_create (gint fd)
{
  PNMScanner *s;

  s = g_new (PNMScanner, 1);
  s->fd = fd;
  s->inbuf = NULL;
  s->eof = !read(s->fd, &(s->cur), 1);
  return(s);
}

/* pnmscanner_destroy ---
 *    Destroys a scanner and its resources.  Doesn't close the fd.
 */
static void
pnmscanner_destroy (PNMScanner *s)
{
  if (s->inbuf) g_free(s->inbuf);
  g_free(s);
}

/* pnmscanner_createbuffer ---
 *    Creates a buffer so we can do buffered reads.
 */
static void
pnmscanner_createbuffer (PNMScanner *s,
			 gint        bufsize)
{
  s->inbuf = g_new (char, bufsize);
  s->inbufsize = bufsize;
  s->inbufpos = 0;
  s->inbufvalidsize = read(s->fd, s->inbuf, bufsize);
}

/* pnmscanner_gettoken ---
 *    Gets the next token, eating any leading whitespace.
 */
static void
pnmscanner_gettoken (PNMScanner *s,
		     gchar      *buf,
		     gint        bufsize)
{
  int ctr=0;

  pnmscanner_eatwhitespace (s);
  while (! s->eof                   &&
         ! g_ascii_isspace (s->cur) &&
         (s->cur != '#')            &&
         (ctr < bufsize))
    {
      buf[ctr++] = s->cur;
      pnmscanner_getchar(s);
    }
  buf[ctr] = '\0';
}

/* pnmscanner_getsmalltoken ---
 *    Gets the next char, eating any leading whitespace.
 */
static void
pnmscanner_getsmalltoken (PNMScanner *s,
			  gchar      *buf)
{
  pnmscanner_eatwhitespace(s);
  if (!(s->eof) && !g_ascii_isspace (s->cur) && (s->cur != '#'))
    {
      *buf = s->cur;
      pnmscanner_getchar(s);
    }
}

/* pnmscanner_getchar ---
 *    Reads a character from the input stream
 */
static void
pnmscanner_getchar (PNMScanner *s)
{
  if (s->inbuf)
    {
      s->cur = s->inbuf[s->inbufpos++];
      if (s->inbufpos >= s->inbufvalidsize)
	{
	  if (s->inbufsize > s->inbufvalidsize)
	    s->eof = 1;
	  else
	    s->inbufvalidsize = read(s->fd, s->inbuf, s->inbufsize);
	  s->inbufpos = 0;
	}
    }
  else
    s->eof = !read(s->fd, &(s->cur), 1);
}

/* pnmscanner_eatwhitespace ---
 *    Eats up whitespace from the input and returns when done or eof.
 *    Also deals with comments.
 */
static void
pnmscanner_eatwhitespace (PNMScanner *s)
{
  int state = 0;

  while (!(s->eof) && (state != -1))
    {
      switch (state)
	{
	case 0:  /* in whitespace */
	  if (s->cur == '#')
	    {
	      state = 1;  /* goto comment */
	      pnmscanner_getchar(s);
	    }
	  else if (!g_ascii_isspace (s->cur))
	    state = -1;
	  else
	    pnmscanner_getchar(s);
	  break;

	case 1:  /* in comment */
	  if (s->cur == '\n')
	    state = 0;  /* goto whitespace */
	  pnmscanner_getchar(s);
	  break;
	}
    }
}
