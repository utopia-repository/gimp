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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* SVG loader for The GIMP
 * (C) Copyright 2003  Dom Lachowicz <cinamod@hotmail.com>
 *
 * Largely rewritten in September 2003 by Sven Neumann <sven@gimp.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <librsvg/rsvg.h>

#include <gtk/gtk.h>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#include "libgimp/stdplugins-intl.h"


#define SVG_VERSION             "2.5.0"
#define SVG_DEFAULT_RESOLUTION  90.0
#define SVG_DEFAULT_SIZE        500
#define SVG_PREVIEW_SIZE        128


typedef struct
{
  gdouble    resolution;
  gint       width;
  gint       height;
  gboolean   import;
  gboolean   merge;
} SvgLoadVals;

static SvgLoadVals load_vals =
{
  SVG_DEFAULT_RESOLUTION,
  0,
  0,
  FALSE,
  FALSE
};


static void  query (void);
static void  run   (const gchar      *name,
                    gint              nparams,
                    const GimpParam  *param,
                    gint             *nreturn_vals,
                    GimpParam       **return_vals);

static gint32      load_image        (const gchar  *filename);
static GdkPixbuf * load_rsvg_pixbuf  (const gchar  *filename,
                                      SvgLoadVals  *vals,
                                      GError      **error);
static gboolean    load_rsvg_size    (const gchar  *filename,
                                      SvgLoadVals  *vals,
                                      GError      **error);
static gboolean    load_dialog       (const gchar  *filename);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run,
};

MAIN ()


