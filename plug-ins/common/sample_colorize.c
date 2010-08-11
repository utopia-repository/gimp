/* sample_colorize.c
 *      A GIMP Plug-In by Wolfgang Hofer
 */

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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


/* Some useful macros */
#define RESPONSE_RESET      1
#define RESPONSE_GET_COLORS 2

#define PLUG_IN_NAME        "plug_in_sample_colorize"
#define NUMBER_IN_ARGS      13
#define HELP_ID             "plug-in-sample-colorize"

#define TILE_CACHE_SIZE      32
#define LUMINOSITY_0(X)      ((X[0] * 30 + X[1] * 59 + X[2] * 11))
#define LUMINOSITY_1(X)      ((X[0] * 30 + X[1] * 59 + X[2] * 11) / 100)
#define MIX_CHANNEL(a, b, m) (((a * m) + (b * (255 - m))) / 255)

#define SMP_GRADIENT         -444
#define SMP_INV_GRADIENT     -445


#define PREVIEW_BPP 3
#define PREVIEW_SIZE_X  256
#define PREVIEW_SIZE_Y  256
#define DA_WIDTH         256
#define DA_HEIGHT        25
#define GRADIENT_HEIGHT  15
#define CONTROL_HEIGHT   DA_HEIGHT - GRADIENT_HEIGHT
#define LEVELS_DA_MASK  (GDK_EXPOSURE_MASK       | \
                         GDK_ENTER_NOTIFY_MASK   | \
                         GDK_BUTTON_PRESS_MASK   | \
                         GDK_BUTTON_RELEASE_MASK | \
                         GDK_BUTTON1_MOTION_MASK | \
                         GDK_POINTER_MOTION_HINT_MASK)

#define LOW_INPUT          0x1
#define GAMMA              0x2
#define HIGH_INPUT         0x4
#define LOW_OUTPUT         0x8
#define HIGH_OUTPUT        0x10
#define INPUT_LEVELS       0x20
#define OUTPUT_LEVELS      0x40
#define INPUT_SLIDERS      0x80
#define OUTPUT_SLIDERS     0x100
#define DRAW               0x200
#define REFRESH_DST        0x400
#define ALL                0xFFF

#define MC_GET_SAMPLE_COLORS 1
#define MC_DST_REMAP         2
#define MC_ALL               (MC_GET_SAMPLE_COLORS | MC_DST_REMAP)

typedef struct
{
  gint32  dst_id;
  gint32  sample_id;

  gint32 hold_inten;       /* TRUE or FALSE */
  gint32 orig_inten;       /* TRUE or FALSE */
  gint32 rnd_subcolors;    /* TRUE or FALSE */
  gint32 guess_missing;    /* TRUE or FALSE */
  gint32 lvl_in_min;       /* 0 upto 254 */
  gint32 lvl_in_max;       /* 1 upto 255 */
  float  lvl_in_gamma;     /* 0.1 upto 10.0  (1.0 == linear) */
  gint32 lvl_out_min;       /* 0 upto 254 */
  gint32 lvl_out_max;       /* 1 upto 255 */

  float  tol_col_err;      /* 0.0% upto 100.0%
                            * this is uesd to findout colors of the same
                            * colortone, while analyzing sample colors,
                            * It does not make much sense for the user to adjust this
                            * value. (I used a param file to findout a suitable value)
                            */
} t_values;

typedef struct
{
  GtkWidget     *dialog;
  GtkWidget     *sample_preview;
  GtkWidget     *dst_preview;
  GtkWidget     *sample_colortab_preview;
  GtkWidget     *sample_drawarea;
  GtkWidget     *in_lvl_gray_preview;
  GtkWidget     *in_lvl_drawarea;
  GtkAdjustment *adj_lvl_in_min;
  GtkAdjustment *adj_lvl_in_max;
  GtkAdjustment *adj_lvl_in_gamma;
  GtkAdjustment *adj_lvl_out_min;
  GtkAdjustment *adj_lvl_out_max;
  GtkWidget     *orig_inten_button;
  gint           active_slider;
  gint           slider_pos[5];  /*  positions for the five sliders  */

  gint32         enable_preview_update;
  gint32         sample_show_selection;
  gint32         dst_show_selection;
  gint32         sample_show_color;
  gint32         dst_show_color;
}   t_samp_interface;



typedef struct
{
  guchar     color[4];  /* R,G,B,A */
  gint32     sum_color;         /* nr. of sourcepixels with (nearly the same) color */
  void      *next;
} t_samp_color_elem;



typedef struct
{
  gint32              all_samples;  /* number of all source pixels with this luminosity */
  gint                from_sample;  /* TRUE: color found in sample, FALSE: interpolated color added */
  t_samp_color_elem  *col_ptr;       /* List of sample colors at same luminosity */
} t_samp_table_elem;


typedef struct
{
   GimpDrawable *drawable;
   void         *sel_gdrw;
   GimpPixelRgn  pr;
   gint          x1;
   gint          y1;
   gint          x2;
   gint          y2;
   gint          index_alpha;   /* 0 == no alpha, 1 == GREYA, 3 == RGBA */
   gint          bpp;
   GimpTile     *tile;
   gint          tile_row;
   gint          tile_col;
   gint          tile_width;
   gint          tile_height;
   gint          tile_dirty;
   gint          shadow;
   gint32        seldeltax;
   gint32        seldeltay;
   gint32        tile_swapcount;
} t_GDRW;

/*
 * Some globals
 */

static t_samp_interface  g_di;  /* global dialog interface varables */
static t_values          g_values = { -1, -1, 1, 1, 0, 1, 0, 255, 1.0, 0, 255, 5.5 };
static t_samp_table_elem g_lum_tab[256];
static guchar            g_lvl_trans_tab[256];
static guchar            g_out_trans_tab[256];
static guchar            g_sample_color_tab[256 * 3];
static guchar            g_dst_preview_buffer[PREVIEW_SIZE_X * PREVIEW_SIZE_Y * 4 ];  /* color copy with mask of dst in previewsize */

static gint32  g_tol_col_err;
static gint32  g_max_col_err;
static gint    g_Sdebug = FALSE;
static gint    g_show_progress = FALSE;

/* Declare a local function.
 */
static void      query  (void);
static void      run    (const gchar      *name,
                         gint              nparams,
                         const GimpParam  *param,
                         gint            *nreturn_vals,
                         GimpParam       **return_vals);

static gint      p_main_colorize(gint);
static void      p_get_filevalues (void);
static void      p_smp_dialog (void);
static void      p_refresh_dst_preview(GtkWidget *preview, guchar *src_buffer);
static void      p_update_preview (gint32 *id_ptr);
static void      p_clear_tables(void);
static void      p_free_colors(void);
static void      p_levels_update (gint update);
static gint      p_level_in_events (GtkWidget *widget, GdkEvent *event, gpointer data);
static gint      p_level_out_events (GtkWidget *widget, GdkEvent *event, gpointer data);
static void      p_calculate_level_transfers (void);
static void      p_get_pixel(t_GDRW *gdrw, gint32 x, gint32 y, guchar *pixel);
static void      p_init_gdrw(t_GDRW *gdrw, GimpDrawable *drawable, gint dirty, gint shadow);
static void      p_end_gdrw(t_GDRW *gdrw);
static gint32    p_is_layer_alive(gint32 drawable_id);
static void      p_remap_pixel (guchar       *pixel,
                                const guchar *original,
                                gint          bpp2);
static void      p_guess_missing_colors(void);
static void      p_fill_missing_colors(void);
static void      p_smp_get_colors(GtkWidget *dialog);
static void      p_get_gradient (gint mode);
static void      p_clear_preview(GtkWidget *preview);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run    /* run_proc   */
};

MAIN ()

static void
query (void)
{
  static GimpParamDef args[]=
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "dst_drawable", "The drawable to be colorized (Type GRAY* or RGB*)" },
    { GIMP_PDB_DRAWABLE, "sample_drawable", "Sample drawable (should be of Type RGB or RGBA)" },
    { GIMP_PDB_INT32, "hold_inten", "hold brightness intensity levels (TRUE, FALSE)" },
    { GIMP_PDB_INT32, "orig_inten", "TRUE: hold brightness of original intensity levels. FALSE: Hold Intensity of input levels" },
    { GIMP_PDB_INT32, "rnd_subcolors", "TRUE: Use all subcolors of same intensity, FALSE: use only one color per intensity" },
    { GIMP_PDB_INT32, "guess_missing", "TRUE: guess samplecolors for the missing intensity values FALSE: use only colors found in the sample" },
    { GIMP_PDB_INT32, "in_low",   "intensity of lowest input (0 <= in_low <= 254)" },
    { GIMP_PDB_INT32, "in_high",  "intensity of highest input (1 <= in_high <= 255)" },
    { GIMP_PDB_FLOAT, "gamma",  "gamma correction factor (0.1 <= gamma <= 10) where 1.0 is linear" },
    { GIMP_PDB_INT32, "out_low",   "lowest sample color intensity (0 <= out_low <= 254)" },
    { GIMP_PDB_INT32, "out_high",  "highest sample color intensity (1 <= out_high <= 255)" }
  };

  static gchar *help_string =
    "This plug-in colorizes the contents of the specified (gray) layer"
    " with the help of a  sample (color) layer."
    " It analyzes all colors in the sample layer."
    " The sample colors are sorted by brightness (== intentisty) and amount"
    " and stored in a sample colortable (where brightness is the index)"
    " The pixels of the destination layer are remapped with the help of the"
    " sample colortable. If use_subcolors is TRUE, the remapping process uses"
    " all sample colors of the corresponding brightness-intensity and"
    " distributes the subcolors according to their amount in the sample"
    " (If the sample has 5 green, 3 yellow, and 1 red pixel of the "
    " intensity value 105, the destination pixels at intensity value 105"
    " are randomly painted in green, yellow and red in a relation of 5:3:1"
    " If use_subcolors is FALSE only one sample color per intensity is used."
    " (green will be used in this example)"
    " The brightness intensity value is transformed at the remapping process"
    " according to the levels: out_lo, out_hi, in_lo, in_high and gamma"
    " The in_low / in_high levels specify an initial mapping of the intensity."
    " The gamma value determines how intensities are interpolated between"
    " the in_lo and in_high levels. A gamma value of 1.0 results in linear"
    " interpolation. Higher gamma values results in more high-level intensities"
    " Lower gamma values results in more low-level intensities"
    " The out_low/out_high levels constrain the resulting intensity index"
    " The intensity index is used to pick the corresponding color"
    " in the sample colortable. If hold_inten is FALSE the picked color"
    " is used 1:1 as resulting remap_color."
    " If hold_inten is TRUE The brightness of the picked color is adjusted"
    " back to the origial intensity value (only hue and saturation are"
    " taken from the picked sample color)"
    " (or to the input level, if orig_inten is set FALSE)"
    " Works on both Grayscale and RGB image with/without alpha channel."
    " (the image with the dst_drawable is converted to RGB if necessary)"
    " The sample_drawable should be of type RGB or RGBA";

  gimp_install_procedure (PLUG_IN_NAME,
                          "Colorize the contents of the specified drawable "
                          "similar to sample drawable",
                          help_string,
                          "Wolfgang Hofer",
                          "hof@hotbot.com",
                          "02/2000",
                          N_("_Sample Colorize..."),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register (PLUG_IN_NAME,
                             "<Image>/Filters/Colors/Map");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpDrawable      *dst_drawable;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  const gchar       *l_env;
  GimpRunMode        run_mode;

  INIT_I18N ();

  l_env = g_getenv ("SAMPLE_COLORIZE_DEBUG");
  if (l_env != NULL)
    {
      if((*l_env != 'n') && (*l_env != 'N')) g_Sdebug = TRUE;
    }

  if(g_Sdebug) printf("sample colorize run\n");
  g_show_progress = FALSE;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  g_values.lvl_out_min = 0;
  g_values.lvl_out_max = 255;
  g_values.lvl_in_min = 0;
  g_values.lvl_in_max = 255;
  g_values.lvl_in_gamma = 1.0;

  /* Possibly retrieve data from a previous run */
  gimp_get_data (PLUG_IN_NAME, &g_values);

  /* fix value */
  g_values.tol_col_err = 5.5;

  /*  Get the specified dst_drawable  */
  g_values.dst_id = param[2].data.d_drawable;
  dst_drawable = gimp_drawable_get (g_values.dst_id );

  p_clear_tables();

  /*  Make sure that the dst_drawable is gray or RGB color        */
  if (gimp_drawable_is_rgb (dst_drawable->drawable_id) ||
      gimp_drawable_is_gray (dst_drawable->drawable_id))
    {
      gimp_tile_cache_ntiles (TILE_CACHE_SIZE);

      switch (run_mode)
        {
        case GIMP_RUN_INTERACTIVE:
          p_smp_dialog ();
          p_free_colors ();
          gimp_set_data (PLUG_IN_NAME, &g_values, sizeof (t_values));
          gimp_displays_flush ();
          break;

        case GIMP_RUN_NONINTERACTIVE:
          if (nparams == NUMBER_IN_ARGS)
            {
              g_values.sample_id     = param[3].data.d_drawable;
              g_values.hold_inten    = param[4].data.d_int32;
              g_values.orig_inten    = param[5].data.d_int32;
              g_values.rnd_subcolors = param[6].data.d_int32;
              g_values.guess_missing = param[7].data.d_int32;
              g_values.lvl_in_min    = param[8].data.d_int32;
              g_values.lvl_in_max    = param[9].data.d_int32;
              g_values.lvl_in_gamma  = param[10].data.d_float;
              g_values.lvl_out_min   = param[11].data.d_int32;
              g_values.lvl_out_max   = param[12].data.d_int32;
            }
          else
            {
              status = GIMP_PDB_CALLING_ERROR;
            }
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          break;
        }
    }
  else
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (dst_drawable);
}

