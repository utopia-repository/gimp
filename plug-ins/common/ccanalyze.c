/*
 * This is a plug-in for the GIMP.
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
 *
 */

/*
 * Analyze colorcube.
 *
 * Author: robert@experimental.net
 */

#include "config.h"

#include <string.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

/* lets prototype */
static void query (void);
static void run   (const gchar      *name,
                   gint              n_params,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static void doDialog    (void);
static void analyze     (GimpDrawable *drawable);

static void histogram   (guchar  r,
                         guchar  g,
                         guchar  b,
                         gdouble a);
static void fillPreview (GtkWidget *preview);
static void insertcolor (guchar  r,
                         guchar  g,
                         guchar  b,
                         gdouble a);

static void doLabel     (GtkWidget  *table,
                         const char *format,
                         ...) G_GNUC_PRINTF (2, 3);

/* some global variables */
static gchar     *filename = NULL;
static gint       width, height, bpp;
static gdouble    hist_red[256], hist_green[256], hist_blue[256];
static gdouble    maxred = 0.0, maxgreen = 0.0, maxblue = 0.0;
static gint       uniques = 0;
static gint32     imageID;

/* size of histogram image */
static const int PREWIDTH = 256;
static const int PREHEIGHT = 150;

/* lets declare what we want to do */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

/* run program */
MAIN ()

/* tell GIMP who we are */
static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",    "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" }
  };

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_INT32, "num_colors", "Number of colors in the image" }
  };

  gimp_install_procedure ("plug_in_ccanalyze",
                          "Colorcube analysis",
                          "Analyze colorcube and print some information about "
                          "the current image (also displays a color-histogram)",
                          "robert@experimental.net",
                          "robert@experimental.net",
                          "June 20th, 1997",
                          N_("Colorcube A_nalysis..."),
                          "RGB*, GRAY*, INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), G_N_ELEMENTS (return_vals),
                          args, return_vals);

  gimp_plugin_menu_register ("plug_in_ccanalyze", "<Image>/Filters/Colors");
  gimp_plugin_menu_register ("plug_in_ccanalyze", "<Image>/Layer/Colors/Info");
}

/* main function */
static void
run (const gchar      *name,
     gint              n_params,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2];
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpDrawable      *drawable;

  run_mode = param[0].data.d_int32;

  INIT_I18N ();

  *nreturn_vals = 2;
  *return_vals = values;

  if (run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      if (n_params != 3)
        status = GIMP_PDB_CALLING_ERROR;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      drawable = gimp_drawable_get (param[2].data.d_drawable);
      imageID = param[1].data.d_image;

      if (gimp_drawable_is_rgb (drawable->drawable_id) ||
          gimp_drawable_is_gray (drawable->drawable_id) ||
          gimp_drawable_is_indexed (drawable->drawable_id))
        {
          memset (hist_red, 0, sizeof (hist_red));
          memset (hist_green, 0, sizeof (hist_green));
          memset (hist_blue, 0, sizeof (hist_blue));

          filename = gimp_image_get_filename (imageID);

          gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

          analyze (drawable);

          /* show dialog after we analyzed image */
          if (run_mode != GIMP_RUN_NONINTERACTIVE)
            doDialog ();
        }
      else
        status = GIMP_PDB_EXECUTION_ERROR;

      gimp_drawable_detach (drawable);
    }

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type          = GIMP_PDB_INT32;
  values[1].data.d_int32  = uniques;
}

