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

/* Original plug-in coded by Tim Newsome.
 *
 * Changed to make use of real-life units by Sven Neumann <sven@gimp.org>.
 *
 * The interface code is heavily commented in the hope that it will
 * help other plug-in developers to adapt their plug-ins to make use
 * of the gimp_size_entry functionality.
 *
 * Note: There is a convenience constructor called gimp_coordinetes_new ()
 *       which simplifies the task of setting up a standard X,Y sizeentry.
 *
 * For more info and bugs see libgimp/gimpsizeentry.h and libgimp/gimpwidgets.h
 *
 * May 2000 tim copperfield [timecop@japan.co.jp]
 * http://www.ne.jp/asahi/linux/timecop
 * Added dynamic preview.  Due to weird implementation of signals from all
 * controls, preview will not auto-update.  But this plugin isn't really
 * crying for real-time updating either.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#define SPIN_BUTTON_WIDTH    8
#define COLOR_BUTTON_WIDTH  55


/* Declare local functions. */
static void   query  (void);
static void   run    (const gchar      *name,
                      gint              nparams,
                      const GimpParam  *param,
                      gint             *nreturn_vals,
                      GimpParam       **return_vals);

static guchar      best_cmap_match (const guchar  *cmap,
                                    gint           ncolors,
                                    const GimpRGB *color);
static void        grid            (gint32         image_ID,
                                    GimpDrawable  *drawable,
                                    GimpPreview   *preview);
static gint        dialog          (gint32         image_ID,
                                    GimpDrawable  *drawable);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static gint sx1, sy1, sx2, sy2;

static GtkWidget *main_dialog    = NULL;
static GtkWidget *hcolor_button  = NULL;
static GtkWidget *vcolor_button  = NULL;

typedef struct
{
  gint    hwidth;
  gint    hspace;
  gint    hoffset;
  GimpRGB hcolor;
  gint    vwidth;
  gint    vspace;
  gint    voffset;
  GimpRGB vcolor;
  gint    iwidth;
  gint    ispace;
  gint    ioffset;
  GimpRGB icolor;
} Config;

Config grid_cfg =
{
  1, 16, 8, { 0.0, 0.0, 0.0, 1.0 },    /* horizontal   */
  1, 16, 8, { 0.0, 0.0, 0.0, 1.0 },    /* vertical     */
  0,  2, 6, { 0.0, 0.0, 0.0, 1.0 },    /* intersection */
};


MAIN ()

static
void query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",    "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },

    { GIMP_PDB_INT32,    "hwidth",   "Horizontal Width   (>= 0)" },
    { GIMP_PDB_INT32,    "hspace",   "Horizontal Spacing (>= 1)" },
    { GIMP_PDB_INT32,    "hoffset",  "Horizontal Offset  (>= 0)" },
    { GIMP_PDB_COLOR,    "hcolor",   "Horizontal Colour" },
    { GIMP_PDB_INT8,     "hopacity", "Horizontal Opacity (0...255)" },

    { GIMP_PDB_INT32,    "vwidth",   "Vertical Width   (>= 0)" },
    { GIMP_PDB_INT32,    "vspace",   "Vertical Spacing (>= 1)" },
    { GIMP_PDB_INT32,    "voffset",  "Vertical Offset  (>= 0)" },
    { GIMP_PDB_COLOR,    "vcolor",   "Vertical Colour" },
    { GIMP_PDB_INT8,     "vopacity", "Vertical Opacity (0...255)" },

    { GIMP_PDB_INT32,    "iwidth",   "Intersection Width   (>= 0)" },
    { GIMP_PDB_INT32,    "ispace",   "Intersection Spacing (>= 0)" },
    { GIMP_PDB_INT32,    "ioffset",  "Intersection Offset  (>= 0)" },
    { GIMP_PDB_COLOR,    "icolor",   "Intersection Colour" },
    { GIMP_PDB_INT8,     "iopacity", "Intersection Opacity (0...255)" }
  };

  gimp_install_procedure ("plug_in_grid",
                          "Draws a grid.",
                          "Draws a grid using the specified colors. "
                          "The grid origin is the upper left corner.",
                          "Tim Newsome",
                          "Tim Newsome, Sven Neumann, Tom Rathborne, TC",
                          "1997 - 2000",
                          N_("_Grid..."),
                          "RGB*, GRAY*, INDEXED*",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (args), 0,
                          args, NULL);

  gimp_plugin_menu_register ("plug_in_grid", "<Image>/Filters/Render/Pattern");
}

