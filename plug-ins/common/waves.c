/**************************************************
 * file: waves/waves.c
 *
 * Copyright (c) 1997 Eric L. Hernes (erich@rrnet.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: waves.c,v 1.48 2004/10/12 21:48:39 mitch Exp $
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

enum
{
  MODE_SMEAR,
  MODE_BLACKEN
};

typedef struct
{
  gdouble  amplitude;
  gdouble  phase;
  gdouble  wavelength;
  gint32   type;
  gboolean reflective;
  gboolean preview;
} piArgs;

static piArgs wvals =
{
  10.0,       /* amplitude  */
  0.0,        /* phase      */
  10.0,       /* wavelength */
  MODE_SMEAR, /* type       */
  FALSE,      /* reflective */
  TRUE        /* preview    */
};

static void query (void);
static void run   (const gchar      *name,
                   gint              nparam,
                   const GimpParam  *param,
                   gint             *nretvals,
                   GimpParam       **retvals);

static void waves            (GimpDrawable *drawable);

static gboolean waves_dialog (GimpDrawable *drawable);

static void waves_preview    (GimpDrawable *drawable,
                              GimpPreview  *preview);

static void wave (guchar  *src,
                  guchar  *dest,
                  gint     width,
                  gint     height,
                  gint     bypp,
                  gboolean has_alpha,
                  gdouble  amplitude,
                  gdouble  wavelength,
                  gdouble  phase,
                  gint     smear,
                  gboolean reflective,
                  gboolean verbose);

#define WITHIN(a, b, c) ((((a) <= (b)) && ((b) <= (c))) ? TRUE : FALSE)

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",   "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",      "The Image"                    },
    { GIMP_PDB_DRAWABLE, "drawable",   "The Drawable"                 },
    { GIMP_PDB_FLOAT,    "amplitude",  "The Amplitude of the Waves"   },
    { GIMP_PDB_FLOAT,    "phase",      "The Phase of the Waves"       },
    { GIMP_PDB_FLOAT,    "wavelength", "The Wavelength of the Waves"  },
    { GIMP_PDB_INT32,    "type",       "Type of waves, black/smeared" },
    { GIMP_PDB_INT32,    "reflective", "Use Reflection"               }
  };

  gimp_install_procedure ("plug_in_waves",
                          "Distort the image with waves",
                          "none yet",
                          "Eric L. Hernes, Stephen Norris",
                          "Stephen Norris",
                          "1997",
                          N_("_Waves..."),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register ("plug_in_waves", "<Image>/Filters/Distorts");
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
  GimpDrawable      *drawable;

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  INIT_I18N ();

  drawable = gimp_drawable_get (param[2].data.d_drawable);

  switch (param[0].data.d_int32)
    {

    case GIMP_RUN_INTERACTIVE:
      gimp_get_data ("plug_in_waves", &wvals);

      if (! waves_dialog (drawable))
        return;

    break;

    case GIMP_RUN_NONINTERACTIVE:
      if (nparams != 8)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      else
        {
          wvals.amplitude  = param[3].data.d_float;
          wvals.phase      = param[4].data.d_float;
          wvals.wavelength = param[5].data.d_float;
          wvals.type       = param[6].data.d_int32;
          wvals.reflective = param[7].data.d_int32;
        }
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      gimp_get_data ("plug_in_waves", &wvals);
      break;

    default:
      break;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      waves (drawable);

      gimp_set_data ("plug_in_waves", &wvals, sizeof (piArgs));
      values[0].data.d_status = status;
    }
}

static void
waves (GimpDrawable *drawable)
{
  GimpPixelRgn  srcPr, dstPr;
  guchar       *src, *dst;
  guint         width, height, bpp, has_alpha;

  width     = drawable->width;
  height    = drawable->height;
  bpp       = drawable->bpp;
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  src = g_new (guchar, width * height * bpp);
  dst = g_new (guchar, width * height * bpp);
  gimp_pixel_rgn_init (&srcPr, drawable, 0, 0, width, height, FALSE, FALSE);
  gimp_pixel_rgn_init (&dstPr, drawable, 0, 0, width, height, TRUE, TRUE);
  gimp_pixel_rgn_get_rect (&srcPr, src, 0, 0, width, height);

  wave (src, dst, width, height, bpp, has_alpha,
        wvals.amplitude, wvals.wavelength, wvals.phase,
        wvals.type == 0, wvals.reflective, TRUE);
  gimp_pixel_rgn_set_rect (&dstPr, dst, 0, 0, width, height);

  g_free (src);
  g_free (dst);

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, 0, 0, width, height);

  gimp_displays_flush ();
}