/* ============================================================================
 * callback and constraint procedures for the dialog
 * ============================================================================
 */

static void
p_smp_response_callback (GtkWidget *widget,
                         gint       response_id,
                         gpointer   data)
{
  switch (response_id)
    {
    case RESPONSE_RESET:
      g_values.lvl_in_min    = 0;
      g_values.lvl_in_max    = 255;
      g_values.lvl_in_gamma  = 1.0;
      g_values.lvl_out_min   = 0;
      g_values.lvl_out_max   = 255;

      p_levels_update (ALL);
      break;

    case RESPONSE_GET_COLORS:
      p_smp_get_colors (widget);
      break;

    case GTK_RESPONSE_APPLY:
      g_show_progress = TRUE;
      if (p_main_colorize (MC_DST_REMAP) == 0)
        {
          gimp_displays_flush ();
          g_show_progress = FALSE;
          return;
        }
      gtk_dialog_set_response_sensitive (GTK_DIALOG (widget),
                                         GTK_RESPONSE_APPLY, FALSE);
      break;

    default:
      gtk_widget_destroy (widget);
      gtk_main_quit ();
      break;
    }
}

static void
p_smp_toggle_callback (GtkWidget *widget,
                       gpointer   data)
{
  gint32 *toggle_val = (gint32 *) data;

  if (GTK_TOGGLE_BUTTON (widget)->active) { *toggle_val = TRUE;  }
  else                                    { *toggle_val = FALSE; }

  if((data == &g_di.sample_show_selection) || (data == &g_di.sample_show_color))
  {
     p_update_preview (&g_values.sample_id);
     return;
  }

  if((data == &g_di.dst_show_selection) || (data == &g_di.dst_show_color))
  {
     p_update_preview (&g_values.dst_id);
     return;
  }

  if((data == &g_values.hold_inten) || (data == &g_values.orig_inten)
  || (data == &g_values.rnd_subcolors))
  {
     if(g_di.orig_inten_button)
     {
       gtk_widget_set_sensitive (g_di.orig_inten_button,g_values.hold_inten);
     }
     p_refresh_dst_preview (g_di.dst_preview,  &g_dst_preview_buffer[0]);
  }

  if(data == &g_values.guess_missing)
  {
     if(g_values.guess_missing) { p_guess_missing_colors(); }
     else                       { p_fill_missing_colors(); }
     p_smp_get_colors (NULL);
  }

}

static void
p_smp_sample_combo_callback (GtkWidget *widget,
                             gpointer   data)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  g_values.sample_id = value;

  if (value == SMP_GRADIENT || value == SMP_INV_GRADIENT)
    {
      p_get_gradient (value);
      p_smp_get_colors (NULL);

      if (g_di.sample_preview)
        p_clear_preview (g_di.sample_preview);

      gtk_dialog_set_response_sensitive (GTK_DIALOG (g_di.dialog),
                                         GTK_RESPONSE_APPLY, TRUE);
      gtk_dialog_set_response_sensitive (GTK_DIALOG (g_di.dialog),
                                         RESPONSE_GET_COLORS, FALSE);
    }
  else
    {
      p_update_preview (&g_values.sample_id);

      gtk_dialog_set_response_sensitive (GTK_DIALOG (g_di.dialog),
                                         RESPONSE_GET_COLORS, TRUE);
    }
}

static void
p_smp_dest_combo_callback (GtkWidget *widget,
                           gpointer   data)
{
  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget),
                                 &g_values.dst_id);

  p_update_preview (&g_values.dst_id);

  gtk_dialog_set_response_sensitive (GTK_DIALOG (g_di.dialog),
                                     RESPONSE_GET_COLORS, TRUE);
}

static gint
p_smp_constrain (gint32    image_id,
                 gint32    drawable_id,
                 gpointer  data)
{
  if (image_id < 0)
    return FALSE;

  /* dont accept layers from indexed images */
  if (gimp_drawable_is_indexed (drawable_id))
    return FALSE;

  return TRUE;
}


static void
p_smp_adj_lvl_in_max_upd_callback (GtkAdjustment *adjustment,
                                   gpointer       data)
{
  gint32 value;
  gint   upd_flags;

  value = CLAMP ((adjustment->value), 1, 255);

  if (value != g_values.lvl_in_max)
  {
    g_values.lvl_in_max = value;
    upd_flags = INPUT_SLIDERS | INPUT_LEVELS | DRAW | REFRESH_DST;
    if(g_values.lvl_in_max < g_values.lvl_in_min)
    {
      g_values.lvl_in_min = g_values.lvl_in_max;
      upd_flags |= LOW_INPUT;
    }
    p_levels_update (upd_flags);
  }
}

static void
p_smp_adj_lvl_in_min_upd_callback(GtkAdjustment *adjustment,
                              gpointer   data)
{
  double value;
  gint   upd_flags;

  value = CLAMP ((adjustment->value), 0, 254);

  if (value != g_values.lvl_in_min)
  {
    g_values.lvl_in_min = value;
    upd_flags = INPUT_SLIDERS | INPUT_LEVELS | DRAW | REFRESH_DST;
    if(g_values.lvl_in_min > g_values.lvl_in_max)
    {
      g_values.lvl_in_max = g_values.lvl_in_min;
      upd_flags |= HIGH_INPUT;
    }
    p_levels_update (upd_flags);
  }
}

static void
p_smp_text_gamma_upd_callback (GtkAdjustment *adjustment,
                               gpointer       data)
{
  double value;

  value = CLAMP ((adjustment->value), 0.1, 10.0);

  if (value != g_values.lvl_in_gamma)
  {
    g_values.lvl_in_gamma = value;
    p_levels_update (INPUT_SLIDERS | INPUT_LEVELS | DRAW | REFRESH_DST);
  }
}

static void
p_smp_adj_lvl_out_max_upd_callback (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  gint32 value;
  gint   upd_flags;

  value = CLAMP ((adjustment->value), 1, 255);

  if (value != g_values.lvl_out_max)
  {
    g_values.lvl_out_max = value;
    upd_flags = OUTPUT_SLIDERS | OUTPUT_LEVELS | DRAW | REFRESH_DST;
    if(g_values.lvl_out_max < g_values.lvl_out_min)
    {
      g_values.lvl_out_min = g_values.lvl_out_max;
      upd_flags |= LOW_OUTPUT;
    }
    p_levels_update (upd_flags);
  }
}

static void
p_smp_adj_lvl_out_min_upd_callback (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  double value;
  gint   upd_flags;

  value = CLAMP ((adjustment->value), 0, 254);

  if (value != g_values.lvl_out_min)
  {
    g_values.lvl_out_min = value;
    upd_flags = OUTPUT_SLIDERS | OUTPUT_LEVELS | DRAW  | REFRESH_DST;
    if(g_values.lvl_out_min > g_values.lvl_out_max)
    {
      g_values.lvl_out_max = g_values.lvl_out_min;
      upd_flags |= HIGH_OUTPUT;
    }
    p_levels_update (upd_flags);
  }
}

/* ============================================================================
 * DIALOG helper procedures
 *    (workers for the updates on the preview widgets)
 * ============================================================================
 */

static void
p_refresh_dst_preview (GtkWidget *preview,
                       guchar    *src_buffer)
{
  guchar  l_allrowsbuf[3 * PREVIEW_SIZE_X * PREVIEW_SIZE_Y];
  guchar *l_ptr;
  guchar *l_src_ptr;
  guchar  l_lum;
  guchar  l_maskbyte;
  gint    l_x, l_y;
  gint    l_preview_bpp;
  gint    l_src_bpp;

  l_preview_bpp = PREVIEW_BPP;
  l_src_bpp = PREVIEW_BPP +1;   /* 3 colors + 1 maskbyte */
  l_src_ptr = src_buffer;

  l_ptr = l_allrowsbuf;
  for(l_y = 0; l_y < PREVIEW_SIZE_Y; l_y++)
    {
      for(l_x = 0; l_x < PREVIEW_SIZE_X; l_x++)
        {
          if((l_maskbyte = l_src_ptr[3]) == 0)
            {
              l_ptr[0] = l_src_ptr[0];
              l_ptr[1] = l_src_ptr[1];
              l_ptr[2] = l_src_ptr[2];
            }
          else
            {
              if(g_di.dst_show_color)
                {
                  p_remap_pixel(l_ptr, l_src_ptr, 3);
                }
              else
                {
                  /* l_lum = g_out_trans_tab[g_lvl_trans_tab[LUMINOSITY_1(l_src_ptr)]]; */
                  /* get brightness from (uncolorized) original */

                  l_lum = g_lvl_trans_tab[LUMINOSITY_1(l_src_ptr)];
                  /* get brightness from (uncolorized) original */

                  l_ptr[0] = l_lum;
                  l_ptr[1] = l_lum;
                  l_ptr[2] = l_lum;
                }

              if (l_maskbyte < 255)
                {
                  l_ptr[0] = MIX_CHANNEL(l_ptr[0], l_src_ptr[0], l_maskbyte);
                  l_ptr[1] = MIX_CHANNEL(l_ptr[1], l_src_ptr[1], l_maskbyte);
                  l_ptr[2] = MIX_CHANNEL(l_ptr[2], l_src_ptr[2], l_maskbyte);
                }
            }
          l_ptr += l_preview_bpp;
          l_src_ptr += l_src_bpp;
        }
    }
  gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview),
                          0, 0, PREVIEW_SIZE_X, PREVIEW_SIZE_Y,
                          GIMP_RGB_IMAGE,
                          l_allrowsbuf,
                          PREVIEW_SIZE_X * 3);
}        /* end p_refresh_dst_preview */

static void
p_clear_preview (GtkWidget *preview)
{
  gimp_preview_area_fill (GIMP_PREVIEW_AREA (preview),
                          0, 0, PREVIEW_SIZE_X, PREVIEW_SIZE_Y,
                          170, 170, 170);
}        /* end p_clear_preview */