static void
run (const gchar      *name,
     gint              n_params,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpDrawable      *drawable;
  gint32             image_ID;
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  *nreturn_vals = 1;
  *return_vals  = values;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;
  image_ID = param[1].data.d_int32;
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  if (run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      if (n_params != 18)
        status = GIMP_PDB_CALLING_ERROR;

      if (status == GIMP_PDB_SUCCESS)
        {
          grid_cfg.hwidth  = MAX (0, param[3].data.d_int32);
          grid_cfg.hspace  = MAX (1, param[4].data.d_int32);
          grid_cfg.hoffset = MAX (0, param[5].data.d_int32);
          grid_cfg.hcolor  = param[6].data.d_color;

          gimp_rgb_set_alpha (&(grid_cfg.hcolor),
                              ((double) (guint) param[7].data.d_int8) / 255.0);


          grid_cfg.vwidth  = MAX (0, param[8].data.d_int32);
          grid_cfg.vspace  = MAX (1, param[9].data.d_int32);
          grid_cfg.voffset = MAX (0, param[10].data.d_int32);
          grid_cfg.vcolor  = param[11].data.d_color;

          gimp_rgb_set_alpha (&(grid_cfg.vcolor),
                              ((double) (guint)  param[12].data.d_int8) / 255.0);



          grid_cfg.iwidth  = MAX (0, param[13].data.d_int32);
          grid_cfg.ispace  = MAX (0, param[14].data.d_int32);
          grid_cfg.ioffset = MAX (0, param[15].data.d_int32);
          grid_cfg.icolor  = param[16].data.d_color;

          gimp_rgb_set_alpha (&(grid_cfg.icolor),
                              ((double) (guint) param[17].data.d_int8) / 255.0);


        }
    }
  else
    {
      gimp_context_get_foreground (&grid_cfg.hcolor);
      grid_cfg.vcolor = grid_cfg.icolor = grid_cfg.hcolor;

      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_grid", &grid_cfg);
    }

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (!dialog (image_ID, drawable))
        {
          /* The dialog was closed, or something similarly evil happened. */
          status = GIMP_PDB_EXECUTION_ERROR;
        }
    }

  if (grid_cfg.hspace <= 0 || grid_cfg.vspace <= 0)
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      gimp_progress_init (_("Drawing Grid..."));
      gimp_tile_cache_ntiles (2 * (drawable->width / gimp_tile_width () + 1));

      grid (image_ID, drawable, NULL);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();

      if (run_mode == GIMP_RUN_INTERACTIVE)
        gimp_set_data ("plug_in_grid", &grid_cfg, sizeof (grid_cfg));

      gimp_drawable_detach (drawable);
    }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}


#define MAXDIFF 195076

static guchar
best_cmap_match (const guchar  *cmap,
                 gint           ncolors,
                 const GimpRGB *color)
{
  guchar cmap_index = 0;
  gint   max = MAXDIFF;
  gint   i, diff, sum;
  guchar r, g, b;

  gimp_rgb_get_uchar (color, &r, &g, &b);

  for (i = 0; i < ncolors; i++)
    {
      diff = r - *cmap++;
      sum = SQR (diff);
      diff = g - *cmap++;
      sum += SQR (diff);
      diff = b - *cmap++;
      sum += SQR (diff);

      if (sum < max)
        {
          cmap_index = i;
          max = sum;
        }
    }

  return cmap_index;
}

