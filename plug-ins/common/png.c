/*
 * "$Id: png.c,v 1.118 2004/11/23 14:28:43 mitch Exp $"
 *
 *   Portable Network Graphics (PNG) plug-in for The GIMP -- an image
 *   manipulation program
 *
 *   Copyright 1997-1998 Michael Sweet (mike@easysw.com) and
 *   Daniel Skarda (0rfelyus@atrey.karlin.mff.cuni.cz).
 *   and 1999-2000 Nick Lamb (njl195@zepler.org.uk)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Contents:
 *
 *   main()                      - Main entry - just call gimp_main()...
 *   query()                     - Respond to a plug-in query...
 *   run()                       - Run the plug-in...
 *   load_image()                - Load a PNG image into a new image window.
 *   respin_cmap()               - Re-order a Gimp colormap for PNG tRNS
 *   save_image()                - Save the specified image to a PNG file.
 *   save_compression_callback() - Update the image compression level.
 *   save_interlace_update()     - Update the interlacing option.
 *   save_dialog()               - Pop up the save dialog.
 *
 * Revision History:
 *
 *   see ChangeLog
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <png.h>                /* PNG library definitions */

#include "libgimp/stdplugins-intl.h"


/*
 * Constants...
 */

#define PLUG_IN_VERSION        "1.3.4 - 03 September 2002"
#define SCALE_WIDTH            125

#define DEFAULT_GAMMA          2.20

#define RESPONSE_LOAD_DEFAULTS 1
#define RESPONSE_SAVE_DEFAULTS 2

#define PNG_DEFAULTS_PARASITE  "png-save-defaults"

/*
 * Structures...
 */

typedef struct
{
  gboolean  interlaced;
  gboolean  bkgd;
  gboolean  gama;
  gboolean  offs;
  gboolean  phys;
  gboolean  time;
  gboolean  comment;
  gboolean  save_transp_pixels;
  gint      compression_level;
}
PngSaveVals;

typedef struct
{
  gboolean   run;

  GtkWidget *interlaced;
  GtkWidget *bkgd;
  GtkWidget *gama;
  GtkWidget *offs;
  GtkWidget *phys;
  GtkWidget *time;
  GtkWidget *comment;
  GtkWidget *save_transp_pixels;
  GtkObject *compression_level;
}
PngSaveGui;


/*
 * Local functions...
 */

static void      query                     (void);
static void      run                       (const gchar      *name,
                                            gint              nparams,
                                            const GimpParam  *param,
                                            gint             *nreturn_vals,
                                            GimpParam       **return_vals);

static gint32    load_image                (const gchar      *filename,
                                            gboolean          interactive);
static gint      save_image                (const gchar      *filename,
                                            gint32            image_ID,
                                            gint32            drawable_ID,
                                            gint32            orig_image_ID);

static void      respin_cmap               (png_structp       pp,
                                            png_infop         info,
                                            guchar           *remap,
                                            gint32            image_ID,
                                            GimpDrawable     *drawable);

static gboolean  save_dialog               (gint32            image_ID,
                                            gboolean          alpha);

static void      save_dialog_response      (GtkWidget        *widget,
                                            gint              response_id,
                                            gpointer          data);

static gboolean  ia_has_transparent_pixels (guchar           *pixels,
                                            gint              numpixels);

static int       find_unused_ia_colour     (guchar           *pixels,
                                            gint              numpixels,
                                            gint             *colors);

static gboolean  load_defaults             (void);
static void      save_defaults             (void);
static void      load_gui_defaults         (PngSaveGui       *pg);

/*
 * Globals...
 */

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run
};

PngSaveVals pngvals =
{
  FALSE,
  TRUE,
  FALSE,
  FALSE,
  TRUE,
  TRUE,
  TRUE,
  TRUE,
  9
};

/*
 * 'main()' - Main entry - just call gimp_main()...
 */

MAIN ()


/*
 * 'query()' - Respond to a plug-in query...
 */

static void
query (void)
{
  static GimpParamDef load_args[] = {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_STRING, "filename", "The name of the file to load"},
    {GIMP_PDB_STRING, "raw_filename", "The name of the file to load"}
  };
  static GimpParamDef load_return_vals[] = {
    {GIMP_PDB_IMAGE, "image", "Output image"}
  };

#define COMMON_SAVE_ARGS \
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"}, \
    {GIMP_PDB_IMAGE, "image", "Input image"},                     \
    {GIMP_PDB_DRAWABLE, "drawable", "Drawable to save"},          \
    {GIMP_PDB_STRING, "filename",                                 \
     "The name of the file to save the image in"},                \
    {GIMP_PDB_STRING, "raw_filename",                             \
     "The name of the file to save the image in"}

#define OLD_CONFIG_ARGS \
    {GIMP_PDB_INT32, "interlace", "Use Adam7 interlacing?"},              \
    {GIMP_PDB_INT32, "compression", "Deflate Compression factor (0--9)"}, \
    {GIMP_PDB_INT32, "bkgd", "Write bKGD chunk?"},                        \
    {GIMP_PDB_INT32, "gama", "Write gAMA chunk?"},                        \
    {GIMP_PDB_INT32, "offs", "Write oFFs chunk?"},                        \
    {GIMP_PDB_INT32, "phys", "Write pHYs chunk?"},                        \
    {GIMP_PDB_INT32, "time", "Write tIME chunk?"}