static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode",     "Interactive, non-interactive"        },
    { GIMP_PDB_STRING, "filename",     "The name of the file to load"        },
    { GIMP_PDB_STRING, "raw_filename", "The name of the file to load"        },
    { GIMP_PDB_FLOAT,  "resolution",
      "Resolution to use for rendering the SVG (defaults to 72 dpi"          },
    { GIMP_PDB_INT32,  "width",
      "Width (in pixels) to load the SVG in. "
      "(0 for original width, a negative width to specify a maximum width)"  },
    { GIMP_PDB_INT32,  "height",
      "Height (in pixels) to load the SVG in. "
      "(0 for original height, a negative width to specify a maximum height)"},
    { GIMP_PDB_INT32,  "paths",
      "Whether to not import paths (0), import paths individually (1) "
      "or merge all imported paths (2)"                                      }
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE,  "image",        "Output image" }
  };

  static GimpParamDef thumb_args[] =
  {
    { GIMP_PDB_STRING, "filename",     "The name of the file to load"  },
    { GIMP_PDB_INT32,  "thumb_size",   "Preferred thumbnail size"      }
  };
  static GimpParamDef thumb_return_vals[] =
  {
    { GIMP_PDB_IMAGE,  "image",        "Thumbnail image"               },
    { GIMP_PDB_INT32,  "image_width",  "Width of full-sized image"     },
    { GIMP_PDB_INT32,  "image_height", "Height of full-sized image"    }
  };

  gimp_install_procedure ("file_svg_load",
                          "Loads files in the SVG file format",
                          "Renders SVG files to raster graphics using librsvg.",
                          "Dom Lachowicz, Sven Neumann",
                          "Dom Lachowicz <cinamod@hotmail.com>",
                          SVG_VERSION,
			  N_("Scalable SVG image"),
			  NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (load_args),
                          G_N_ELEMENTS (load_return_vals),
                          load_args, load_return_vals);

  gimp_register_file_handler_mime ("file_svg_load", "image/svg+xml");
  gimp_register_magic_load_handler ("file_svg_load",
				    "svg", "",
				    "0,string,<?xml,0,string,<svg");

  gimp_install_procedure ("file_svg_load_thumb",
                          "Loads a small preview from an SVG image",
                          "",
                          "Dom Lachowicz, Sven Neumann",
                          "Dom Lachowicz <cinamod@hotmail.com>",
                          SVG_VERSION,
			  NULL,
			  NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (thumb_args),
                          G_N_ELEMENTS (thumb_return_vals),
                          thumb_args, thumb_return_vals);

  gimp_register_thumbnail_loader ("file_svg_load", "file_svg_load_thumb");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[4];
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  /* MUST call this before any RSVG funcs */
  g_type_init ();

  if (strcmp (name, "file_svg_load") == 0)
    {
      gimp_get_data ("file_svg_load", &load_vals);

      switch (run_mode)
        {
        case GIMP_RUN_NONINTERACTIVE:
          if (nparams > 3)  load_vals.resolution = param[3].data.d_float;
          if (nparams > 4)  load_vals.width      = param[4].data.d_int32;
          if (nparams > 5)  load_vals.height     = param[5].data.d_int32;
          if (nparams > 6)
            {
              load_vals.import = param[6].data.d_int32 != FALSE;
              load_vals.merge  = param[6].data.d_int32 > TRUE;
            }
          break;

        case GIMP_RUN_INTERACTIVE:
	  if (!load_dialog (param[1].data.d_string))
	    status = GIMP_PDB_CANCEL;
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          break;
	}

      if (load_vals.resolution < GIMP_MIN_RESOLUTION ||
          load_vals.resolution > GIMP_MAX_RESOLUTION)
        {
          load_vals.resolution = SVG_DEFAULT_RESOLUTION;
        }

      if (status == GIMP_PDB_SUCCESS)
	{
	  const gchar *filename = param[1].data.d_string;
          gint32       image_ID = load_image (filename);

	  if (image_ID != -1)
	    {
              if (load_vals.import)
                gimp_path_import (image_ID, filename, load_vals.merge, TRUE);

	      *nreturn_vals = 2;
	      values[1].type         = GIMP_PDB_IMAGE;
	      values[1].data.d_image = image_ID;
	    }
	  else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }

	  gimp_set_data ("file_svg_load", &load_vals, sizeof (load_vals));
        }
    }
  else if (strcmp (name, "file_svg_load_thumb") == 0)
    {
      if (nparams < 2)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      else
        {
          const gchar *filename = param[0].data.d_string;
          gint         width    = 0;
          gint         height   = 0;
          gint32       image_ID;

          if (load_rsvg_size (filename, &load_vals, NULL))
            {
              width  = load_vals.width;
              height = load_vals.height;
            }

          load_vals.resolution = SVG_DEFAULT_RESOLUTION;
          load_vals.width      = - param[1].data.d_int32;
          load_vals.height     = - param[1].data.d_int32;

          image_ID = load_image (filename);

          if (image_ID != -1)
            {
	      *nreturn_vals = 4;
	      values[1].type         = GIMP_PDB_IMAGE;
	      values[1].data.d_image = image_ID;
	      values[2].type         = GIMP_PDB_INT32;
	      values[2].data.d_int32 = width;
	      values[3].type         = GIMP_PDB_INT32;
	      values[3].data.d_int32 = height;
            }
          else
            {
              status = GIMP_PDB_EXECUTION_ERROR;
            }
        }
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
  gint32        image;
  gint32	layer;
  GimpDrawable *drawable;
  GimpPixelRgn	rgn;
  GdkPixbuf    *pixbuf;
  gchar        *pixels;
  gint          width;
  gint          height;
  gint          rowstride;
  gint          bpp;
  gpointer      pr;
  GError       *error = NULL;

  pixbuf = load_rsvg_pixbuf (filename, &load_vals, &error);
  if (!pixbuf)
    {
      /*  Do not rely on librsvg setting GError on failure!  */
      g_message (_("Could not open '%s' for reading: %s"),
                 gimp_filename_to_utf8 (filename),
                 error ? error->message : _("Unknown reason"));
      gimp_quit ();
    }

  gimp_progress_init (_("Rendering SVG..."));

  width  = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  image = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_set_filename (image, filename);
  gimp_image_set_resolution (image,
                             load_vals.resolution, load_vals.resolution);

  layer = gimp_layer_new (image, _("Rendered SVG"), width, height,
                          GIMP_RGBA_IMAGE, 100, GIMP_NORMAL_MODE);

  drawable = gimp_drawable_get (layer);

  gimp_pixel_rgn_init (&rgn, drawable, 0, 0, width, height, TRUE, FALSE);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  bpp       = gdk_pixbuf_get_n_channels (pixbuf);
  pixels    = gdk_pixbuf_get_pixels (pixbuf);

  g_assert (bpp == rgn.bpp);

  for (pr = gimp_pixel_rgns_register (1, &rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      const guchar *src;
      guchar       *dest;
      gint          y;

      src  = pixels + rgn.y * rowstride + rgn.x * bpp;
      dest = rgn.data;

      for (y = 0; y < rgn.h; y++)
        {
          memcpy (dest, src, rgn.w * rgn.bpp);

          src  += rowstride;
          dest += rgn.rowstride;
        }
    }

  gimp_drawable_detach (drawable);
  g_object_unref (pixbuf);

  gimp_progress_update (1.0);

  gimp_image_add_layer (image, layer, 0);

  return image;
}

/*  This is the callback used from load_rsvg_pixbuf().  */
static void
load_set_size_callback (gint     *width,
                        gint     *height,
                        gpointer  data)
{
  SvgLoadVals *vals = data;

  if (*width < 1 || *height < 1)
    {
      *width  = SVG_DEFAULT_SIZE;
      *height = SVG_DEFAULT_SIZE;
    }

  if (!vals->width || !vals->height)
    return;

  /*  either both arguments negative or none  */
  if ((vals->width * vals->height) < 0)
    return;

  if (vals->width > 0)
    {
      *width  = vals->width;
      *height = vals->height;
    }
  else
    {
      gdouble w      = *width;
      gdouble h      = *height;
      gdouble aspect = (gdouble) vals->width / (gdouble) vals->height;

      if (aspect > (w / h))
        {
          *height = abs (vals->height);
          *width  = (gdouble) abs (vals->width) * (w / h) + 0.5;
        }
      else
        {
          *width  = abs (vals->width);
          *height = (gdouble) abs (vals->height) / (w / h) + 0.5;
        }

      vals->width  = *width;
      vals->height = *height;
    }
}


/*  This function renders a pixbuf from an SVG file according to vals.  */
static GdkPixbuf *
load_rsvg_pixbuf (const gchar  *filename,
                  SvgLoadVals  *vals,
                  GError      **error)
{
  GdkPixbuf  *pixbuf  = NULL;
  RsvgHandle *handle;
  GIOChannel *io;
  GIOStatus   status  = G_IO_STATUS_NORMAL;
  gboolean    success = TRUE;

  io = g_io_channel_new_file (filename, "r", error);
  if (!io)
    return NULL;

  g_io_channel_set_encoding (io, NULL, NULL);

  handle = rsvg_handle_new ();

  rsvg_handle_set_dpi (handle, vals->resolution);
  rsvg_handle_set_size_callback (handle, load_set_size_callback, vals, NULL);

  while (success && status != G_IO_STATUS_EOF)
    {
      guchar buf[8192];
      gsize  len;

      status = g_io_channel_read_chars (io, buf, sizeof (buf), &len, error);

      switch (status)
        {
        case G_IO_STATUS_ERROR:
          success = FALSE;
          break;
        case G_IO_STATUS_EOF:
          success = rsvg_handle_close (handle, error);
          break;
        case G_IO_STATUS_NORMAL:
          success = rsvg_handle_write (handle, buf, len, error);
          break;
        case G_IO_STATUS_AGAIN:
          break;
        }
    }

  g_io_channel_unref (io);

  if (success)
    pixbuf = rsvg_handle_get_pixbuf (handle);

  rsvg_handle_free (handle);

  return pixbuf;
}

static GtkWidget *size_label = NULL;

/*  This is the callback used from load_rsvg_size().  */
static void
load_get_size_callback (gint     *width,
                        gint     *height,
                        gpointer  data)
{
  SvgLoadVals *vals = data;

  if (*width < 1 || *height < 1)
    {
      *width  = SVG_DEFAULT_SIZE;
      *height = SVG_DEFAULT_SIZE;

      if (size_label)
        gtk_label_set_text (GTK_LABEL (size_label),
                            _("SVG file does not\nspecify a size!"));
    }
  else
    {
      if (size_label)
        {
          gchar *text = g_strdup_printf (_("%d x %d"), *width, *height);

          gtk_label_set_text (GTK_LABEL (size_label), text);
          g_free (text);
        }
    }

  vals->width  = *width;
  vals->height = *height;

  /*  cancel loading  */
  vals->resolution = 0.0;
}

/*  This function retrieves the pixel size from an SVG file. Parsing
 *  stops after the first chunk that provided the parser with enough
 *  information to determine the size. This is usally the opening
 *  <svg> element and should thus be in the first chunk (1024 bytes).
 */
static gboolean
load_rsvg_size (const gchar  *filename,
                SvgLoadVals  *vals,
                GError      **error)
{
  RsvgHandle *handle;
  GIOChannel *io;
  GIOStatus   status  = G_IO_STATUS_NORMAL;
  gboolean    success = TRUE;

  io = g_io_channel_new_file (filename, "r", error);
  if (!io)
    return FALSE;

  g_io_channel_set_encoding (io, NULL, NULL);

  handle = rsvg_handle_new ();

  rsvg_handle_set_dpi (handle, vals->resolution);
  rsvg_handle_set_size_callback (handle, load_get_size_callback, vals, NULL);

  while (success && status != G_IO_STATUS_EOF && vals->resolution > 0.0)
    {
      guchar buf[1024];
      gsize  len;

      status = g_io_channel_read_chars (io, buf, sizeof (buf), &len, error);

      switch (status)
        {
        case G_IO_STATUS_ERROR:
          success = FALSE;
          break;
        case G_IO_STATUS_EOF:
          success = rsvg_handle_close (handle, error);
          break;
        case G_IO_STATUS_NORMAL:
          success = rsvg_handle_write (handle, buf, len, error);
          break;
        case G_IO_STATUS_AGAIN:
          break;
        }
    }

  g_io_channel_unref (io);
  rsvg_handle_free (handle);

  if (vals->width  < 1)  vals->width  = 1;
  if (vals->height < 1)  vals->height = 1;

  return success;
}


/*  User interface  */

static GimpSizeEntry *size       = NULL;
static GtkObject     *xadj       = NULL;
static GtkObject     *yadj       = NULL;
static GtkWidget     *constrain  = NULL;
static gdouble        ratio_x    = 1.0;
static gdouble        ratio_y    = 1.0;
static gint           svg_width  = 0;
static gint           svg_height = 0;

static void  load_dialog_set_ratio (gdouble x,
                                    gdouble y);


static void
load_dialog_size_callback (GtkWidget *widget,
                           gpointer   data)
{
  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (constrain)))
    {
      gdouble x = gimp_size_entry_get_refval (size, 0) / (gdouble) svg_width;
      gdouble y = gimp_size_entry_get_refval (size, 1) / (gdouble) svg_height;

      if (x != ratio_x)
        {
          load_dialog_set_ratio (x, x);
        }
      else if (y != ratio_y)
        {
          load_dialog_set_ratio (y, y);
        }
    }
}