static void
p_update_pv (GtkWidget *preview,
             gint32     show_selection,
             t_GDRW    *gdrw,
             guchar    *dst_buffer,
             gint       is_color)
{
  guchar  l_allrowsbuf[4 * PREVIEW_SIZE_X * PREVIEW_SIZE_Y];
  guchar  l_pixel[4];
  guchar *l_ptr;
  gint    l_x, l_y;
  gint    l_x2, l_y2;
  gint    l_ofx, l_ofy;
  gint    l_sel_width, l_sel_height;
  double  l_scale_x, l_scale_y;
  guchar *l_buf_ptr;
  guchar  l_dummy[4];
  guchar  l_maskbytes[4];
  gint    l_dstep;
  guchar  l_alpha;


  if (!preview)
    return;

  /* init gray pixel (if we are called without a sourceimage (gdwr == NULL) */
  l_pixel[0] = l_pixel[1] =l_pixel[2] = l_pixel[3] = 127;

  /* calculate scale factors and offsets */
  if(show_selection)
    {
      l_sel_width  = gdrw->x2 - gdrw->x1;
      l_sel_height = gdrw->y2 - gdrw->y1;

      if(l_sel_height > l_sel_width)
        {
          l_scale_y = (gfloat) l_sel_height / PREVIEW_SIZE_Y;
          l_scale_x = l_scale_y;
          l_ofx = (gdrw->x1 +
                   ((l_sel_width - (PREVIEW_SIZE_X * l_scale_x)) / 2));
          l_ofy = gdrw->y1;
        }
      else
        {
          l_scale_x = (gfloat) l_sel_width / PREVIEW_SIZE_X;
          l_scale_y = l_scale_x;
          l_ofx = gdrw->x1;
          l_ofy = (gdrw->y1 +
                   ((l_sel_height - (PREVIEW_SIZE_Y * l_scale_y)) / 2));
        }
    }
  else
    {
      if (gdrw->drawable->height > gdrw->drawable->width)
        {
          l_scale_y = (gfloat) gdrw->drawable->height / PREVIEW_SIZE_Y;
          l_scale_x = l_scale_y;
          l_ofx = (gdrw->drawable->width - (PREVIEW_SIZE_X * l_scale_x)) / 2;
          l_ofy = 0;
        }
      else
        {
          l_scale_x = (gfloat) gdrw->drawable->width / PREVIEW_SIZE_X;
          l_scale_y = l_scale_x;
          l_ofx = 0;
          l_ofy = (gdrw->drawable->height - (PREVIEW_SIZE_Y * l_scale_y)) / 2;
        }
    }

  /* check if output goes to previw widget or to dst_buffer */
  if (dst_buffer) { l_buf_ptr = dst_buffer; l_dstep = PREVIEW_BPP +1; }
  else            { l_buf_ptr = &l_dummy[0]; l_dstep = 0; }


  /* render preview */
  l_ptr = l_allrowsbuf;
  for (l_y = 0; l_y < PREVIEW_SIZE_Y; l_y++)
    {
      for(l_x = 0; l_x < PREVIEW_SIZE_X; l_x++)
        {
          if (gdrw->drawable)
            {
              l_x2 = l_ofx + (l_x * l_scale_x);
              l_y2 = l_ofy + (l_y * l_scale_y);
              p_get_pixel(gdrw, l_x2, l_y2, &l_pixel[0]);
              if (gdrw->sel_gdrw)
                {
                  p_get_pixel (gdrw->sel_gdrw,
                               l_x2 + gdrw->seldeltax,
                               l_y2 + gdrw->seldeltay,
                               &l_maskbytes[0]);
                }
              else
                {
                  l_maskbytes[0] = 255;
                }
            }

          l_alpha = l_pixel[gdrw->index_alpha];
          if (is_color && (gdrw->bpp > 2))
            {
              l_buf_ptr[0] = l_ptr[0] = l_pixel[0];
              l_buf_ptr[1] = l_ptr[1] = l_pixel[1];
              l_buf_ptr[2] = l_ptr[2] = l_pixel[2];
            }
          else
            {
              if(gdrw->bpp > 2)  *l_ptr = LUMINOSITY_1(l_pixel);
              else               *l_ptr = l_pixel[0];

              *l_buf_ptr   = *l_ptr;
              l_buf_ptr[1] = l_ptr[1] = *l_ptr;
              l_buf_ptr[2] = l_ptr[2] = *l_ptr;
            }
          if (gdrw->index_alpha == 0)             /* has no alpha channel */
            l_buf_ptr[3] = l_ptr[3] = 255;
          else          
            l_buf_ptr[3] = l_ptr[3] = MIN(l_maskbytes[0], l_alpha);
          l_buf_ptr += l_dstep;   /* advance (or stay at dummy byte) */
          l_ptr += 4;
        }

    }

  if (dst_buffer == NULL)
    {
      gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview),
                              0, 0, PREVIEW_SIZE_X, PREVIEW_SIZE_Y,
                              GIMP_RGBA_IMAGE,
                              l_allrowsbuf,
                              PREVIEW_SIZE_X * 4);
      gtk_widget_queue_draw (preview);
    }
}        /* end p_update_pv */

static void
p_update_preview (gint32 *id_ptr)
{
  GimpDrawable *drawable;
  t_GDRW     l_gdrw;

  if(g_Sdebug)  printf("UPD PREVIEWS   ID:%d ENABLE_UPD:%d\n", (int)*id_ptr, (int)g_di.enable_preview_update);

  if(id_ptr == NULL)                 { return; }
  if(!g_di.enable_preview_update)    { return; }
  if(p_is_layer_alive(*id_ptr) < 0)
    {
      /* clear preview on invalid drawable id
       * (SMP_GRADIENT and SMP_INV_GRADIENT)
       */
      if(id_ptr == &g_values.sample_id)
        {
          p_clear_preview(g_di.sample_preview);
        }
      if(id_ptr == &g_values.dst_id)
        {
          p_clear_preview(g_di.dst_preview);
        }
      return;
    }

  drawable = gimp_drawable_get (*id_ptr);

  if(id_ptr == &g_values.sample_id)
    {
      p_init_gdrw(&l_gdrw, drawable, FALSE, FALSE);
      p_update_pv(g_di.sample_preview, g_di.sample_show_selection, &l_gdrw, NULL, g_di.sample_show_color);
    }
  else if (id_ptr == &g_values.dst_id)
    {
      p_init_gdrw(&l_gdrw, drawable, FALSE, FALSE);
      p_update_pv(g_di.dst_preview, g_di.dst_show_selection, &l_gdrw, &g_dst_preview_buffer[0], g_di.dst_show_color);
      p_refresh_dst_preview(g_di.dst_preview,  &g_dst_preview_buffer[0]);
    }

  if (drawable)
    p_end_gdrw (&l_gdrw);
}        /* end p_update_preview */

static void
p_levels_draw_slider (GdkWindow *window,
                      GdkGC     *border_gc,
                      GdkGC     *fill_gc,
                      gint       xpos)
{
  gint y;

  for (y = 0; y < CONTROL_HEIGHT; y++)
    gdk_draw_line(window, fill_gc, xpos - y / 2, y,
                  xpos + y / 2, y);

  gdk_draw_line(window, border_gc, xpos, 0,
                xpos - (CONTROL_HEIGHT - 1) / 2, CONTROL_HEIGHT - 1);

  gdk_draw_line(window, border_gc, xpos, 0,
                xpos + (CONTROL_HEIGHT - 1) / 2, CONTROL_HEIGHT - 1);

  gdk_draw_line(window, border_gc,
                xpos - (CONTROL_HEIGHT - 1) / 2, CONTROL_HEIGHT - 1,
                xpos + (CONTROL_HEIGHT - 1) / 2, CONTROL_HEIGHT - 1);
}        /* end p_levels_draw_slider */

static void
p_levels_erase_slider (GdkWindow *window,
                       gint       xpos)
{
  gdk_window_clear_area (window, xpos - (CONTROL_HEIGHT - 1) / 2, 0,
                         CONTROL_HEIGHT - 1, CONTROL_HEIGHT);
}        /* end p_levels_erase_slider */

static void
p_smp_get_colors (GtkWidget *dialog)
{
  gint   i;
  guchar buffer[3 * DA_WIDTH * GRADIENT_HEIGHT];

  p_update_preview (&g_values.sample_id);

  if (dialog)
    {
      if (p_main_colorize (MC_GET_SAMPLE_COLORS) >= 0)  /* do not colorize, just analyze sample colors */
        {
          gtk_dialog_set_response_sensitive (GTK_DIALOG (g_di.dialog),
                                             GTK_RESPONSE_APPLY, TRUE);
        }
    }
  for (i = 0; i < GRADIENT_HEIGHT; i++)
    {
      memcpy (buffer+i*3*DA_WIDTH, g_sample_color_tab, 3*DA_WIDTH);
    }
  p_update_preview (&g_values.dst_id);

  gimp_preview_area_draw (GIMP_PREVIEW_AREA (g_di.sample_colortab_preview),
                          0, 0, DA_WIDTH, GRADIENT_HEIGHT,
                          GIMP_RGB_IMAGE,
                          buffer,
                          DA_WIDTH * 3);
}


static void
p_levels_update (gint update)
{
  gint i;

  if(g_Sdebug)  printf("p_levels_update: update reques %x\n", update);

  /*  Recalculate the transfer array  */
  p_calculate_level_transfers();
  if (update & REFRESH_DST)
    {
      p_refresh_dst_preview(g_di.dst_preview,  &g_dst_preview_buffer[0]);
    }

  /* update the spinbutton entry widgets */
  if (update & LOW_INPUT)
    {
      gtk_adjustment_set_value (g_di.adj_lvl_in_min,
                                g_values.lvl_in_min);
    }
  if (update & GAMMA)
    {
      gtk_adjustment_set_value (g_di.adj_lvl_in_gamma,
                                g_values.lvl_in_gamma);
    }
  if (update & HIGH_INPUT)
    {
      gtk_adjustment_set_value (g_di.adj_lvl_in_max,
                                g_values.lvl_in_max);
    }
  if (update & LOW_OUTPUT)
    {
      gtk_adjustment_set_value (g_di.adj_lvl_out_min,
                                g_values.lvl_out_min);
    }
  if (update & HIGH_OUTPUT)
    {
      gtk_adjustment_set_value (g_di.adj_lvl_out_max,
                                g_values.lvl_out_max);
    }
  if (update & INPUT_LEVELS)
    {
      guchar buffer[DA_WIDTH * GRADIENT_HEIGHT];
      for (i = 0; i < GRADIENT_HEIGHT; i++)
        {
          memcpy (buffer+DA_WIDTH*i, g_lvl_trans_tab, DA_WIDTH);
        }
      gimp_preview_area_draw (GIMP_PREVIEW_AREA (g_di.in_lvl_gray_preview),
                              0, 0, DA_WIDTH, GRADIENT_HEIGHT,
                              GIMP_GRAY_IMAGE,
                              buffer,
                              DA_WIDTH);
    }

  if (update & INPUT_SLIDERS)
    {
      gdouble width, mid, tmp;

      p_levels_erase_slider (g_di.in_lvl_drawarea->window, g_di.slider_pos[0]);
      p_levels_erase_slider (g_di.in_lvl_drawarea->window, g_di.slider_pos[1]);
      p_levels_erase_slider (g_di.in_lvl_drawarea->window, g_di.slider_pos[2]);

      g_di.slider_pos[0] = DA_WIDTH * ((double) g_values.lvl_in_min / 255.0);
      g_di.slider_pos[2] = DA_WIDTH * ((double) g_values.lvl_in_max / 255.0);

      width = (double) (g_di.slider_pos[2] - g_di.slider_pos[0]) / 2.0;
      mid = g_di.slider_pos[0] + width;
      tmp = log10 (1.0 / g_values.lvl_in_gamma);
      g_di.slider_pos[1] = (int) (mid + width * tmp + 0.5);

      p_levels_draw_slider (g_di.in_lvl_drawarea->window,
                            g_di.in_lvl_drawarea->style->black_gc,
                            g_di.in_lvl_drawarea->style->dark_gc[GTK_STATE_NORMAL],
                            g_di.slider_pos[1]);
      p_levels_draw_slider (g_di.in_lvl_drawarea->window,
                            g_di.in_lvl_drawarea->style->black_gc,
                            g_di.in_lvl_drawarea->style->black_gc,
                            g_di.slider_pos[0]);
      p_levels_draw_slider (g_di.in_lvl_drawarea->window,
                            g_di.in_lvl_drawarea->style->black_gc,
                            g_di.in_lvl_drawarea->style->white_gc,
                            g_di.slider_pos[2]);
    }

  if (update & OUTPUT_SLIDERS)
    {
      p_levels_erase_slider (g_di.sample_drawarea->window, g_di.slider_pos[3]);
      p_levels_erase_slider (g_di.sample_drawarea->window, g_di.slider_pos[4]);

      g_di.slider_pos[3] = DA_WIDTH * ((double) g_values.lvl_out_min / 255.0);
      g_di.slider_pos[4] = DA_WIDTH * ((double) g_values.lvl_out_max / 255.0);

      p_levels_draw_slider (g_di.sample_drawarea->window,
                            g_di.sample_drawarea->style->black_gc,
                            g_di.sample_drawarea->style->black_gc,
                            g_di.slider_pos[3]);
      p_levels_draw_slider (g_di.sample_drawarea->window,
                            g_di.sample_drawarea->style->black_gc,
                            g_di.sample_drawarea->style->black_gc,
                            g_di.slider_pos[4]);
    }
}        /* end p_levels_update */

