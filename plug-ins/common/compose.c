/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Compose plug-in (C) 1997,1999 Peter Kirchgessner
 * e-mail: peter@kirchgessner.net, WWW: http://www.kirchgessner.net
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

/*
 * This plug-in composes RGB-images from several types of channels
 */

/*  Lab colorspace support originally written by Alexey Dyachenko,
 *  merged into the officical plug-in by Sven Neumann.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <libgimpmath/gimpmath.h>

#include "libgimp/stdplugins-intl.h"


/* Declare local functions
 */
static void      query  (void);
static void      run    (const gchar      *name,
			 gint              nparams,
			 const GimpParam  *param,
			 gint             *nreturn_vals,
			 GimpParam       **return_vals);

static gint32    compose          (const gchar    *compose_type,
                                   gint32         *compose_ID,
                                   gboolean        compose_by_drawable);

static gint32    create_new_image (const gchar    *filename,
				   guint           width,
				   guint           height,
				   GimpImageType   gdtype,
				   gint32         *layer_ID,
				   GimpDrawable  **drawable,
				   GimpPixelRgn   *pixel_rgn);

static void  compose_rgb       (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_rgba      (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_hsv       (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_cmy       (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_cmyk      (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_lab       (guchar **src,
                                gint    *incr, gint numpix, guchar *dst);
static void  compose_ycbcr470  (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_ycbcr709  (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_ycbcr470f (guchar **src,
				gint    *incr, gint numpix, guchar *dst);
static void  compose_ycbcr709f (guchar **src,
				gint    *incr, gint numpix, guchar *dst);

static gboolean  compose_dialog (const gchar *compose_type,
                                 gint32       drawable_ID);

static gboolean  check_gray     (gint32    image_id,
				 gint32    drawable_id,
				 gpointer  data);

static void      compose_type_toggle_update  (GtkWidget *widget,
                                              gpointer   data);


/* LAB colorspace constants */
const double Xn	= 0.951;
const double Yn	= 1.0;
const double Zn	= 1.089;

/* Maximum number of images to compose */
#define MAX_COMPOSE_IMAGES 4


/* Description of a composition */
typedef struct
{
  const gchar  *compose_type;  /*  Type of composition ("RGB", "RGBA",...)  */
  gint          num_images;    /*  Number of input images needed            */
                               /*  Channel names and stock ids for dialog   */
  const gchar  *channel_name[MAX_COMPOSE_IMAGES];
  const gchar  *channel_icon[MAX_COMPOSE_IMAGES];
  const gchar  *filename;      /*  Name of new image                        */
                               /*  Compose functon                          */
  void  (* compose_fun) (guchar **src,
			 gint    *incr_src,
			 gint     numpix,
			 guchar  *dst);
} COMPOSE_DSC;

/* Array of available compositions. */

static COMPOSE_DSC compose_dsc[] =
{
  { N_("RGB"), 3,
    { N_("Red:"),
      N_("Green:"),
      N_("Blue:"),
      NULL },
    { GIMP_STOCK_CHANNEL_RED,
      GIMP_STOCK_CHANNEL_GREEN,
      GIMP_STOCK_CHANNEL_BLUE,
      NULL },
    "rgb-compose",  compose_rgb },
  { N_("RGBA"), 4,
    { N_("Red:"),
      N_("Green:"),
      N_("Blue:"),
      N_("Alpha:") },
    { GIMP_STOCK_CHANNEL_RED,
      GIMP_STOCK_CHANNEL_GREEN,
      GIMP_STOCK_CHANNEL_BLUE,
      GIMP_STOCK_CHANNEL_ALPHA },
    "rgba-compose",  compose_rgba },
  { N_("HSV"),  3,
    { N_("Hue:"),
      N_("Saturation:"),
      N_("Value:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "hsv-compose",  compose_hsv },
  { N_("CMY"),  3,
    { N_("Cyan:"),
      N_("Magenta:"),
      N_("Yellow:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "cmy-compose",  compose_cmy },
  { N_("CMYK"), 4,
    { N_("Cyan:"),
      N_("Magenta:"),
      N_("Yellow:"),
      N_("Black:") },
    { NULL, NULL, NULL, NULL },
    "cmyk-compose", compose_cmyk },
  { N_("LAB"), 3,
    { "L",
      "A",
      "B",
      NULL },
    { NULL, NULL, NULL, NULL },
    "lab-compose", compose_lab },
  { "YCbCr_ITU_R470", 3,
    { N_("Luma_y470:"),
      N_("Blueness_cb470:"),
      N_("Redness_cr470:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "ycbcr470-compose",  compose_ycbcr470 },
  { "YCbCr_ITU_R709", 3,
    { N_("Luma_y709:"),
      N_("Blueness_cb709:"),
      N_("Redness_cr709:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "ycbcr709-compose", compose_ycbcr709 },
  { "YCbCr_ITU_R470_256", 3,
    { N_("Luma_y470f:"),
      N_("Blueness_cb470f:"),
      N_("Redness_cr470f:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "ycbcr470F-compose",  compose_ycbcr470f },
  { "YCbCr_ITU_R709_256", 3,
    { N_("Luma_y709f:"),
      N_("Blueness_cb709f:"),
      N_("Redness_cr709f:"),
      NULL },
    { NULL, NULL, NULL, NULL },
    "ycbcr709F-compose",  compose_ycbcr709f },
};


typedef struct
{
  gint32 compose_ID[MAX_COMPOSE_IMAGES];  /* Image IDs of input images */
  gchar  compose_type[32];                /* type of composition */
} ComposeVals;

/* Dialog structure */
typedef struct
{
  gint       width, height;                     /* Size of selected image */

  GtkWidget *channel_label[MAX_COMPOSE_IMAGES]; /* The labels to change */
  GtkWidget *channel_icon[MAX_COMPOSE_IMAGES];  /* The icons  */
  GtkWidget *channel_menu[MAX_COMPOSE_IMAGES];  /* The menues */

  gint32     select_ID[MAX_COMPOSE_IMAGES];     /* Image Ids selected by menu */
  gint       compose_flag[G_N_ELEMENTS (compose_dsc)];   /* toggle data of compose type */
} ComposeInterface;

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static ComposeVals composevals =
{
  { 0 },  /* Image IDs of images to compose */
  "rgb"   /* Type of composition */
};

static ComposeInterface composeint =
{
  0, 0,     /* width, height */
  { NULL }, /* Label Widgets */
  { NULL }, /* Icon Widgets */
  { NULL }, /* Menu Widgets */
  { 0 },    /* Image IDs from menues */
  { 0 }     /* Compose type toggle flags */
};

static GimpRunMode run_mode;


MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image1", "First input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable (not used)" },
    { GIMP_PDB_IMAGE, "image2", "Second input image" },
    { GIMP_PDB_IMAGE, "image3", "Third input image" },
    { GIMP_PDB_IMAGE, "image4", "Fourth input image" },
    { GIMP_PDB_STRING, "compose_type", "What to compose: RGB, RGBA, HSV, CMY, CMYK" }
  };
  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Output image" }
  };

  static GimpParamDef drw_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image1", "First input image (not used)" },
    { GIMP_PDB_DRAWABLE, "drawable1", "First input drawable" },
    { GIMP_PDB_DRAWABLE, "drawable2", "Second input drawable" },
    { GIMP_PDB_DRAWABLE, "drawable3", "Third input drawable" },
    { GIMP_PDB_DRAWABLE, "drawable4", "Fourth input drawable" },
    { GIMP_PDB_STRING, "compose_type", "What to compose: RGB, RGBA, HSV, CMY, CMYK" }
  };
  static GimpParamDef drw_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Output image" }
  };

  gimp_install_procedure ("plug_in_compose",
			  "Compose an image from multiple gray images",
			  "This function creates a new image from "
			  "multiple gray images",
			  "Peter Kirchgessner",
			  "Peter Kirchgessner (peter@kirchgessner.net)",
			  "1997",
			  N_("C_ompose..."),
			  "GRAY*",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (args),
                          G_N_ELEMENTS (return_vals),
			  args, return_vals);

  gimp_plugin_menu_register ("plug_in_compose", "<Image>/Filters/Colors");
  gimp_plugin_menu_register ("plug_in_compose", "<Image>/Image/Mode");

  gimp_install_procedure ("plug_in_drawable_compose",
			  "Compose an image from multiple drawables of gray images",
			  "This function creates a new image from "
			  "multiple drawables of gray images",
			  "Peter Kirchgessner",
			  "Peter Kirchgessner (peter@kirchgessner.net)",
			  "1998",
			  NULL,   /* It is not available in interactive mode */
			  "GRAY*",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (drw_args),
                          G_N_ELEMENTS (drw_return_vals),
			  drw_args, drw_return_vals);
}


static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam  values[2];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32            image_ID, drawable_ID;
  gint              compose_by_drawable;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;
  compose_by_drawable = (strcmp (name, "plug_in_drawable_compose") == 0);

  *nreturn_vals = 2;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type          = GIMP_PDB_IMAGE;
  values[1].data.d_int32  = -1;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      gimp_get_data (name , &composevals);

      /* The dialog is now drawable based. Get a drawable-ID of the image */
      if (strcmp (name, "plug_in_compose") == 0)
        {
          gint32 *layer_list;
          gint nlayers;

          layer_list = gimp_image_get_layers (param[1].data.d_int32, &nlayers);
          if ((layer_list == NULL) || (nlayers <= 0))
            {
              g_message (_("Could not get layers for image %d"),
                         (gint) param[1].data.d_int32);
              return;
            }
          drawable_ID = layer_list[0];
          g_free (layer_list);
        }
      else
        {
          drawable_ID = param[2].data.d_int32;
        }

      compose_by_drawable = 1;

      /*  First acquire information with a dialog  */
      if (! compose_dialog (composevals.compose_type, drawable_ID))
	return;

      break;

    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      if (nparams != 7)
	{
	  status = GIMP_PDB_CALLING_ERROR;
	}
      else
	{
	  composevals.compose_ID[0] = (compose_by_drawable ?
                                       param[2].data.d_int32 :
                                       param[1].data.d_int32);
	  composevals.compose_ID[1] = param[3].data.d_int32;
	  composevals.compose_ID[2] = param[4].data.d_int32;
	  composevals.compose_ID[3] = param[5].data.d_int32;
	  strncpy (composevals.compose_type, param[6].data.d_string,
		   sizeof (composevals.compose_type));
	  composevals.compose_type[sizeof (composevals.compose_type)-1] = '\0';
	}
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      gimp_get_data (name, &composevals);
      break;

    default:
      break;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      gimp_progress_init (_("Composing..."));

      image_ID = compose (composevals.compose_type,
			  composevals.compose_ID,
                          compose_by_drawable);

      if (image_ID < 0)
	{
	  status = GIMP_PDB_EXECUTION_ERROR;
	}
      else
	{
	  values[1].data.d_int32 = image_ID;
	  gimp_image_undo_enable (image_ID);
	  gimp_image_clean_all (image_ID);
	  if (run_mode != GIMP_RUN_NONINTERACTIVE)
	    gimp_display_new (image_ID);
	}

      /*  Store data  */
      if (run_mode == GIMP_RUN_INTERACTIVE)
        gimp_set_data (name, &composevals, sizeof (ComposeVals));
    }

  values[0].data.d_status = status;
}


/* Compose an image from several gray-images */
static gint32
compose (const gchar *compose_type,
         gint32      *compose_ID,
         gboolean     compose_by_drawable)
{
  gint           width, height, tile_height, scan_lines;
  gint           num_images, compose_idx, incr_src[MAX_COMPOSE_IMAGES];
  gint           i, j;
  gint           num_layers;
  gint32         layer_ID_dst, image_ID_dst;
  guchar        *src[MAX_COMPOSE_IMAGES];
  guchar        *dst;
  GimpImageType  gdtype_dst;
  GimpDrawable  *drawable_src[MAX_COMPOSE_IMAGES], *drawable_dst;
  GimpPixelRgn   pixel_rgn_src[MAX_COMPOSE_IMAGES], pixel_rgn_dst;

  /* Search type of composing */
  compose_idx = -1;
  for (j = 0; j < G_N_ELEMENTS (compose_dsc); j++)
    {
      if (g_ascii_strcasecmp (compose_type, compose_dsc[j].compose_type) == 0)
	compose_idx = j;
    }
  if (compose_idx < 0)
    return -1;

  num_images = compose_dsc[compose_idx].num_images;
  tile_height = gimp_tile_height ();

  /* Check image sizes */
  if (compose_by_drawable)
    {
      width = gimp_drawable_width (compose_ID[0]);
      height = gimp_drawable_height (compose_ID[0]);

      for (j = 1; j < num_images; j++)
	{
	  if ((width != (gint)gimp_drawable_width (compose_ID[j])) ||
	      (height != (gint)gimp_drawable_height (compose_ID[j])))
	    {
	      g_message (_("Drawables have different size"));
	      return -1;
	    }
	}
      for (j = 0; j < num_images; j++)
	drawable_src[j] = gimp_drawable_get (compose_ID[j]);
    }
  else    /* Compose by image ID */
    {
      width = gimp_image_width (compose_ID[0]);
      height = gimp_image_height (compose_ID[0]);

      for (j = 1; j < num_images; j++)
	{
	  if ((width != (gint)gimp_image_width (compose_ID[j])) ||
	      (height != (gint)gimp_image_height (compose_ID[j])))
	    {
	      g_message (_("Images have different size"));
	      return -1;
	    }
	}

      /* Get first layer/drawable for all input images */
      for (j = 0; j < num_images; j++)
	{
	  gint32 *g32;

	  /* Get first layer of image */
	  g32 = gimp_image_get_layers (compose_ID[j], &num_layers);
	  if ((g32 == NULL) || (num_layers <= 0))
	    {
	      g_message (_("Error in getting layer IDs"));
	      return -1;
	    }

	  /* Get drawable for layer */
	  drawable_src[j] = gimp_drawable_get (g32[0]);
	  g_free (g32);
	}
    }

  /* Get pixel region for all input drawables */
  for (j = 0; j < num_images; j++)
    {
      /* Check bytes per pixel */
      incr_src[j] = drawable_src[j]->bpp;
      if ((incr_src[j] != 1) && (incr_src[j] != 2))
	{
	  g_message (_("Image is not a gray image (bpp=%d)"),
		     incr_src[j]);
	  return -1;
	}

      /* Get pixel region */
      gimp_pixel_rgn_init (&(pixel_rgn_src[j]), drawable_src[j], 0, 0,
			   width, height, FALSE, FALSE);

      /* Get memory for retrieving information */
      src[j] = g_new (guchar, tile_height * width * drawable_src[j]->bpp);
    }

  /* Create new image */
  gdtype_dst = (compose_dsc[compose_idx].compose_fun == compose_rgba)
    ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE;

  image_ID_dst = create_new_image (compose_dsc[compose_idx].filename,
				   width, height, gdtype_dst,
				   &layer_ID_dst, &drawable_dst,
				   &pixel_rgn_dst);

  if (! compose_by_drawable)
    {
      gdouble  xres, yres;

      gimp_image_get_resolution (compose_ID[0], &xres, &yres);
      gimp_image_set_resolution (image_ID_dst, xres, yres);
    }

  dst = g_new (guchar, tile_height * width * drawable_dst->bpp);

  /* Do the composition */
  i = 0;
  while (i < height)
    {
      scan_lines = (i+tile_height-1 < height) ? tile_height : (height-i);

      /* Get source pixel regions */
      for (j = 0; j < num_images; j++)
	gimp_pixel_rgn_get_rect (&(pixel_rgn_src[j]), src[j], 0, i,
				 width, scan_lines);

      /* Do the composition */
      compose_dsc[compose_idx].compose_fun (src,
					    incr_src,
					    width * tile_height,
					    dst);

      /* Set destination pixel region */
      gimp_pixel_rgn_set_rect (&pixel_rgn_dst, dst, 0, i, width, scan_lines);

      i += scan_lines;

      gimp_progress_update ((gdouble) i / (gdouble) height);
    }

  for (j = 0; j < num_images; j++)
    {
      g_free (src[j]);
      gimp_drawable_detach (drawable_src[j]);
    }
  g_free (dst);
  gimp_drawable_detach (drawable_dst);
  gimp_drawable_update (layer_ID_dst, 0, 0,
                        gimp_drawable_width (layer_ID_dst),
                        gimp_drawable_height (layer_ID_dst));

  return image_ID_dst;
}


/* Create an image. Sets layer_ID, drawable and rgn. Returns image_ID */
static gint32
create_new_image (const gchar    *filename,
                  guint           width,
                  guint           height,
                  GimpImageType   gdtype,
                  gint32         *layer_ID,
                  GimpDrawable  **drawable,
                  GimpPixelRgn   *pixel_rgn)
{
  gint32            image_ID;
  GimpImageBaseType gitype;

  if ((gdtype == GIMP_GRAY_IMAGE) || (gdtype == GIMP_GRAYA_IMAGE))
    gitype = GIMP_GRAY;
  else if ((gdtype == GIMP_INDEXED_IMAGE) || (gdtype == GIMP_INDEXEDA_IMAGE))
    gitype = GIMP_INDEXED;
  else
    gitype = GIMP_RGB;

  image_ID = gimp_image_new (width, height, gitype);

  gimp_image_undo_disable (image_ID);
  gimp_image_set_filename (image_ID, filename);

  *layer_ID = gimp_layer_new (image_ID, _("Background"), width, height,
			      gdtype, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_ID, *layer_ID, 0);

  *drawable = gimp_drawable_get (*layer_ID);
  gimp_pixel_rgn_init (pixel_rgn, *drawable, 0, 0, (*drawable)->width,
		       (*drawable)->height, TRUE, FALSE);

  return image_ID;
}

static void
compose_rgb (guchar **src,
             gint    *incr_src,
             gint     numpix,
             guchar  *dst)
{
  register const guchar *red_src   = src[0];
  register const guchar *green_src = src[1];
  register const guchar *blue_src  = src[2];
  register       guchar *rgb_dst   = dst;
  register       gint    count     = numpix;
  gint red_incr   = incr_src[0];
  gint green_incr = incr_src[1];
  gint blue_incr  = incr_src[2];

  if ((red_incr == 1) && (green_incr == 1) && (blue_incr == 1))
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = *(red_src++);
	  *(rgb_dst++) = *(green_src++);
	  *(rgb_dst++) = *(blue_src++);
	}
    }
  else
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = *red_src;     red_src += red_incr;
	  *(rgb_dst++) = *green_src;   green_src += green_incr;
	  *(rgb_dst++) = *blue_src;    blue_src += blue_incr;
	}
    }
}


static void
compose_rgba (guchar **src,
              gint    *incr_src,
              gint     numpix,
              guchar  *dst)
{
  register const guchar *red_src   = src[0];
  register const guchar *green_src = src[1];
  register const guchar *blue_src  = src[2];
  register const guchar *alpha_src = src[3];
  register       guchar *rgb_dst   = dst;
  register       gint    count     = numpix;
  gint red_incr   = incr_src[0];
  gint green_incr = incr_src[1];
  gint blue_incr  = incr_src[2];
  gint alpha_incr = incr_src[3];

  if ((red_incr == 1) && (green_incr == 1) && (blue_incr == 1) &&
      (alpha_incr == 1))
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = *(red_src++);
	  *(rgb_dst++) = *(green_src++);
	  *(rgb_dst++) = *(blue_src++);
	  *(rgb_dst++) = *(alpha_src++);
	}
    }
  else
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = *red_src;    red_src += red_incr;
	  *(rgb_dst++) = *green_src;  green_src += green_incr;
	  *(rgb_dst++) = *blue_src;   blue_src += blue_incr;
	  *(rgb_dst++) = *alpha_src;  alpha_src += alpha_incr;
	}
    }
}


static void
compose_hsv (guchar **src,
             gint    *incr_src,
             gint     numpix,
             guchar  *dst)
{
  register const guchar *hue_src = src[0];
  register const guchar *sat_src = src[1];
  register const guchar *val_src = src[2];
  register       guchar *rgb_dst = dst;
  register       gint    count   = numpix;
  gint hue_incr = incr_src[0];
  gint sat_incr = incr_src[1];
  gint val_incr = incr_src[2];

  while (count-- > 0)
    {
      gimp_hsv_to_rgb4 (rgb_dst, (gdouble) *hue_src / 255.0,
			         (gdouble) *sat_src / 255.0,
			         (gdouble) *val_src / 255.0);
      rgb_dst += 3;
      hue_src += hue_incr;
      sat_src += sat_incr;
      val_src += val_incr;
    }
}


static void
compose_cmy (guchar **src,
             gint    *incr_src,
             gint     numpix,
             guchar  *dst)
{
  register const guchar *cyan_src    = src[0];
  register const guchar *magenta_src = src[1];
  register const guchar *yellow_src  = src[2];
  register       guchar *rgb_dst     = dst;
  register       gint    count       = numpix;
  gint cyan_incr    = incr_src[0];
  gint magenta_incr = incr_src[1];
  gint yellow_incr  = incr_src[2];

  if ((cyan_incr == 1) && (magenta_incr == 1) && (yellow_incr == 1))
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = 255 - *(cyan_src++);
	  *(rgb_dst++) = 255 - *(magenta_src++);
	  *(rgb_dst++) = 255 - *(yellow_src++);
	}
    }
  else
    {
      while (count-- > 0)
	{
	  *(rgb_dst++) = 255 - *cyan_src;
	  *(rgb_dst++) = 255 - *magenta_src;
	  *(rgb_dst++) = 255 - *yellow_src;
	  cyan_src += cyan_incr;
	  magenta_src += magenta_incr;
	  yellow_src += yellow_incr;
	}
    }
}


static void
compose_cmyk (guchar **src,
              gint    *incr_src,
              gint     numpix,
              guchar  *dst)
{
  register const guchar *cyan_src    = src[0];
  register const guchar *magenta_src = src[1];
  register const guchar *yellow_src  = src[2];
  register const guchar *black_src   = src[3];
  register       guchar *rgb_dst     = dst;
  register       gint    count       = numpix;
  gint cyan, magenta, yellow, black;
  gint cyan_incr    = incr_src[0];
  gint magenta_incr = incr_src[1];
  gint yellow_incr  = incr_src[2];
  gint black_incr   = incr_src[3];

  while (count-- > 0)
    {
      black = (gint)*black_src;
      if (black)
	{
	  cyan    = (gint) *cyan_src;
	  magenta = (gint) *magenta_src;
	  yellow  = (gint) *yellow_src;

	  cyan    += black; if (cyan > 255) cyan = 255;
	  magenta += black; if (magenta > 255) magenta = 255;
	  yellow  += black; if (yellow > 255) yellow = 255;

	  *(rgb_dst++) = 255 - cyan;
	  *(rgb_dst++) = 255 - magenta;
	  *(rgb_dst++) = 255 - yellow;
	}
      else
	{
	  *(rgb_dst++) = 255 - *cyan_src;
	  *(rgb_dst++) = 255 - *magenta_src;
	  *(rgb_dst++) = 255 - *yellow_src;
	}
      cyan_src += cyan_incr;
      magenta_src += magenta_incr;
      yellow_src += yellow_incr;
      black_src += black_incr;
    }
}

static void
compose_lab (guchar **src,
             gint    *incr_src,
             gint     numpix,
             guchar  *dst)
{
  register guchar *l_src = src[0];
  register guchar *a_src = src[1];
  register guchar *b_src = src[2];
  register guchar *rgb_dst = dst;

  register gint count = numpix;
  gint l_incr = incr_src[0], a_incr = incr_src[1], b_incr = incr_src[2];

  gdouble red, green, blue;
  gdouble x, y, z;
  gdouble l, a, b;

  gdouble p, yyn;
  gdouble ha, hb, sqyyn;

  while (count-- > 0)
    {
      l = *l_src / 2.550;
      a = ( *a_src - 128.0 ) / 1.27;
      b = ( *b_src - 128.0 ) / 1.27;

      p = (l + 16.) / 116.;
      yyn = p*p*p;

      if (yyn > 0.008856)
        {
          y = Yn * yyn;
          ha = (p + a/500.);
          x = Xn * ha*ha*ha;
          hb = (p - b/200.);
          z = Zn * hb*hb*hb;
        }
      else
        {
          y = Yn * l/903.3;
          sqyyn = pow(l/903.3,1./3.);
          ha = a/500./7.787 + sqyyn;
          x = Xn * ha*ha*ha;
          hb = sqyyn - b/200./7.787;
          z = Zn * hb*hb*hb;
        };

      red   =  3.063 * x - 1.393 * y - 0.476 * z;
      green = -0.969 * x + 1.876 * y + 0.042 * z;
      blue  =  0.068 * x - 0.229 * y + 1.069 * z;

      red   = ( red   > 0 ) ? red   : 0;
      green = ( green > 0 ) ? green : 0;
      blue  = ( blue  > 0 ) ? blue  : 0;

      red   = ( red   < 1.0 ) ? red   : 1.0;
      green = ( green < 1.0 ) ? green : 1.0;
      blue  = ( blue  < 1.0 ) ? blue  : 1.0;

      rgb_dst[0] = (guchar) ( red   * 255.999 );
      rgb_dst[1] = (guchar) ( green * 255.999 );
      rgb_dst[2] = (guchar) ( blue  * 255.999 );

      rgb_dst += 3;
      l_src += l_incr;
      a_src += a_incr;
      b_src += b_incr;
    }
}


/* these are here so the code is more readable and we can use
   the standart values instead of some scaled and rounded fixpoint values */
#define FIX(a) ((int)((a)*256.0*256.0 + 0.5))
#define FIXY(a) ((int)((a)*256.0*256.0*255.0/219.0 + 0.5))
#define FIXC(a) ((int)((a)*256.0*256.0*255.0/224.0 + 0.5))


static void
compose_ycbcr470 (guchar **src,
                  gint    *incr_src,
                  gint     numpix,
                  guchar  *dst)
{
  register const guchar *y_src   = src[0];
  register const guchar *cb_src  = src[1];
  register const guchar *cr_src  = src[2];
  register       guchar *rgb_dst = dst;
  register       gint    count   = numpix;
  gint y_incr  = incr_src[0];
  gint cb_incr = incr_src[1];
  gint cr_incr = incr_src[2];

  while (count-- > 0)
    {
      int r,g,b,y,cb,cr;
      y = *y_src  - 16;
      cb= *cb_src - 128;
      cr= *cr_src - 128;
      y_src  += y_incr;
      cb_src += cb_incr;
      cr_src += cr_incr;

      r = (FIXY(1.0)*y                   + FIXC(1.4022)*cr + FIX(0.5))>>16;
      g = (FIXY(1.0)*y - FIXC(0.3456)*cb - FIXC(0.7145)*cr + FIX(0.5))>>16;
      b = (FIXY(1.0)*y + FIXC(1.7710)*cb                   + FIX(0.5))>>16;

      if(((unsigned)r) > 255) r = ((r>>10)&255)^255;
      if(((unsigned)g) > 255) g = ((g>>10)&255)^255;
      if(((unsigned)b) > 255) b = ((b>>10)&255)^255;

      *(rgb_dst++) = r;
      *(rgb_dst++) = g;
      *(rgb_dst++) = b;
    }
}


static void
compose_ycbcr709 (guchar **src,
                  gint    *incr_src,
                  gint     numpix,
                  guchar  *dst)
{
  register const guchar *y_src   = src[0];
  register const guchar *cb_src  = src[1];
  register const guchar *cr_src  = src[2];
  register       guchar *rgb_dst = dst;
  register       gint    count   = numpix;
  gint y_incr  = incr_src[0];
  gint cb_incr = incr_src[1];
  gint cr_incr = incr_src[2];

  while (count-- > 0)
    {
      int r,g,b,y,cb,cr;
      y = *y_src  - 16;
      cb= *cb_src - 128;
      cr= *cr_src - 128;
      y_src  += y_incr;
      cb_src += cb_incr;
      cr_src += cr_incr;

      r = (FIXY(1.0)*y                   + FIXC(1.5748)*cr + FIX(0.5))>>16;
      g = (FIXY(1.0)*y - FIXC(0.1873)*cb - FIXC(0.4681)*cr + FIX(0.5))>>16;
      b = (FIXY(1.0)*y + FIXC(1.8556)*cb                   + FIX(0.5))>>16;

      if(((unsigned)r) > 255) r = ((r>>10)&255)^255;
      if(((unsigned)g) > 255) g = ((g>>10)&255)^255;
      if(((unsigned)b) > 255) b = ((b>>10)&255)^255;

      *(rgb_dst++) = r;
      *(rgb_dst++) = g;
      *(rgb_dst++) = b;
    }
}


static void
compose_ycbcr470f (guchar **src,
                   gint    *incr_src,
                   gint     numpix,
                   guchar  *dst)
{
  register const guchar *y_src   = src[0];
  register const guchar *cb_src  = src[1];
  register const guchar *cr_src  = src[2];
  register       guchar *rgb_dst = dst;
  register       gint    count   = numpix;
  gint y_incr  = incr_src[0];
  gint cb_incr = incr_src[1];
  gint cr_incr = incr_src[2];

  while (count-- > 0)
    {
      int r,g,b,y,cb,cr;
      y = *y_src;
      cb= *cb_src - 128;
      cr= *cr_src - 128;
      y_src  += y_incr;
      cb_src += cb_incr;
      cr_src += cr_incr;

      r = (FIX(1.0)*y                  + FIX(1.4022)*cr + FIX(0.5))>>16;
      g = (FIX(1.0)*y - FIX(0.3456)*cb - FIX(0.7145)*cr + FIX(0.5))>>16;
      b = (FIX(1.0)*y + FIX(1.7710)*cb                  + FIX(0.5))>>16;

      if(((unsigned)r) > 255) r = ((r>>10)&255)^255;
      if(((unsigned)g) > 255) g = ((g>>10)&255)^255;
      if(((unsigned)b) > 255) b = ((b>>10)&255)^255;

      *(rgb_dst++) = r;
      *(rgb_dst++) = g;
      *(rgb_dst++) = b;
    }
}


static void
compose_ycbcr709f (guchar **src,
                   gint    *incr_src,
                   gint     numpix,
                   guchar  *dst)
{
  register const guchar *y_src   = src[0];
  register const guchar *cb_src  = src[1];
  register const guchar *cr_src  = src[2];
  register       guchar *rgb_dst = dst;
  register       gint    count   = numpix;
  gint y_incr  = incr_src[0];
  gint cb_incr = incr_src[1];
  gint cr_incr = incr_src[2];

  while (count-- > 0)
    {
      int r,g,b,y,cb,cr;
      y = *y_src;
      cb= *cb_src - 128;
      cr= *cr_src - 128;
      y_src  += y_incr;
      cb_src += cb_incr;
      cr_src += cr_incr;

      r = (FIX(1.0)*y                   + FIX(1.5748)*cr + FIX(0.5))>>16;
      g = (FIX(1.0)*y - FIX(0.1873)*cb  - FIX(0.4681)*cr + FIX(0.5))>>16;
      b = (FIX(1.0)*y + FIX(1.8556)*cb                   + FIX(0.5))>>16;

      if(((unsigned)r) > 255) r = ((r>>10)&255)^255;
      if(((unsigned)g) > 255) g = ((g>>10)&255)^255;
      if(((unsigned)b) > 255) b = ((b>>10)&255)^255;

      *(rgb_dst++) = r;
      *(rgb_dst++) = g;
      *(rgb_dst++) = b;
    }
}


static gboolean
compose_dialog (const gchar *compose_type,
                gint32       drawable_ID)
{
  GtkWidget *dlg;
  GtkWidget *toggle;
  GtkWidget *left_frame, *right_frame;
  GtkWidget *left_vbox, *right_vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *image;
  GSList    *group;
  gint32    *layer_list;
  gint       nlayers;
  gint       j, compose_idx;
  gboolean   run;

  /* Check default compose type */
  compose_idx = -1;
  for (j = 0; j < G_N_ELEMENTS (compose_dsc); j++)
    {
      if (g_ascii_strcasecmp (compose_type, compose_dsc[j].compose_type) == 0)
	compose_idx = j;
    }
  if (compose_idx < 0) compose_idx = 0;

  /* Save original image width/height */
  composeint.width  = gimp_drawable_width (drawable_ID);
  composeint.height = gimp_drawable_height (drawable_ID);

  gimp_ui_init ("compose", TRUE);

  layer_list = gimp_image_get_layers (gimp_drawable_get_image (drawable_ID),
                                      &nlayers);

  dlg = gimp_dialog_new (_("Compose"), "compose",
                         NULL, 0,
			 gimp_standard_help_func, "plug-in-compose",

			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK,     GTK_RESPONSE_OK,

			 NULL);

  /*  parameter settings  */
  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* The left frame keeps the compose type toggles */
  left_frame = gimp_frame_new (_("Compose Channels"));
  gtk_box_pack_start (GTK_BOX (hbox), left_frame, FALSE, FALSE, 0);

  left_vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (left_frame), left_vbox);

  /* The right frame keeps the selection menues for images. */
  /* Because the labels within this frame will change when a toggle */
  /* in the left frame is changed, fill in the right part first. */
  /* Otherwise it can occur, that a non-existing label might be changed. */

  right_frame = gimp_frame_new (_("Channel Representations"));
  gtk_box_pack_start (GTK_BOX (hbox), right_frame, TRUE, TRUE, 0);

  right_vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (right_frame), right_vbox);

  table = gtk_table_new (MAX_COMPOSE_IMAGES, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (right_vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  for (j = 0; j < MAX_COMPOSE_IMAGES; j++)
    {
      GtkWidget   *combo;
      const gchar *text;

      image = gtk_image_new_from_stock (compose_dsc[compose_idx].channel_icon[j],
                                        GTK_ICON_SIZE_BUTTON);
      composeint.channel_icon[j] = image;
      gtk_table_attach (GTK_TABLE (table), image, 0, 1, j, j+1,
                        GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (image);

      text = (compose_dsc[compose_idx].channel_name[j] ?
              gettext (compose_dsc[compose_idx].channel_name[j]) : NULL);
      composeint.channel_label[j] = label = gtk_label_new (text);

      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_table_attach (GTK_TABLE (table), label, 1, 2, j, j+1,
			GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (label);

      if (nlayers >= compose_dsc[compose_idx].num_images)
        composeint.select_ID[j] = layer_list[nlayers - (j + 1)];
      else
        composeint.select_ID[j] = drawable_ID;

      combo = gimp_drawable_combo_box_new (check_gray, NULL);
      gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                                  composeint.select_ID[j],
                                  G_CALLBACK (gimp_int_combo_box_get_active),
                                  &composeint.select_ID[j]);

      gtk_table_attach (GTK_TABLE (table), combo, 2, 3, j, j+1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (combo);

      composeint.channel_menu[j] = combo;
    }
  g_free (layer_list);

  /* Set sensitivity of last menu */
  gtk_widget_set_sensitive (composeint.channel_menu[3],
                            compose_dsc[compose_idx].channel_name[3] != NULL);

  /* Compose types */
  group = NULL;
  for (j = 0; j < G_N_ELEMENTS (compose_dsc); j++)
    {
      toggle = gtk_radio_button_new_with_label (group,
						gettext (compose_dsc[j].compose_type));
      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
      gtk_box_pack_start (GTK_BOX (left_vbox), toggle, TRUE, TRUE, 0);
      composeint.compose_flag[j] = (j == compose_idx);
      gtk_widget_show (toggle);

      g_signal_connect (toggle, "toggled",
                        GTK_SIGNAL_FUNC (compose_type_toggle_update),
                        &(composeint.compose_flag[j]));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    composeint.compose_flag[j]);
    }

  gtk_widget_show (left_vbox);
  gtk_widget_show (right_vbox);
  gtk_widget_show (left_frame);
  gtk_widget_show (right_frame);
  gtk_widget_show (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dlg);

  if (run)
    {
      gint j;

      for (j = 0; j < MAX_COMPOSE_IMAGES; j++)
        composevals.compose_ID[j] = composeint.select_ID[j];

      for (j = 0; j < G_N_ELEMENTS (compose_dsc); j++)
        {
          if (composeint.compose_flag[j])
            {
              strcpy (composevals.compose_type, compose_dsc[j].compose_type);
              break;
            }
        }
    }

  return run;
}

/*  Compose interface functions  */

static gboolean
check_gray (gint32   image_id,
            gint32   drawable_id,
            gpointer data)

{
  return ((gimp_image_base_type (image_id) == GIMP_GRAY) &&
	  (gimp_image_width  (image_id) == composeint.width) &&
	  (gimp_image_height (image_id) == composeint.height));
}

static void
compose_type_toggle_update (GtkWidget *widget,
                            gpointer   data)
{
  gboolean *toggle_val;
  gint      compose_idx, j;

  toggle_val = (gboolean *) data;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    {
      const gchar *text;

      *toggle_val = TRUE;

      compose_idx = toggle_val - &(composeint.compose_flag[0]);

      for (j = 0; j < MAX_COMPOSE_IMAGES; j++)
        {
          text = (compose_dsc[compose_idx].channel_name[j] ?
                  gettext (compose_dsc[compose_idx].channel_name[j]) : NULL);

          gtk_label_set_text (GTK_LABEL (composeint.channel_label[j]), text);

          gtk_image_set_from_stock (GTK_IMAGE (composeint.channel_icon[j]),
                                    compose_dsc[compose_idx].channel_icon[j],
                                    GTK_ICON_SIZE_BUTTON);
        }

      /* Set sensitivity of last menu */
      gtk_widget_set_sensitive (composeint.channel_menu[3],
                                compose_dsc[compose_idx].channel_name[3] != NULL);
    }
  else
    {
      *toggle_val = FALSE;
    }
}