/* scatter_hsv.c -- This is a plug-in for the GIMP (1.0's API)
 * Author: Shuji Narazaki <narazaki@InetQ.or.jp>
 * Time-stamp: <2000-01-08 02:49:39 yasuhiro>
 * Version: 0.42
 *
 * Copyright (C) 1997 Shuji Narazaki <narazaki@InetQ.or.jp>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


#define PLUG_IN_NAME "plug_in_scatter_hsv"
#define SHORT_NAME   "scatter_hsv"
#define HELP_ID      "plug-in-scatter-hsv"

static void     query               (void);
static void     run                 (const gchar      *name,
                                     gint              nparams,
                                     const GimpParam  *param,
                                     gint             *nreturn_vals,
                                     GimpParam       **return_vals);

static void     scatter_hsv         (GimpDrawable     *drawable);
static gboolean scatter_hsv_dialog  (GimpDrawable     *drawable);
static void     scatter_hsv_preview (GimpPreview      *preview);

static void     scatter_hsv_scatter (guchar           *r,
                                     guchar           *g,
                                     guchar           *b);

static gint     randomize_value     (gint              now,
                                     gint              min,
                                     gint              max,
                                     gint              mod_p,
                                     gint              rand_max);

#define SCALE_WIDTH         100
#define ENTRY_WIDTH           3

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

typedef struct
{
  gint     holdness;
  gint     hue_distance;
  gint     saturation_distance;
  gint     value_distance;
  gboolean preview;
} ValueType;

static ValueType VALS =
{
  2,
  3,
  10,
  10,
  TRUE
};

MAIN ()

static void
query (void)
{
  static GimpParamDef args [] =
  {
    { GIMP_PDB_INT32,    "run_mode",            "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",               "Input image (not used)" },
    { GIMP_PDB_DRAWABLE, "drawable",            "Input drawable" },
    { GIMP_PDB_INT32,    "holdness",            "convolution strength" },
    { GIMP_PDB_INT32,    "hue_distance",        "distribution distance on hue axis [0,255]" },
    { GIMP_PDB_INT32,    "saturation_distance", "distribution distance on saturation axis [0,255]" },
    { GIMP_PDB_INT32,    "value_distance",      "distribution distance on value axis [0,255]" }
  };

  gimp_install_procedure (PLUG_IN_NAME,
                          "Scattering pixel values in HSV space",
                          "Scattering pixel values in HSV space",
                          "Shuji Narazaki (narazaki@InetQ.or.jp)",
                          "Shuji Narazaki",
                          "1997",
                          N_("S_catter HSV..."),
                          "RGB*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register (PLUG_IN_NAME, "<Image>/Filters/Noise");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpRunMode        run_mode;
  GimpDrawable      *drawable;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;
  drawable = gimp_drawable_get (param[2].data.d_int32);

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      gimp_get_data (PLUG_IN_NAME, &VALS);
      if (!gimp_drawable_is_rgb (drawable->drawable_id))
        {
          g_message ("Cannot operate on non-RGB drawables.");
          return;
        }
      if (! scatter_hsv_dialog (drawable))
        return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      VALS.holdness            = param[3].data.d_int32;
      VALS.hue_distance        = param[4].data.d_int32;
      VALS.saturation_distance = param[5].data.d_int32;
      VALS.value_distance      = param[6].data.d_int32;
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      gimp_get_data (PLUG_IN_NAME, &VALS);
      break;
    }

  scatter_hsv (drawable);

  if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush();
  if (run_mode == GIMP_RUN_INTERACTIVE && status == GIMP_PDB_SUCCESS )
    gimp_set_data (PLUG_IN_NAME, &VALS, sizeof (ValueType));

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}

static void
scatter_hsv_func (const guchar *src,
                  guchar       *dest,
                  gint          bpp,
                  gpointer      data)
{
  guchar h, s, v;

  h = src[0];
  s = src[1];
  v = src[2];

  scatter_hsv_scatter (&h, &s, &v);

  dest[0] = h;
  dest[1] = s;
  dest[2] = v;

  if (bpp == 4)
    dest[3] = src[3];
}

static void
scatter_hsv (GimpDrawable *drawable)
{
  gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

  gimp_progress_init (_("Scattering HSV..."));

  gimp_rgn_iterate2 (drawable, 0 /* unused */, scatter_hsv_func, NULL);

  gimp_drawable_detach (drawable);
}