static gboolean
p_level_in_events (GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   data)
{
  GdkEventButton *bevent;
  GdkEventMotion *mevent;
  gchar text[20];
  double width, mid, tmp;
  gint x, distance;
  gint i;
  gint update = FALSE;


  switch (event->type)
    {
    case GDK_EXPOSE:
      if(g_Sdebug) printf("EVENT: GDK_EXPOSE\n");
      if (widget == g_di.in_lvl_drawarea)
        p_levels_update (INPUT_SLIDERS);
      break;

    case GDK_BUTTON_PRESS:
      if(g_Sdebug) printf("EVENT: GDK_BUTTON_PRESS\n");
      gtk_grab_add (widget);
      bevent = (GdkEventButton *) event;

      distance = G_MAXINT;
      for (i = 0; i < 3; i++)
        {
          if (fabs (bevent->x - g_di.slider_pos[i]) < distance)
            {
              g_di.active_slider = i;
              distance = fabs (bevent->x - g_di.slider_pos[i]);
            }
        }

      x = bevent->x;
      update = TRUE;
      break;

    case GDK_BUTTON_RELEASE:
      if(g_Sdebug) printf("EVENT: GDK_BUTTON_RELEASE\n");
      gtk_grab_remove (widget);
      switch (g_di.active_slider)
        {
        case 0:  /*  low input  */
          p_levels_update (LOW_INPUT | GAMMA | DRAW);
          break;
        case 1:  /*  gamma  */
          p_levels_update (GAMMA);
          break;
        case 2:  /*  high input  */
          p_levels_update (HIGH_INPUT | GAMMA | DRAW);
          break;
        }

      p_refresh_dst_preview(g_di.dst_preview,  &g_dst_preview_buffer[0]);
      break;

    case GDK_MOTION_NOTIFY:
      if(g_Sdebug) printf("EVENT: GDK_MOTION_NOTIFY\n");
      mevent = (GdkEventMotion *) event;
      gdk_window_get_pointer (widget->window, &x, NULL, NULL);
      update = TRUE;
      break;

    default:
      if(g_Sdebug)  printf("EVENT: default\n");
      break;
    }

  if (update)
    {
      if(g_Sdebug)  printf("EVENT: ** update **\n");
      switch (g_di.active_slider)
        {
        case 0:  /*  low input  */
          g_values.lvl_in_min = ((double) x / (double) DA_WIDTH) * 255.0;
          g_values.lvl_in_min = CLAMP (g_values.lvl_in_min,
                                       0, g_values.lvl_in_max);
          break;

        case 1:  /*  gamma  */
          width = (double) (g_di.slider_pos[2] - g_di.slider_pos[0]) / 2.0;
          mid = g_di.slider_pos[0] + width;

          x = CLAMP (x, g_di.slider_pos[0], g_di.slider_pos[2]);
          tmp = (double) (x - mid) / width;
          g_values.lvl_in_gamma = 1.0 / pow (10, tmp);

          /*  round the gamma value to the nearest 1/100th  */
          sprintf (text, "%2.2f", g_values.lvl_in_gamma);
          g_values.lvl_in_gamma = atof (text);
          break;

        case 2:  /*  high input  */
          g_values.lvl_in_max = ((double) x / (double) DA_WIDTH) * 255.0;
          g_values.lvl_in_max = CLAMP (g_values.lvl_in_max,
                                       g_values.lvl_in_min, 255);
          break;
        }

      p_levels_update (INPUT_SLIDERS | INPUT_LEVELS | DRAW);
    }

  return FALSE;
}        /* end p_level_in_events */

static gboolean
p_level_out_events (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   data)
{
  GdkEventButton *bevent;
  GdkEventMotion *mevent;
  gint x, distance;
  gint i;
  gint update = FALSE;

  switch (event->type)
    {
    case GDK_EXPOSE:
      if(g_Sdebug) printf("OUT_EVENT: GDK_EXPOSE\n");
      if (widget == g_di.sample_drawarea)
        p_levels_update (OUTPUT_SLIDERS);
      break;

    case GDK_BUTTON_PRESS:
      if(g_Sdebug) printf("OUT_EVENT: GDK_BUTTON_PRESS\n");
      bevent = (GdkEventButton *) event;

      distance = G_MAXINT;
      for (i = 3; i < 5; i++)
        {
          if (fabs (bevent->x - g_di.slider_pos[i]) < distance)
            {
              g_di.active_slider = i;
              distance = fabs (bevent->x - g_di.slider_pos[i]);
            }
        }

      x = bevent->x;
      update = TRUE;
      break;

    case GDK_BUTTON_RELEASE:
      if(g_Sdebug) printf("OUT_EVENT: GDK_BUTTON_RELEASE\n");
      switch (g_di.active_slider)
        {
        case 3:  /*  low output  */
          p_levels_update (LOW_OUTPUT | DRAW);
          break;
        case 4:  /*  high output  */
          p_levels_update (HIGH_OUTPUT | DRAW);
          break;
        }

      p_refresh_dst_preview(g_di.dst_preview,  &g_dst_preview_buffer[0]);
      break;

    case GDK_MOTION_NOTIFY:
      if(g_Sdebug) printf("OUT_EVENT: GDK_MOTION_NOTIFY\n");
      mevent = (GdkEventMotion *) event;
      gdk_window_get_pointer (widget->window, &x, NULL, NULL);
      update = TRUE;
      break;

    default:
      if(g_Sdebug)  printf("OUT_EVENT: default\n");
     break;
    }

  if (update)
    {
      if(g_Sdebug)  printf("OUT_EVENT: ** update **\n");
      switch (g_di.active_slider)
        {
        case 3:  /*  low output  */
          g_values.lvl_out_min = ((double) x / (double) DA_WIDTH) * 255.0;
          g_values.lvl_out_min = CLAMP (g_values.lvl_out_min,
                                       0, g_values.lvl_out_max);
          break;

        case 4:  /*  high output  */
          g_values.lvl_out_max = ((double) x / (double) DA_WIDTH) * 255.0;
          g_values.lvl_out_max = CLAMP (g_values.lvl_out_max,
                                       g_values.lvl_out_min, 255);
          break;
        }

      p_levels_update (OUTPUT_SLIDERS | OUTPUT_LEVELS | DRAW);
    }

  return FALSE;
}        /* end p_level_out_events */


/* ============================================================================
 * p_smp_dialog
 *        The Interactive Dialog
 * ============================================================================
 */
static void
p_smp_dialog (void)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *vbox2;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *check_button;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *spinbutton;
  GtkObject *data;
  gint       l_ty;

  /* set flags for check buttons from mode value bits */
  if (g_Sdebug) g_print ("p_smp_dialog START\n");

  /* init some dialog variables */
  g_di.enable_preview_update = FALSE;
  g_di.sample_show_selection = TRUE;
  g_di.dst_show_selection    = TRUE;
  g_di.dst_show_color        = TRUE;
  g_di.sample_show_color     = TRUE;
  g_di.orig_inten_button     = NULL;

  /* Init GTK  */
  gimp_ui_init ("sample_colorize", TRUE);

  /* Main Dialog */
  g_di.dialog = dialog =
    gimp_dialog_new (_("Sample Colorize"), "sample_colorize",
                     NULL, 0,
                     gimp_standard_help_func, HELP_ID,

                     GIMP_STOCK_RESET,       RESPONSE_RESET,
                     _("Get sample colors"), RESPONSE_GET_COLORS,
                     GTK_STOCK_CLOSE,        GTK_RESPONSE_CLOSE,
                     _("Apply"),             GTK_RESPONSE_APPLY,

                     NULL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (p_smp_response_callback),
                    dialog);

  /* table for values */
  table = gtk_table_new (7, 5, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      table, TRUE, TRUE, 0);

  l_ty = 0;
  /* layer combo_box (Dst) */
  label = gtk_label_new (_("Destination:"));
  gtk_misc_set_alignment (GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, l_ty, l_ty + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  combo = gimp_layer_combo_box_new (p_smp_constrain, NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), g_values.dst_id,
                              G_CALLBACK (p_smp_dest_combo_callback),
                              NULL);

  gtk_table_attach (GTK_TABLE (table), combo, 1, 2, l_ty, l_ty + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  /* layer combo_box (Sample) */
  label = gtk_label_new (_("Sample:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 3, 4, l_ty, l_ty + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  combo = gimp_layer_combo_box_new (p_smp_constrain, NULL);

  gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    SMP_INV_GRADIENT,
                              GIMP_INT_STORE_LABEL,    _("From reverse gradient"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_GRADIENT,
                              -1);
  gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    SMP_GRADIENT,
                              GIMP_INT_STORE_LABEL,    _("From gradient"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_GRADIENT,
                              -1);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), g_values.sample_id,
                              G_CALLBACK (p_smp_sample_combo_callback),
                              NULL);

  gtk_table_attach (GTK_TABLE (table), combo, 4, 5, l_ty, l_ty + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  l_ty++;


  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, l_ty, l_ty + 1,
                    GTK_FILL, 0, 0, 0);
  gtk_widget_show (hbox);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Show selection"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_di.dst_show_selection);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_di.dst_show_selection);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Show color"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_di.dst_show_color);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_di.dst_show_color);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 3, 5, l_ty, l_ty + 1,
                    GTK_FILL, 0, 0, 0);
  gtk_widget_show (hbox);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Show selection"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    (GtkSignalFunc)p_smp_toggle_callback ,
                    &g_di.sample_show_selection);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_di.sample_show_selection);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Show color"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    (GtkSignalFunc)p_smp_toggle_callback ,
                    &g_di.sample_show_color);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_di.sample_show_color);

  l_ty++;

  /* Preview (Dst) */

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table),
                    frame, 0, 2, l_ty, l_ty + 1, 0, 0, 0, 0);
  gtk_widget_show (frame);

  g_di.dst_preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (g_di.dst_preview,
                               PREVIEW_SIZE_X, PREVIEW_SIZE_Y);
  gtk_container_add (GTK_CONTAINER (frame), g_di.dst_preview);
  gtk_widget_show (g_di.dst_preview);

  /* Preview (Sample)*/

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_table_attach (GTK_TABLE (table),
                    frame, 3, 5, l_ty, l_ty + 1, 0, 0, 0, 0);
  gtk_widget_show (frame);

  g_di.sample_preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (g_di.sample_preview,
                               PREVIEW_SIZE_X, PREVIEW_SIZE_Y);
  gtk_container_add (GTK_CONTAINER (frame), g_di.sample_preview);
  gtk_widget_show (g_di.sample_preview);

  l_ty++;

  /*  The levels graylevel prevev  */
  frame = gtk_frame_new (NULL);

  vbox2 = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);
  gtk_table_attach (GTK_TABLE (table),
                    frame, 0, 2, l_ty, l_ty + 1, 0, 0, 0, 0);

  g_di.in_lvl_gray_preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (g_di.in_lvl_gray_preview,
                               DA_WIDTH, GRADIENT_HEIGHT);
  gtk_widget_set_events (g_di.in_lvl_gray_preview, LEVELS_DA_MASK);
  gtk_box_pack_start (GTK_BOX (vbox2), g_di.in_lvl_gray_preview, FALSE, TRUE, 0);
  gtk_widget_show (g_di.in_lvl_gray_preview);

  g_signal_connect (g_di.in_lvl_gray_preview, "event",
                    G_CALLBACK (p_level_in_events),
                    &g_di);

  /*  The levels drawing area  */
  g_di.in_lvl_drawarea = gtk_drawing_area_new ();
  gtk_widget_set_size_request (g_di.in_lvl_drawarea,
                               DA_WIDTH, CONTROL_HEIGHT);
  gtk_widget_set_events (g_di.in_lvl_drawarea, LEVELS_DA_MASK);
  gtk_box_pack_start (GTK_BOX (vbox2), g_di.in_lvl_drawarea, FALSE, TRUE, 0);
  gtk_widget_show (g_di.in_lvl_drawarea);

  g_signal_connect (g_di.in_lvl_drawarea, "event",
                    G_CALLBACK (p_level_in_events),
                    &g_di);

  gtk_widget_show(vbox2);
  gtk_widget_show(frame);

  /*  The sample_colortable prevev  */
  frame = gtk_frame_new (NULL);

  vbox2 = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox2);
  gtk_table_attach (GTK_TABLE (table),
                    frame, 3, 5, l_ty, l_ty + 1, 0, 0, 0, 0);

  g_di.sample_colortab_preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (g_di.sample_colortab_preview,
                               DA_WIDTH, GRADIENT_HEIGHT);
  gtk_box_pack_start (GTK_BOX (vbox2), g_di.sample_colortab_preview, FALSE, TRUE, 0);
  gtk_widget_show (g_di.sample_colortab_preview);

  /*  The levels drawing area  */
  g_di.sample_drawarea = gtk_drawing_area_new ();
  gtk_widget_set_size_request (g_di.sample_drawarea,
                               DA_WIDTH, CONTROL_HEIGHT);
  gtk_widget_set_events (g_di.sample_drawarea, LEVELS_DA_MASK);
  gtk_box_pack_start (GTK_BOX (vbox2), g_di.sample_drawarea, FALSE, TRUE, 0);
  gtk_widget_show (g_di.sample_drawarea);

  g_signal_connect (g_di.sample_drawarea, "event",
                    G_CALLBACK (p_level_out_events),
                    &g_di);

  gtk_widget_show(vbox2);
  gtk_widget_show(frame);


  l_ty++;

  /*  Horizontal box for INPUT levels text widget  */
  hbox = gtk_hbox_new (TRUE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, l_ty, l_ty+1,
                    GTK_FILL, 0, 0, 0);

  label = gtk_label_new (_("Input levels:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* min input spinbutton */
  data = gtk_adjustment_new ((gfloat)g_values.lvl_in_min, 0.0, 254.0, 1, 10, 10);
  g_di.adj_lvl_in_min = GTK_ADJUSTMENT (data);

  spinbutton = gtk_spin_button_new (g_di.adj_lvl_in_min, 0.5, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (g_di.adj_lvl_in_min, "value_changed",
                    G_CALLBACK (p_smp_adj_lvl_in_min_upd_callback),
                    &g_di);

  /* input gamma spinbutton */
  data = gtk_adjustment_new ((gfloat)g_values.lvl_in_gamma, 0.1, 10.0, 0.02, 0.2, 0.2);
  g_di.adj_lvl_in_gamma = GTK_ADJUSTMENT (data);

  spinbutton = gtk_spin_button_new (g_di.adj_lvl_in_gamma, 0.5, 2);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (g_di.adj_lvl_in_gamma, "value_changed",
                    G_CALLBACK (p_smp_text_gamma_upd_callback),
                    &g_di);

  /* high input spinbutton */
  data = gtk_adjustment_new ((gfloat)g_values.lvl_in_max, 1.0, 255.0, 1, 10, 10);
  g_di.adj_lvl_in_max = GTK_ADJUSTMENT (data);

  spinbutton = gtk_spin_button_new (g_di.adj_lvl_in_max, 0.5, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (g_di.adj_lvl_in_max, "value_changed",
                    G_CALLBACK (p_smp_adj_lvl_in_max_upd_callback),
                    &g_di);

  gtk_widget_show (hbox);

  /*  Horizontal box for OUTPUT levels text widget  */
  hbox = gtk_hbox_new (TRUE, 4);
  gtk_table_attach (GTK_TABLE (table), hbox, 3, 5, l_ty, l_ty+1,
                    GTK_FILL, 0, 0, 0);

  label = gtk_label_new (_("Output Levels:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /*  min output spinbutton */
  data = gtk_adjustment_new ((gfloat)g_values.lvl_out_min, 0.0, 254.0, 1, 10, 10);
  g_di.adj_lvl_out_min = GTK_ADJUSTMENT (data);

  spinbutton = gtk_spin_button_new (g_di.adj_lvl_out_min, 0.5, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (g_di.adj_lvl_out_min, "value_changed",
                    G_CALLBACK (p_smp_adj_lvl_out_min_upd_callback),
                    &g_di);

  /* high output spinbutton */
  data = gtk_adjustment_new ((gfloat)g_values.lvl_out_max, 0.0, 255.0, 1, 10, 10);
  g_di.adj_lvl_out_max = GTK_ADJUSTMENT (data);

  spinbutton = gtk_spin_button_new (g_di.adj_lvl_out_max, 0.5, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (g_di.adj_lvl_out_max, "value_changed",
                    G_CALLBACK (p_smp_adj_lvl_out_max_upd_callback),
                    &g_di);

  gtk_widget_show (hbox);

  l_ty++;

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, l_ty, l_ty+1,
                    GTK_FILL, 0, 0, 0);
  gtk_widget_show (hbox);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Hold intensity"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_values.hold_inten);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_values.hold_inten);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Original intensity"));
  g_di.orig_inten_button = check_button;
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (g_di.orig_inten_button, g_values.hold_inten);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_values.orig_inten);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_values.orig_inten);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_table_attach (GTK_TABLE (table), hbox, 3, 5, l_ty, l_ty+1,
                    GTK_FILL, 0, 0, 0);
  gtk_widget_show (hbox);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Use subcolors"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_values.rnd_subcolors);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_values.rnd_subcolors);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Smooth samples"));
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);
  gtk_widget_show (check_button);

  g_signal_connect (check_button, "toggled",
                    G_CALLBACK (p_smp_toggle_callback),
                    &g_values.guess_missing);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                g_values.guess_missing);

  l_ty++;

  gtk_widget_show (table);
  gtk_widget_show (frame);

  gtk_widget_show (dialog);

  /* set old_id's different (to force updates of the previews) */
  g_di.enable_preview_update = TRUE;
  p_smp_get_colors (NULL);
  p_update_preview (&g_values.dst_id);
  p_levels_update (INPUT_SLIDERS | INPUT_LEVELS | DRAW);

  gtk_main ();
}        /* end p_smp_dialog */

