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

#include "config.h"

#include <glib-object.h>

#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/tile-manager.h"

#include "paint-funcs/paint-funcs.h"

#include "gimpimage.h"
#include "gimplayer.h"
#include "gimplayer-floating-sel.h"
#include "gimplayermask.h"
#include "gimplist.h"
#include "gimpprojection.h"
#include "gimpprojection-construct.h"


/*  local function prototypes  */

static void   gimp_projection_construct_layers   (GimpProjection *proj,
                                                  gint            x,
                                                  gint            y,
                                                  gint            w,
                                                  gint            h);
static void   gimp_projection_construct_channels (GimpProjection *proj,
                                                  gint            x,
                                                  gint            y,
                                                  gint            w,
                                                  gint            h);
static void   gimp_projection_initialize         (GimpProjection *proj,
                                                  gint            x,
                                                  gint            y,
                                                  gint            w,
                                                  gint            h);

static void   project_intensity                  (GimpProjection *proj,
                                                  GimpLayer      *layer,
                                                  PixelRegion    *src,
                                                  PixelRegion    *dest,
                                                  PixelRegion    *mask);
static void   project_intensity_alpha            (GimpProjection *proj,
                                                  GimpLayer      *layer,
                                                  PixelRegion    *src,
                                                  PixelRegion    *dest,
                                                  PixelRegion    *mask);
static void   project_indexed                    (GimpProjection *proj,
                                                  GimpLayer      *layer,
                                                  PixelRegion    *src,
                                                  PixelRegion    *dest);
static void   project_indexed_alpha              (GimpProjection *proj,
                                                  GimpLayer      *layer,
                                                  PixelRegion    *src,
                                                  PixelRegion    *dest,
                                                  PixelRegion    *mask);
static void   project_channel                    (GimpProjection *proj,
                                                  GimpChannel    *channel,
                                                  PixelRegion    *src,
                                                  PixelRegion    *src2);


/*  public functions  */

void
gimp_projection_construct (GimpProjection *proj,
                           gint            x,
                           gint            y,
                           gint            w,
                           gint            h)
{
  g_return_if_fail (GIMP_IS_PROJECTION (proj));

#if 0
  gint xoff;
  gint yoff;

  /*  set the construct flag, used to determine if anything
   *  has been written to the gimage raw image yet.
   */
  gimage->construct_flag = FALSE;

  if (gimage->layers)
    {
      gimp_item_offsets (GIMP_ITEM (gimage->layers->data), &xoff, &yoff);
    }

  if ((gimage->layers) &&                         /* There's a layer.      */
      (! g_slist_next (gimage->layers)) &&        /* It's the only layer.  */
      (gimp_drawable_has_alpha (GIMP_DRAWABLE (gimage->layers->data))) &&
                                                  /* It's !flat.           */
      (gimp_item_get_visible (GIMP_ITEM (gimage->layers->data))) &&
                                                  /* It's visible.         */
      (gimp_item_width  (GIMP_ITEM (gimage->layers->data)) ==
       gimage->width) &&
      (gimp_item_height (GIMP_ITEM (gimage->layers->data)) ==
       gimage->height) &&                         /* Covers all.           */
      (!gimp_drawable_is_indexed (GIMP_DRAWABLE (gimage->layers->data))) &&
                                                  /* Not indexed.          */
      (((GimpLayer *)(gimage->layers->data))->opacity == GIMP_OPACITY_OPAQUE)
                                                  /* Opaque                */
      )
    {
      gint xoff;
      gint yoff;

      gimp_item_offsets (GIMP_ITEM (gimage->layers->data), &xoff, &yoff);

      if ((xoff==0) && (yoff==0)) /* Starts at 0,0         */
	{
	  PixelRegion srcPR, destPR;
	  gpointer    pr;

	  g_warning("Can use cow-projection hack.  Yay!");

	  pixel_region_init (&srcPR, gimp_drawable_data
			     (GIMP_DRAWABLE (gimage->layers->data)),
			     x, y, w,h, FALSE);
	  pixel_region_init (&destPR,
			     gimp_image_projection (gimage),
			     x, y, w,h, TRUE);

	  for (pr = pixel_regions_register (2, &srcPR, &destPR);
	       pr != NULL;
	       pr = pixel_regions_process (pr))
	    {
	      tile_manager_map_over_tile (destPR.tiles,
					  destPR.curtile, srcPR.curtile);
	    }

	  gimage->construct_flag = TRUE;
	  gimp_image_construct_channels (gimage, x, y, w, h);

	  return;
	}
    }
#else
  proj->construct_flag = FALSE;
#endif

  /*  First, determine if the projection image needs to be
   *  initialized--this is the case when there are no visible
   *  layers that cover the entire canvas--either because layers
   *  are offset or only a floating selection is visible
   */
  gimp_projection_initialize (proj, x, y, w, h);

  /*  call functions which process the list of layers and
   *  the list of channels
   */
  gimp_projection_construct_layers (proj, x, y, w, h);
  gimp_projection_construct_channels (proj, x, y, w, h);
}


