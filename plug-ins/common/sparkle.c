/* Sparkle --- image filter plug-in for The Gimp image manipulation program
 * Copyright (C) 1996 by John Beale;  ported to Gimp by Michael J. Hammel;
 *
 * It has been optimized a little, bugfixed and modified by Martin Weber
 * for additional functionality.  Also bugfixed by Seth Burgess (9/17/03)
 * to take rowstrides into account when selections are present (bug #50911).
 * Attempted reformatting.
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
 *
 * You can contact Michael at mjhammel@csn.net
 * You can contact Martin at martweb@gmx.net
 * You can contact Seth at sjburges@gimp.org
 */

/*
 * Sparkle 1.27 - simulate pixel bloom and diffraction effects
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


#define  SCALE_WIDTH  175
#define  ENTRY_WIDTH    7
#define  MAX_CHANNELS   4
#define  PSV            2  /* point spread value */
#define  EPSILON        0.001

#define  NATURAL     0
#define  FOREGROUND  1
#define  BACKGROUND  2

typedef struct
{
  gdouble   lum_threshold;
  gdouble   flare_inten;
  gdouble   spike_len;
  gdouble   spike_pts;
  gdouble   spike_angle;
  gdouble   density;
  gdouble   opacity;
  gdouble   random_hue;
  gdouble   random_saturation;
  gboolean  preserve_luminosity;
  gboolean  inverse;
  gboolean  border;
  gint      colortype;
} SparkleVals;


/* Declare local functions.
 */

static void      query  (void);
static void      run    (const gchar      *name,
                         gint              nparams,
                         const GimpParam  *param,
                         gint             *nreturn_vals,
                         GimpParam       **return_vals);

static gboolean  sparkle_dialog        (void);

static gint      compute_luminosity    (const guchar *pixel,
                                        gboolean      gray,
                                        gboolean      has_alpha);
static gint      compute_lum_threshold (GimpDrawable *drawable,
                                        gdouble       percentile);
static void      sparkle               (GimpDrawable *drawable,
                                        gint          threshold);
static void      fspike                (GimpPixelRgn *src_rgn,
                                        GimpPixelRgn *dest_rgn,
                                        gint          x1,
                                        gint          y1,
                                        gint          x2,
                                        gint          y2,
                                        gint          xr,
                                        gint          yr,
                                        gint          tile_width,
                                        gint          tile_height,
                                        gdouble       inten,
                                        gdouble       length,
                                        gdouble       angle,
                                        GRand        *gr);
static GimpTile * rpnt                 (GimpDrawable *drawable,
                                        GimpTile     *tile,
                                        gint          x1,
                                        gint          y1,
                                        gint          x2,
                                        gint          y2,
                                        gdouble       xr,
                                        gdouble       yr,
                                        gint          tile_width,
                                        gint          tile_height,
                                        gint         *row,
                                        gint         *col,
                                        gint          bytes,
                                        gdouble       inten,
                                        guchar        color[MAX_CHANNELS]);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static SparkleVals svals =
{
  0.001,  /* luminosity threshold */
  0.5,    /* flare intensity      */
  20.0,   /* spike length         */
  4.0,    /* spike points         */
  15.0,   /* spike angle          */
  1.0,    /* spike density        */
  0.0,    /* opacity              */
  0.0,    /* random hue           */
  0.0,    /* random saturation    */
  FALSE,  /* preserve_luminosity  */
  FALSE,  /* inverse              */
  FALSE,  /* border               */
  NATURAL /* colortype            */
};

static gint num_sparkles;


MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "drawable",      "Input drawable" },
    { GIMP_PDB_FLOAT,    "lum_threshold", "Luminosity threshold (0.0 - 1.0)" },
    { GIMP_PDB_FLOAT,    "flare_inten",   "Flare intensity (0.0 - 1.0)" },
    { GIMP_PDB_INT32,    "spike_len",     "Spike length (in pixels)" },
    { GIMP_PDB_INT32,    "spike_pts",     "# of spike points" },
    { GIMP_PDB_INT32,    "spike_angle",   "Spike angle (0-360 degrees, -1: random)" },
    { GIMP_PDB_FLOAT,    "density",       "Spike density (0.0 - 1.0)" },
    { GIMP_PDB_FLOAT,    "opacity",       "Opacity (0.0 - 1.0)" },
    { GIMP_PDB_FLOAT,    "random_hue",    "Random hue (0.0 - 1.0)" },
    { GIMP_PDB_FLOAT,    "random_saturation",   "Random saturation (0.0 - 1.0)" },
    { GIMP_PDB_INT32,    "preserve_luminosity", "Preserve luminosity (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "inverse",       "Inverse (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "border",        "Add border (TRUE/FALSE)" },
    { GIMP_PDB_INT32,    "colortype",     "Color of sparkles: { NATURAL (0), FOREGROUND (1), BACKGROUND (2) }" }
  };

  gimp_install_procedure ("plug_in_sparkle",
                          "Simulates pixel bloom and diffraction effects",
                          "Uses a percentage based luminoisty threhsold to find "
                          "candidate pixels for adding some sparkles (spikes). ",
                          "John Beale, & (ported to GIMP v0.54) Michael "
                          "J. Hammel & ted to GIMP v1.0) & Seth Burgess & "
                          "Spencer Kimball",
                          "John Beale",
                          "Version 1.27, September 2003",
                          N_("_Sparkle..."),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register ("plug_in_sparkle",
                             "<Image>/Filters/Light Effects");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpDrawable      *drawable;
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  gint               threshold, x1, y1, x2, y2;

  run_mode = param[0].data.d_int32;

  INIT_I18N ();

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_sparkle", &svals);

      /*  First acquire information with a dialog  */
      if (! sparkle_dialog ())
    return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      if (nparams != 16)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      else
        {
          svals.lum_threshold = param[3].data.d_float;
          svals.flare_inten = param[4].data.d_float;
          svals.spike_len = param[5].data.d_int32;
          svals.spike_pts = param[6].data.d_int32;
          svals.spike_angle = param[7].data.d_int32;
          svals.density = param[8].data.d_float;
          svals.opacity = param[9].data.d_float;
          svals.random_hue = param[10].data.d_float;
          svals.random_saturation = param[11].data.d_float;
          svals.preserve_luminosity = (param[12].data.d_int32) ? TRUE : FALSE;
          svals.inverse = (param[13].data.d_int32) ? TRUE : FALSE;
          svals.border = (param[14].data.d_int32) ? TRUE : FALSE;
          svals.colortype = param[15].data.d_int32;

          if (svals.lum_threshold < 0.0 || svals.lum_threshold > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.flare_inten < 0.0 || svals.flare_inten > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.spike_len < 0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.spike_pts < 0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.spike_angle < -1 || svals.spike_angle > 360)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.density < 0.0 || svals.density > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.opacity < 0.0 || svals.opacity > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.random_hue < 0.0 || svals.random_hue > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.random_saturation < 0.0 ||
               svals.random_saturation > 1.0)
            status = GIMP_PDB_CALLING_ERROR;
          else if (svals.colortype < NATURAL || svals.colortype > BACKGROUND)
            status = GIMP_PDB_CALLING_ERROR;
        }
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_sparkle", &svals);
      break;

    default:
      break;
    }

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  /*  Make sure that the drawable is gray or RGB color  */
  if (gimp_drawable_is_rgb (drawable->drawable_id) ||
      gimp_drawable_is_gray (drawable->drawable_id))
    {
      gimp_progress_init (_("Sparkling..."));
      gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

      if (svals.border)
        {
          gimp_drawable_mask_bounds (drawable->drawable_id,
                                     &x1, &y1, &x2, &y2);
          num_sparkles = 2 * (x2 - x1 + y2 - y1);
          threshold = 255;
        }
      else
        {
          /*  compute the luminosity which exceeds the luminosity threshold  */
          threshold = compute_lum_threshold (drawable, svals.lum_threshold);
        }

      sparkle (drawable, threshold);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();

      /*  Store mvals data  */
      if (run_mode == GIMP_RUN_INTERACTIVE)
        gimp_set_data ("plug_in_sparkle", &svals, sizeof (SparkleVals));
    }
  else
    {
      /* gimp_message ("sparkle: cannot operate on indexed color images"); */
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (drawable);
}