static void
load_dialog_ratio_callback (GtkAdjustment *adj,
                            gpointer       data)
{
  gdouble x = gtk_adjustment_get_value (GTK_ADJUSTMENT (xadj));
  gdouble y = gtk_adjustment_get_value (GTK_ADJUSTMENT (yadj));

  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (constrain)))
    {
      if (x != ratio_x)
        y = x;
      else
        x = y;
    }

  load_dialog_set_ratio (x, y);
}

static void
load_dialog_resolution_callback (GimpSizeEntry *res,
                                 const gchar   *filename)
{
  SvgLoadVals  vals = { 0.0, 0, 0 };

  load_vals.resolution = vals.resolution = gimp_size_entry_get_refval (res, 0);

  if (!load_rsvg_size (filename, &vals, NULL))
    return;

  svg_width  = vals.width;
  svg_height = vals.height;

  load_dialog_set_ratio (ratio_x, ratio_y);
}

static void
load_dialog_set_ratio (gdouble x,
                       gdouble y)
{
  ratio_x = x;
  ratio_y = y;

  g_signal_handlers_block_by_func (size, load_dialog_size_callback, NULL);

  gimp_size_entry_set_refval (size, 0, svg_width  * x);
  gimp_size_entry_set_refval (size, 1, svg_height * y);

  g_signal_handlers_unblock_by_func (size, load_dialog_size_callback, NULL);

  g_signal_handlers_block_by_func (xadj, load_dialog_ratio_callback, NULL);
  g_signal_handlers_block_by_func (yadj, load_dialog_ratio_callback, NULL);

  gtk_adjustment_set_value (GTK_ADJUSTMENT (xadj), x);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (yadj), y);

  g_signal_handlers_unblock_by_func (xadj, load_dialog_ratio_callback, NULL);
  g_signal_handlers_unblock_by_func (yadj, load_dialog_ratio_callback, NULL);
}