/*  private functions  */

static void
gimp_projection_construct_layers (GimpProjection *proj,
                                  gint            x,
                                  gint            y,
                                  gint            w,
                                  gint            h)
{
  GimpLayer   *layer;
  gint         x1, y1, x2, y2;
  PixelRegion  src1PR, src2PR, maskPR;
  PixelRegion * mask;
  GList       *list;
  GList       *reverse_list;
  gint         off_x;
  gint         off_y;

  /*  composite the floating selection if it exists  */
  if ((layer = gimp_image_floating_sel (proj->gimage)))
    floating_sel_composite (layer, x, y, w, h, FALSE);

  reverse_list = NULL;

  for (list = GIMP_LIST (proj->gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      /*  only add layers that are visible and not floating selections
       *  to the list
       */
      if (! gimp_layer_is_floating_sel (layer) &&
	  gimp_item_get_visible (GIMP_ITEM (layer)))
	{
	  reverse_list = g_list_prepend (reverse_list, layer);
	}
    }

  for (list = reverse_list; list; list = g_list_next (list))
    {
      layer = (GimpLayer *) list->data;

      gimp_item_offsets (GIMP_ITEM (layer), &off_x, &off_y);

      x1 = CLAMP (off_x, x, x + w);
      y1 = CLAMP (off_y, y, y + h);
      x2 = CLAMP (off_x + gimp_item_width  (GIMP_ITEM (layer)), x, x + w);
      y2 = CLAMP (off_y + gimp_item_height (GIMP_ITEM (layer)), y, y + h);

      /* configure the pixel regions  */
      pixel_region_init (&src1PR, gimp_projection_get_tiles (proj),
			 x1, y1, (x2 - x1), (y2 - y1),
			 TRUE);

      /*  If we're showing the layer mask instead of the layer...  */
      if (layer->mask && layer->mask->show_mask)
	{
	  pixel_region_init (&src2PR,
			     gimp_drawable_data (GIMP_DRAWABLE (layer->mask)),
			     (x1 - off_x), (y1 - off_y),
			     (x2 - x1), (y2 - y1), FALSE);

	  copy_gray_to_region (&src2PR, &src1PR);
	}
      /*  Otherwise, normal  */
      else
	{
	  pixel_region_init (&src2PR,
			     gimp_drawable_data (GIMP_DRAWABLE (layer)),
			     (x1 - off_x), (y1 - off_y),
			     (x2 - x1), (y2 - y1), FALSE);

	  if (layer->mask && layer->mask->apply_mask)
	    {
	      pixel_region_init (&maskPR,
				 gimp_drawable_data (GIMP_DRAWABLE (layer->mask)),
				 (x1 - off_x), (y1 - off_y),
				 (x2 - x1), (y2 - y1), FALSE);
	      mask = &maskPR;
	    }
	  else
	    mask = NULL;

	  /*  Based on the type of the layer, project the layer onto the
	   *  projection image...
	   */
	  switch (gimp_drawable_type (GIMP_DRAWABLE (layer)))
	    {
	    case GIMP_RGB_IMAGE: case GIMP_GRAY_IMAGE:
	      /* no mask possible */
	      project_intensity (proj, layer, &src2PR, &src1PR, mask);
	      break;

	    case GIMP_RGBA_IMAGE: case GIMP_GRAYA_IMAGE:
	      project_intensity_alpha (proj, layer, &src2PR, &src1PR, mask);
	      break;

	    case GIMP_INDEXED_IMAGE:
	      /* no mask possible */
	      project_indexed (proj, layer, &src2PR, &src1PR);
	      break;

	    case GIMP_INDEXEDA_IMAGE:
	      project_indexed_alpha (proj, layer, &src2PR, &src1PR, mask);
	      break;

	    default:
	      break;
	    }
	}

      proj->construct_flag = TRUE;  /*  something was projected  */
    }

  g_list_free (reverse_list);
}

static void
gimp_projection_construct_channels (GimpProjection *proj,
                                    gint            x,
                                    gint            y,
                                    gint            w,
                                    gint            h)
{
  GimpChannel *channel;
  PixelRegion  src1PR;
  PixelRegion  src2PR;
  GList       *list;
  GList       *reverse_list = NULL;

  /*  reverse the channel list  */
  for (list = GIMP_LIST (proj->gimage->channels)->list;
       list;
       list = g_list_next (list))
    {
      reverse_list = g_list_prepend (reverse_list, list->data);
    }

  for (list = reverse_list; list; list = g_list_next (list))
    {
      channel = (GimpChannel *) list->data;

      if (gimp_item_get_visible (GIMP_ITEM (channel)))
	{
	  /* configure the pixel regions  */
	  pixel_region_init (&src1PR,
			     gimp_projection_get_tiles (proj),
			     x, y, w, h,
			     TRUE);
	  pixel_region_init (&src2PR,
			     gimp_drawable_data (GIMP_DRAWABLE (channel)),
			     x, y, w, h,
			     FALSE);

	  project_channel (proj, channel, &src1PR, &src2PR);

	  proj->construct_flag = TRUE;
	}
    }

  g_list_free (reverse_list);
}

static void
gimp_projection_initialize (GimpProjection *proj,
                            gint            x,
                            gint            y,
                            gint            w,
                            gint            h)
{

  GList    *list;
  gboolean  coverage = FALSE;

  /*  this function determines whether a visible layer
   *  provides complete coverage over the image.  If not,
   *  the projection is initialized to transparent
   */

  for (list = GIMP_LIST (proj->gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      GimpItem *item = list->data;
      gint      off_x, off_y;

      gimp_item_offsets (item, &off_x, &off_y);

      if (gimp_item_get_visible (item)                     &&
	  ! gimp_drawable_has_alpha (GIMP_DRAWABLE (item)) &&
	  (off_x <= x)                                     &&
	  (off_y <= y)                                     &&
	  (off_x + gimp_item_width  (item) >= x + w)       &&
	  (off_y + gimp_item_height (item) >= y + h))
	{
	  coverage = TRUE;
	  break;
	}
    }

  if (!coverage)
    {
      PixelRegion PR;
      guchar      clear[4] = { 0, 0, 0, 0 };

      pixel_region_init (&PR, gimp_projection_get_tiles (proj),
			 x, y, w, h, TRUE);
      color_region (&PR, clear);
    }
}

static void
project_intensity (GimpProjection *proj,
		   GimpLayer      *layer,
		   PixelRegion    *src,
		   PixelRegion    *dest,
		   PixelRegion    *mask)
{
  if (! proj->construct_flag)
    initial_region (src, dest, mask, NULL,
                    layer->opacity * 255.999,
                    layer->mode,
                    proj->gimage->visible,
                    INITIAL_INTENSITY);
  else
    combine_regions (dest, src, dest, mask, NULL,
                     layer->opacity * 255.999,
                     layer->mode,
                     proj->gimage->visible,
                     COMBINE_INTEN_A_INTEN);
}

static void
project_intensity_alpha (GimpProjection *proj,
			 GimpLayer      *layer,
			 PixelRegion    *src,
			 PixelRegion    *dest,
			 PixelRegion    *mask)
{
  if (! proj->construct_flag)
    initial_region (src, dest, mask, NULL,
                    layer->opacity * 255.999,
                    layer->mode,
                    proj->gimage->visible,
                    INITIAL_INTENSITY_ALPHA);
  else
    combine_regions (dest, src, dest, mask, NULL,
                     layer->opacity * 255.999,
                     layer->mode,
                     proj->gimage->visible,
                     COMBINE_INTEN_A_INTEN_A);
}

static void
project_indexed (GimpProjection *proj,
		 GimpLayer      *layer,
		 PixelRegion    *src,
		 PixelRegion    *dest)
{
  g_return_if_fail (proj->gimage->cmap != NULL);

  if (! proj->construct_flag)
    initial_region (src, dest, NULL, proj->gimage->cmap,
                    layer->opacity * 255.999,
                    layer->mode,
                    proj->gimage->visible,
                    INITIAL_INDEXED);
  else
    g_warning ("%s: unable to project indexed image.", G_STRFUNC);
}

static void
project_indexed_alpha (GimpProjection *proj,
		       GimpLayer      *layer,
		       PixelRegion    *src,
		       PixelRegion    *dest,
		       PixelRegion    *mask)
{
  g_return_if_fail (proj->gimage->cmap != NULL);

  if (! proj->construct_flag)
    initial_region (src, dest, mask, proj->gimage->cmap,
                    layer->opacity * 255.999,
                    layer->mode,
                    proj->gimage->visible,
                    INITIAL_INDEXED_ALPHA);
  else
    combine_regions (dest, src, dest, mask, proj->gimage->cmap,
                     layer->opacity * 255.999,
                     layer->mode,
                     proj->gimage->visible,
                     COMBINE_INTEN_A_INDEXED_A);
}

static void
project_channel (GimpProjection *proj,
		 GimpChannel    *channel,
		 PixelRegion    *src,
		 PixelRegion    *src2)
{
  guchar  col[3];
  guchar  opacity;
  gint    type;

  gimp_rgba_get_uchar (&channel->color,
		       &col[0], &col[1], &col[2], &opacity);

  if (! proj->construct_flag)
    {
      type = (channel->show_masked) ?
	INITIAL_CHANNEL_MASK : INITIAL_CHANNEL_SELECTION;

      initial_region (src2, src, NULL, col,
                      opacity,
                      GIMP_NORMAL_MODE,
                      NULL,
                      type);
    }
  else
    {
      type = (channel->show_masked) ?
	COMBINE_INTEN_A_CHANNEL_MASK : COMBINE_INTEN_A_CHANNEL_SELECTION;

      combine_regions (src, src2, src, NULL, col,
                       opacity,
                       GIMP_NORMAL_MODE,
                       NULL,
                       type);
    }
}