/* -----------------------------
 * DEBUG print procedures START
 * -----------------------------
 */

static void
p_print_ppm (const gchar *ppm_name)
{
  FILE *fp;
  gint l_idx;
  gint l_cnt;
  gint l_r, l_g, l_b;
  t_samp_color_elem *col_ptr;

  if (ppm_name == NULL)
    return;

  fp = fopen (ppm_name, "w");
  if(fp)
    {
      fprintf(fp, "P3\n# CREATOR: Gimp sample coloros\n256 256\n255\n");
      for(l_idx = 0; l_idx < 256; l_idx++)
        {
          col_ptr = g_lum_tab[l_idx].col_ptr;

          for(l_cnt = 0; l_cnt < 256; l_cnt++)
            {
              l_r = l_g = l_b = 0;
              if(col_ptr)
                {
                  if((col_ptr->sum_color > 0) && (l_cnt != 20))
                    {
                      l_r = (gint) col_ptr->color[0];
                      l_g = (gint) col_ptr->color[1];
                      l_b = (gint) col_ptr->color[2];
                    }

                  if (l_cnt > 20) col_ptr = col_ptr->next;
                }
              fprintf(fp, "%d %d %d\n", l_r, l_g, l_b);
            }
        }
      fclose(fp);
    }
}

static void
p_print_color_list (FILE *fp, t_samp_color_elem *col_ptr)
{
  if (fp == NULL)
    return;

  while(col_ptr)
    {
      fprintf (fp, "  RGBA: %03d %03d %03d %03d  sum: [%d]\n",
               (gint)col_ptr->color[0],
               (gint)col_ptr->color[1],
               (gint)col_ptr->color[2],
               (gint)col_ptr->color[3],
               (gint)col_ptr->sum_color);

      col_ptr = col_ptr->next;
    }
}

static void
p_print_table (FILE *fp)
{
  gint l_idx;

  if(fp == NULL) return;

  fprintf(fp, "---------------------------\n");
  fprintf(fp, "p_print_table\n");
  fprintf(fp, "---------------------------\n");

  for(l_idx = 0; l_idx < 256; l_idx++)
  {
    fprintf(fp, "LUM [%03d]  pixcount:%d\n", l_idx, (int)g_lum_tab[l_idx].all_samples);
    p_print_color_list(fp, g_lum_tab[l_idx].col_ptr);
  }
}

static void
p_print_transtable (FILE *fp)
{
  gint l_idx;

  if (fp == NULL)
    return;

  fprintf(fp, "---------------------------\n");
  fprintf(fp, "p_print_transtable\n");
  fprintf(fp, "---------------------------\n");

  for(l_idx = 0; l_idx < 256; l_idx++)
    {
      fprintf(fp, "LVL_TRANS [%03d]  in_lvl: %3d  out_lvl: %3d\n",
              l_idx, (int)g_lvl_trans_tab[l_idx], (int)g_out_trans_tab[l_idx]);
    }
}

static void
p_print_values (FILE *fp)
{
  if (fp == NULL)
    return;

  fprintf(fp, "sample_colorize: params\n");
  fprintf(fp, "g_values.hold_inten     :%d\n", (int)g_values.hold_inten);
  fprintf(fp, "g_values.orig_inten     :%d\n", (int)g_values.orig_inten);
  fprintf(fp, "g_values.rnd_subcolors  :%d\n", (int)g_values.rnd_subcolors);
  fprintf(fp, "g_values.guess_missing  :%d\n", (int)g_values.guess_missing);
  fprintf(fp, "g_values.lvl_in_min     :%d\n", (int)g_values.lvl_in_min);
  fprintf(fp, "g_values.lvl_in_max     :%d\n", (int)g_values.lvl_in_max);
  fprintf(fp, "g_values.lvl_in_gamma   :%f\n", g_values.lvl_in_gamma);
  fprintf(fp, "g_values.lvl_out_min    :%d\n", (int)g_values.lvl_out_min);
  fprintf(fp, "g_values.lvl_out_max    :%d\n", (int)g_values.lvl_out_max);

  fprintf(fp, "g_values.tol_col_err    :%f\n", g_values.tol_col_err);
}

/* -----------------------------
 * DEBUG print procedures END
 * -----------------------------
 */

/* DEBUG: read values from file */
static void
p_get_filevalues (void)
{
  FILE *l_fp;
  gchar  l_buf[1000];

/*
  g_values.lvl_out_min = 0;
  g_values.lvl_out_max = 255;
  g_values.lvl_in_min = 0;
  g_values.lvl_in_max = 255;
  g_values.lvl_in_gamma = 1.0;
*/
  g_values.tol_col_err = 5.5;

  l_fp = fopen("sample_colorize.values", "r");
  if (l_fp != NULL)
    {
      fgets(&l_buf[0], 999, l_fp);
      sscanf (&l_buf[0], "%f", &g_values.tol_col_err);
      fclose(l_fp);
    }

  printf("g_values.tol_col_err    :%f\n", g_values.tol_col_err);
}        /* end p_get_filevalues */

static gint32
p_color_error (guchar ref_red, guchar ref_green, guchar ref_blue,
               guchar cmp_red, guchar cmp_green, guchar cmp_blue)
{
  glong   l_ff;
  glong   l_fs;
  glong   cmp_h, ref_h;

  /* 1. Brightness differences */
  cmp_h = (3 * cmp_red + 6 * cmp_green + cmp_blue) / 10;
  ref_h = (3 * ref_red + 6 * ref_green + ref_blue) / 10;

  l_fs = abs(ref_h - cmp_h);
  l_ff = l_fs * l_fs;

  /* 2. add Red Color differences */
  l_fs = abs(ref_red - cmp_red);
  l_ff += (l_fs * l_fs);

  /* 3. add  Green Color differences */
  l_fs = abs(ref_green - cmp_green);
  l_ff += (l_fs * l_fs);

  /* 4. add  Blue Color differences */
  l_fs = abs(ref_blue - cmp_blue);
  l_ff += (l_fs * l_fs);

  return((gint32)(l_ff));
}        /* end p_color_error */

static void
p_provide_tile (t_GDRW *gdrw,
                gint    col,
                gint    row,
                gint    shadow)
{
  guchar  *ptr;
  gint i;

  if ( col != gdrw->tile_col || row != gdrw->tile_row || !gdrw->tile )
    {
      if (gdrw->tile)
        {
          gimp_tile_unref (gdrw->tile, gdrw->tile_dirty);
        }

      gdrw->tile_col = col;
      gdrw->tile_row = row;
      gdrw->tile = gimp_drawable_get_tile (gdrw->drawable, shadow,
                                           gdrw->tile_row, gdrw->tile_col);
      gdrw->tile_dirty = FALSE;
      gimp_tile_ref (gdrw->tile);

      gdrw->tile_swapcount++;

      return;

      /* debug start */

      printf("\np_provide_tile: row: %d col: %d data:", (int)row, (int)col);

      ptr = gdrw->tile->data;
      for (i = 0; i < 16; i++)
        {
          printf(" %d", (int)(*ptr));
          ptr++;
        }
      printf("\n\n");

      /* debug stop */
    }
}        /* end p_provide_tile */

/* get pixel value
 *   return light gray transparent pixel if out of bounds
 *   (should occur in the previews only)
 */
static void
p_get_pixel (t_GDRW *gdrw,
             gint32  x,
             gint32  y,
             guchar *pixel)
{
  gint   row, col;
  gint   offx, offy;
  guchar  *ptr;

  if((x < 0)
     || (x > gdrw->drawable->width   -1)
     || (y < 0)
     || (y > gdrw->drawable->height - 1))
    {
      pixel[0] = pixel[1] = pixel[2] = 200;
      pixel[3] = 0;
      return;
    }

  col = x / gdrw->tile_width;
  row = y / gdrw->tile_height;
  offx = x % gdrw->tile_width;
  offy = y % gdrw->tile_height;

  p_provide_tile (gdrw, col, row, gdrw->shadow);

  pixel[1] = pixel[3] = 0;  /* simulate full transparent alpha channel */
  ptr = (gdrw->tile->data +
         (((offy * gdrw->tile->ewidth) + offx) * gdrw->bpp));
  memcpy (pixel, ptr, gdrw->bpp);

  return;

  printf("p_get_pixel: x: %d  y: %d bpp:%d RGBA:%d %d %d %d\n", (int)x, (int)y, (int)gdrw->bpp, (int)pixel[0], (int)pixel[1], (int)pixel[2], (int)pixel[3]);
}