static gboolean
sparkle_dialog (void)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *toggle;
  GtkWidget *r1, *r2, *r3;
  GtkObject *scale_data;
  gboolean   run;

  gimp_ui_init ("sparkle", FALSE);

  dlg = gimp_dialog_new (_("Sparkle"), "sparkle",
                         NULL, 0,
                         gimp_standard_help_func, "plug-in-sparkle",

                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,

                         NULL);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), main_vbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);

  table = gtk_table_new (9, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
              _("Luminosity _Threshold:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.lum_threshold, 0.0, 0.1, 0.001, 0.01, 3,
              TRUE, 0, 0,
              _("Adjust the Luminosity Threshold"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.lum_threshold);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
              _("F_lare intensity:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.flare_inten, 0.0, 1.0, 0.01, 0.1, 2,
              TRUE, 0, 0,
              _("Adjust the Flare Intensity"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.flare_inten);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
              _("_Spike length:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.spike_len, 1, 100, 1, 10, 0,
              TRUE, 0, 0,
              _("Adjust the Spike Length"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.spike_len);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 3,
              _("Sp_ike points:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.spike_pts, 0, 16, 1, 4, 0,
              TRUE, 0, 0,
              _("Adjust the Number of Spikes"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.spike_pts);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 4,
              _("Spi_ke angle (-1: random):"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.spike_angle, -1, 360, 1, 15, 0,
              TRUE, 0, 0,
              _("Adjust the Spike Angle "
                "(-1 means a Random Angle is chosen)"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.spike_angle);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 5,
              _("Spik_e density:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.density, 0.0, 1.0, 0.01, 0.1, 2,
              TRUE, 0, 0,
              _("Adjust the Spike Density"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.density);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 6,
              _("Op_acity:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.opacity, 0.0, 1.0, 0.01, 0.1, 2,
              TRUE, 0, 0,
              _("Adjust the Opacity of the Spikes"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.opacity);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 7,
              _("_Random hue:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.random_hue, 0.0, 1.0, 0.01, 0.1, 2,
              TRUE, 0, 0,
              _("Adjust the Value how much the Hue should "
                "be changed randomly"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.random_hue);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 8,
              _("Rando_m saturation:"), SCALE_WIDTH, ENTRY_WIDTH,
              svals.random_saturation, 0.0, 1.0, 0.01, 0.1, 2,
              TRUE, 0, 0,
              _("Adjust the Value how much the Saturation should "
                "be changed randomly"), NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &svals.random_saturation);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  toggle = gtk_check_button_new_with_mnemonic (_("_Preserve luminosity"));
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                svals.preserve_luminosity);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
                           _("Should the Luminosity be preserved?"), NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &svals.preserve_luminosity);

  toggle = gtk_check_button_new_with_mnemonic (_("In_verse"));
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), svals.inverse);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
                           _("Should an Inverse Effect be done?"), NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &svals.inverse);

  toggle = gtk_check_button_new_with_mnemonic (_("A_dd border"));
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), svals.border);
  gtk_widget_show (toggle);

  gimp_help_set_help_data (toggle,
                           _("Draw a Border of Spikes around the Image"), NULL);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &svals.border);

  /*  colortype  */
  vbox = gimp_int_radio_group_new (FALSE, NULL,
                                   G_CALLBACK (gimp_radio_button_update),
                                   &svals.colortype, svals.colortype,

                                   _("_Natural color"),    NATURAL,    &r1,
                                   _("_Foreground color"), FOREGROUND, &r2,
                                   _("_Background color"), BACKGROUND, &r3,

                                   NULL);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  gimp_help_set_help_data (r1, _("Use the color of the image"), NULL);
  gimp_help_set_help_data (r2, _("Use the foreground color"), NULL);
  gimp_help_set_help_data (r3, _("Use the background color"), NULL);

  gtk_widget_show (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dlg);

  return run;
}

static gint
compute_luminosity (const guchar *pixel,
                    gboolean      gray,
                    gboolean      has_alpha)
{
  gint pixel0, pixel1, pixel2;

  if (svals.inverse)
    {
      pixel0 = 255 - pixel[0];
      pixel1 = 255 - pixel[1];
      pixel2 = 255 - pixel[2];
    }
  else
    {
      pixel0 = pixel[0];
      pixel1 = pixel[1];
      pixel2 = pixel[2];
    }

  if (gray)
    {
      if (has_alpha)
        return (pixel0 * pixel1) / 255;
      else
        return (pixel0);
    }
  else
    {
      gint min, max;

      min = MIN (pixel0, pixel1);
      min = MIN (min, pixel2);
      max = MAX (pixel0, pixel1);
      max = MAX (max, pixel2);

      if (has_alpha)
        return ((min + max) * pixel[3]) / 510;
      else
        return (min + max) / 2;
    }
}