#define FULL_CONFIG_ARGS \
    OLD_CONFIG_ARGS,                                                      \
    {GIMP_PDB_INT32, "comment", "Write comment?"},                        \
    {GIMP_PDB_INT32, "svtrans", "Preserve color of transparent pixels?"}

  static GimpParamDef save_args[] = {
    COMMON_SAVE_ARGS,
    OLD_CONFIG_ARGS
  };

  static GimpParamDef save_args2[] = {
    COMMON_SAVE_ARGS,
    FULL_CONFIG_ARGS
  };

  static GimpParamDef save_args_defaults[] = {
    COMMON_SAVE_ARGS
  };

  static GimpParamDef save_get_defaults_return_vals[] = {
    FULL_CONFIG_ARGS
  };

  static GimpParamDef save_args_set_defaults[] = {
    FULL_CONFIG_ARGS
  };

  gimp_install_procedure ("file_png_load",
                          "Loads files in PNG file format",
                          "This plug-in loads Portable Network Graphics (PNG) files.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          N_("PNG image"),
                          NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (load_args),
                          G_N_ELEMENTS (load_return_vals),
                          load_args, load_return_vals);

  gimp_register_file_handler_mime ("file_png_load", "image/png");
  gimp_register_magic_load_handler ("file_png_load",
                                    "png", "", "0,string,\211PNG\r\n\032\n");

  gimp_install_procedure ("file_png_save",
                          "Saves files in PNG file format",
                          "This plug-in saves Portable Network Graphics (PNG) files.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          N_("PNG image"),
                          "RGB*,GRAY*,INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_register_file_handler_mime ("file_png_save", "image/png");

  gimp_install_procedure ("file_png_save2",
                          "Saves files in PNG file format",
                          "This plug-in saves Portable Network Graphics (PNG) files. "
                          "This procedure adds 2 extra parameters to file_png_save that allows to control whether image comments are saved and whether transparent pixels are saved or nullified.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          N_("PNG image"),
                          "RGB*,GRAY*,INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args2), 0,
                          save_args2, NULL);

  gimp_register_file_handler_mime ("file_png_save2", "image/png");

  gimp_install_procedure ("file_png_save_defaults",
                          "Saves files in PNG file format",
                          "This plug-in saves Portable Network Graphics (PNG) files, using the default settings stored as a parasite.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          N_("PNG image"),
                          "RGB*,GRAY*,INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args_defaults), 0,
                          save_args_defaults, NULL);

  gimp_register_file_handler_mime ("file_png_save_defaults", "image/png");
  gimp_register_save_handler ("file_png_save_defaults", "png", "");

  gimp_install_procedure ("file_png_get_defaults",
                          "Get the current set of defaults used by the PNG file save plug-in",
                          "This procedure returns the current set of defaults stored as a parasite for the PNG save plug-in. "
                          "These defaults are used to seed the UI, by the file_png_save_defaults procedure, and by gimp_file_save when it detects to use PNG.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          NULL,
                          NULL,
                          GIMP_PLUGIN,
                          0, G_N_ELEMENTS (save_get_defaults_return_vals),
                          NULL, save_get_defaults_return_vals);

  gimp_install_procedure ("file_png_set_defaults",
                          "Set the current set of defaults used by the PNG file save plug-in",
                          "This procedure set the current set of defaults stored as a parasite for the PNG save plug-in. "
                          "These defaults are used to seed the UI, by the file_png_save_defaults procedure, and by gimp_file_save when it detects to use PNG.",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>",
                          "Michael Sweet <mike@easysw.com>, Daniel Skarda <0rfelyus@atrey.karlin.mff.cuni.cz>, Nick Lamb <njl195@zepler.org.uk>",
                          PLUG_IN_VERSION,
                          NULL,
			  NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args_set_defaults), 0,
                          save_args_set_defaults, NULL);
}


/*
 * 'run()' - Run the plug-in...
 */

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam  values[10];
  GimpRunMode       run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32            image_ID;
  gint32            drawable_ID;
  gint32            orig_image_ID;
  GimpExportReturn  export = GIMP_EXPORT_CANCEL;

  INIT_I18N ();

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  if (strcmp (name, "file_png_load") == 0)
    {
      run_mode = param[0].data.d_int32;

      image_ID = load_image (param[1].data.d_string,
                             run_mode == GIMP_RUN_INTERACTIVE);

      if (image_ID != -1)
        {
          *nreturn_vals = 2;
          values[1].type = GIMP_PDB_IMAGE;
          values[1].data.d_image = image_ID;
        }
      else
        {
          status = GIMP_PDB_EXECUTION_ERROR;
        }
    }
  else if (strcmp (name, "file_png_save")  == 0 ||
           strcmp (name, "file_png_save2") == 0 ||
           strcmp (name, "file_png_save_defaults") == 0)
    {
      gboolean alpha;

      run_mode    = param[0].data.d_int32;
      image_ID    = orig_image_ID = param[1].data.d_int32;
      drawable_ID = param[2].data.d_int32;

      load_defaults ();

      /*  eventually export the image */
      switch (run_mode)
        {
        case GIMP_RUN_INTERACTIVE:
        case GIMP_RUN_WITH_LAST_VALS:
          gimp_ui_init ("png", FALSE);
          export = gimp_export_image (&image_ID, &drawable_ID, "PNG",
                                      (GIMP_EXPORT_CAN_HANDLE_RGB |
                                       GIMP_EXPORT_CAN_HANDLE_GRAY |
                                       GIMP_EXPORT_CAN_HANDLE_INDEXED |
                                       GIMP_EXPORT_CAN_HANDLE_ALPHA));
          if (export == GIMP_EXPORT_CANCEL)
            {
              *nreturn_vals = 1;
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
          /*
           * Possibly retrieve data...
           */
          gimp_get_data ("file_png_save", &pngvals);

          alpha = gimp_drawable_has_alpha (drawable_ID);

	  /*
	   * If the image has no transparency, then there is usually
	   * no need to save a bKGD chunk.  For more information, see:
	   * http://bugzilla.gnome.org/show_bug.cgi?id=92395
	   */
	  if (! alpha)
	    pngvals.bkgd = FALSE;

          /*
           * Then acquire information with a dialog...
           */
          if (! save_dialog (orig_image_ID, alpha))
            status = GIMP_PDB_CANCEL;
          break;

        case GIMP_RUN_NONINTERACTIVE:
          /*
           * Make sure all the arguments are there!
           */
	  if (nparams != 5)
            {
              if (nparams != 12 && nparams != 14)
                {
                  status = GIMP_PDB_CALLING_ERROR;
                }
              else
                {
                  pngvals.interlaced        = param[5].data.d_int32;
                  pngvals.compression_level = param[6].data.d_int32;
                  pngvals.bkgd              = param[7].data.d_int32;
                  pngvals.gama              = param[8].data.d_int32;
                  pngvals.offs              = param[9].data.d_int32;
                  pngvals.phys              = param[10].data.d_int32;
                  pngvals.time              = param[11].data.d_int32;

                  if (nparams == 14)
                    {
                      pngvals.comment            = param[12].data.d_int32;
                      pngvals.save_transp_pixels = param[13].data.d_int32;
                    }
                  else
                    {
                      pngvals.comment            = TRUE;
                      pngvals.save_transp_pixels = TRUE;
                    }

                  if (pngvals.compression_level < 0 ||
                      pngvals.compression_level > 9)
                    status = GIMP_PDB_CALLING_ERROR;
                }
            }
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          /*
           * Possibly retrieve data...
           */
          gimp_get_data ("file_png_save", &pngvals);
          break;

        default:
          break;
        }

      if (status == GIMP_PDB_SUCCESS)
        {
          if (save_image (param[3].data.d_string,
                          image_ID, drawable_ID, orig_image_ID))
            {
              gimp_set_data ("file_png_save", &pngvals, sizeof (pngvals));
            }
          else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }
        }

      if (export == GIMP_EXPORT_EXPORT)
        gimp_image_delete (image_ID);
    }
  else if (strcmp (name, "file_png_get_defaults") == 0)
    {
      load_defaults ();

      *nreturn_vals = 9;

#define SET_VALUE(index, field)	G_STMT_START { \
 values[(index)].type = GIMP_PDB_INT32;        \
 values[(index)].data.d_int32 = pngvals.field; \
} G_STMT_END

      SET_VALUE (1, interlaced);
      SET_VALUE (2, compression_level);
      SET_VALUE (3, bkgd);
      SET_VALUE (4, gama);
      SET_VALUE (5, offs);
      SET_VALUE (6, phys);
      SET_VALUE (7, time);
      SET_VALUE (8, comment);
      SET_VALUE (9, save_transp_pixels);