static inline void
pix_composite (guchar   *p1,
               guchar    p2[4],
               gint      bytes,
               gboolean  blend,
               gboolean  alpha)
{
  gint b;

  if (alpha)
    {
      bytes--;
    }

  if (blend)
    {
      for (b = 0; b < bytes; b++)
        {
          *p1 = *p1 * (1.0 - p2[3]/255.0) + p2[b] * p2[3]/255.0;
          p1++;
        }
    }
  else
    {
      /* blend should only be TRUE for indexed (bytes == 1) */
      *p1++ = *p2;
    }

  if (alpha && *p1 < 255)
    {
      b = *p1 + 255.0 * ((double)p2[3] / (255.0 - *p1));
      *p1 = b > 255 ? 255 : b;
    }
}

static void
grid (gint32        image_ID,
      GimpDrawable *drawable,
      GimpPreview  *preview)
{
  GimpPixelRgn  srcPR, destPR;
  gint          bytes;
  gint          x_offset, y_offset;
  guchar       *dest, *buffer = NULL;
  gint          x, y;
  gboolean      alpha;
  gboolean      blend;
  guchar        hcolor[4];
  guchar        vcolor[4];
  guchar        icolor[4];
  guchar       *cmap;
  gint          ncolors;

  gimp_rgba_get_uchar (&grid_cfg.hcolor,
                       hcolor, hcolor + 1, hcolor + 2, hcolor + 3);
  gimp_rgba_get_uchar (&grid_cfg.vcolor,
                       vcolor, vcolor + 1, vcolor + 2, vcolor + 3);
  gimp_rgba_get_uchar (&grid_cfg.icolor,
                       icolor, icolor + 1, icolor + 2, icolor + 3);

  switch (gimp_image_base_type (image_ID))
    {
    case GIMP_RGB:
      blend = TRUE;
      break;

    case GIMP_GRAY:
      hcolor[0] = gimp_rgb_intensity_uchar (&grid_cfg.hcolor);
      vcolor[0] = gimp_rgb_intensity_uchar (&grid_cfg.vcolor);
      icolor[0] = gimp_rgb_intensity_uchar (&grid_cfg.icolor);
      blend = TRUE;
      break;

    case GIMP_INDEXED:
      cmap = gimp_image_get_colormap (image_ID, &ncolors);

      hcolor[0] = best_cmap_match (cmap, ncolors, &grid_cfg.hcolor);
      vcolor[0] = best_cmap_match (cmap, ncolors, &grid_cfg.vcolor);
      icolor[0] = best_cmap_match (cmap, ncolors, &grid_cfg.icolor);

      g_free (cmap);
      blend = FALSE;
      break;

    default:
      g_assert_not_reached ();
      blend = FALSE;
    }

  bytes = drawable->bpp;
  alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  if (preview)
    {
      gimp_preview_get_position (preview, &sx1, &sy1);
      gimp_preview_get_size (preview, &sx2, &sy2);

      buffer = g_new (guchar, bytes * sx2 * sy2);

      sx2 += sx1;
      sy2 += sy1;
    }
  else
    {
      gimp_drawable_mask_bounds (drawable->drawable_id, &sx1, &sy1, &sx2, &sy2);

      gimp_pixel_rgn_init (&destPR,
                           drawable, 0, 0, sx2 - sx1, sy2 - sy1, TRUE, TRUE);
    }

  gimp_pixel_rgn_init (&srcPR,
                       drawable, 0, 0, sx2 - sx1, sy2 - sy1, FALSE, FALSE);

  dest = g_new (guchar, (sx2 - sx1) * bytes);

  for (y = sy1; y < sy2; y++)
    {
      gimp_pixel_rgn_get_row (&srcPR, dest, sx1, y, (sx2 - sx1));

      y_offset = y - grid_cfg.hoffset;
      while (y_offset < 0)
        y_offset += grid_cfg.hspace;

      if ((y_offset +
           (grid_cfg.hwidth / 2)) % grid_cfg.hspace < grid_cfg.hwidth)
        {
          for (x = sx1; x < sx2; x++)
            {
              pix_composite (&dest[(x-sx1) * bytes],
                             hcolor, bytes, blend, alpha);
            }
        }

      for (x = sx1; x < sx2; x++)
        {
          x_offset = grid_cfg.vspace + x - grid_cfg.voffset;
          while (x_offset < 0)
            x_offset += grid_cfg.vspace;

          if ((x_offset +
               (grid_cfg.vwidth / 2)) % grid_cfg.vspace < grid_cfg.vwidth)
            {
              pix_composite (&dest[(x-sx1) * bytes],
                             vcolor, bytes, blend, alpha);
            }

          if ((x_offset +
               (grid_cfg.iwidth / 2)) % grid_cfg.vspace < grid_cfg.iwidth
              &&
              ((y_offset % grid_cfg.hspace >= grid_cfg.ispace
                &&
                y_offset % grid_cfg.hspace < grid_cfg.ioffset)
               ||
               (grid_cfg.hspace -
                (y_offset % grid_cfg.hspace) >= grid_cfg.ispace
                &&
                grid_cfg.hspace -
                (y_offset % grid_cfg.hspace) < grid_cfg.ioffset)))
            {
              pix_composite (&dest[(x-sx1) * bytes],
                             icolor, bytes, blend, alpha);
            }
        }

      if ((y_offset +
           (grid_cfg.iwidth / 2)) % grid_cfg.hspace < grid_cfg.iwidth)
        {
          for (x = sx1; x < sx2; x++)
            {
              x_offset = grid_cfg.vspace + x - grid_cfg.voffset;
              while (x_offset < 0)
                x_offset += grid_cfg.vspace;

              if ((x_offset % grid_cfg.vspace >= grid_cfg.ispace
                   &&
                   x_offset % grid_cfg.vspace < grid_cfg.ioffset)
                  ||
                  (grid_cfg.vspace -
                   (x_offset % grid_cfg.vspace) >= grid_cfg.ispace
                   &&
                   grid_cfg.vspace -
                   (x_offset % grid_cfg.vspace) < grid_cfg.ioffset))
                {
                  pix_composite (&dest[(x-sx1) * bytes],
                                 icolor, bytes, blend, alpha);
                }
            }
        }

      if (preview)
        {
          memcpy (buffer + (y - sy1) * (sx2 - sx1) * bytes,
                  dest,
                  (sx2 - sx1) * bytes);
        }
      else
        {
          gimp_pixel_rgn_set_row (&destPR, dest, sx1, y, (sx2 - sx1) );
          gimp_progress_update ((double) y / (double) (sy2 - sy1));
        }
    }

  g_free (dest);

  if (preview)
    {
      gimp_preview_draw_buffer (preview, buffer, bytes * (sx2 - sx1));
      g_free (buffer);
    }
  else
    {
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id,
                            sx1, sy1, sx2 - sx1, sy2 - sy1);
    }
}