/* do the analyzing */
static void
analyze (GimpDrawable *drawable)
{
  GimpPixelRgn  srcPR;
  guchar       *src_row, *cmap;
  gint          x, y, numcol;
  gint          x1, y1, x2, y2;
  guchar        r, g, b;
  gint          a;
  guchar        idx;
  gboolean      gray;
  gboolean      has_alpha;
  gboolean      has_sel;
  guchar       *sel;
  GimpPixelRgn  selPR;
  gint          ofsx, ofsy;
  GimpDrawable *selDrawable;

  gimp_progress_init (_("Colorcube Analysis..."));

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  /*
   * Get the size of the input image (this will/must be the same
   * as the size of the output image).
   */
  width = drawable->width;
  height = drawable->height;
  bpp = drawable->bpp;

  has_sel = !gimp_selection_is_empty (imageID);
  gimp_drawable_offsets (drawable->drawable_id, &ofsx, &ofsy);

  /* initialize the pixel region */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);

  cmap = gimp_image_get_colormap (imageID, &numcol);
  gray = (gimp_drawable_is_gray (drawable->drawable_id)
          || gimp_drawable_is_channel (drawable->drawable_id));
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  selDrawable = gimp_drawable_get (gimp_image_get_selection (imageID));
  gimp_pixel_rgn_init (&selPR,
                       selDrawable,
                       0, 0, width, height, FALSE, FALSE);

  /* allocate row buffer */
  src_row = g_new (guchar, (x2 - x1) * bpp);
  sel = g_new (guchar, x2 - x1);

  for (y = y1; y < y2; y++)
    {
      gimp_pixel_rgn_get_row (&srcPR, src_row, x1, y, (x2 - x1));
      if (has_sel)
        gimp_pixel_rgn_get_row (&selPR, sel, x1 + ofsx, y + ofsy, (x2 - x1));

      for (x = 0; x < x2 - x1; x++)
        {
          /* Start with full opacity.  */
          a = 255;

          /*
           * If the image is indexed, fetch RGB values
           * from colormap.
           */
          if (cmap)
            {
              idx = src_row[x * bpp];

              r = cmap[idx * 3];
              g = cmap[idx * 3 + 1];
              b = cmap[idx * 3 + 2];
              if (has_alpha)
                a = src_row[x * bpp + 1];
            }
          else if (gray)
            {
              r = g = b = src_row[x * bpp];
              if (has_alpha)
                a = src_row[x * bpp + 1];
            }
          else
            {
              r = src_row[x * bpp];
              g = src_row[x * bpp + 1];
              b = src_row[x * bpp + 2];
              if (has_alpha)
                a = src_row[x * bpp + 3];
            }

          if (has_sel)
            a *= sel[x];
          else
            a *= 255;

          if (a != 0)
            insertcolor (r, g, b, (gdouble) a * (1.0 / (255.0 * 255.0)));
        }

      /* tell the user what we're doing */
      if ((y % 10) == 0)
        gimp_progress_update ((gdouble) y / (gdouble) (y2 - y1));
    }

  gimp_progress_update (1.0);

  /* clean up */
  gimp_drawable_detach (selDrawable);
  g_free (src_row);
  g_free (sel);
}

static void
insertcolor (guchar  r,
             guchar  g,
             guchar  b,
             gdouble a)
{
  static GHashTable *hash_table;
  guint key;

  if (!hash_table)
    hash_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  histogram (r, g, b, a);

  key = r + 256 * (g + 256 * b);
  if (g_hash_table_lookup (hash_table, GINT_TO_POINTER (key)))
    {
      return;
    }

  g_hash_table_insert (hash_table, GINT_TO_POINTER (key),
                       GINT_TO_POINTER (1));

  uniques++;
}

/*
 * Update RGB count, and keep track of maximum values (which aren't used
 * anywhere as of yet, but they might be useful sometime).
 */
static void
histogram (guchar  r,
           guchar  g,
           guchar  b,
           gdouble a)
{
  hist_red[r] += a;
  hist_green[g] += a;
  hist_blue[b] += a;

  if (hist_red[r] > maxred)
    maxred = hist_red[r];

  if (hist_green[g] > maxgreen)
    maxgreen = hist_green[g];

  if (hist_blue[b] > maxblue)
    maxblue = hist_blue[b];
}