static gint
compute_lum_threshold (GimpDrawable *drawable,
                       gdouble       percentile)
{
  GimpPixelRgn src_rgn;
  gpointer     pr;
  gint         values[256];
  gint         total, sum;
  gboolean     gray;
  gboolean     has_alpha;
  gint         i;
  gint         x1, y1, x2, y2;

  /*  zero out the luminosity values array  */
  memset (values, 0, sizeof (gint) * 256);

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  gray = gimp_drawable_is_gray (drawable->drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  gimp_pixel_rgn_init (&src_rgn, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);

  for (pr = gimp_pixel_rgns_register (1, &src_rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
     {
       const guchar *src, *s;
       gint          sx, sy;

       src = src_rgn.data;

       for (sy = 0; sy < src_rgn.h; sy++)
         {
           s = src;

           for (sx = 0; sx < src_rgn.w; sx++)
             {
               values [compute_luminosity (s, gray, has_alpha)]++;
               s += src_rgn.bpp;
             }

           src += src_rgn.rowstride;
         }
     }

  total = (x2 - x1) * (y2 - y1);
  sum = 0;

  for (i = 255; i >= 0; i--)
    {
      sum += values[i];
      if ((gdouble) sum > percentile * (gdouble) total)
        {
           num_sparkles = sum;
           return i;
        }
    }
  return 0;
}

static void
sparkle (GimpDrawable *drawable,
         gint          threshold)
{
  GimpPixelRgn src_rgn, dest_rgn;
  gdouble      nfrac, length, inten, spike_angle;
  gint         cur_progress, max_progress;
  gint         x1, y1, x2, y2;
  gint         lum, x, y, b;
  gboolean     gray, has_alpha;
  gint         alpha;
  gpointer     pr;
  gint         tile_width, tile_height;
  GRand       *gr;

  gr = g_rand_new ();

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  gray = gimp_drawable_is_gray (drawable->drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  alpha = (has_alpha) ? drawable->bpp - 1 : drawable->bpp;

  tile_width  = gimp_tile_width();
  tile_height = gimp_tile_height();

  /* initialize the progress dialog */
  cur_progress = 0;
  max_progress = num_sparkles;

  /* copy what is already there */
  gimp_pixel_rgn_init (&src_rgn, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);
  gimp_pixel_rgn_init (&dest_rgn, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), TRUE, TRUE);

  for (pr = gimp_pixel_rgns_register (2, &src_rgn, &dest_rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      const guchar *src,  *s;
      guchar       *dest, *d;

      src = src_rgn.data;
      dest = dest_rgn.data;

      for (y = 0; y < src_rgn.h; y++)
        {
          s = src;
          d = dest;

          for (x = 0; x < src_rgn.w; x++)
            {
              if (has_alpha && s[alpha] == 0)
                {
                  memset (d, 0, alpha);
                }
              else
                {
                  for (b = 0; b < alpha; b++)
                    d[b] = s[b];
                }

              if (has_alpha)
                d[alpha] = s[alpha];

              s += src_rgn.bpp;
              d += dest_rgn.bpp;
            }

          src += src_rgn.rowstride;
          dest += dest_rgn.rowstride;
        }
    }
  /* add effects to new image based on intensity of old pixels */

  gimp_pixel_rgn_init (&src_rgn, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);
  gimp_pixel_rgn_init (&dest_rgn, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), TRUE, TRUE);

  for (pr = gimp_pixel_rgns_register (2, &src_rgn, &dest_rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      const guchar *src, *s;

      src = src_rgn.data;

      for (y = 0; y < src_rgn.h; y++)
        {
          s = src;

          for (x = 0; x < src_rgn.w; x++)
            {
              if (svals.border)
                {
                  if (x + src_rgn.x == 0 ||
                      y + src_rgn.y == 0 ||
                      x + src_rgn.x == drawable->width  - 1 ||
                      y + src_rgn.y == drawable->height - 1)
                    {
                      lum = 255;
                    }
                  else
                    {
                      lum = 0;
                    }
                }
              else
                {
                  lum = compute_luminosity (s, gray, has_alpha);
                }

              if (lum >= threshold)
                {
                  nfrac = fabs ((gdouble) (lum + 1 - threshold) /
                                (gdouble) (256 - threshold));
                  length = ((gdouble) svals.spike_len *
                            (gdouble) pow (nfrac, 0.8));
                  inten = svals.flare_inten * nfrac;

                  /* fspike im x,y intens rlength angle */
                  if (svals.spike_pts > 0)
                    {
                      /* major spikes */
                      if (svals.spike_angle == -1)
                        spike_angle = g_rand_double_range (gr, 0, 360.0);
                      else
                        spike_angle = svals.spike_angle;

                      if (g_rand_double (gr) <= svals.density)
                        {
                          fspike (&src_rgn, &dest_rgn, x1, y1, x2, y2,
                                  x + src_rgn.x, y + src_rgn.y,
                                  tile_width, tile_height,
                                  inten, length, spike_angle, gr);

                          /* minor spikes */
                          fspike (&src_rgn, &dest_rgn, x1, y1, x2, y2,
                                  x + src_rgn.x, y + src_rgn.y,
                                  tile_width, tile_height,
                                  inten * 0.7, length * 0.7,
                                  ((gdouble)spike_angle+180.0/svals.spike_pts),
                                  gr);
                        }
                   }

                 cur_progress ++;

                 if ((cur_progress % 5) == 0)
                   gimp_progress_update ((double) cur_progress /
                                         (double) max_progress);
               }

              s += src_rgn.bpp;
            }

          src += src_rgn.rowstride;
        }
    }

  gimp_progress_update (1.0);

  /*  update the blurred region  */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));

  g_rand_free (gr);
}

static inline GimpTile *
rpnt (GimpDrawable *drawable,
      GimpTile     *tile,
      gint          x1,
      gint          y1,
      gint          x2,
      gint          y2,
      gdouble       xr,
      gdouble       yr,
      gint          tile_width,
      gint          tile_height,
      gint         *row,
      gint         *col,
      gint          bytes,
      gdouble       inten,
      guchar        color[MAX_CHANNELS])
{
  gint     x, y, b;
  gdouble  dx, dy, rs, val;
  guchar  *pixel;
  gdouble  new;
  gint     newcol, newrow;
  gint     newcoloff, newrowoff;

  x = (int) (xr);    /* integer coord. to upper left of real point */
  y = (int) (yr);

  if (x >= x1 && y >= y1 && x < x2 && y < y2)
    {
      newcol    = x / tile_width;
      newcoloff = x % tile_width;
      newrow    = y / tile_height;
      newrowoff = y % tile_height;

      if ((newcol != *col) || (newrow != *row))
        {
          *col = newcol;
          *row = newrow;

          if (tile)
            gimp_tile_unref (tile, TRUE);

          tile = gimp_drawable_get_tile (drawable, TRUE, *row, *col);
          gimp_tile_ref (tile);
        }

      pixel = tile->data + tile->bpp * (tile->ewidth * newrowoff + newcoloff);
      dx = xr - x; dy = yr - y;
      rs = dx * dx + dy * dy;
      val = inten * exp (-rs / PSV);

      for (b = 0; b < bytes; b++)
        {
          if (svals.inverse)
            new = 255 - pixel[b];
          else
            new = pixel[b];

          if (svals.preserve_luminosity)
            {
              if (new < color[b])
                {
                  new *= (1.0 - val * (1.0 - svals.opacity));
                }
              else
                {
                  new -= val * color[b] * (1.0 - svals.opacity);
                  if (new < 0.0)
                    new = 0.0;
                }
            }

          new *= 1.0 - val * svals.opacity;
          new += val * color[b];

          if (new > 255)
            new = 255;

          if (svals.inverse)
            pixel[b] = 255 - new;
          else
            pixel[b] = new;
        }
    }

  return tile;
}

static void
fspike (GimpPixelRgn *src_rgn,
        GimpPixelRgn *dest_rgn,
        gint          x1,
        gint          y1,
        gint          x2,
        gint          y2,
        gint          xr,
        gint          yr,
        gint          tile_width,
        gint          tile_height,
        gdouble       inten,
        gdouble       length,
        gdouble       angle,
        GRand        *gr)
{
  const gdouble efac = 2.0;
  gdouble xrt, yrt, dx, dy;
  gdouble rpos;
  gdouble in;
  gdouble theta;
  gdouble sfac;
  gint    r, g, b;
  GimpTile  *tile = NULL;
  gint    row, col;
  gint    i;
  gint    bytes;
  gint    ok;
  GimpRGB gimp_color;
  guchar  pixel[MAX_CHANNELS];
  guchar  chosen_color[MAX_CHANNELS];
  guchar  color[MAX_CHANNELS];

  theta = angle;
  bytes = dest_rgn->bpp;
  row = -1;
  col = -1;

  switch (svals.colortype)
    {
    case NATURAL:
      break;

    case FOREGROUND:
      gimp_context_get_foreground (&gimp_color);
      gimp_rgb_get_uchar (&gimp_color, &chosen_color[0], &chosen_color[1],
                          &chosen_color[2]);
      break;

    case BACKGROUND:
      gimp_context_get_background (&gimp_color);
      gimp_rgb_get_uchar (&gimp_color, &chosen_color[0], &chosen_color[1],
                          &chosen_color[2]);
      break;
    }

  /* draw the major spikes */
  for (i = 0; i < svals.spike_pts; i++)
    {
      gimp_pixel_rgn_get_pixel (dest_rgn, pixel, xr, yr);

      if (svals.colortype == NATURAL)
        {
          color[0] = pixel[0];
          color[1] = pixel[1];
          color[2] = pixel[2];
        }
      else
        {
          color[0] = chosen_color[0];
          color[1] = chosen_color[1];
          color[2] = chosen_color[2];
        }

      color[3] = pixel[3];

      if (svals.inverse)
        {
          color[0] = 255 - color[0];
          color[1] = 255 - color[1];
          color[2] = 255 - color[2];
        }

      if (svals.random_hue > 0.0 || svals.random_saturation > 0.0)
        {
          r = 255 - color[0];
          g = 255 - color[1];
          b = 255 - color[2];

          gimp_rgb_to_hsv_int (&r, &g, &b);

          r += svals.random_hue * g_rand_double_range (gr, -0.5, 0.5) * 255;

          if (r >= 255)
            r -= 255;
          else if (r < 0)
            r += 255;

          b += (svals.random_saturation *
                g_rand_double_range (gr, -1.0, 1.0)) * 255;

          if (b > 255)
            b = 255;

          gimp_hsv_to_rgb_int (&r, &g, &b);

          color[0] = 255 - r;
          color[1] = 255 - g;
          color[2] = 255 - b;
        }

      dx = 0.2 * cos (theta * G_PI / 180.0);
      dy = 0.2 * sin (theta * G_PI / 180.0);
      xrt = (gdouble) xr; /* (gdouble) is needed because some      */
      yrt = (gdouble) yr; /* compilers optimize too much otherwise */
      rpos = 0.2;

      do
        {
          sfac = inten * exp (-pow (rpos / length, efac));
          ok = FALSE;

          in = 0.2 * sfac;
          if (in > 0.01)
            ok = TRUE;

          tile = rpnt (dest_rgn->drawable, tile, x1, y1, x2, y2,
                       xrt, yrt, tile_width, tile_height,
                       &row, &col, bytes, in, color);
          tile = rpnt (dest_rgn->drawable, tile, x1, y1, x2, y2,
                       xrt + 1.0, yrt, tile_width, tile_height,
                       &row, &col, bytes, in, color);
          tile = rpnt (dest_rgn->drawable, tile, x1, y1, x2, y2,
                       xrt + 1.0, yrt + 1.0, tile_width, tile_height,
                       &row, &col, bytes, in, color);
          tile = rpnt (dest_rgn->drawable, tile, x1, y1, x2, y2,
                       xrt, yrt + 1.0, tile_width, tile_height,
                       &row, &col, bytes, in, color);

          xrt += dx;
          yrt += dy;
          rpos += 0.2;

        } while (ok);

      theta += 360.0 / svals.spike_pts;
    }

  if (tile)
    gimp_tile_unref (tile, TRUE);
}