/***************************************************
 * GUI stuff
 */


static void
update_values (void)
{
  GtkWidget *entry;

  entry = g_object_get_data (G_OBJECT (main_dialog), "width");

  grid_cfg.hwidth =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 0));
  grid_cfg.vwidth =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 1));
  grid_cfg.iwidth =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 2));

  entry = g_object_get_data (G_OBJECT (main_dialog), "space");

  grid_cfg.hspace =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 0));
  grid_cfg.vspace =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 1));
  grid_cfg.ispace =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 2));

  entry = g_object_get_data (G_OBJECT (main_dialog), "offset");

  grid_cfg.hoffset =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 0));
  grid_cfg.voffset =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 1));
  grid_cfg.ioffset =
    RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (entry), 2));
}

static void
update_preview (GimpPreview  *preview,
                GimpDrawable *drawable)
{
  update_values ();

  grid (gimp_drawable_get_image (drawable->drawable_id), drawable, preview);
}

static void
entry_callback (GtkWidget *widget,
                gpointer   data)
{
  static gdouble x = -1.0;
  static gdouble y = -1.0;
  gdouble new_x;
  gdouble new_y;

  new_x = gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 0);
  new_y = gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (widget), 1);

  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (data)))
    {
      if (new_x != x)
        {
          y = new_y = x = new_x;
          gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (widget), 1, y);
        }
      if (new_y != y)
        {
          x = new_x = y = new_y;
          gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (widget), 0, x);
        }
    }
  else
    {
      x = new_x;
      y = new_y;
    }
}