/* show our results */
static void
doDialog (void)
{
  GtkWidget   *dialog;
  GtkWidget   *vbox;
  GtkWidget   *hbox;
  GtkWidget   *frame;
  GtkWidget   *preview;
  gchar       *memsize;
  struct stat  st;

  gimp_ui_init ("ccanalyze", TRUE);

  dialog = gimp_dialog_new (_("Colorcube Analysis"), "ccanalyze",
                            NULL, 0,
                            gimp_standard_help_func, "plug-in-ccanalyze",

                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,

                            NULL);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
                      TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);

  /* use preview for histogram window */
  preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (preview, PREWIDTH, PREHEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), preview);

  /* output results */
  doLabel (vbox, _("Image dimensions: %d x %d"), width, height);

  if (uniques == 0)
    doLabel (vbox, _("No colors"));
  else if (uniques == 1)
    doLabel (vbox, _("Only one unique color"));
  else
    doLabel (vbox, _("Number of unique colors: %d"), uniques);

  memsize = gimp_memsize_to_string (width * height * bpp);
  doLabel (vbox, _("Uncompressed size: %s"), memsize);
  g_free (memsize);

  if (filename && !stat (filename, &st) && !gimp_image_is_dirty (imageID))
    {
      gchar *memsize = gimp_memsize_to_string (st.st_size);

      doLabel (vbox, _("Filename: %s"), filename);
      doLabel (vbox, _("Compressed size: %s"), memsize);
      doLabel (vbox, _("Compression ratio (approx.): %d to 1"),
                      (gint) RINT ((gdouble) (width * height * bpp) / st.st_size));

      g_free (memsize);
    }

  /* show stuff */
  gtk_widget_show_all (dialog);

  fillPreview (preview);

  gimp_dialog_run (GIMP_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}

/* shortcut */
static void
doLabel (GtkWidget   *vbox,
         const gchar *format,
         ...)
{
  GtkWidget *label;
  gchar     *text;
  va_list    args;

  va_start (args, format);
  text = g_strdup_vprintf (format, args);
  va_end (args);

  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_free (text);
}

/* fill our preview image with the color-histogram */
static void
fillPreview (GtkWidget *preview)
{
  guchar  *image, *column, *pixel;
  gint     x, y, rowstride;
  gdouble  histcount, val;

  rowstride = PREWIDTH * 3;

  image = g_new0 (guchar, PREWIDTH * rowstride);

  for (x = 0, column = image; x < PREWIDTH; x++, column += 3)
    {
      /*
       * For every channel, calculate a logarithmic value, scale it,
       * and build a one-pixel bar.
       *  ... in the respective channel, preserving the other ones. --hb
       */
      histcount = hist_red[x] > 1.0 ? hist_red[x] : 1.0;

      val = log (histcount) * (PREHEIGHT / 12);

      if (val > PREHEIGHT)
        val = PREHEIGHT;

      y = PREHEIGHT - 1;
      pixel = column + (y * rowstride);
      for (; y > (PREHEIGHT - val); y--)
        {
          pixel[0] = 255;
          pixel -= rowstride;
        }

      histcount = hist_green[x] > 1.0 ? hist_green[x] : 1.0;

      val = log (histcount) * (PREHEIGHT / 12);

      if (val > PREHEIGHT)
        val = PREHEIGHT;

      y = PREHEIGHT - 1;
      pixel = column + (y * rowstride);
      for (; y > (PREHEIGHT - val); y--)
        {
          pixel[1] = 255;
          pixel -= rowstride;
        }

      histcount = hist_blue[x] > 1.0 ? hist_blue[x] : 1.0;

      val = log (histcount) * (PREHEIGHT / 12);

      if (val > PREHEIGHT)
        val = PREHEIGHT;

      y = PREHEIGHT - 1;
      pixel = column + (y * rowstride);
      for (; y > (PREHEIGHT - val); y--)
        {
          pixel[2] = 255;
          pixel -= rowstride;
        }
    }

  /* move our data into the preview image */
  gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview),
                          0, 0, PREWIDTH, PREHEIGHT,
                          GIMP_RGB_IMAGE,
                          image,
                          3 * PREWIDTH);

  g_free (image);
}