/* clear table */
static void
p_clear_tables (void)
{
  gint l_idx;

  for (l_idx = 0; l_idx < 256; l_idx++)
    {
      g_lum_tab[l_idx].col_ptr = NULL;
      g_lum_tab[l_idx].all_samples = 0;
      g_lvl_trans_tab[l_idx] = l_idx;
      g_out_trans_tab[l_idx] = l_idx;
      g_sample_color_tab[3 * l_idx + 0] = l_idx;
      g_sample_color_tab[3 * l_idx + 1] = l_idx;
      g_sample_color_tab[3 * l_idx + 2] = l_idx;
    }
}

/* free all allocated sample colors in table g_lum_tab */
static void
p_free_colors (void)
{
  gint l_lum;

  t_samp_color_elem *l_col_ptr;
  t_samp_color_elem *l_next_ptr;

  for (l_lum = 0; l_lum < 256; l_lum++)
    {
      for (l_col_ptr = g_lum_tab[l_lum].col_ptr;
           l_col_ptr != NULL;
           l_col_ptr = l_next_ptr)
        {
          l_next_ptr = (t_samp_color_elem *)l_col_ptr->next;
          g_free (l_col_ptr);
        }

      g_lum_tab[l_lum].col_ptr = NULL;
      g_lum_tab[l_lum].all_samples = 0;
    }
}        /* end p_free_colors */

/* setup lum transformer table according to input_levels, gamma and output levels
 * (uses sam algorithm as GIMP Level Tool)
 */
static void
p_calculate_level_transfers (void)
{
  double inten;
  gint i;
  gint l_in_min, l_in_max;
  gint l_out_min, l_out_max;

  if(g_values.lvl_in_max >= g_values.lvl_in_min)
    {
      l_in_max = g_values.lvl_in_max;
      l_in_min = g_values.lvl_in_min;
    }
  else
    {
      l_in_max = g_values.lvl_in_min;
      l_in_min = g_values.lvl_in_max;
    }
  if(g_values.lvl_out_max >= g_values.lvl_out_min)
    {
      l_out_max = g_values.lvl_out_max;
      l_out_min = g_values.lvl_out_min;
    }
  else
    {
      l_out_max = g_values.lvl_out_min;
      l_out_min = g_values.lvl_out_max;
    }

  /*  Recalculate the levels arrays  */
  for (i = 0; i < 256; i++)
    {
      /*  determine input intensity  */
      inten = (double) i / 255.0;
      if (g_values.lvl_in_gamma != 0.0)
        {
          inten = pow (inten, (1.0 / g_values.lvl_in_gamma));
        }
      inten = (double) (inten * (l_in_max - l_in_min) + l_in_min);
      inten = CLAMP (inten, 0.0, 255.0);
      g_lvl_trans_tab[i] = (guchar) (inten + 0.5);

      /*  determine the output intensity  */
      inten = (double) i / 255.0;
      inten = (double) (inten * (l_out_max - l_out_min) + l_out_min);
      inten = CLAMP (inten, 0.0, 255.0);
      g_out_trans_tab[i] = (guchar) (inten + 0.5);
    }
}        /* end p_calculate_level_transfers */



/* alloc and init new col Element */
static t_samp_color_elem *
p_new_samp_color (const guchar *color)
{
  t_samp_color_elem *l_col_ptr;

  l_col_ptr = g_new0 (t_samp_color_elem, 1);
  if(l_col_ptr == NULL)
    {
      printf("Error: cant get Memory\n");
      return(NULL);
    }

  memcpy (&l_col_ptr->color[0], color, 4);

  l_col_ptr->sum_color = 1;
  l_col_ptr->next = NULL;

  return l_col_ptr;
}        /* end p_new_samp_color */


/* store color in g_lum_tab  */
static void
p_add_color (const guchar *color)
{
  gint32             l_lum;
  t_samp_color_elem *l_col_ptr;

  l_lum = LUMINOSITY_1(color);

  g_lum_tab[l_lum].all_samples++;
  g_lum_tab[l_lum].from_sample = TRUE;

  /* check if exactly the same color is already in the list */
  for(l_col_ptr = g_lum_tab[l_lum].col_ptr;
      l_col_ptr != NULL;
      l_col_ptr = (t_samp_color_elem *)l_col_ptr->next)
    {
      if((color[0] == l_col_ptr->color[0])
         && (color[1] == l_col_ptr->color[1])
         && (color[2] == l_col_ptr->color[2]))
        {
          l_col_ptr->sum_color++;
          return;
        }
    }

  /* alloc and init element for the new color  */
  l_col_ptr = p_new_samp_color(color);

  if(l_col_ptr != NULL)
    {
     /* add new color element as 1.st of the list */
     l_col_ptr->next = g_lum_tab[l_lum].col_ptr;
     g_lum_tab[l_lum].col_ptr = l_col_ptr;
    }
}        /* end p_add_color */

/* sort Sublists (color) by descending sum_color in g_lum_tab
 */
static void
p_sort_color (gint32 lum)
{
  t_samp_color_elem *l_col_ptr;
  t_samp_color_elem *l_next_ptr;
  t_samp_color_elem *l_prev_ptr;
  t_samp_color_elem *l_sorted_col_ptr;
  gint32             l_min;
  gint32             l_min_next;

  l_sorted_col_ptr = NULL;
  l_min_next           = 0;

  while (g_lum_tab[lum].col_ptr != NULL)
    {
      l_min = l_min_next;
      l_next_ptr = NULL;
      l_prev_ptr = NULL;

      for (l_col_ptr = g_lum_tab[lum].col_ptr;
           l_col_ptr != NULL;
           l_col_ptr = l_next_ptr)
        {
          l_next_ptr = l_col_ptr->next;
          if (l_col_ptr->sum_color > l_min)
            {
              /* check min value for next loop */
              if((l_col_ptr->sum_color < l_min_next) || (l_min == l_min_next))
                {
                  l_min_next = l_col_ptr->sum_color;
                }
              l_prev_ptr = l_col_ptr;
            }
          else
            {
              /* add element at head of sorted list */
              l_col_ptr->next = l_sorted_col_ptr;
              l_sorted_col_ptr = l_col_ptr;

              /* remove element from list */
              if(l_prev_ptr == NULL)
                {
                  g_lum_tab[lum].col_ptr = l_next_ptr;  /* remove 1.st element */
                }
              else
                {
                  l_prev_ptr->next = l_next_ptr;
                }

            }
        }
    }

  g_lum_tab[lum].col_ptr = l_sorted_col_ptr;
}        /* end p_sort_color */

static void
p_cnt_same_sample_colortones (t_samp_color_elem *ref_ptr,
                              guchar            *prev_color,
                              guchar            *color_tone,
                              gint              *csum)
{
  gint32             l_col_error, l_ref_error;
  t_samp_color_elem *l_col_ptr;

  l_ref_error = 0;
  if (prev_color != NULL)
    {
      l_ref_error = p_color_error (ref_ptr->color[0], ref_ptr->color[1], ref_ptr->color[2],
                                  prev_color[0],      prev_color[1],     prev_color[2]);
    }

  /* collect colors that are (nearly) the same */
  for(l_col_ptr = ref_ptr->next;
      l_col_ptr != NULL;
      l_col_ptr = (t_samp_color_elem *)l_col_ptr->next)
    {
      if(l_col_ptr->sum_color < 1)
        {
          continue;
        }
      l_col_error = p_color_error(ref_ptr->color[0],   ref_ptr->color[1],   ref_ptr->color[2],
                                  l_col_ptr->color[0], l_col_ptr->color[1], l_col_ptr->color[2]);

     if (l_col_error <= g_tol_col_err )
       {
         /* cout color of the same colortone */
         *csum += l_col_ptr->sum_color;
         /* mark the already checked color with negative sum_color value */
         l_col_ptr->sum_color = 0 - l_col_ptr->sum_color;

         if(prev_color != NULL)
           {
             l_col_error = p_color_error(l_col_ptr->color[0], l_col_ptr->color[1], l_col_ptr->color[2],
                                         prev_color[0],       prev_color[1],       prev_color[2]);
             if(l_col_error < l_ref_error)
               {
                 /* use the color that is closest to prev_color */
                 memcpy(color_tone, &l_col_ptr->color[0], 3);
                 l_ref_error = l_col_error;
               }
           }
       }
    }
}        /* end p_cnt_same_sample_colortones */

/* find the dominant colortones (out of all sample colors)
 * for each available brightness intensity value.
 * and store them in g_sample_color_tab
 */
static void
p_ideal_samples (void)
{
  gint32             l_lum;
  t_samp_color_elem *l_col_ptr;
  guchar            *l_color;

  guchar             l_color_tone[4];
  guchar             l_color_ideal[4];
  gint               l_csum, l_maxsum;

  l_color = NULL;
  for(l_lum = 0; l_lum < 256; l_lum++)
  {
    if(g_lum_tab[l_lum].col_ptr == NULL) { continue; }

    p_sort_color(l_lum);
    l_col_ptr = g_lum_tab[l_lum].col_ptr;
    memcpy(&l_color_ideal[0], &l_col_ptr->color[0], 3);

    l_maxsum = 0;

    /* collect colors that are (nearly) the same */
    for(;
        l_col_ptr != NULL;
        l_col_ptr = (t_samp_color_elem *)l_col_ptr->next)
      {
        l_csum = 0;
        if(l_col_ptr->sum_color > 0)
          {
            memcpy(&l_color_tone[0], &l_col_ptr->color[0], 3);
            p_cnt_same_sample_colortones(l_col_ptr, l_color, &l_color_tone[0], &l_csum);
            if(l_csum > l_maxsum)
              {
                l_maxsum = l_csum;
                memcpy(&l_color_ideal[0], &l_color_tone[0], 3);
              }
          }
        else
          {
            l_col_ptr->sum_color = abs(l_col_ptr->sum_color);
          }
      }

    /* store ideal color and keep track of the color */
    l_color = &g_sample_color_tab[3 * l_lum];
    memcpy (l_color, &l_color_ideal[0], 3);
  }
}        /* end p_ideal_samples */

static void
p_guess_missing_colors (void)
{
  gint32  l_lum;
  gint32  l_idx;
  gfloat  l_div;

  guchar  l_lo_color[4];
  guchar  l_hi_color[4];
  guchar  l_new_color[4];

  l_lo_color[0] = 0;
  l_lo_color[1] = 0;
  l_lo_color[2] = 0;
  l_lo_color[3] = 255;
  l_hi_color[0] = 255;
  l_hi_color[1] = 255;
  l_hi_color[2] = 255;
  l_hi_color[3] = 255;
  l_new_color[0] = 0;
  l_new_color[1] = 0;
  l_new_color[2] = 0;
  l_new_color[3] = 255;

  for(l_lum = 0; l_lum < 256; l_lum++)
    {
      if((g_lum_tab[l_lum].col_ptr == NULL) || (g_lum_tab[l_lum].from_sample == FALSE))
        {
          if(l_lum > 0)
            {
              for(l_idx = l_lum; l_idx < 256; l_idx++)
                {
                  if((g_lum_tab[l_idx].col_ptr != NULL) && (g_lum_tab[l_idx].from_sample))
                    {
                      memcpy(&l_hi_color[0], &g_sample_color_tab[l_idx + l_idx + l_idx], 3);
                      break;
                    }
                  if(l_idx == 255)
                    {
                      l_hi_color[0] = 255;
                      l_hi_color[1] = 255;
                      l_hi_color[2] = 255;
                      break;
                    }
                }

              l_div = l_idx - (l_lum -1);
              l_new_color[0] = l_lo_color[0] + ((float)(l_hi_color[0] - l_lo_color[0]) / l_div);
              l_new_color[1] = l_lo_color[1] + ((float)(l_hi_color[1] - l_lo_color[1]) / l_div);
              l_new_color[2] = l_lo_color[2] + ((float)(l_hi_color[2] - l_lo_color[2]) / l_div);

/*
 *          printf("LO: %03d %03d %03d HI: %03d %03d %03d   NEW: %03d %03d %03d\n",
 *                (int)l_lo_color[0],  (int)l_lo_color[1],  (int)l_lo_color[2],
 *                (int)l_hi_color[0],  (int)l_hi_color[1],  (int)l_hi_color[2],
 *                (int)l_new_color[0], (int)l_new_color[1], (int)l_new_color[2]);
 */

            }
          g_lum_tab[l_lum].col_ptr = p_new_samp_color(&l_new_color[0]);
          g_lum_tab[l_lum].from_sample = FALSE;
          memcpy(&g_sample_color_tab [3 * l_lum], &l_new_color[0], 3);
        }

    memcpy (&l_lo_color[0], &g_sample_color_tab [3 * l_lum], 3);
  }
}        /* end p_guess_missing_colors */