#undef SET_VALUE
    }
  else if (strcmp (name, "file_png_set_defaults") == 0)
    {
      if (nparams == 10)
        {
          pngvals.interlaced          = param[1].data.d_int32;
          pngvals.compression_level   = param[2].data.d_int32;
          pngvals.bkgd                = param[3].data.d_int32;
          pngvals.gama                = param[4].data.d_int32;
          pngvals.offs                = param[5].data.d_int32;
          pngvals.phys                = param[6].data.d_int32;
          pngvals.time                = param[7].data.d_int32;
          pngvals.comment             = param[8].data.d_int32;
          pngvals.save_transp_pixels  = param[9].data.d_int32;

          save_defaults ();
        }
      else
        status = GIMP_PDB_CALLING_ERROR;
    }
  else
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;
}


/* Try to find a colour in the palette which isn't actually
 * used in the image, so that we can use it as the transparency
 * index. Taken from gif.c */
static gint
find_unused_ia_colour (guchar *pixels,
                       gint    numpixels,
                       gint   *colors)
{
  gint     i;
  gboolean ix_used[256];
  gboolean trans_used = FALSE;

  for (i = 0; i < *colors; i++)
    {
      ix_used[i] = FALSE;
    }

  for (i = 0; i < numpixels; i++)
    {
      /* If alpha is over a threshold, the colour index in the
       * palette is taken. Otherwise, this pixel is transparent. */
      if (pixels[i * 2 + 1] > 127)
        ix_used[pixels[i * 2]] = TRUE;
      else
        trans_used = TRUE;
    }

  /* If there is no transparency, ignore alpha. */
  if (trans_used == FALSE)
    return -1;

  for (i = 0; i < *colors; i++)
    {
      if (ix_used[i] == FALSE)
        {
          return i;
        }
    }

  /* Couldn't find an unused colour index within the number of
     bits per pixel we wanted.  Will have to increment the number
     of colours in the image and assign a transparent pixel there. */
  if ((*colors) < 256)
    {
      (*colors)++;
      return ((*colors) - 1);
    }

  return (-1);
}


/*
 * 'load_image()' - Load a PNG image into a new image window.
 */