static gboolean
waves_dialog (GimpDrawable *drawable)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *smear;
  GtkWidget *blacken;
  GtkWidget *table;
  GtkWidget *preview;
  GtkWidget *toggle;
  GtkObject *adj;
  gboolean   run;

  gimp_ui_init ("waves", TRUE);

  dialog = gimp_dialog_new (_("Waves"), "waves",
                            NULL, 0,
                            gimp_standard_help_func, "plug-in-waves",

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  preview = gimp_aspect_preview_new (drawable, &wvals.preview);
  gtk_box_pack_start_defaults (GTK_BOX (main_vbox), preview);
  gtk_widget_show (preview);
  g_signal_connect_swapped (preview, "invalidated",
                            G_CALLBACK (waves_preview),
                            drawable);

  frame = gimp_int_radio_group_new (TRUE, _("Mode"),
                                    G_CALLBACK (gimp_radio_button_update),
                                    &wvals.type, wvals.type,

                                    _("_Smear"),   MODE_SMEAR,   &smear,
                                    _("_Blacken"), MODE_BLACKEN, &blacken,

                                    NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);
  g_signal_connect_swapped (smear, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (blacken, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  toggle = gtk_check_button_new_with_mnemonic ( _("_Reflective"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), wvals.reflective);
  gtk_box_pack_start (GTK_BOX (main_vbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &wvals.reflective);
  g_signal_connect_swapped (toggle, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("_Amplitude:"), 140, 6,
                              wvals.amplitude, 0.0, 101.0, 1.0, 5.0, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &wvals.amplitude);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                              _("_Phase:"), 140, 6,
                              wvals.phase, 0.0, 360.0, 2.0, 5.0, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &wvals.phase);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                              _("_Wavelength:"), 140, 6,
                              wvals.wavelength, 0.1, 50.0, 1.0, 5.0, 2,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &wvals.wavelength);
  g_signal_connect_swapped (adj, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}

static void
waves_preview (GimpDrawable *drawable,
               GimpPreview  *preview)
{
  guchar *src, *dest;
  gint    width, height;
  gint    bpp;

  gimp_preview_get_size (preview, &width, &height);
  src = gimp_drawable_get_thumbnail_data (drawable->drawable_id,
                                          &width, &height, &bpp);
  dest = g_new (guchar, width * height * bpp);

  wave (src, dest, width, height, bpp,
        bpp == 2 || bpp == 4,
        wvals.amplitude * width / drawable->width,
        wvals.wavelength * height / drawable->height,
        wvals.phase, wvals.type == 0, wvals.reflective, FALSE);

  gimp_preview_draw_buffer (preview, dest, width * bpp);

  g_free (src);
  g_free (dest);
}

/* Wave the image.  For each pixel in the destination image
 * which is inside the selection, we compute the corresponding
 * waved location in the source image.  We use bilinear
 * interpolation to avoid ugly jaggies.
 *
 * Let's assume that we are operating on a circular area.
 * Every point within <radius> distance of the wave center is
 * waveed to its destination position.
 *
 * Basically, introduce a wave into the image. I made up the
 * forumla initially, but it looks good. Quartic added the
 * wavelength etc. to it while I was asleep :) Just goes to show
 * we should work with people in different time zones more.
 *
 */

static void
wave (guchar  *src,
      guchar  *dst,
      gint     width,
      gint     height,
      gint     bypp,
      gboolean has_alpha,
      gdouble  amplitude,
      gdouble  wavelength,
      gdouble  phase,
      gint     smear,
      gboolean reflective,
      gboolean verbose)
{
  glong   rowsiz;
  guchar *p;
  guchar *dest;
  gint    x1, y1, x2, y2;
  gint    x, y;
  gint    prog_interval=0;
  gint    x1_in, y1_in, x2_in, y2_in;

  gdouble cen_x, cen_y;       /* Center of wave */
  gdouble xhsiz, yhsiz;       /* Half size of selection */
  gdouble radius, radius2;    /* Radius and radius^2 */
  gdouble amnt, d;
  gdouble needx, needy;
  gdouble dx, dy;
  gdouble xscale, yscale;

  gint xi, yi;

  guchar *values[4];
  guchar  zeroes[4] = { 0, 0, 0, 0 };

  phase = phase * G_PI / 180;
  rowsiz   = width * bypp;

  if (verbose)
    {
      gimp_progress_init (_("Waving..."));
      prog_interval=height/10;
    }

  x1 = y1 = 0;
  x2 = width;
  y2 = height;

  /* Center of selection */
  cen_x = (double) (x2 - 1 + x1) / 2.0;
  cen_y = (double) (y2 - 1 + y1) / 2.0;

  /* Compute wave radii (semiaxes) */
  xhsiz = (double) (x2 - x1) / 2.0;
  yhsiz = (double) (y2 - y1) / 2.0;

  /* These are the necessary scaling factors to turn the wave
     ellipse into a large circle */

  if (xhsiz < yhsiz)
    {
      xscale = yhsiz / xhsiz;
      yscale = 1.0;
    }
  else if (xhsiz > yhsiz)
    {
      xscale = 1.0;
      yscale = xhsiz / yhsiz;
    }
  else
    {
      xscale = 1.0;
      yscale = 1.0;
    }

  radius  = MAX (xhsiz, yhsiz);
  radius2 = radius * radius;

  /* Wave the image! */

  dst += y1 * rowsiz + x1 * bypp;

  wavelength *= 2;

  for (y = y1; y < y2; y++)
    {
      dest = dst;

      if (verbose && (y % prog_interval == 0))
        gimp_progress_update ((double) y / (double) height);

      for (x = x1; x < x2; x++)
        {
          /* Distance from current point to wave center, scaled */
          dx = (x - cen_x) * xscale;
          dy = (y - cen_y) * yscale;

          /* Distance^2 to center of *circle* (our scaled ellipse) */
          d = sqrt (dx * dx + dy * dy);

          /* Use the formula described above. */

          /* Calculate waved point and scale again to ellipsify */

          /*
           * Reflective waves are strange - the effect is much
           * more like a mirror which is in the shape of
           * the wave than anything else.
           */

          if (reflective)
            {
              amnt = amplitude * fabs (sin (((d / wavelength) * (2.0 * G_PI) +
                                             phase)));

              needx = (amnt * dx) / xscale + cen_x;
              needy = (amnt * dy) / yscale + cen_y;
            }
          else
            {
              amnt = amplitude * sin (((d / wavelength) * (2.0 * G_PI) +
                                       phase));

              needx = (amnt + dx) / xscale + cen_x;
              needy = (amnt + dy) / yscale + cen_y;
            }

          /* Calculations complete; now copy the proper pixel */

          if (smear)
            {
              xi = CLAMP (needx, 0, width - 2);
              yi = CLAMP (needy, 0, height - 2);
            }
          else
            {
              xi = needx;
              yi = needy;
            }

          p = src + rowsiz * yi + xi * bypp;

          x1_in = WITHIN (0, xi, width - 1);
          y1_in = WITHIN (0, yi, height - 1);
          x2_in = WITHIN (0, xi + 1, width - 1);
          y2_in = WITHIN (0, yi + 1, height - 1);

          if (x1_in && y1_in)
            values[0] = p;
          else
            values[0] = zeroes;

          if (x2_in && y1_in)
            values[1] = p + bypp;
          else
            values[1] = zeroes;

          if (x1_in && y2_in)
            values[2] = p + rowsiz;
          else
            values[2] = zeroes;

          if (x2_in && y2_in)
            values[3] = p + bypp + rowsiz;
          else
            values[3] = zeroes;

          gimp_bilinear_pixels_8 (dest, needx, needy, bypp, has_alpha, values);
          dest += bypp;
        }

      dst += rowsiz;
    }

  if (verbose)
    gimp_progress_update (1.0);
}