static void
p_fill_missing_colors (void)
{
  gint32             l_lum;
  gint32             l_idx;
  gint32             l_lo_idx;

  guchar             l_lo_color[4];
  guchar             l_hi_color[4];
  guchar             l_new_color[4];

  l_lo_color[0] = 0;
  l_lo_color[1] = 0;
  l_lo_color[2] = 0;
  l_lo_color[3] = 255;
  l_hi_color[0] = 255;
  l_hi_color[1] = 255;
  l_hi_color[2] = 255;
  l_hi_color[3] = 255;
  l_new_color[0] = 0;
  l_new_color[1] = 0;
  l_new_color[2] = 0;
  l_new_color[3] = 255;

  l_lo_idx = 0;
  for(l_lum = 0; l_lum < 256; l_lum++)
    {
      if((g_lum_tab[l_lum].col_ptr == NULL) || (g_lum_tab[l_lum].from_sample == FALSE))
        {
          if(l_lum > 0)
            {
              for(l_idx = l_lum; l_idx < 256; l_idx++)
                {
                  if((g_lum_tab[l_idx].col_ptr != NULL) && (g_lum_tab[l_idx].from_sample))
                    {
                      memcpy (&l_hi_color[0],
                              &g_sample_color_tab[l_idx + l_idx + l_idx], 3);
                      break;
                    }

                  if(l_idx == 255)
                    {
/*
 *               l_hi_color[0] = 255;
 *               l_hi_color[1] = 255;
 *               l_hi_color[2] = 255;
 */
                      memcpy(&l_hi_color[0], &l_lo_color[0], 3);
                      break;
                    }
                }

              if((l_lum > (l_lo_idx + ((l_idx - l_lo_idx ) / 2)))
                 || (l_lo_idx == 0))
                {
                  l_new_color[0] = l_hi_color[0];
                  l_new_color[1] = l_hi_color[1];
                  l_new_color[2] = l_hi_color[2];
                }
              else
                {
                  l_new_color[0] = l_lo_color[0];
                  l_new_color[1] = l_lo_color[1];
                  l_new_color[2] = l_lo_color[2];
                }
            }

          g_lum_tab[l_lum].col_ptr = p_new_samp_color(&l_new_color[0]);
          g_lum_tab[l_lum].from_sample = FALSE;
          memcpy(&g_sample_color_tab[3 * l_lum], &l_new_color[0], 3);
        }
      else
        {
          l_lo_idx = l_lum;
          memcpy(&l_lo_color[0], &g_sample_color_tab[3 * l_lum], 3);
        }
    }
}        /* end p_fill_missing_colors */

/* get 256 samples of active gradient (optional in invers order) */
static void
p_get_gradient (gint mode)
{
  gchar   *name;
  gint     n_f_samples;
  gdouble *f_samples;
  gdouble *f_samp;        /* float samples */
  gint     l_lum;

  p_free_colors ();

  name = gimp_context_get_gradient ();

  gimp_gradient_get_uniform_samples (name, 256 /* n_samples */,
                                     mode == SMP_INV_GRADIENT,
                                     &n_f_samples, &f_samples);

  g_free (name);

  for (l_lum = 0; l_lum < 256; l_lum++)
    {
      f_samp = &f_samples[l_lum * 4];

      g_sample_color_tab[3 * l_lum + 0] = f_samp[0] * 255;
      g_sample_color_tab[3 * l_lum + 1] = f_samp[1] * 255;
      g_sample_color_tab[3 * l_lum + 2] = f_samp[2] * 255;

      g_lum_tab[l_lum].col_ptr =
        p_new_samp_color (&g_sample_color_tab[3 * l_lum]);
      g_lum_tab[l_lum].from_sample = TRUE;
      g_lum_tab[l_lum].all_samples = 1;
    }

  g_free (f_samples);
}        /* end p_get_gradient */

static gint32
p_is_layer_alive(gint32 drawable_id)
{
  /* return -1 if layer has become invalid */

  if (drawable_id < 0)
    {
      return -1;
    }

  if (gimp_drawable_get_image (drawable_id) < 0)
    {
      printf ("sample colorize: unknown layer_id %d (Image closed?)\n",
              (int)drawable_id);
      return -1;
    }

  return drawable_id;
}

static void
p_end_gdrw(t_GDRW *gdrw)
{
  t_GDRW  *sel_gdrw;

  if (gdrw->tile)
    {
      gimp_tile_unref (gdrw->tile, gdrw->tile_dirty);
      gdrw->tile = NULL;
    }

  sel_gdrw = (t_GDRW*)(gdrw->sel_gdrw);
  if (sel_gdrw)
    {
      if(sel_gdrw->tile)
        {
          gimp_tile_unref (sel_gdrw->tile, sel_gdrw->tile_dirty);
       sel_gdrw->tile = NULL;
        }
      gdrw->sel_gdrw = NULL;
    }
}

static void
p_init_gdrw (t_GDRW       *gdrw,
             GimpDrawable *drawable,
             gint          dirty,
             gint          shadow)
{
  gint32  l_image_id;
  gint32  l_sel_channel_id;
  gint32  l_x1, l_x2, l_y1, l_y2;
  gint    l_offsetx, l_offsety;
  gint    l_sel_offsetx, l_sel_offsety;
  t_GDRW *l_sel_gdrw;
  gint32  non_empty;

  if(g_Sdebug)  printf("\np_init_gdrw: drawable %p  ID: %d\n", drawable, (int)drawable->drawable_id);

  gdrw->drawable = drawable;
  gdrw->tile = NULL;
  gdrw->tile_dirty = FALSE;
  gdrw->tile_width = gimp_tile_width ();
  gdrw->tile_height = gimp_tile_height ();
  gdrw->shadow = shadow;
  gdrw->tile_swapcount = 0;
  gdrw->seldeltax = 0;
  gdrw->seldeltay = 0;
  gimp_drawable_offsets (drawable->drawable_id, &l_offsetx, &l_offsety);  /* get offsets within the image */

  gimp_drawable_mask_bounds (drawable->drawable_id,
                             &gdrw->x1, &gdrw->y1, &gdrw->x2, &gdrw->y2);

  gdrw->bpp = drawable->bpp;
  if (gimp_drawable_has_alpha(drawable->drawable_id))
    {
      /* index of the alpha channelbyte {1|3} */
      gdrw->index_alpha = gdrw->bpp -1;
    }
  else
    {
      gdrw->index_alpha = 0;      /* there is no alpha channel */
    }

  l_image_id = gimp_drawable_get_image (drawable->drawable_id);

  /* check and see if we have a selection mask */
  l_sel_channel_id  = gimp_image_get_selection (l_image_id);

  if(g_Sdebug)
    {
      printf("p_init_gdrw: image_id %d sel_channel_id: %d\n", (int)l_image_id, (int)l_sel_channel_id);
      printf("p_init_gdrw: BOUNDS     x1: %d y1: %d x2:%d y2: %d\n",
             (int)gdrw->x1,  (int)gdrw->y1, (int)gdrw->x2,(int)gdrw->y2);
      printf("p_init_gdrw: OFFS       x: %d y: %d\n", (int)l_offsetx, (int)l_offsety );
    }

  gimp_selection_bounds (l_image_id, &non_empty, &l_x1, &l_y1, &l_x2, &l_y2);

  if (non_empty && (l_sel_channel_id >= 0))
    {
      /* selection is TRUE */
      l_sel_gdrw = (t_GDRW *) calloc(1, sizeof(t_GDRW));
      l_sel_gdrw->drawable = gimp_drawable_get (l_sel_channel_id);

      l_sel_gdrw->tile = NULL;
      l_sel_gdrw->tile_dirty = FALSE;
      l_sel_gdrw->tile_width = gimp_tile_width ();
      l_sel_gdrw->tile_height = gimp_tile_height ();
      l_sel_gdrw->shadow = shadow;
      l_sel_gdrw->tile_swapcount = 0;
      l_sel_gdrw->x1 = l_x1;
      l_sel_gdrw->y1 = l_y1;
      l_sel_gdrw->x2 = l_x2;
      l_sel_gdrw->y2 = l_y2;
      l_sel_gdrw->seldeltax = 0;
      l_sel_gdrw->seldeltay = 0;
      l_sel_gdrw->bpp = l_sel_gdrw->drawable->bpp;  /* should be always 1 */
      l_sel_gdrw->index_alpha = 0;   /* there is no alpha channel */
      l_sel_gdrw->sel_gdrw = NULL;

      /* offset delta between drawable and selection
       * (selection always has image size and should always have offsets of 0 )
       */
      gimp_drawable_offsets (l_sel_channel_id, &l_sel_offsetx, &l_sel_offsety);
      gdrw->seldeltax = l_offsetx - l_sel_offsetx;
      gdrw->seldeltay = l_offsety - l_sel_offsety;

      gdrw->sel_gdrw = (t_GDRW *) l_sel_gdrw;

      if (g_Sdebug)
        {
          printf("p_init_gdrw: SEL_BOUNDS x1: %d y1: %d x2:%d y2: %d\n",
                 (int)l_sel_gdrw->x1,  (int)l_sel_gdrw->y1, (int)l_sel_gdrw->x2,(int)l_sel_gdrw->y2);
          printf("p_init_gdrw: SEL_OFFS   x: %d y: %d\n", (int)l_sel_offsetx, (int)l_sel_offsety );

          printf("p_init_gdrw: SEL_DELTA  x: %d y: %d\n", (int)gdrw->seldeltax, (int)gdrw->seldeltay );
        }
    }
  else
    {
      gdrw->sel_gdrw = NULL;     /* selection is FALSE */
    }
}

/* analyze the colors in the sample_drawable */
static int
p_sample_analyze (t_GDRW *sample_gdrw)
{
   gint32             l_sample_pixels;
   gint32             l_row, l_col;
   gint32             l_first_row, l_first_col, l_last_row, l_last_col;
   gint32             l_x, l_y;
   gint32             l_x2, l_y2;
   float l_progress_step;
   float l_progress_max;
   float l_progress;

   guchar             color[4];

   FILE     *prot_fp;

   l_sample_pixels = 0;

   /* init progress */
   l_progress_max = (sample_gdrw->x2 - sample_gdrw->x1);
   l_progress_step = 1.0 / l_progress_max;
   l_progress = 0.0;
   if(g_show_progress) gimp_progress_init (_("Sample Analyze..."));

   prot_fp = NULL;
   if(g_Sdebug) prot_fp = fopen("sample_colors.dump", "w");
   p_print_values(prot_fp);

   /* ------------------------------------------------
    * foreach pixel in the SAMPLE_drawable:
    *  calculate brightness intensity LUM
    * ------------------------------------------------
    * the inner loops (l_x/l_y) are designed to process
    * all pixels of one tile in the sample drawable, the outer loops (row/col) do step
    * to the next tiles. (this was done to reduce tile swapping)
    */

   l_first_row = sample_gdrw->y1 / sample_gdrw->tile_height;
   l_last_row  = (sample_gdrw->y2 / sample_gdrw->tile_height);
   l_first_col = sample_gdrw->x1 / sample_gdrw->tile_width;
   l_last_col  = (sample_gdrw->x2 / sample_gdrw->tile_width);

   for(l_row = l_first_row; l_row <= l_last_row; l_row++)
     {
       for(l_col = l_first_col; l_col <= l_last_col; l_col++)
         {
           if(l_col == l_first_col)    l_x = sample_gdrw->x1;
           else                        l_x = l_col * sample_gdrw->tile_width;
           if(l_col == l_last_col)     l_x2 = sample_gdrw->x2;
           else                        l_x2 = (l_col +1) * sample_gdrw->tile_width;

           for( ; l_x < l_x2; l_x++)
             {
               if(l_row == l_first_row)    l_y = sample_gdrw->y1;
               else                        l_y = l_row * sample_gdrw->tile_height;
               if(l_row == l_last_row)     l_y2 = sample_gdrw->y2;
               else                        l_y2 = (l_row +1) * sample_gdrw->tile_height ;

               /* printf("X: %4d Y:%4d Y2:%4d\n", (int)l_x, (int)l_y, (int)l_y2); */

               for( ; l_y < l_y2; l_y++)
                 {
                   /* check if the pixel is in the selection */
                   if(sample_gdrw->sel_gdrw)
                     {
                       p_get_pixel(sample_gdrw->sel_gdrw,
                                   (l_x + sample_gdrw->seldeltax),
                                   (l_y + sample_gdrw->seldeltay),
                                   &color[0]);

                       if(color[0] == 0)
                         {
                           continue;
                         }
                     }

                   p_get_pixel(sample_gdrw, l_x, l_y, &color[0]);

                   /* if this is a visible (non-transparent) pixel */
                   if((sample_gdrw->index_alpha < 1) || (color[sample_gdrw->index_alpha] != 0))
                     {
                       /* store color in the sublists of g_lum_tab  */
                       p_add_color(&color[0]);
                       l_sample_pixels++;
                     }
                 }
               if(g_show_progress) gimp_progress_update (l_progress += l_progress_step);
             }
         }
     }

   if(g_Sdebug) printf("ROWS: %d - %d  COLS: %d - %d\n", (int)l_first_row, (int)l_last_row, (int)l_first_col, (int)l_last_col);

   p_print_table(prot_fp);

   if(g_Sdebug) p_print_ppm("sample_color_all.ppm");

   /* findout ideal sample colors for each brightness intensity (lum)
    * and set g_sample_color_tab to the ideal colors.
    */
   p_ideal_samples();
   p_calculate_level_transfers ();
   if(g_values.guess_missing) { p_guess_missing_colors(); }
   else                       { p_fill_missing_colors(); }

   p_print_table(prot_fp);
   if(g_Sdebug) p_print_ppm("sample_color_2.ppm");
   p_print_transtable(prot_fp);
   if(prot_fp) fclose(prot_fp);

   /* check if there was at least one visible pixel */
   if (l_sample_pixels == 0)
     {
       printf("Error: Source sample has no visible Pixel\n");
       return -1;
     }
   return 0;
}                /* end p_sample_analyze */