static gint32
load_image (const gchar *filename,
            gboolean     interactive)
{
  int i,                        /* Looping var */
    trns,                       /* Transparency present */
    bpp,                        /* Bytes per pixel */
    image_type,                 /* Type of image */
    layer_type,                 /* Type of drawable/layer */
    empty,                      /* Number of fully transparent indices */
    num_passes,                 /* Number of interlace passes in file */
    pass,                       /* Current pass in file */
    tile_height,                /* Height of tile in GIMP */
    begin,                      /* Beginning tile row */
    end,                        /* Ending tile row */
    num;                        /* Number of rows to load */
  FILE *fp;                     /* File pointer */
  volatile gint32 image;        /* Image -- preserved against setjmp() */
  gint32 layer;                 /* Layer */
  GimpDrawable *drawable;       /* Drawable for layer */
  GimpPixelRgn pixel_rgn;       /* Pixel region for layer */
  png_structp pp;               /* PNG read pointer */
  png_infop info;               /* PNG info pointers */
  guchar **pixels,              /* Pixel rows */
   *pixel;                      /* Pixel data */
  gchar *progress;              /* Title for progress display... */
  guchar alpha[256],            /* Index -> Alpha */
   *alpha_ptr;                  /* Temporary pointer */

  png_textp  text;
  gint       num_texts;

  /*
   * PNG 0.89 and newer have a sane, forwards compatible constructor.
   * Some SGI IRIX users will not have a new enough version though
   */

#if PNG_LIBPNG_VER > 88
  pp = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info = png_create_info_struct (pp);
#else
  pp = (png_structp) calloc (sizeof (png_struct), 1);
  png_read_init (pp);

  info = (png_infop) calloc (sizeof (png_info), 1);
#endif /* PNG_LIBPNG_VER > 88 */

  if (setjmp (pp->jmpbuf))
    {
      g_message (_("Error while reading '%s'. File corrupted?"),
                 gimp_filename_to_utf8 (filename));
      return image;
    }

  /* initialise image here, thus avoiding compiler warnings */

  image = -1;

  /*
   * Open the file and initialize the PNG read "engine"...
   */

  fp = fopen (filename, "rb");

  if (fp == NULL)
    {
      g_message (_("Could not open '%s' for reading: %s"),
                 gimp_filename_to_utf8 (filename), g_strerror (errno));
      gimp_quit ();
    }

  png_init_io (pp, fp);

  progress = g_strdup_printf (_("Opening '%s'..."),
                              gimp_filename_to_utf8 (filename));
  gimp_progress_init (progress);
  g_free (progress);

  /*
   * Get the image dimensions and create the image...
   */

  png_read_info (pp, info);

  /*
   * Latest attempt, this should be my best yet :)
   */

  if (info->bit_depth == 16)
    {
      png_set_strip_16 (pp);
    }

  if (info->color_type == PNG_COLOR_TYPE_GRAY && info->bit_depth < 8)
    {
      png_set_expand (pp);
    }

  if (info->color_type == PNG_COLOR_TYPE_PALETTE && info->bit_depth < 8)
    {
      png_set_packing (pp);
    }

  /*
   * Expand G+tRNS to GA, RGB+tRNS to RGBA
   */

  if (info->color_type != PNG_COLOR_TYPE_PALETTE &&
      (info->valid & PNG_INFO_tRNS))
    {
      png_set_expand (pp);
    }

  /*
   * Turn on interlace handling... libpng returns just 1 (ie single pass)
   * if the image is not interlaced
   */

  num_passes = png_set_interlace_handling (pp);

  /*
   * Special handling for INDEXED + tRNS (transparency palette)
   */

#if PNG_LIBPNG_VER > 99
  if (png_get_valid (pp, info, PNG_INFO_tRNS) &&
      info->color_type == PNG_COLOR_TYPE_PALETTE)
    {
      png_get_tRNS (pp, info, &alpha_ptr, &num, NULL);
      /* Copy the existing alpha values from the tRNS chunk */
      for (i = 0; i < num; ++i)
        alpha[i] = alpha_ptr[i];
      /* And set any others to fully opaque (255)  */
      for (i = num; i < 256; ++i)
        alpha[i] = 255;
      trns = 1;
    }
  else
    {
      trns = 0;
    }
#else
  trns = 0;
#endif /* PNG_LIBPNG_VER > 99 */

  /*
   * Update the info structures after the transformations take effect
   */

  png_read_update_info (pp, info);

  switch (info->color_type)
    {
    case PNG_COLOR_TYPE_RGB:           /* RGB */
      bpp = 3;
      image_type = GIMP_RGB;
      layer_type = GIMP_RGB_IMAGE;
      break;

    case PNG_COLOR_TYPE_RGB_ALPHA:     /* RGBA */
      bpp = 4;
      image_type = GIMP_RGB;
      layer_type = GIMP_RGBA_IMAGE;
      break;

    case PNG_COLOR_TYPE_GRAY:          /* Grayscale */
      bpp = 1;
      image_type = GIMP_GRAY;
      layer_type = GIMP_GRAY_IMAGE;
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:    /* Grayscale + alpha */
      bpp = 2;
      image_type = GIMP_GRAY;
      layer_type = GIMP_GRAYA_IMAGE;
      break;

    case PNG_COLOR_TYPE_PALETTE:       /* Indexed */
      bpp = 1;
      image_type = GIMP_INDEXED;
      layer_type = GIMP_INDEXED_IMAGE;
      break;
    default:                   /* Aie! Unknown type */
      g_message (_("Unknown color model in PNG file '%s'."),
                 gimp_filename_to_utf8 (filename));
      return -1;
    };

  image = gimp_image_new (info->width, info->height, image_type);
  if (image == -1)
    {
      g_message ("Could not create new image for '%s'",
                 gimp_filename_to_utf8 (filename));
      gimp_quit ();
    };

  /*
   * Create the "background" layer to hold the image...
   */

  layer = gimp_layer_new (image, _("Background"), info->width, info->height,
                          layer_type, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image, layer, 0);

  /*
   * Find out everything we can about the image resolution
   * This is only practical with the new 1.0 APIs, I'm afraid
   * due to a bug in libpng-1.0.6, see png-implement for details
   */

#if PNG_LIBPNG_VER > 99
  if (png_get_valid (pp, info, PNG_INFO_gAMA))
    {
      GimpParasite *parasite;
      gchar         buf[G_ASCII_DTOSTR_BUF_SIZE];
      gdouble       gamma;

      png_get_gAMA (pp, info, &gamma);

      g_ascii_dtostr (buf, sizeof (buf), gamma);

      parasite = gimp_parasite_new ("gamma",
                                    GIMP_PARASITE_PERSISTENT,
                                    strlen (buf) + 1, buf);
      gimp_image_parasite_attach (image, parasite);
      gimp_parasite_free (parasite);
    }

  if (png_get_valid (pp, info, PNG_INFO_oFFs))
    {
      gint offset_x = png_get_x_offset_pixels (pp, info);
      gint offset_y = png_get_y_offset_pixels (pp, info);

      gimp_layer_set_offsets (layer, offset_x, offset_y);

      if ((abs (offset_x) > info->width) || (abs (offset_y) > info->height))
        {
          if (interactive)
            g_message (_("The PNG file specifies an offset that caused "
                         "the layer to be positioned outside the image."));
        }
    }

  if (png_get_valid (pp, info, PNG_INFO_pHYs))
    {
      png_uint_32  xres;
      png_uint_32  yres;
      gint         unit_type;

      if (png_get_pHYs (pp, info, &xres, &yres, &unit_type))
        {
          switch (unit_type)
            {
            case PNG_RESOLUTION_UNKNOWN:
              {
                gdouble image_xres, image_yres;

                gimp_image_get_resolution (image, &image_xres, &image_yres);

                if (xres > yres)
                  image_xres = image_yres * (gdouble) xres / (gdouble) yres;
                else
                  image_yres = image_xres * (gdouble) yres / (gdouble) xres;

                gimp_image_set_resolution (image, image_xres, image_yres);
              }
              break;

            case PNG_RESOLUTION_METER:
              gimp_image_set_resolution (image,
                                         (gdouble) xres * 0.0254,
                                         (gdouble) yres * 0.0254);
              break;

            default:
              break;
            }
        }

    }
#endif /* PNG_LIBPNG_VER > 99 */

  gimp_image_set_filename (image, filename);

  /*
   * Load the colormap as necessary...
   */

  empty = 0;                    /* by default assume no full transparent palette entries */

  if (info->color_type & PNG_COLOR_MASK_PALETTE)
    {

#if PNG_LIBPNG_VER > 99
      if (png_get_valid (pp, info, PNG_INFO_tRNS))
        {
          for (empty = 0; empty < 256 && alpha[empty] == 0; ++empty)
            /* Calculates number of fully transparent "empty" entries */;

          /*  keep at least one entry  */
          empty = MIN (empty, info->num_palette - 1);

          gimp_image_set_colormap (image, (guchar *) (info->palette + empty),
                                   info->num_palette - empty);
        }
      else
        {
          gimp_image_set_colormap (image, (guchar *) info->palette,
                                   info->num_palette);
        }
#else
      gimp_image_set_colormap (image, (guchar *) info->palette,
                               info->num_palette);
#endif /* PNG_LIBPNG_VER > 99 */

    }

  /*
   * Get the drawable and set the pixel region for our load...
   */

  drawable = gimp_drawable_get (layer);

  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width,
                       drawable->height, TRUE, FALSE);

  /*
   * Temporary buffer...
   */

  tile_height = gimp_tile_height ();
  pixel = g_new (guchar, tile_height * info->width * bpp);
  pixels = g_new (guchar *, tile_height);

  for (i = 0; i < tile_height; i++)
    pixels[i] = pixel + info->width * info->channels * i;

  for (pass = 0; pass < num_passes; pass++)
    {
      /*
       * This works if you are only reading one row at a time...
       */

      for (begin = 0, end = tile_height;
           begin < info->height; begin += tile_height, end += tile_height)
        {
          if (end > info->height)
            end = info->height;

          num = end - begin;

          if (pass != 0)        /* to handle interlaced PiNGs */
            gimp_pixel_rgn_get_rect (&pixel_rgn, pixel, 0, begin,
                                     drawable->width, num);

          png_read_rows (pp, pixels, NULL, num);

          gimp_pixel_rgn_set_rect (&pixel_rgn, pixel, 0, begin,
                                   drawable->width, num);

          gimp_progress_update (((double) pass +
                                 (double) end / (double) info->height) /
                                (double) num_passes);
        };
    };

  png_read_end (pp, info);

  if (png_get_text (pp, info, &text, &num_texts))
    {
      gchar *comment = NULL;

      for (i = 0; i < num_texts && !comment; i++)
	{
	  if (text->key == NULL || strcmp (text->key, "Comment"))
	    continue;

	  if (text->text_length > 0)   /*  tEXt  */
	    {
	      comment = g_convert (text->text, -1,
				   "UTF-8", "ISO-8859-1",
				   NULL, NULL, NULL);
	    }
	  else if (g_utf8_validate (text->text, -1, NULL))
	    {                          /*  iTXt  */
	      comment = g_strdup (text->text);
	    }
	}

      if (comment && *comment)
	{
	  GimpParasite *parasite;

	  parasite = gimp_parasite_new ("gimp-comment",
					GIMP_PARASITE_PERSISTENT,
					strlen (comment) + 1, comment);
	  gimp_image_parasite_attach (image, parasite);
	  gimp_parasite_free (parasite);
	}

      g_free (comment);
    }

  /*
   * Done with the file...
   */

  png_read_destroy (pp, info, NULL);

  g_free (pixel);
  g_free (pixels);
  free (pp);
  free (info);

  fclose (fp);

  if (trns)
    {
      gimp_layer_add_alpha (layer);
      drawable = gimp_drawable_get (layer);
      gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width,
                           drawable->height, TRUE, FALSE);

      pixel = g_new (guchar, tile_height * drawable->width * 2);        /* bpp == 1 */

      for (begin = 0, end = tile_height;
           begin < drawable->height; begin += tile_height, end += tile_height)
        {
          if (end > drawable->height)
            end = drawable->height;
          num = end - begin;

          gimp_pixel_rgn_get_rect (&pixel_rgn, pixel, 0, begin,
                                   drawable->width, num);

          for (i = 0; i < tile_height * drawable->width; ++i)
            {
              pixel[i * 2 + 1] = alpha[pixel[i * 2]];
              pixel[i * 2] -= empty;
            }

          gimp_pixel_rgn_set_rect (&pixel_rgn, pixel, 0, begin,
                                   drawable->width, num);
        }
      g_free (pixel);
    }

  /*
   * Update the display...
   */

  gimp_drawable_flush (drawable);
  gimp_drawable_detach (drawable);

  return (image);
}