static void
color_callback (GtkWidget *widget,
                gpointer   data)
{
  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (data)))
    {
      GimpRGB  color;

      gimp_color_button_get_color (GIMP_COLOR_BUTTON (widget), &color);

      if (widget == vcolor_button)
        gimp_color_button_set_color (GIMP_COLOR_BUTTON (hcolor_button), &color);
      else if (widget == hcolor_button)
        gimp_color_button_set_color (GIMP_COLOR_BUTTON (vcolor_button), &color);
    }
}


static gint
dialog (gint32        image_ID,
        GimpDrawable *drawable)
{
  GtkWidget    *dlg;
  GtkWidget    *main_vbox;
  GtkWidget    *vbox;
  GtkSizeGroup *group;
  GtkWidget    *label;
  GtkWidget    *preview;
  GtkWidget    *button;
  GtkWidget    *width;
  GtkWidget    *space;
  GtkWidget    *offset;
  GtkWidget    *chain_button;
  GtkWidget    *table;
  GimpUnit      unit;
  gdouble       xres;
  gdouble       yres;
  gboolean      run;

  g_return_val_if_fail (main_dialog == NULL, FALSE);

  gimp_ui_init ("grid", TRUE);

  main_dialog = dlg = gimp_dialog_new (_("Grid"), "grid",
                                       NULL, 0,
                                       gimp_standard_help_func, "plug-in-grid",

                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OK,     GTK_RESPONSE_OK,

                                       NULL);

  /*  Get the image resolution and unit  */
  gimp_image_get_resolution (image_ID, &xres, &yres);
  unit = gimp_image_get_unit (image_ID);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  preview = gimp_drawable_preview_new (drawable, NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), preview, TRUE, TRUE, 0);
  gtk_widget_show (preview);

  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (update_preview),
                    drawable);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  /*  The width entries  */
  width = gimp_size_entry_new (3,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               TRUE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               SPIN_BUTTON_WIDTH,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */


  gtk_box_pack_start (GTK_BOX (vbox), width, FALSE, FALSE, 0);
  gtk_widget_show (width);

  /*  set the unit back to pixels, since most times we will want pixels */
  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (width), GIMP_UNIT_PIXEL);

  /*  set the resolution to the image resolution  */
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 0, xres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 1, yres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (width), 2, xres, TRUE);

  /*  set the size (in pixels) that will be treated as 0% and 100%  */
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (width), 0, 0.0, drawable->width);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (width), 1, 0.0, drawable->height);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (width), 2, 0.0, drawable->width);

  /*  set upper and lower limits (in pixels)  */
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (width), 0, 0.0,
                                         drawable->width);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (width), 1, 0.0,
                                         drawable->height);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (width), 2, 0.0,
                                         MAX (drawable->width,
                                              drawable->height));
  gtk_table_set_row_spacing (GTK_TABLE (width), 0, 6);
  gtk_table_set_col_spacings (GTK_TABLE (width), 6);
  gtk_table_set_col_spacing (GTK_TABLE (width), 2, 12);

  /*  initialize the values  */
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (width), 0, grid_cfg.hwidth);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (width), 1, grid_cfg.vwidth);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (width), 2, grid_cfg.iwidth);

  /*  attach labels  */
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), _("Horizontal"),
                                0, 1, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), _("Vertical"),
                                0, 2, 0.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), _("Intersection"),
                                0, 3, 0.0);

  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (width), _("Width:"),
                                        1, 0, 0.0);

  group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (group, label);
  g_object_unref (group);

  /*  put a chain_button under the size_entries  */
  chain_button = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);
  if (grid_cfg.hwidth == grid_cfg.vwidth)
    gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (chain_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (width), chain_button, 1, 3, 2, 3);
  gtk_widget_show (chain_button);

  /* connect to the 'value_changed' signal because we have to take care
   * of keeping the entries in sync when the chainbutton is active
   */
  g_signal_connect (width, "value_changed",
                    G_CALLBACK (entry_callback),
                    chain_button);
  g_signal_connect_swapped (width, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /*  The spacing entries  */
  space = gimp_size_entry_new (3,                            /*  number_of_fields  */
                               unit,                         /*  unit              */
                               "%a",                         /*  unit_format       */
                               TRUE,                         /*  menu_show_pixels  */
                               TRUE,                         /*  menu_show_percent */
                               FALSE,                        /*  show_refval       */
                               SPIN_BUTTON_WIDTH,            /*  spinbutton_usize  */
                               GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (vbox), space, FALSE, FALSE, 0);
  gtk_widget_show (space);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (space), GIMP_UNIT_PIXEL);

  /*  set the resolution to the image resolution  */
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (space), 0, xres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (space), 1, yres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (space), 2, xres, TRUE);

  /*  set the size (in pixels) that will be treated as 0% and 100%  */
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (space), 0, 0.0, drawable->width);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (space), 1, 0.0, drawable->height);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (space), 2, 0.0, drawable->width);

  /*  set upper and lower limits (in pixels)  */
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (space), 0, 1.0,
                                         drawable->width);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (space), 1, 1.0,
                                         drawable->height);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (space), 2, 0.0,
                                         MAX (drawable->width,
                                              drawable->height));
  gtk_table_set_col_spacings (GTK_TABLE (space), 6);
  gtk_table_set_col_spacing (GTK_TABLE (space), 2, 12);

  /*  initialize the values  */
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (space), 0, grid_cfg.hspace);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (space), 1, grid_cfg.vspace);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (space), 2, grid_cfg.ispace);

  /*  attach labels  */
  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (space), _("Spacing:"),
                                        1, 0, 0.0);
  gtk_size_group_add_widget (group, label);

  /*  put a chain_button under the spacing_entries  */
  chain_button = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);
  if (grid_cfg.hspace == grid_cfg.vspace)
    gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (chain_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (space), chain_button, 1, 3, 2, 3);
  gtk_widget_show (chain_button);

  /* connect to the 'value_changed' and "unit_changed" signals because
   * we have to take care of keeping the entries in sync when the
   * chainbutton is active
   */
  g_signal_connect (space, "value_changed",
                    G_CALLBACK (entry_callback),
                    chain_button);
  g_signal_connect (space, "unit_changed",
                    G_CALLBACK (entry_callback),
                    chain_button);
  g_signal_connect_swapped (space, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /*  The offset entries  */
  offset = gimp_size_entry_new (3,                            /*  number_of_fields  */
                                unit,                         /*  unit              */
                                "%a",                         /*  unit_format       */
                                TRUE,                         /*  menu_show_pixels  */
                                TRUE,                         /*  menu_show_percent */
                                FALSE,                        /*  show_refval       */
                                SPIN_BUTTON_WIDTH,            /*  spinbutton_usize  */
                                GIMP_SIZE_ENTRY_UPDATE_SIZE); /*  update_policy     */

  gtk_box_pack_start (GTK_BOX (vbox), offset, FALSE, FALSE, 0);
  gtk_widget_show (offset);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (offset), GIMP_UNIT_PIXEL);

  /*  set the resolution to the image resolution  */
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (offset), 0, xres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (offset), 1, yres, TRUE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (offset), 2, xres, TRUE);

  /*  set the size (in pixels) that will be treated as 0% and 100%  */
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (offset), 0, 0.0, drawable->width);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (offset), 1, 0.0, drawable->height);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (offset), 2, 0.0, drawable->width);

  /*  set upper and lower limits (in pixels)  */
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (offset), 0, 0.0,
                                         drawable->width);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (offset), 1, 0.0,
                                         drawable->height);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (offset), 2, 0.0,
                                         MAX (drawable->width,
                                              drawable->height));
  gtk_table_set_col_spacings (GTK_TABLE (offset), 6);
  gtk_table_set_col_spacing (GTK_TABLE (offset), 2, 12);

  /*  initialize the values  */
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (offset), 0, grid_cfg.hoffset);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (offset), 1, grid_cfg.voffset);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (offset), 2, grid_cfg.ioffset);

  /*  attach labels  */
  label = gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (offset), _("Offset:"),
                                        1, 0, 0.0);
  gtk_size_group_add_widget (group, label);

  /*  this is a weird hack: we put a table into the offset table  */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_attach_defaults (GTK_TABLE (offset), table, 1, 4, 2, 3);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 10);
  gtk_table_set_col_spacing (GTK_TABLE (table), 1, 12);

  /*  put a chain_button under the offset_entries  */
  chain_button = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);
  if (grid_cfg.hoffset == grid_cfg.voffset)
    gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (chain_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), chain_button, 0, 2, 0, 1);
  gtk_widget_show (chain_button);

  /* connect to the 'value_changed' and "unit_changed" signals because
   * we have to take care of keeping the entries in sync when the
   * chainbutton is active
   */
  g_signal_connect (offset, "value_changed",
                    G_CALLBACK (entry_callback),
                    chain_button);
  g_signal_connect (offset, "unit_changed",
                    G_CALLBACK (entry_callback),
                    chain_button);
  g_signal_connect_swapped (offset, "value_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /*  put a chain_button under the color_buttons  */
  chain_button = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);
  if (gimp_rgba_distance (&grid_cfg.hcolor, &grid_cfg.vcolor) < 0.0001)
    gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (chain_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), chain_button, 0, 2, 2, 3);
  gtk_widget_show (chain_button);

  /*  attach color selectors  */
  hcolor_button = gimp_color_button_new (_("Horizontal Color"),
                                         COLOR_BUTTON_WIDTH, 16,
                                         &grid_cfg.hcolor,
                                         GIMP_COLOR_AREA_SMALL_CHECKS);
  gimp_color_button_set_update (GIMP_COLOR_BUTTON (hcolor_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), hcolor_button, 0, 1, 1, 2);
  gtk_widget_show (hcolor_button);

  g_signal_connect (hcolor_button, "color_changed",
                    G_CALLBACK (gimp_color_button_get_color),
                    &grid_cfg.hcolor);
  g_signal_connect (hcolor_button, "color_changed",
                    G_CALLBACK (color_callback),
                    chain_button);
  g_signal_connect_swapped (hcolor_button, "color_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  vcolor_button = gimp_color_button_new (_("Vertical Color"),
                                         COLOR_BUTTON_WIDTH, 16,
                                         &grid_cfg.vcolor,
                                         GIMP_COLOR_AREA_SMALL_CHECKS);
  gimp_color_button_set_update (GIMP_COLOR_BUTTON (vcolor_button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), vcolor_button, 1, 2, 1, 2);
  gtk_widget_show (vcolor_button);

  g_signal_connect (vcolor_button, "color_changed",
                    G_CALLBACK (gimp_color_button_get_color),
                    &grid_cfg.vcolor);
  g_signal_connect (vcolor_button, "color_changed",
                    G_CALLBACK (color_callback),
                    chain_button);
  g_signal_connect_swapped (vcolor_button, "color_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  button = gimp_color_button_new (_("Intersection Color"),
                                  COLOR_BUTTON_WIDTH, 16,
                                  &grid_cfg.icolor,
                                  GIMP_COLOR_AREA_SMALL_CHECKS);
  gimp_color_button_set_update (GIMP_COLOR_BUTTON (button), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), button, 2, 3, 1, 2);
  gtk_widget_show (button);

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (gimp_color_button_get_color),
                    &grid_cfg.icolor);
  g_signal_connect_swapped (button, "color_changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  gtk_widget_show (table);

  gtk_widget_show (dlg);

  g_object_set_data (G_OBJECT (dlg), "width",  width);
  g_object_set_data (G_OBJECT (dlg), "space",  space);
  g_object_set_data (G_OBJECT (dlg), "offset", offset);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  if (run)
    update_values ();

  gtk_widget_destroy (dlg);

  return run;
}