static void
p_rnd_remap (gint32  lum,
             guchar *mapped_color)
{
  t_samp_color_elem *l_col_ptr;
  gint               l_rnd;
  gint               l_ct;
  gint               l_idx;

  if (g_lum_tab[lum].all_samples > 1)
    {
      l_rnd = g_random_int_range (0, g_lum_tab[lum].all_samples);
      l_ct  = 0;
      l_idx = 0;

      for (l_col_ptr = g_lum_tab[lum].col_ptr;
           l_col_ptr != NULL;
           l_col_ptr = (t_samp_color_elem *)l_col_ptr->next)
        {
          l_ct += l_col_ptr->sum_color;

          if (l_rnd < l_ct)
            {
              /* printf("RND_remap: rnd: %d all:%d  ct:%d idx:%d\n",
               * l_rnd, (int)g_lum_tab[lum].all_samples, l_ct, l_idx);
               */
              memcpy (mapped_color, &l_col_ptr->color[0], 3);
              return;
            }
          l_idx++;
        }
    }

  memcpy (mapped_color, &g_sample_color_tab[lum + lum + lum], 3);
}

static void
p_remap_pixel (guchar       *pixel,
               const guchar *original,
               gint          bpp2)
{
  guchar mapped_color[4];
  gint   l_lum;
  double l_orig_lum, l_mapped_lum;
  double l_grn, l_red, l_blu;
  double l_mg,  l_mr,  l_mb;
  double l_dg,  l_dr,  l_db;
  double l_dlum;

  /* get brightness from (uncolorized) original */
  l_lum = g_out_trans_tab[g_lvl_trans_tab[LUMINOSITY_1(original)]];
  if (g_values.rnd_subcolors)
    {
      p_rnd_remap (l_lum, mapped_color);
    }
  else
    {
      memcpy (mapped_color, &g_sample_color_tab[3 * l_lum], 3);
    }

  if (g_values.hold_inten)
    {
      if (g_values.orig_inten)
        {
          l_orig_lum = LUMINOSITY_0(original);
        }
      else
        {
          l_orig_lum = 100.0 * g_lvl_trans_tab[LUMINOSITY_1(original)];
        }

      l_mapped_lum = LUMINOSITY_0 (mapped_color);

      if (l_mapped_lum == 0)
        {
          /* convert black to greylevel with desired brightness value */
          mapped_color[0] = l_orig_lum / 100.0;
          mapped_color[1] = mapped_color[0];
          mapped_color[2] = mapped_color[0];
        }
      else
        {
          /* Calculate theoretical RGB to reach given intensity LUM
           * value (l_orig_lum)
           */
          l_mr = mapped_color[0];
          l_mg = mapped_color[1];
          l_mb = mapped_color[2];

          if(l_mr > 0.0)
            {
              l_red =
                l_orig_lum / (30 + (59 * l_mg / l_mr) + (11 * l_mb / l_mr));
              l_grn = l_mg * l_red / l_mr;
              l_blu = l_mb * l_red / l_mr;
            }
          else if (l_mg > 0.0)
            {
              l_grn =
                l_orig_lum / ((30 * l_mr / l_mg) + 59 + (11 * l_mb / l_mg));
              l_red = l_mr * l_grn / l_mg;
              l_blu = l_mb * l_grn / l_mg;
            }
          else
            {
              l_blu =
                l_orig_lum / ((30 * l_mr / l_mb) + (59 * l_mg / l_mb) + 11);
              l_grn = l_mg * l_blu / l_mb;
              l_red = l_mr * l_blu / l_mb;
            }

          /* on overflow: Calculate real RGB values
           * (this may change the hue and saturation,
           * more and more into white)
           */

          if (l_red > 255)
            {
              if ((l_blu < 255) && (l_grn < 255))
                {
                  /* overflow in the red channel (compensate with green and blu)  */
                  l_dlum = (l_red - 255.0) * 30.0;
                  if (l_mg > 0)
                    {
                      l_dg = l_dlum / (59.0 + (11.0 * l_mb / l_mg));
                      l_db = l_dg * l_mb / l_mg;
                    }
                  else if (l_mb > 0)
                    {
                      l_db = l_dlum / (11.0 + (59.0 * l_mg / l_mb));
                      l_dg = l_db * l_mg / l_mb;
                    }
                  else
                    {
                      l_db = l_dlum / (11.0 + 59.0);
                      l_dg = l_dlum / (59.0 + 11.0);
                    }

                  l_grn += l_dg;
                  l_blu += l_db;
                }

              l_red = 255.0;

              if (l_grn > 255)
                {
                  l_grn = 255.0;
                  l_blu = (l_orig_lum - 22695) / 11;  /* 22695 =  (255*30) + (255*59)  */
                }
              if (l_blu > 255)
                {
                  l_blu = 255.0;
                  l_grn = (l_orig_lum - 10455) / 59;  /* 10455 =  (255*30) + (255*11)  */
                }
            }
          else if (l_grn > 255)
            {
              if ((l_blu < 255) && (l_red < 255))
                {
                  /* overflow in the green channel (compensate with red and blu)  */
                  l_dlum = (l_grn - 255.0) * 59.0;

                  if (l_mr > 0)
                    {
                      l_dr = l_dlum / (30.0 + (11.0 * l_mb / l_mr));
                      l_db = l_dr * l_mb / l_mr;
                    }
                  else if (l_mb > 0)
                    {
                      l_db = l_dlum / (11.0 + (30.0 * l_mr / l_mb));
                      l_dr = l_db * l_mr / l_mb;
                    }
                  else
                    {
                      l_db = l_dlum / (11.0 + 30.0);
                      l_dr = l_dlum / (30.0 + 11.0);
                    }

                  l_red += l_dr;
                  l_blu += l_db;
                }

              l_grn = 255.0;

              if (l_red > 255)
                {
                  l_red = 255.0;
                  l_blu = (l_orig_lum - 22695) / 11;  /* 22695 =  (255*59) + (255*30)  */
                }
              if (l_blu > 255)
                {
                  l_blu = 255.0;
                  l_red = (l_orig_lum - 17850) / 30;  /* 17850 =  (255*59) + (255*11)  */
                }
            }
          else if (l_blu > 255)
            {
              if ((l_red < 255) && (l_grn < 255))
                {
                  /* overflow in the blue channel (compensate with green and red)  */
                  l_dlum = (l_blu - 255.0) * 11.0;

                  if (l_mg > 0)
                    {
                      l_dg = l_dlum / (59.0 + (30.0 * l_mr / l_mg));
                      l_dr = l_dg * l_mr / l_mg;
                    }
                  else if (l_mr > 0)
                    {
                      l_dr = l_dlum / (30.0 + (59.0 * l_mg / l_mr));
                      l_dg = l_dr * l_mg / l_mr;
                    }
                  else
                    {
                      l_dr = l_dlum / (30.0 + 59.0);
                      l_dg = l_dlum / (59.0 + 30.0);
                    }

                  l_grn += l_dg;
                  l_red += l_dr;
                }

              l_blu = 255.0;

              if (l_grn > 255)
                {
                  l_grn = 255.0;
                  l_red = (l_orig_lum - 17850) / 30;  /* 17850 =  (255*11) + (255*59)  */
                }
              if (l_red > 255)
                {
                  l_red = 255.0;
                  l_grn = (l_orig_lum - 10455) / 59;  /* 10455 =  (255*11) + (255*30)  */
                }
            }

          mapped_color[0] = CLAMP0255 (l_red + 0.5);
          mapped_color[1] = CLAMP0255 (l_grn + 0.5);
          mapped_color[2] = CLAMP0255 (l_blu + 0.5);
        }
    }

  /* set colorized pixel in shadow pr */
  memcpy (pixel, &mapped_color[0], bpp2);
}

static void
colorize_func (const guchar *src,
               guchar       *dest,
               gint          bpp,
               gpointer      data)
{
  gboolean has_alpha = GPOINTER_TO_INT (data);

  if (has_alpha)
    {
      bpp--;
      dest[bpp] = src[bpp];
    }

  p_remap_pixel (dest, src, bpp);
}

static void
p_colorize_drawable (gint32 drawable_id)
{
  GimpDrawable *drawable;
  gboolean      has_alpha;

  drawable = gimp_drawable_get (drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  if (g_show_progress)
    gimp_progress_init (_("Remap Colorized..."));

  gimp_rgn_iterate2 (drawable, 0 /* unused */, colorize_func,
                     GINT_TO_POINTER (has_alpha));

  if (g_show_progress)
    gimp_progress_update (0.0);
}

/* colorize dst_drawable like sample_drawable */
static int
p_main_colorize (gint mc_flags)
{
   GimpDrawable *dst_drawable;
   GimpDrawable *sample_drawable;
   t_GDRW        l_sample_gdrw;
   gint32        l_max;
   gint32        l_id;
   gint          l_rc;

   if(g_Sdebug)  p_get_filevalues ();  /* for debugging: read values from file */
   sample_drawable = NULL;
   dst_drawable = NULL;

   /* calculate value of tolerable color error */
   l_max = p_color_error (0,0, 0, 255, 255, 255); /* 260100 */
   g_tol_col_err = (((float)l_max  * (g_values.tol_col_err * g_values.tol_col_err)) / (100.0 *100.0));
   g_max_col_err = l_max;

   l_rc = 0;

   if (mc_flags & MC_GET_SAMPLE_COLORS)
     {
       l_id = g_values.sample_id;
       if ((l_id == SMP_GRADIENT) || (l_id == SMP_INV_GRADIENT))
         {
           p_get_gradient (l_id);
         }
       else
         {
           if (p_is_layer_alive (l_id) < 0)
             {
               return -1;
             }

           sample_drawable = gimp_drawable_get (l_id);
           p_init_gdrw (&l_sample_gdrw, sample_drawable, FALSE, FALSE);
           p_free_colors ();
           l_rc = p_sample_analyze (&l_sample_gdrw);
         }
     }

   if ((mc_flags & MC_DST_REMAP) && (l_rc == 0))
     {
       if (p_is_layer_alive (g_values.dst_id) < 0)
         {
           return -1;
         }
       dst_drawable = gimp_drawable_get (g_values.dst_id);
       if (gimp_drawable_is_gray (g_values.dst_id))
         {
           if (mc_flags & MC_DST_REMAP)
             {
               gimp_image_convert_rgb (gimp_drawable_get_image (g_values.dst_id));
             }
         }

       p_colorize_drawable (dst_drawable->drawable_id);
     }

   if (sample_drawable)
     {
       p_end_gdrw (&l_sample_gdrw);
     }

   return l_rc;
}