/*
 * 'save_image ()' - Save the specified image to a PNG file.
 */

static gint
save_image (const gchar *filename,
            gint32       image_ID,
            gint32       drawable_ID,
            gint32       orig_image_ID)
{
  int i, k,                     /* Looping vars */
    bpp = 0,                    /* Bytes per pixel */
    type,                       /* Type of drawable/layer */
    num_passes,                 /* Number of interlace passes in file */
    pass,                       /* Current pass in file */
    tile_height,                /* Height of tile in GIMP */
    begin,                      /* Beginning tile row */
    end,                        /* Ending tile row */
    num;                        /* Number of rows to load */
  FILE *fp;                     /* File pointer */
  GimpDrawable *drawable;       /* Drawable for layer */
  GimpPixelRgn pixel_rgn;       /* Pixel region for layer */
  png_structp pp;               /* PNG read pointer */
  png_infop info;               /* PNG info pointer */
  gint num_colors;              /* Number of colors in colormap */
  gint offx, offy;              /* Drawable offsets from origin */
  guchar **pixels,              /* Pixel rows */
   *fixed,                      /* Fixed-up pixel data */
   *pixel;                      /* Pixel data */
  gchar *progress;              /* Title for progress display... */
  gdouble xres, yres;           /* GIMP resolution (dpi) */
  png_color_16 background;      /* Background color */
  png_time mod_time;            /* Modification time (ie NOW) */
  guchar red, green, blue;      /* Used for palette background */
  time_t cutime;                /* Time since epoch */
  struct tm *gmt;               /* GMT broken down */

  guchar remap[256];            /* Re-mapping for the palette */

  png_textp  text = NULL;

  if (pngvals.comment)
    {
      GimpParasite *parasite;

      parasite = gimp_image_parasite_find (orig_image_ID, "gimp-comment");
      if (parasite)
        {
          gchar *comment = g_strndup (gimp_parasite_data (parasite),
                                      gimp_parasite_data_size (parasite));

          gimp_parasite_free (parasite);

          text = g_new0 (png_text, 1);
          text->key         = "Comment";

#ifdef PNG_iTXt_SUPPORTED

          text->compression = PNG_ITXT_COMPRESSION_NONE;
          text->text        = comment;
          text->itxt_length = strlen (comment);

#else

          text->compression = PNG_TEXT_COMPRESSION_NONE;
          text->text        = g_convert (comment, -1,
                                         "ISO-8859-1", "UTF-8",
                                         NULL, &text->text_length,
                                         NULL);

#endif

          if (!text->text)
            {
              g_free (text);
              text = NULL;
            }
        }
    }

  /*
   * PNG 0.89 and newer have a sane, forwards compatible constructor.
   * Some SGI IRIX users will not have a new enough version though
   */

#if PNG_LIBPNG_VER > 88
  pp = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info = png_create_info_struct (pp);
#else
  pp = (png_structp) calloc (sizeof (png_struct), 1);
  png_write_init (pp);

  info = (png_infop) calloc (sizeof (png_info), 1);
#endif /* PNG_LIBPNG_VER > 88 */

  if (setjmp (pp->jmpbuf))
    {
      g_message (_("Error while saving '%s'. Could not save image."),
                 gimp_filename_to_utf8 (filename));
      return 0;
    }

  if (text)
    png_set_text (pp, info, text, 1);

  /*
   * Open the file and initialize the PNG write "engine"...
   */

  fp = fopen (filename, "wb");
  if (fp == NULL)
    {
      g_message (_("Could not open '%s' for writing: %s"),
                 gimp_filename_to_utf8 (filename), g_strerror (errno));
      return 0;
    }

  png_init_io (pp, fp);

  progress = g_strdup_printf (_("Saving '%s'..."),
                              gimp_filename_to_utf8 (filename));
  gimp_progress_init (progress);
  g_free (progress);

  /*
   * Get the drawable for the current image...
   */

  drawable = gimp_drawable_get (drawable_ID);
  type = gimp_drawable_type (drawable_ID);

  /*
   * Set the image dimensions, bit depth, interlacing and compression
   */

  png_set_compression_level (pp, pngvals.compression_level);

  info->width          = drawable->width;
  info->height         = drawable->height;
  info->bit_depth      = 8;
  info->interlace_type = pngvals.interlaced;

  /*
   * Initialise remap[]
   */
  for (i = 0; i < 256; i++)
    remap[i] = i;

  /*
   * Set color type and remember bytes per pixel count
   */

  switch (type)
    {
    case GIMP_RGB_IMAGE:
      info->color_type = PNG_COLOR_TYPE_RGB;
      bpp = 3;
      break;
    case GIMP_RGBA_IMAGE:
      info->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
      bpp = 4;
      break;
    case GIMP_GRAY_IMAGE:
      info->color_type = PNG_COLOR_TYPE_GRAY;
      bpp = 1;
      break;
    case GIMP_GRAYA_IMAGE:
      info->color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
      bpp = 2;
      break;
    case GIMP_INDEXED_IMAGE:
      bpp = 1;
      info->color_type = PNG_COLOR_TYPE_PALETTE;
      info->valid |= PNG_INFO_PLTE;
      info->palette =
        (png_colorp) gimp_image_get_colormap (image_ID, &num_colors);
      info->num_palette = num_colors;
      break;
    case GIMP_INDEXEDA_IMAGE:
      bpp = 2;
      info->color_type = PNG_COLOR_TYPE_PALETTE;
      /* fix up transparency */
      respin_cmap (pp, info, remap, image_ID, drawable);
      break;
    default:
      g_message ("Image type can't be saved as PNG");
      return 0;
    };

  /*
   * Fix bit depths for (possibly) smaller colormap images
   */

  if (info->valid & PNG_INFO_PLTE)
    {
      if (info->num_palette <= 2)
        info->bit_depth = 1;
      else if (info->num_palette <= 4)
        info->bit_depth = 2;
      else if (info->num_palette <= 16)
        info->bit_depth = 4;
      /* otherwise the default is fine */
    }

  /* All this stuff is optional extras, if the user is aiming for smallest
     possible file size she can turn them all off */

#if PNG_LIBPNG_VER > 99
  if (pngvals.bkgd)
    {
      GimpRGB color;

      gimp_context_get_background (&color);
      gimp_rgb_get_uchar (&color, &red, &green, &blue);

      background.index = 0;
      background.red = red;
      background.green = green;
      background.blue = blue;
      background.gray = gimp_rgb_intensity_uchar (&color);
      png_set_bKGD (pp, info, &background);
    }
  else
    {
      /* used to save_transp_pixels */
      red = green = blue = 0;
    }

  if (pngvals.gama)
    {
      GimpParasite *parasite;
      gdouble       gamma = 1.0 / DEFAULT_GAMMA;

      parasite = gimp_image_parasite_find (orig_image_ID, "gamma");
      if (parasite)
	{
          gamma = g_ascii_strtod (gimp_parasite_data (parasite), NULL);
	  gimp_parasite_free (parasite);
	}

      png_set_gAMA (pp, info, gamma);
    }

  if (pngvals.offs)
    {
      gimp_drawable_offsets (drawable_ID, &offx, &offy);
      if (offx != 0 || offy != 0)
        {
          png_set_oFFs (pp, info, offx, offy, PNG_OFFSET_PIXEL);
        }
    }

  if (pngvals.phys)
    {
      gimp_image_get_resolution (orig_image_ID, &xres, &yres);
      png_set_pHYs (pp, info, RINT (xres / 0.0254), RINT (yres / 0.0254),
                    PNG_RESOLUTION_METER);
    }

  if (pngvals.time)
    {
      cutime = time (NULL);     /* time right NOW */
      gmt = gmtime (&cutime);

      mod_time.year = gmt->tm_year + 1900;
      mod_time.month = gmt->tm_mon + 1;
      mod_time.day = gmt->tm_mday;
      mod_time.hour = gmt->tm_hour;
      mod_time.minute = gmt->tm_min;
      mod_time.second = gmt->tm_sec;
      png_set_tIME (pp, info, &mod_time);
    }

#endif /* PNG_LIBPNG_VER > 99 */

  png_write_info (pp, info);

  /*
   * Turn on interlace handling...
   */

  if (pngvals.interlaced)
    num_passes = png_set_interlace_handling (pp);
  else
    num_passes = 1;

  /*
   * Convert unpacked pixels to packed if necessary
   */

  if (info->color_type == PNG_COLOR_TYPE_PALETTE && info->bit_depth < 8)
    png_set_packing (pp);

  /*
   * Allocate memory for "tile_height" rows and save the image...
   */

  tile_height = gimp_tile_height ();
  pixel = g_new (guchar, tile_height * drawable->width * bpp);
  pixels = g_new (guchar *, tile_height);

  for (i = 0; i < tile_height; i++)
    pixels[i] = pixel + drawable->width * bpp * i;

  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width,
                       drawable->height, FALSE, FALSE);

  for (pass = 0; pass < num_passes; pass++)
    {
      /* This works if you are only writing one row at a time... */
      for (begin = 0, end = tile_height;
           begin < drawable->height; begin += tile_height, end += tile_height)
        {
          if (end > drawable->height)
            end = drawable->height;

          num = end - begin;

          gimp_pixel_rgn_get_rect (&pixel_rgn, pixel, 0, begin,
                                   drawable->width, num);
          /*if we are with a RGBA image and have to pre-multiply the alpha channel */
          if (bpp == 4 && ! pngvals.save_transp_pixels)
            {
              for (i = 0; i < num; ++i)
                {
                  fixed = pixels[i];
                  for (k = 0; k < drawable->width; ++k)
                    {
                      int aux;
                      aux = k << 2;
                      if (! fixed[aux + 3])
                        {
                          fixed[aux] = red;
                          fixed[aux + 1] = green;
                          fixed[aux + 2] = blue;
                        }
                    }
                }
            }

          /* If we're dealing with a paletted image with
           * transparency set, write out the remapped palette */
          if (info->valid & PNG_INFO_tRNS)
            {
              for (i = 0; i < num; ++i)
                {
                  fixed = pixels[i];
                  for (k = 0; k < drawable->width; ++k)
                    {
                      fixed[k] = (fixed[k*2+1] > 127) ?
                                 remap[fixed[k*2]] :
                                 0;
                    }
                }
            }
          /* Otherwise if we have a paletted image and transparency
           * couldn't be set, we ignore the alpha channel */
          else if (info->valid & PNG_INFO_PLTE && bpp == 2)
            {
              for (i = 0; i < num; ++i)
                {
                  fixed = pixels[i];
                  for (k = 0; k < drawable->width; ++k)
                    {
                      fixed[k] = fixed[k * 2];
                    }
                }
            }

          png_write_rows (pp, pixels, num);

          gimp_progress_update (((double) pass + (double) end /
                                 (double) info->height) /
                                (double) num_passes);
        };
    };

  png_write_end (pp, info);
  png_write_destroy (pp);

  g_free (pixel);
  g_free (pixels);

  /*
   * Done with the file...
   */

  if (text)
    {
      g_free (text->text);
      g_free (text);
    }

  free (pp);
  free (info);

  fclose (fp);

  return (1);
}