static gint
randomize_value (gint now,
                 gint min,
                 gint max,
                 gint mod_p,
                 gint rand_max)
{
  gint    flag, new, steps, index;
  gdouble rand_val;

  steps = max - min + 1;
  rand_val = g_random_double ();
  for (index = 1; index < VALS.holdness; index++)
    {
      double tmp = g_random_double ();
      if (tmp < rand_val)
        rand_val = tmp;
    }

  if (g_random_double () < 0.5)
    flag = -1;
  else
    flag = 1;

  new = now + flag * ((int) (rand_max * rand_val) % steps);

  if (new < min)
    {
      if (mod_p == 1)
        new += steps;
      else
        new = min;
    }
  if (max < new)
    {
      if (mod_p == 1)
        new -= steps;
      else
        new = max;
    }
  return new;
}

static void scatter_hsv_scatter (guchar *r,
                                 guchar *g,
                                 guchar *b)
{
  gint h, s, v;
  gint h1, s1, v1;
  gint h2, s2, v2;

  h = *r; s = *g; v = *b;

  gimp_rgb_to_hsv_int (&h, &s, &v);

  if (0 < VALS.hue_distance)
    h = randomize_value (h, 0, 255, 1, VALS.hue_distance);
  if ((0 < VALS.saturation_distance))
    s = randomize_value (s, 0, 255, 0, VALS.saturation_distance);
  if ((0 < VALS.value_distance))
    v = randomize_value (v, 0, 255, 0, VALS.value_distance);

  h1 = h; s1 = s; v1 = v;

  gimp_hsv_to_rgb_int (&h, &s, &v); /* don't believe ! */

  h2 = h; s2 = s; v2 = v;

  gimp_rgb_to_hsv_int (&h2, &s2, &v2); /* h2 should be h1. But... */

  if ((abs (h1 - h2) <= VALS.hue_distance)
      && (abs (s1 - s2) <= VALS.saturation_distance)
      && (abs (v1 - v2) <= VALS.value_distance))
    {
      *r = h;
      *g = s;
      *b = v;
    }
}

static void
scatter_hsv_preview (GimpPreview *preview)
{
  GimpDrawable *drawable;
  GimpPixelRgn  src_rgn;
  guchar       *src, *dst;
  gint          i;
  gint          x1, y1;
  gint          width, height;
  gint          bpp;

  drawable =
    gimp_drawable_preview_get_drawable (GIMP_DRAWABLE_PREVIEW (preview));

  gimp_preview_get_position (preview, &x1, &y1);
  gimp_preview_get_size (preview, &width, &height);

  bpp = drawable->bpp;

  src = g_new (guchar, width * height * bpp);
  dst = g_new (guchar, width * height * bpp);

  gimp_pixel_rgn_init (&src_rgn, drawable,
                       x1, y1, width, height,
                       FALSE, FALSE);
  gimp_pixel_rgn_get_rect (&src_rgn, src, x1, y1, width, height);

  for (i = 0; i < width * height; i++)
    scatter_hsv_func (src + i * bpp, dst + i * bpp, bpp, NULL);

  gimp_preview_draw_buffer (preview, dst, width * bpp);

  g_free (src);
  g_free (dst);
}

/* dialog stuff */
static gboolean
scatter_hsv_dialog (GimpDrawable *drawable)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *preview;
  GtkWidget *table;
  GtkObject *adj;
  gboolean   run;

  gimp_ui_init (SHORT_NAME, TRUE);

  dialog = gimp_dialog_new (_("Scatter HSV"), SHORT_NAME,
                            NULL, 0,
                            gimp_standard_help_func, HELP_ID,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  preview = gimp_drawable_preview_new (drawable, &VALS.preview);
  gtk_box_pack_start_defaults (GTK_BOX (main_vbox), preview);
  gtk_widget_show (preview);
  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (scatter_hsv_preview),
                    NULL);

  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("_Holdness:"), SCALE_WIDTH, ENTRY_WIDTH,
                              VALS.holdness, 1, 8, 1, 2, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &VALS.holdness);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                              _("H_ue:"), SCALE_WIDTH, ENTRY_WIDTH,
                              VALS.hue_distance, 0, 255, 1, 8, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &VALS.hue_distance);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                              _("_Saturation:"), SCALE_WIDTH, ENTRY_WIDTH,
                              VALS.saturation_distance, 0, 255, 1, 8, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &VALS.saturation_distance);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 3,
                              _("_Value:"), SCALE_WIDTH, ENTRY_WIDTH,
                              VALS.value_distance, 0, 255, 1, 8, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &VALS.value_distance);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}
