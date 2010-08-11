/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Gradient Map plug-in
 * Copyright (C) 1997 Eiichi Takamori <taka@ma1.seikyou.ne.jp>
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

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"


#ifdef RCSID
static char rcsid[] = "$Id: gradmap.c,v 1.27 2004/10/12 21:48:38 mitch Exp $";
#endif

/* Some useful macros */

#define NSAMPLES      256
#define LUMINOSITY(X) (GIMP_RGB_INTENSITY (X[0], X[1], X[2]) + 0.5)

/* Declare a local function.
 */
static void     query       (void);
static void     run         (const gchar      *name,
                             gint              nparams,
                             const GimpParam  *param,
                             gint             *nreturn_vals,
                             GimpParam       **return_vals);

static void     gradmap     (GimpDrawable     *drawable);
static guchar * get_samples (GimpDrawable     *drawable);


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
  static GimpParamDef args[]=
  {
    { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",    "Input image (unused)"         },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable"               }
  };

  gimp_install_procedure ("plug_in_gradmap",
                          "Map the contents of the specified drawable with "
                          "active gradient",
                          "This plug-in maps the contents of the specified "
                          "drawable with active gradient. It calculates "
                          "luminosity of each pixel and replaces the pixel "
                          "by the sample of active gradient at the position "
                          "proportional to that luminosity. Complete black "
                          "pixel becomes the leftmost color of the gradient, "
                          "and complete white becomes the rightmost. Works on "
                          "both Grayscale and RGB image with/without alpha "
                          "channel.",
                          "Eiichi Takamori",
                          "Eiichi Takamori",
                          "1997",
                          N_("_Gradient Map"),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register ("plug_in_gradmap", "<Image>/Filters/Colors/Map");
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
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  GimpRunMode        run_mode;

  run_mode = param[0].data.d_int32;

  INIT_I18N();

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  /*  Make sure that the drawable is gray or RGB color  */
  if (gimp_drawable_is_rgb (drawable->drawable_id) ||
      gimp_drawable_is_gray (drawable->drawable_id))
    {
      gimp_progress_init (_("Gradient Map..."));
      gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

      gradmap (drawable);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();
    }
  else
    {
      /* g_message ("gradmap: cannot operate on indexed color images"); */
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (drawable);
}

typedef struct
{
  guchar   *samples;
  gboolean  is_rgb;
  gboolean  has_alpha;
} GradMapParam;

static void
gradmap_func (const guchar *src,
              guchar       *dest,
              gint          bpp,
              gpointer      data)
{
  GradMapParam *param = data;
  gint          lum;
  gint          b;
  guchar       *samp;

  lum = (param->is_rgb) ? LUMINOSITY (src) : src[0];
  samp = &param->samples[lum * bpp];

  if (param->has_alpha)
    {
      for (b = 0; b < bpp - 1; b++)
        dest[b] = samp[b];
      dest[b] = ((guint)samp[b] * (guint)src[b]) / 255;
    }
  else
    {
      for (b = 0; b < bpp; b++)
        dest[b] = samp[b];
    }
}

static void
gradmap (GimpDrawable *drawable)
{
  GradMapParam param;

  param.is_rgb = gimp_drawable_is_rgb (drawable->drawable_id);
  param.has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  param.samples = get_samples (drawable);

  gimp_rgn_iterate2 (drawable, 0 /* unused */, gradmap_func, &param);
}

/*
  Returns 256 samples of active gradient.
  Each sample has (gimp_drawable_bpp (drawable->drawable_id)) bytes.
 */
static guchar *
get_samples (GimpDrawable *drawable)
{
  gchar   *gradient_name;
  gint     n_f_samples;
  gdouble *f_samples, *f_samp;  /* float samples */
  guchar  *b_samples, *b_samp;  /* byte samples */
  gint     bpp, color, has_alpha, alpha;
  gint     i, j;

  gradient_name = gimp_context_get_gradient ();

#ifdef __GNUC__
#warning FIXME: "reverse" hardcoded to FALSE.
#endif
  gimp_gradient_get_uniform_samples (gradient_name, NSAMPLES, FALSE,
                                     &n_f_samples, &f_samples);

  bpp       = gimp_drawable_bpp (drawable->drawable_id);
  color     = gimp_drawable_is_rgb (drawable->drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  alpha     = (has_alpha ? bpp - 1 : bpp);

  b_samples = g_new (guchar, NSAMPLES * bpp);

  for (i = 0; i < NSAMPLES; i++)
    {
      b_samp = &b_samples[i * bpp];
      f_samp = &f_samples[i * 4];
      if (color)
        for (j = 0; j < 3; j++)
          b_samp[j] = f_samp[j] * 255;
      else
        b_samp[0] = LUMINOSITY (f_samp) * 255;

      if (has_alpha)
        b_samp[alpha] = f_samp[3] * 255;
    }

  g_free (f_samples);

  return b_samples;
}