static gboolean
ia_has_transparent_pixels (guchar *pixels,
                           gint    numpixels)
{
  while (numpixels --)
    {
      if (pixels [1] <= 127)
        return TRUE;
      pixels += 2;
    }
  return FALSE;
}

static void
respin_cmap (png_structp pp,
             png_infop info,
             guchar * remap,
             gint32 image_ID,
             GimpDrawable * drawable)
{
  static const guchar trans[] = { 0 };

  gint          colors;
  guchar       *before;
  gint          transparent;
  gint          cols, rows;
  GimpPixelRgn  pixel_rgn;
  guchar       *pixels;
  gint          numpixels;

  before = gimp_image_get_colormap (image_ID, &colors);

  /*
   * Make sure there is something in the colormap.
   */
  if (colors == 0)
    {
      before = g_new0 (guchar, 3);
      colors = 1;
    }

  cols      = drawable->width;
  rows      = drawable->height;
  numpixels = cols * rows;

  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0,
                       drawable->width, drawable->height, FALSE, FALSE);

  pixels = (guchar *) g_malloc (numpixels * 2);

  gimp_pixel_rgn_get_rect (&pixel_rgn, pixels, 0, 0,
                           drawable->width, drawable->height);


  /* Try to find an entry which isn't actually used in the
     image, for a transparency index. */