static gboolean
load_dialog (const gchar *filename)
{
  GtkWidget *dialog;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *image;
  GdkPixbuf *preview;
  GtkWidget *table;
  GtkWidget *table2;
  GtkWidget *abox;
  GtkWidget *res;
  GtkWidget *label;
  GtkWidget *spinbutton;
  GtkWidget *toggle;
  GtkWidget *toggle2;
  GtkObject *adj;
  gboolean   run;
  GError    *error = NULL;

  SvgLoadVals  vals = { SVG_DEFAULT_RESOLUTION,
                        - SVG_PREVIEW_SIZE, - SVG_PREVIEW_SIZE };

  preview = load_rsvg_pixbuf (filename, &vals, &error);

  if (!preview)
    {
      /*  Do not rely on librsvg setting GError on failure!  */
      g_message (_("Could not open '%s' for reading: %s"),
                 gimp_filename_to_utf8 (filename),
                 error ? error->message : _("Unknown reason"));
      return FALSE;
    }

  gimp_ui_init ("svg", FALSE);

  /* Scalable Vector Graphics is SVG, should perhaps not be translated */
  dialog = gimp_dialog_new (_("Render Scalable Vector Graphics"), "svg",
                            NULL, 0,
                            gimp_standard_help_func, "file-svg-load",

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /*  The SVG preview  */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), abox, FALSE, FALSE, 0);
  gtk_widget_show (abox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (abox), frame);
  gtk_widget_show (frame);

  image = gtk_image_new_from_pixbuf (preview);
  gtk_container_add (GTK_CONTAINER (frame), image);
  gtk_widget_show (image);

  size_label = gtk_label_new (NULL);
  gtk_label_set_justify (GTK_LABEL (size_label), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (size_label), 0.5, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), size_label, TRUE, TRUE, 4);
  gtk_widget_show (size_label);

  /*  query the initial size after the size label is created  */
  vals.resolution = load_vals.resolution;

  load_rsvg_size (filename, &vals, NULL);

  svg_width  = vals.width;
  svg_height = vals.height;

  table = gtk_table_new (7, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 2, 2);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  /*  Width and Height  */
  label = gtk_label_new (_("Width:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Height:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  spinbutton = gimp_spin_button_new (&adj, 1, 1, 1, 1, 10, 1, 1, 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), 10);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  size = GIMP_SIZE_ENTRY (gimp_size_entry_new (1, GIMP_UNIT_PIXEL, "%a",
                                               TRUE, FALSE, FALSE, 10,
                                               GIMP_SIZE_ENTRY_UPDATE_SIZE));
  gtk_table_set_col_spacing (GTK_TABLE (size), 1, 6);

  gimp_size_entry_add_field (size, GTK_SPIN_BUTTON (spinbutton), NULL);

  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (size), FALSE, FALSE, 0);
  gtk_widget_show (GTK_WIDGET (size));

  gimp_size_entry_set_refval_boundaries (size, 0,
                                         GIMP_MIN_IMAGE_SIZE,
                                         GIMP_MAX_IMAGE_SIZE);
  gimp_size_entry_set_refval_boundaries (size, 1,
                                         GIMP_MIN_IMAGE_SIZE,
                                         GIMP_MAX_IMAGE_SIZE);

  gimp_size_entry_set_refval (size, 0, svg_width);
  gimp_size_entry_set_refval (size, 1, svg_height);

  gimp_size_entry_set_resolution (size, 0, load_vals.resolution, FALSE);
  gimp_size_entry_set_resolution (size, 1, load_vals.resolution, FALSE);

  g_signal_connect (size, "value_changed",
		    G_CALLBACK (load_dialog_size_callback),
                    NULL);

  /*  Scale ratio  */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 2, 4,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  table2 = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table2), 0, 2);
  gtk_table_set_row_spacing (GTK_TABLE (table2), 0, 4);
  gtk_box_pack_start (GTK_BOX (hbox), table2, FALSE, FALSE, 0);

  spinbutton =
    gimp_spin_button_new (&xadj,
                          ratio_x,
                          (gdouble) GIMP_MIN_IMAGE_SIZE / (gdouble) svg_width,
                          (gdouble) GIMP_MAX_IMAGE_SIZE / (gdouble) svg_width,
                          0.01, 0.1, 1,
                          0.01, 4);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), 10);
  gtk_table_attach_defaults (GTK_TABLE (table2), spinbutton, 0, 1, 0, 1);
  gtk_widget_show (spinbutton);

  g_signal_connect (xadj, "value_changed",
		    G_CALLBACK (load_dialog_ratio_callback),
		    NULL);

  label = gtk_label_new_with_mnemonic (_("_X ratio:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  spinbutton =
    gimp_spin_button_new (&yadj,
                          ratio_y,
                          (gdouble) GIMP_MIN_IMAGE_SIZE / (gdouble) svg_height,
                          (gdouble) GIMP_MAX_IMAGE_SIZE / (gdouble) svg_height,
                          0.01, 0.1, 1,
                          0.01, 4);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), 10);
  gtk_table_attach_defaults (GTK_TABLE (table2), spinbutton, 0, 1, 1, 2);
  gtk_widget_show (spinbutton);

  g_signal_connect (yadj, "value_changed",
		    G_CALLBACK (load_dialog_ratio_callback),
		    NULL);

  label = gtk_label_new_with_mnemonic (_("_Y ratio:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
		    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /*  the constrain ratio chainbutton  */
  constrain = gimp_chain_button_new (GIMP_CHAIN_RIGHT);
  gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (constrain), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table2), constrain, 1, 2, 0, 2);
  gtk_widget_show (constrain);

  gimp_help_set_help_data (GIMP_CHAIN_BUTTON (constrain)->button,
                           _("Constrain aspect ratio"), NULL);

  gtk_widget_show (table2);

  /*  Resolution   */
  label = gtk_label_new (_("Resolution:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  res = gimp_size_entry_new (1, GIMP_UNIT_INCH, _("pixels/%a"),
                             FALSE, FALSE, FALSE, 10,
                             GIMP_SIZE_ENTRY_UPDATE_RESOLUTION);
  gtk_table_set_col_spacing (GTK_TABLE (res), 1, 6);

  gtk_table_attach (GTK_TABLE (table), res, 1, 2, 4, 5,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (res);

  /* don't let the resolution become too small, librsvg tends to
     crash with very small resolutions */
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (res), 0,
                                         5.0, GIMP_MAX_RESOLUTION);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (res), 0, load_vals.resolution);

  g_signal_connect (res, "value-changed",
                    G_CALLBACK (load_dialog_resolution_callback),
                    (gpointer) filename);

  /*  Path Import  */
  toggle = gtk_check_button_new_with_mnemonic (_("Import _paths"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 2, 5, 6,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
                           _("Import path elements of the SVG so they "
                             "can be used with the GIMP path tool"),
                           NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), load_vals.import);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &load_vals.import);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_sensitive_update),
                    NULL);

  toggle2 = gtk_check_button_new_with_mnemonic (_("Merge imported paths"));
  gtk_table_attach (GTK_TABLE (table), toggle2, 0, 2, 6, 7,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_set_sensitive (toggle2, load_vals.import);
  gtk_widget_show (toggle2);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle2), load_vals.merge);
  g_signal_connect (toggle2, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &load_vals.merge);

  g_object_set_data (G_OBJECT (toggle), "set_sensitive", toggle2);


  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  if (run)
    {
      load_vals.width  = ROUND (gimp_size_entry_get_refval (size, 0));
      load_vals.height = ROUND (gimp_size_entry_get_refval (size, 1));
    }

  gtk_widget_destroy (dialog);

  return run;
}