#if PNG_LIBPNG_VER > 99
  if (ia_has_transparent_pixels (pixels, numpixels))
    {
      transparent = find_unused_ia_colour (pixels, numpixels, &colors);

      if (transparent != -1)        /* we have a winner for a transparent
                                     * index - do like gif2png and swap
                                     * index 0 and index transparent */
        {
          png_color palette[256];
          gint i;

          png_set_tRNS (pp, info, (png_bytep) trans, 1, NULL);

          /* Transform all pixels with a value = transparent to
           * 0 and vice versa to compensate for re-ordering in palette
           * due to png_set_tRNS() */

          remap[0] = transparent;
          remap[transparent] = 0;

          /* Copy from index 0 to index transparent - 1 to index 1 to
           * transparent of after, then from transparent+1 to colors-1
           * unchanged, and finally from index transparent to index 0. */

          for (i = 0; i < colors; i++)
            {
              palette[i].red = before[3 * remap[i]];
              palette[i].green = before[3 * remap[i] + 1];
              palette[i].blue = before[3 * remap[i] + 2];
            }

          png_set_PLTE (pp, info, palette, colors);
        }
      else
        {
          /* Inform the user that we couldn't losslessly save the
           * transparency & just use the full palette */
          g_message (_("Couldn't losslessly save transparency, "
                       "saving opacity instead."));
          png_set_PLTE (pp, info, (png_colorp) before, colors);
        }
    }
  else
    png_set_PLTE (pp, info, (png_colorp) before, colors);

#else
  info->valid |= PNG_INFO_PLTE;
  info->palette = (png_colorp) before;
  info->num_palette = colors;
#endif /* PNG_LIBPNG_VER > 99 */

  g_free (pixels);

}

static gboolean
save_dialog (gint32    image_ID,
             gboolean  alpha)
{
  PngSaveGui    pg;
  GtkWidget    *dlg;
  GtkWidget    *table;
  GtkWidget    *toggle;
  GtkObject    *scale;
  GimpParasite *parasite;

  dlg = gimp_dialog_new (_("Save as PNG"), "png",
                         NULL, 0,
                         gimp_standard_help_func, "file-png-save-defaults",

			 _("_Load defaults"), RESPONSE_LOAD_DEFAULTS,
			 _("_Save defaults"), RESPONSE_SAVE_DEFAULTS,
                         GTK_STOCK_CANCEL,   GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,       GTK_RESPONSE_OK,

                         NULL);

  g_signal_connect (dlg, "response",
                    G_CALLBACK (save_dialog_response),
                    &pg);
  g_signal_connect (dlg, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  table = gtk_table_new (9, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  pg.interlaced = toggle =
    gtk_check_button_new_with_mnemonic (_("_Interlacing (Adam7)"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                pngvals.interlaced);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &pngvals.interlaced);

  pg.bkgd = toggle =
    gtk_check_button_new_with_mnemonic (_("Save _background color"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.bkgd);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update), &pngvals.bkgd);

  pg.gama = toggle = gtk_check_button_new_with_mnemonic (_("Save _gamma"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.gama);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update), &pngvals.gama);

  pg.offs = toggle =
    gtk_check_button_new_with_mnemonic (_("Save layer o_ffset"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 3, 4, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.offs);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &pngvals.offs);

  pg.phys = toggle = gtk_check_button_new_with_mnemonic (_("Save _resolution"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 4, 5, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.phys);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update), &pngvals.phys);

  pg.time = toggle =
    gtk_check_button_new_with_mnemonic (_("Save creation _time"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 5, 6, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.time);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update), &pngvals.time);

  pg.comment = toggle = gtk_check_button_new_with_mnemonic (_("Save comme_nt"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 6, 7, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), pngvals.comment);
  gtk_widget_show (toggle);

  parasite = gimp_image_parasite_find (image_ID, "gimp-comment");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                pngvals.comment && parasite != NULL);
  gtk_widget_set_sensitive (toggle, parasite != NULL);
  gimp_parasite_free (parasite);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update), &pngvals.comment);

  pg.save_transp_pixels = toggle =
    gtk_check_button_new_with_mnemonic (_("Save color _values from transparent pixels"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 7, 8, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                alpha && pngvals.save_transp_pixels);
  gtk_widget_set_sensitive (toggle, alpha);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &pngvals.save_transp_pixels);

  pg.compression_level = scale =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 8,
                          _("Co_mpression level:"),
                          SCALE_WIDTH, 0,
                          pngvals.compression_level,
                          0.0, 9.0, 1.0, 1.0, 0, TRUE, 0.0, 0.0,
                          _("Choose a high compression level "
                            "for small file size"), NULL);

  g_signal_connect (scale, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &pngvals.compression_level);

  gtk_widget_show (dlg);

  pg.run = FALSE;

  gtk_main ();

  return pg.run;
}

static void
save_dialog_response (GtkWidget *widget,
                      gint       response_id,
                      gpointer   data)
{
  PngSaveGui *pg = data;

  switch (response_id)
    {
    case RESPONSE_LOAD_DEFAULTS:
      load_gui_defaults (pg);
      break;

    case RESPONSE_SAVE_DEFAULTS:
      save_defaults ();
      break;

    case GTK_RESPONSE_OK:
      pg->run = TRUE;

    default:
      gtk_widget_destroy (widget);
      break;
    }
}

static gboolean
load_defaults (void)
{
  GimpParasite *parasite;
  gchar        *def_str;
  PngSaveVals   tmpvals;
  gint          num_fields;

  parasite = gimp_parasite_find (PNG_DEFAULTS_PARASITE);

  if (! parasite)
    return FALSE;

  def_str = g_strndup (gimp_parasite_data (parasite),
                       gimp_parasite_data_size (parasite));

  gimp_parasite_free (parasite);

  num_fields = sscanf (def_str, "%d %d %d %d %d %d %d %d %d",
                       &tmpvals.interlaced,
                       &tmpvals.bkgd,
                       &tmpvals.gama,
                       &tmpvals.offs,
                       &tmpvals.phys,
                       &tmpvals.time,
                       &tmpvals.comment,
                       &tmpvals.save_transp_pixels,
                       &tmpvals.compression_level);

  g_free (def_str);

  if (num_fields == 9)
    {
      memcpy (&pngvals, &tmpvals, sizeof (tmpvals));
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
save_defaults (void)
{
  GimpParasite *parasite;
  gchar        *def_str;

  def_str = g_strdup_printf ("%d %d %d %d %d %d %d %d %d",
                             pngvals.interlaced,
                             pngvals.bkgd,
                             pngvals.gama,
                             pngvals.offs,
                             pngvals.phys,
                             pngvals.time,
                             pngvals.comment,
                             pngvals.save_transp_pixels,
                             pngvals.compression_level);

  parasite = gimp_parasite_new (PNG_DEFAULTS_PARASITE,
                                GIMP_PARASITE_PERSISTENT,
                                strlen (def_str), def_str);

  gimp_parasite_attach (parasite);

  gimp_parasite_free (parasite);
  g_free (def_str);
}

static void
load_gui_defaults (PngSaveGui *pg)
{
  if (! load_defaults ())
    {
      g_message (_("Could not load PNG defaults"));
      return;
    }

#define SET_ACTIVE(field) \
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pg->field), pngvals.field)

  SET_ACTIVE (interlaced);
  SET_ACTIVE (bkgd);
  SET_ACTIVE (gama);
  SET_ACTIVE (offs);
  SET_ACTIVE (phys);
  SET_ACTIVE (time);
  SET_ACTIVE (comment);
  SET_ACTIVE (save_transp_pixels);

#undef SET_ACTIVE

  gtk_adjustment_set_value (GTK_ADJUSTMENT (pg->compression_level),
                            pngvals.compression_level);
}