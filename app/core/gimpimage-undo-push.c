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

#include <string.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "config/gimpconfig.h"
#include "config/gimpconfig-utils.h"

#include "base/pixel-region.h"
#include "base/tile-manager.h"

#include "paint-funcs/paint-funcs.h"

#include "gimp-parasites.h"
#include "gimp.h"
#include "gimpgrid.h"
#include "gimpimage-colormap.h"
#include "gimpimage-grid.h"
#include "gimpimage-guides.h"
#include "gimpimage-undo.h"
#include "gimpimage.h"
#include "gimpitemundo.h"
#include "gimplayer-floating-sel.h"
#include "gimplayer.h"
#include "gimplayermask.h"
#include "gimplist.h"
#include "gimpparasitelist.h"
#include "gimpselection.h"

#include "text/gimptextlayer.h"
#include "text/gimptextundo.h"

#include "vectors/gimpvectors.h"

#include "gimp-intl.h"


/*********************/
/*  Image Type Undo  */
/*********************/

typedef struct _ImageTypeUndo ImageTypeUndo;

struct _ImageTypeUndo
{
  GimpImageBaseType base_type;
};

static gboolean undo_pop_image_type  (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode,
                                      GimpUndoAccumulator *accum);
static void     undo_free_image_type (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_type (GimpImage   *gimage,
                                 const gchar *undo_desc)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ImageTypeUndo),
                                   sizeof (ImageTypeUndo),
                                   GIMP_UNDO_IMAGE_TYPE, undo_desc,
                                   GIMP_DIRTY_IMAGE,
                                   undo_pop_image_type,
                                   undo_free_image_type,
                                   NULL)))
    {
      ImageTypeUndo *itu = new->data;

      itu->base_type = gimage->base_type;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_type (GimpUndo            *undo,
                     GimpUndoMode         undo_mode,
                     GimpUndoAccumulator *accum)
{
  ImageTypeUndo     *itu = undo->data;
  GimpImageBaseType  tmp;

  tmp = itu->base_type;
  itu->base_type = undo->gimage->base_type;
  g_object_set (undo->gimage, "base-type", tmp, NULL);

  gimp_image_colormap_changed (undo->gimage, -1);

  if (itu->base_type != undo->gimage->base_type)
    accum->mode_changed = TRUE;

  return TRUE;
}

static void
undo_free_image_type (GimpUndo     *undo,
                      GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/*********************/
/*  Image Size Undo  */
/*********************/

typedef struct _ImageSizeUndo ImageSizeUndo;

struct _ImageSizeUndo
{
  gint width;
  gint height;
};

static gboolean undo_pop_image_size  (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode,
                                      GimpUndoAccumulator *accum);
static void     undo_free_image_size (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_size (GimpImage   *gimage,
                                 const gchar *undo_desc)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ImageSizeUndo),
                                   sizeof (ImageSizeUndo),
                                   GIMP_UNDO_IMAGE_SIZE, undo_desc,
                                   GIMP_DIRTY_IMAGE | GIMP_DIRTY_IMAGE_SIZE,
                                   undo_pop_image_size,
                                   undo_free_image_size,
                                   NULL)))
    {
      ImageSizeUndo *isu = new->data;

      isu->width  = gimage->width;
      isu->height = gimage->height;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_size (GimpUndo            *undo,
                     GimpUndoMode         undo_mode,
                     GimpUndoAccumulator *accum)
{
  ImageSizeUndo *isu = undo->data;
  gint           width;
  gint           height;

  width  = isu->width;
  height = isu->height;

  isu->width  = undo->gimage->width;
  isu->height = undo->gimage->height;

  g_object_set (undo->gimage,
                "width",  width,
                "height", height,
                NULL);

  gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (gimp_image_get_mask (undo->gimage)));

  if (undo->gimage->width  != isu->width ||
      undo->gimage->height != isu->height)
    accum->size_changed = TRUE;

  return TRUE;
}

static void
undo_free_image_size (GimpUndo      *undo,
                      GimpUndoMode   undo_mode)
{
  g_free (undo->data);
}


/***************************/
/*  Image Resolution Undo  */
/***************************/

typedef struct _ResolutionUndo ResolutionUndo;

struct _ResolutionUndo
{
  gdouble   xres;
  gdouble   yres;
  GimpUnit  unit;
};

static gboolean undo_pop_image_resolution  (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode,
                                            GimpUndoAccumulator *accum);
static void     undo_free_image_resolution (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_resolution (GimpImage   *gimage,
                                       const gchar *undo_desc)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ResolutionUndo),
                                   sizeof (ResolutionUndo),
                                   GIMP_UNDO_IMAGE_RESOLUTION, undo_desc,
                                   GIMP_DIRTY_IMAGE,
                                   undo_pop_image_resolution,
                                   undo_free_image_resolution,
                                   NULL)))
    {
      ResolutionUndo *ru = new->data;

      ru->xres = gimage->xresolution;
      ru->yres = gimage->yresolution;
      ru->unit = gimage->resolution_unit;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_resolution (GimpUndo            *undo,
                           GimpUndoMode         undo_mode,
                           GimpUndoAccumulator *accum)
{
  ResolutionUndo *ru = undo->data;

  if (ABS (ru->xres - undo->gimage->xresolution) >= 1e-5 ||
      ABS (ru->yres - undo->gimage->yresolution) >= 1e-5)
    {
      gdouble xres;
      gdouble yres;

      xres = undo->gimage->xresolution;
      yres = undo->gimage->yresolution;

      undo->gimage->xresolution = ru->xres;
      undo->gimage->yresolution = ru->yres;

      ru->xres = xres;
      ru->yres = yres;

      accum->resolution_changed = TRUE;
    }

  if (ru->unit != undo->gimage->resolution_unit)
    {
      GimpUnit unit;

      unit = undo->gimage->resolution_unit;
      undo->gimage->resolution_unit = ru->unit;
      ru->unit = unit;

      accum->unit_changed = TRUE;
    }

  return TRUE;
}

static void
undo_free_image_resolution (GimpUndo      *undo,
                            GimpUndoMode   undo_mode)
{
  g_free (undo->data);
}


/****************/
/*  Grid Undo   */
/****************/

typedef struct _GridUndo GridUndo;

struct _GridUndo
{
  GimpGrid *grid;
};

static gboolean undo_pop_image_grid  (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode,
                                      GimpUndoAccumulator *accum);
static void     undo_free_image_grid (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_grid (GimpImage   *gimage,
                                 const gchar *undo_desc,
                                 GimpGrid    *grid)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_GRID (grid), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (GridUndo),
                                   sizeof (GridUndo),
                                   GIMP_UNDO_IMAGE_GRID, undo_desc,
                                   GIMP_DIRTY_IMAGE_META,
                                   undo_pop_image_grid,
                                   undo_free_image_grid,
                                   NULL)))
    {
      GridUndo *gu = new->data;

      gu->grid = gimp_config_duplicate (GIMP_CONFIG (grid));

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_grid (GimpUndo            *undo,
                     GimpUndoMode         undo_mode,
                     GimpUndoAccumulator *accum)
{
  GridUndo *gu = undo->data;
  GimpGrid *grid;

  grid = gimp_config_duplicate (GIMP_CONFIG (undo->gimage->grid));

  gimp_image_set_grid (undo->gimage, gu->grid, FALSE);

  g_object_unref (gu->grid);
  gu->grid = grid;

  return TRUE;
}

static void
undo_free_image_grid (GimpUndo     *undo,
                      GimpUndoMode  undo_mode)
{
  GridUndo *gu = undo->data;

  if (gu->grid)
    g_object_unref (gu->grid);

  g_free (gu);
}


/****************/
/*  Guide Undo  */
/****************/

typedef struct _GuideUndo GuideUndo;

struct _GuideUndo
{
  GimpGuide           *guide;
  gint                 position;
  GimpOrientationType  orientation;
};

static gboolean undo_pop_image_guide  (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode,
                                       GimpUndoAccumulator *accum);
static void     undo_free_image_guide (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_guide (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpGuide   *guide)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (guide != NULL, FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (GuideUndo),
                                   sizeof (GuideUndo),
                                   GIMP_UNDO_IMAGE_GUIDE, undo_desc,
                                   GIMP_DIRTY_IMAGE_META,
                                   undo_pop_image_guide,
                                   undo_free_image_guide,
                                   NULL)))
    {
      GuideUndo *gu = new->data;

      gu->guide       = gimp_image_guide_ref (guide);
      gu->position    = guide->position;
      gu->orientation = guide->orientation;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_guide (GimpUndo            *undo,
                      GimpUndoMode         undo_mode,
                      GimpUndoAccumulator *accum)
{
  GuideUndo           *gu = undo->data;
  gint                 old_position;
  GimpOrientationType  old_orientation;

  old_position    = gu->guide->position;
  old_orientation = gu->guide->orientation;

  if (gu->guide->position == -1)
    {
      undo->gimage->guides = g_list_prepend (undo->gimage->guides, gu->guide);
      gu->guide->position = gu->position;
      gimp_image_guide_ref (gu->guide);
      gimp_image_update_guide (undo->gimage, gu->guide);
    }
  else if (gu->position == -1)
    {
      gimp_image_remove_guide (undo->gimage, gu->guide, FALSE);
    }
  else
    {
      gimp_image_update_guide (undo->gimage, gu->guide);
      gu->guide->position = gu->position;
      gimp_image_update_guide (undo->gimage, gu->guide);
    }

  gu->guide->orientation = gu->orientation;

  gu->position    = old_position;
  gu->orientation = old_orientation;

  return TRUE;
}

static void
undo_free_image_guide (GimpUndo     *undo,
                       GimpUndoMode  undo_mode)
{
  GuideUndo *gu = undo->data;

  gimp_image_guide_unref (gu->guide);
  g_free (gu);
}


/*******************/
/*  Colormap Undo  */
/*******************/

typedef struct _ColormapUndo ColormapUndo;

struct _ColormapUndo
{
  gint    num_colors;
  guchar *cmap;
};

static gboolean undo_pop_image_colormap  (GimpUndo            *undo,
                                          GimpUndoMode         undo_mode,
                                          GimpUndoAccumulator *accum);
static void     undo_free_image_colormap (GimpUndo            *undo,
                                          GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_colormap (GimpImage   *gimage,
                                     const gchar *undo_desc)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ColormapUndo),
                                   sizeof (ColormapUndo),
                                   GIMP_UNDO_IMAGE_COLORMAP, undo_desc,
                                   GIMP_DIRTY_IMAGE,
                                   undo_pop_image_colormap,
                                   undo_free_image_colormap,
                                   NULL)))
    {
      ColormapUndo *cu = new->data;

      cu->num_colors = gimp_image_get_colormap_size (gimage);
      cu->cmap       = g_memdup (gimp_image_get_colormap (gimage),
                                 cu->num_colors * 3);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_colormap (GimpUndo            *undo,
                         GimpUndoMode         undo_mode,
                         GimpUndoAccumulator *accum)
{
  ColormapUndo *cu = undo->data;
  guchar       *cmap;
  gint          num_colors;

  num_colors = gimp_image_get_colormap_size (undo->gimage);
  cmap       = g_memdup (gimp_image_get_colormap (undo->gimage),
                         num_colors * 3);

  gimp_image_set_colormap (undo->gimage, cu->cmap, cu->num_colors, FALSE);

  if (cu->cmap)
    g_free (cu->cmap);

  cu->num_colors = num_colors;
  cu->cmap       = cmap;

  return TRUE;
}

static void
undo_free_image_colormap (GimpUndo     *undo,
                          GimpUndoMode  undo_mode)
{
  ColormapUndo *cu = undo->data;

  if (cu->cmap)
    g_free (cu->cmap);

  g_free (cu);
}


/*******************/
/*  Drawable Undo  */
/*******************/

typedef struct _DrawableUndo DrawableUndo;

struct _DrawableUndo
{
  TileManager *tiles;
  gboolean     sparse;
  gint         x, y, width, height;
};

static gboolean undo_pop_drawable  (GimpUndo            *undo,
                                    GimpUndoMode         undo_mode,
                                    GimpUndoAccumulator *accum);
static void     undo_free_drawable (GimpUndo            *undo,
                                    GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_drawable (GimpImage    *gimage,
                               const gchar  *undo_desc,
                               GimpDrawable *drawable,
                               TileManager  *tiles,
                               gboolean      sparse,
                               gint          x,
                               gint          y,
                               gint          width,
                               gint          height)
{
  GimpItem *item;
  GimpUndo *new;
  gint64    size;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);
  g_return_val_if_fail (tiles != NULL, FALSE);
  g_return_val_if_fail (sparse == TRUE ||
                        tile_manager_width (tiles) == width, FALSE);
  g_return_val_if_fail (sparse == TRUE ||
                        tile_manager_height (tiles) == height, FALSE);

  item = GIMP_ITEM (drawable);

  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);
  g_return_val_if_fail (sparse == FALSE ||
                        tile_manager_width (tiles) == gimp_item_width (item),
                        FALSE);
  g_return_val_if_fail (sparse == FALSE ||
                        tile_manager_height (tiles) == gimp_item_height (item),
                        FALSE);

  size = sizeof (DrawableUndo) + tile_manager_get_memsize (tiles, sparse);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (DrawableUndo),
                                   GIMP_UNDO_DRAWABLE, undo_desc,
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE,
                                   undo_pop_drawable,
                                   undo_free_drawable,
                                   "item", item,
                                   NULL)))
    {
      DrawableUndo *drawable_undo = new->data;

      drawable_undo->tiles  = tile_manager_ref (tiles);
      drawable_undo->sparse = sparse;
      drawable_undo->x      = x;
      drawable_undo->y      = y;
      drawable_undo->width  = width;
      drawable_undo->height = height;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_drawable (GimpUndo            *undo,
                   GimpUndoMode         undo_mode,
                   GimpUndoAccumulator *accum)
{
  DrawableUndo *drawable_undo = undo->data;

  undo->size -= tile_manager_get_memsize (drawable_undo->tiles,
                                          drawable_undo->sparse);

  gimp_drawable_swap_pixels (GIMP_DRAWABLE (GIMP_ITEM_UNDO (undo)->item),
                             drawable_undo->tiles,
                             drawable_undo->sparse,
                             drawable_undo->x,
                             drawable_undo->y,
                             drawable_undo->width,
                             drawable_undo->height);

  undo->size += tile_manager_get_memsize (drawable_undo->tiles,
                                          drawable_undo->sparse);

  return TRUE;
}

static void
undo_free_drawable (GimpUndo     *undo,
                    GimpUndoMode  undo_mode)
{
  DrawableUndo *drawable_undo = undo->data;

  tile_manager_unref (drawable_undo->tiles);
  g_free (drawable_undo);
}


/***********************/
/*  Drawable Mod Undo  */
/***********************/

typedef struct _DrawableModUndo DrawableModUndo;

struct _DrawableModUndo
{
  TileManager   *tiles;
  GimpImageType  type;
  gint           offset_x;
  gint		 offset_y;
};

static gboolean undo_pop_drawable_mod  (GimpUndo            *undo,
                                        GimpUndoMode         undo_mode,
                                        GimpUndoAccumulator *accum);
static void     undo_free_drawable_mod (GimpUndo            *undo,
                                        GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_drawable_mod (GimpImage    *gimage,
                                   const gchar  *undo_desc,
                                   GimpDrawable *drawable)
{
  GimpUndo *new;
  gint64    size;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (drawable)), FALSE);

  size = sizeof (DrawableModUndo) + tile_manager_get_memsize (drawable->tiles,
                                                              FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (DrawableModUndo),
                                   GIMP_UNDO_DRAWABLE_MOD, undo_desc,
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE,
                                   undo_pop_drawable_mod,
                                   undo_free_drawable_mod,
                                   "item", drawable,
                                   NULL)))
    {
      DrawableModUndo *drawable_undo = new->data;

      drawable_undo->tiles    = tile_manager_ref (drawable->tiles);
      drawable_undo->type     = drawable->type;
      drawable_undo->offset_x = GIMP_ITEM (drawable)->offset_x;
      drawable_undo->offset_y = GIMP_ITEM (drawable)->offset_y;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_drawable_mod (GimpUndo            *undo,
                       GimpUndoMode         undo_mode,
                       GimpUndoAccumulator *accum)
{
  DrawableModUndo *drawable_undo = undo->data;
  GimpDrawable    *drawable      = GIMP_DRAWABLE (GIMP_ITEM_UNDO (undo)->item);
  TileManager     *tiles;
  GimpImageType    drawable_type;
  gint             offset_x, offset_y;

  undo->size -= tile_manager_get_memsize (drawable_undo->tiles, FALSE);

  tiles         = drawable_undo->tiles;
  drawable_type = drawable_undo->type;
  offset_x      = drawable_undo->offset_x;
  offset_y      = drawable_undo->offset_y;

  drawable_undo->tiles    = tile_manager_ref (drawable->tiles);
  drawable_undo->type     = drawable->type;
  drawable_undo->offset_x = GIMP_ITEM (drawable)->offset_x;
  drawable_undo->offset_y = GIMP_ITEM (drawable)->offset_y;

  gimp_drawable_set_tiles_full (drawable, FALSE, NULL,
                                tiles, drawable_type, offset_x, offset_y);
  tile_manager_unref (tiles);

  undo->size += tile_manager_get_memsize (drawable_undo->tiles, FALSE);

  return TRUE;
}

static void
undo_free_drawable_mod (GimpUndo     *undo,
                        GimpUndoMode  undo_mode)
{
  DrawableModUndo *drawable_undo = undo->data;

  tile_manager_unref (drawable_undo->tiles);
  g_free (drawable_undo);
}


/***************/
/*  Mask Undo  */
/***************/

typedef struct _MaskUndo MaskUndo;

struct _MaskUndo
{
  TileManager *tiles;  /*  the actual mask  */
  gint         x, y;   /*  offsets          */
};

static gboolean undo_pop_mask  (GimpUndo            *undo,
                                GimpUndoMode         undo_mode,
                                GimpUndoAccumulator *accum);
static void     undo_free_mask (GimpUndo            *undo,
                                GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_mask (GimpImage   *gimage,
                           const gchar *undo_desc,
                           GimpChannel *mask)
{
  TileManager *undo_tiles = NULL;
  gint         x1, y1, x2, y2;
  GimpUndo    *new;
  gint64       size;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (mask), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (mask)), FALSE);

  size = sizeof (MaskUndo);

  if (gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    {
      GimpDrawable *drawable = GIMP_DRAWABLE (mask);
      PixelRegion   srcPR, destPR;

      undo_tiles = tile_manager_new (x2 - x1, y2 - y1,
                                     gimp_drawable_bytes (drawable));

      pixel_region_init (&srcPR, gimp_drawable_data (drawable),
                         x1, y1, x2 - x1, y2 - y1, FALSE);
      pixel_region_init (&destPR, undo_tiles,
                         0, 0, x2 - x1, y2 - y1, TRUE);

      copy_region (&srcPR, &destPR);

      size += tile_manager_get_memsize (undo_tiles, FALSE);
    }

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (MaskUndo),
                                   GIMP_UNDO_MASK, undo_desc,
                                   GIMP_IS_SELECTION (mask) ?
                                   GIMP_DIRTY_SELECTION :
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE,
                                   undo_pop_mask,
                                   undo_free_mask,
                                   "item", mask,
                                   NULL)))
    {
      MaskUndo *mask_undo = new->data;

      mask_undo->tiles = undo_tiles;
      mask_undo->x     = x1;
      mask_undo->y     = y1;

      return TRUE;
    }

  if (undo_tiles)
    tile_manager_unref (undo_tiles);

  return FALSE;
}

static gboolean
undo_pop_mask (GimpUndo            *undo,
               GimpUndoMode         undo_mode,
               GimpUndoAccumulator *accum)
{
  MaskUndo    *mu      = undo->data;
  GimpChannel *channel = GIMP_CHANNEL (GIMP_ITEM_UNDO (undo)->item);
  TileManager *new_tiles;
  PixelRegion  srcPR, destPR;
  gint         x1, y1, x2, y2;
  gint         width  = 0;
  gint         height = 0;
  guchar       empty  = 0;

  if (mu->tiles)
    undo->size -= tile_manager_get_memsize (mu->tiles, FALSE);

  if (gimp_channel_bounds (channel, &x1, &y1, &x2, &y2))
    {
      new_tiles = tile_manager_new ((x2 - x1), (y2 - y1), 1);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles,
                         x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, new_tiles,
			 0, 0, (x2 - x1), (y2 - y1), TRUE);

      copy_region (&srcPR, &destPR);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (channel)->tiles,
			 x1, y1, (x2 - x1), (y2 - y1), TRUE);

      color_region (&srcPR, &empty);
    }
  else
    {
      new_tiles = NULL;
    }

  if (mu->tiles)
    {
      width  = tile_manager_width (mu->tiles);
      height = tile_manager_height (mu->tiles);

      pixel_region_init (&srcPR, mu->tiles,
			 0, 0, width, height, FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE (channel)->tiles,
			 mu->x, mu->y, width, height, TRUE);

      copy_region (&srcPR, &destPR);

      tile_manager_unref (mu->tiles);
    }

  /* invalidate the current bounds and boundary of the mask */
  gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (channel));

  if (mu->tiles)
    {
      channel->empty = FALSE;
      channel->x1    = mu->x;
      channel->y1    = mu->y;
      channel->x2    = mu->x + width;
      channel->y2    = mu->y + height;
    }
  else
    {
      channel->empty = TRUE;
      channel->x1    = 0;
      channel->y1    = 0;
      channel->x2    = GIMP_ITEM (channel)->width;
      channel->y2    = GIMP_ITEM (channel)->height;
    }

  /* we know the bounds */
  channel->bounds_known = TRUE;

  /*  set the new mask undo parameters  */
  mu->tiles = new_tiles;
  mu->x     = x1;
  mu->y     = y1;

  gimp_drawable_update (GIMP_DRAWABLE (channel),
                        0, 0,
                        GIMP_ITEM (channel)->width,
                        GIMP_ITEM (channel)->height);

  if (mu->tiles)
    undo->size += tile_manager_get_memsize (mu->tiles, FALSE);

  return TRUE;
}

static void
undo_free_mask (GimpUndo     *undo,
                GimpUndoMode  undo_mode)
{
  MaskUndo *mu = undo->data;

  if (mu->tiles)
    tile_manager_unref (mu->tiles);

  g_free (mu);
}


/**********************/
/*  Item Rename Undo  */
/**********************/

typedef struct _ItemRenameUndo ItemRenameUndo;

struct _ItemRenameUndo
{
  gchar *name;
};

static gboolean undo_pop_item_rename  (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode,
                                       GimpUndoAccumulator *accum);
static void     undo_free_item_rename (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_item_rename (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpItem    *item)
{
  GimpUndo    *new;
  gint64       size;
  const gchar *name;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  name = gimp_object_get_name (GIMP_OBJECT (item));

  size = sizeof (ItemRenameUndo) + strlen (name) + 1;

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (ItemRenameUndo),
                                   GIMP_UNDO_ITEM_RENAME, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_item_rename,
                                   undo_free_item_rename,
                                   "item", item,
                                   NULL)))
    {
      ItemRenameUndo *iru = new->data;

      iru->name = g_strdup (name);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_item_rename (GimpUndo            *undo,
                      GimpUndoMode         undo_mode,
                      GimpUndoAccumulator *accum)
{
  ItemRenameUndo *iru  = undo->data;
  GimpItem       *item = GIMP_ITEM_UNDO (undo)->item;
  gchar          *tmp;

  undo->size -= strlen (iru->name);

  tmp = g_strdup (gimp_object_get_name (GIMP_OBJECT (item)));
  gimp_object_set_name (GIMP_OBJECT (item), iru->name);
  g_free (iru->name);
  iru->name = tmp;

  undo->size += strlen (iru->name);

  return TRUE;
}

static void
undo_free_item_rename (GimpUndo     *undo,
                       GimpUndoMode  undo_mode)
{
  ItemRenameUndo *iru = undo->data;

  g_free (iru->name);
  g_free (iru);
}


/****************************/
/*  Item displacement Undo  */
/****************************/

typedef struct _ItemDisplaceUndo ItemDisplaceUndo;

struct _ItemDisplaceUndo
{
  gint old_offset_x;
  gint old_offset_y;
};

static gboolean undo_pop_item_displace  (GimpUndo            *undo,
                                         GimpUndoMode         undo_mode,
                                         GimpUndoAccumulator *accum);
static void     undo_free_item_displace (GimpUndo            *undo,
                                         GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_item_displace (GimpImage   *gimage,
                                    const gchar *undo_desc,
                                    GimpItem    *item)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (ItemDisplaceUndo),
                                   sizeof (ItemDisplaceUndo),
                                   GIMP_UNDO_ITEM_DISPLACE, undo_desc,
                                   GIMP_IS_DRAWABLE (item) ?
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE :
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_VECTORS,
                                   undo_pop_item_displace,
                                   undo_free_item_displace,
                                   "item", item,
                                   NULL)))
    {
      ItemDisplaceUndo *idu = new->data;

      gimp_item_offsets (item, &idu->old_offset_x, &idu->old_offset_y);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_item_displace (GimpUndo            *undo,
                        GimpUndoMode         undo_mode,
                        GimpUndoAccumulator *accum)
{
  ItemDisplaceUndo *idu  = undo->data;
  GimpItem         *item = GIMP_ITEM_UNDO (undo)->item;
  gint              offset_x;
  gint              offset_y;

  gimp_item_offsets (item, &offset_x, &offset_y);

  gimp_item_translate (item,
                       idu->old_offset_x - offset_x,
                       idu->old_offset_y - offset_y,
                       FALSE);

  idu->old_offset_x = offset_x;
  idu->old_offset_y = offset_y;

  return TRUE;
}

static void
undo_free_item_displace (GimpUndo     *undo,
                         GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/******************************/
/*  Item Visibility Undo  */
/******************************/

typedef struct _ItemVisibilityUndo ItemVisibilityUndo;

struct _ItemVisibilityUndo
{
  gboolean old_visible;
};

static gboolean undo_pop_item_visibility  (GimpUndo            *undo,
                                           GimpUndoMode         undo_mode,
                                           GimpUndoAccumulator *accum);
static void     undo_free_item_visibility (GimpUndo            *undo,
                                           GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_item_visibility (GimpImage   *gimage,
                                      const gchar *undo_desc,
                                      GimpItem    *item)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (ItemVisibilityUndo),
                                   sizeof (ItemVisibilityUndo),
                                   GIMP_UNDO_ITEM_VISIBILITY, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_item_visibility,
                                   undo_free_item_visibility,
                                   "item", item,
                                   NULL)))
    {
      ItemVisibilityUndo *ivu = new->data;

      ivu->old_visible = gimp_item_get_visible (item);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_item_visibility (GimpUndo            *undo,
                          GimpUndoMode         undo_mode,
                          GimpUndoAccumulator *accum)
{
  ItemVisibilityUndo *ivu  = undo->data;
  GimpItem           *item = GIMP_ITEM_UNDO (undo)->item;
  gboolean            visible;

  visible = gimp_item_get_visible (item);
  gimp_item_set_visible (item, ivu->old_visible, FALSE);
  ivu->old_visible = visible;

  return TRUE;
}

static void
undo_free_item_visibility (GimpUndo     *undo,
                           GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/**********************/
/*  Item linked Undo  */
/**********************/

typedef struct _ItemLinkedUndo ItemLinkedUndo;

struct _ItemLinkedUndo
{
  gboolean old_linked;
};

static gboolean undo_pop_item_linked  (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode,
                                       GimpUndoAccumulator *accum);
static void     undo_free_item_linked (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_item_linked (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpItem    *item)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (ItemLinkedUndo),
                                   sizeof (ItemLinkedUndo),
                                   GIMP_UNDO_ITEM_LINKED, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_item_linked,
                                   undo_free_item_linked,
                                   "item", item,
                                   NULL)))
    {
      ItemLinkedUndo *ilu = new->data;

      ilu->old_linked = gimp_item_get_linked (item);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_item_linked (GimpUndo            *undo,
                      GimpUndoMode         undo_mode,
                      GimpUndoAccumulator *accum)
{
  ItemLinkedUndo *ilu  = undo->data;
  GimpItem       *item = GIMP_ITEM_UNDO (undo)->item;
  gboolean        linked;

  linked = gimp_item_get_linked (item);
  gimp_item_set_linked (item, ilu->old_linked, FALSE);
  ilu->old_linked = linked;

  return TRUE;
}

static void
undo_free_item_linked (GimpUndo     *undo,
                       GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/***************************/
/*  Layer Add/Remove Undo  */
/***************************/

typedef struct _LayerUndo LayerUndo;

struct _LayerUndo
{
  gint       prev_position;   /*  former position in list  */
  GimpLayer *prev_layer;      /*  previous active layer    */
};

static gboolean undo_push_layer (GimpImage           *gimage,
                                 const gchar         *undo_desc,
                                 GimpUndoType         type,
                                 GimpLayer           *layer,
                                 gint                 prev_position,
                                 GimpLayer           *prev_layer);
static gboolean undo_pop_layer  (GimpUndo            *undo,
                                 GimpUndoMode         undo_mode,
                                 GimpUndoAccumulator *accum);
static void     undo_free_layer (GimpUndo            *undo,
                                 GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_layer_add (GimpImage   *gimage,
                                const gchar *undo_desc,
                                GimpLayer   *layer,
                                gint         prev_position,
                                GimpLayer   *prev_layer)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (! gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);
  g_return_val_if_fail (prev_layer == NULL || GIMP_IS_LAYER (prev_layer),
                        FALSE);

  return undo_push_layer (gimage, undo_desc, GIMP_UNDO_LAYER_ADD,
                          layer, prev_position, prev_layer);
}

gboolean
gimp_image_undo_push_layer_remove (GimpImage   *gimage,
                                   const gchar *undo_desc,
                                   GimpLayer   *layer,
                                   gint         prev_position,
                                   GimpLayer   *prev_layer)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);
  g_return_val_if_fail (prev_layer == NULL || GIMP_IS_LAYER (prev_layer),
                        FALSE);

  return undo_push_layer (gimage, undo_desc, GIMP_UNDO_LAYER_REMOVE,
                          layer, prev_position, prev_layer);
}

static gboolean
undo_push_layer (GimpImage    *gimage,
                 const gchar  *undo_desc,
		 GimpUndoType  type,
                 GimpLayer    *layer,
                 gint          prev_position,
                 GimpLayer    *prev_layer)
{
  GimpUndo *new;
  gint64    size;

  size = sizeof (LayerUndo);

  if (type == GIMP_UNDO_LAYER_REMOVE)
    size += gimp_object_get_memsize (GIMP_OBJECT (layer), NULL);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (LayerUndo),
                                   type, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_layer,
                                   undo_free_layer,
                                   "item", layer,
                                   NULL)))
    {
      LayerUndo *lu = new->data;

      lu->prev_position = prev_position;
      lu->prev_layer    = prev_layer;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer (GimpUndo            *undo,
                GimpUndoMode         undo_mode,
                GimpUndoAccumulator *accum)
{
  LayerUndo *lu    = undo->data;
  GimpLayer *layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);
  gboolean   old_has_alpha;

  old_has_alpha = gimp_image_has_alpha (undo->gimage);

  if ((undo_mode       == GIMP_UNDO_MODE_UNDO &&
       undo->undo_type == GIMP_UNDO_LAYER_ADD) ||
      (undo_mode       == GIMP_UNDO_MODE_REDO &&
       undo->undo_type == GIMP_UNDO_LAYER_REMOVE))
    {
      /*  remove layer  */

      undo->size += gimp_object_get_memsize (GIMP_OBJECT (layer), NULL);

      /*  record the current position  */
      lu->prev_position = gimp_image_get_layer_index (undo->gimage, layer);

      gimp_container_remove (undo->gimage->layers, GIMP_OBJECT (layer));
      undo->gimage->layer_stack = g_slist_remove (undo->gimage->layer_stack,
                                                  layer);

      if (gimp_layer_is_floating_sel (layer))
	{
          /*  invalidate the boundary *before* setting the
           *  floating_sel pointer to NULL because the selection's
           *  outline is affected by the floating_sel and won't be
           *  completely cleared otherwise (bug #160247).
           */
          gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (layer));

	  undo->gimage->floating_sel = NULL;

	  /*  activate the underlying drawable  */
	  floating_sel_activate_drawable (layer);

          gimp_image_floating_selection_changed (undo->gimage);
	}
      else if (layer == gimp_image_get_active_layer (undo->gimage))
        {
          if (lu->prev_layer)
            {
              gimp_image_set_active_layer (undo->gimage, lu->prev_layer);
            }
          else if (undo->gimage->layer_stack)
            {
              gimp_image_set_active_layer (undo->gimage,
                                           undo->gimage->layer_stack->data);
            }
          else
            {
              gimp_image_set_active_layer (undo->gimage, NULL);
            }
        }

      gimp_item_removed (GIMP_ITEM (layer));
    }
  else
    {
      /*  restore layer  */

      undo->size -= gimp_object_get_memsize (GIMP_OBJECT (layer), NULL);

      /*  record the active layer  */
      lu->prev_layer = gimp_image_get_active_layer (undo->gimage);

      /*  if this is a floating selection, set the fs pointer  */
      if (gimp_layer_is_floating_sel (layer))
	undo->gimage->floating_sel = layer;

      gimp_container_insert (undo->gimage->layers,
			     GIMP_OBJECT (layer), lu->prev_position);
      gimp_image_set_active_layer (undo->gimage, layer);

      if (gimp_layer_is_floating_sel (layer))
	gimp_image_floating_selection_changed (undo->gimage);

      GIMP_ITEM (layer)->removed = FALSE;

      if (layer->mask)
        GIMP_ITEM (layer->mask)->removed = FALSE;
    }

  if (old_has_alpha != gimp_image_has_alpha (undo->gimage))
    accum->alpha_changed = TRUE;

  return TRUE;
}

static void
undo_free_layer (GimpUndo     *undo,
                 GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/********************************/
/*  Layer Mask Add/Remove Undo  */
/********************************/

typedef struct _LayerMaskUndo LayerMaskUndo;

struct _LayerMaskUndo
{
  GimpLayerMask *mask;
};

static gboolean undo_push_layer_mask (GimpImage           *gimage,
                                      const gchar         *undo_desc,
                                      GimpUndoType         type,
                                      GimpLayer           *layer,
                                      GimpLayerMask       *mask);
static gboolean undo_pop_layer_mask  (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode,
                                      GimpUndoAccumulator *accum);
static void     undo_free_layer_mask (GimpUndo            *undo,
                                      GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_layer_mask_add (GimpImage     *gimage,
                                     const gchar   *undo_desc,
                                     GimpLayer     *layer,
                                     GimpLayerMask *mask)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER_MASK (mask), FALSE);
  g_return_val_if_fail (! gimp_item_is_attached (GIMP_ITEM (mask)), FALSE);

  return undo_push_layer_mask (gimage, undo_desc, GIMP_UNDO_LAYER_MASK_ADD,
                               layer, mask);
}

gboolean
gimp_image_undo_push_layer_mask_remove (GimpImage     *gimage,
                                        const gchar   *undo_desc,
                                        GimpLayer     *layer,
                                        GimpLayerMask *mask)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER_MASK (mask), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (mask)), FALSE);
  g_return_val_if_fail (mask->layer == layer, FALSE);
  g_return_val_if_fail (layer->mask == mask, FALSE);

  return undo_push_layer_mask (gimage, undo_desc, GIMP_UNDO_LAYER_MASK_REMOVE,
                               layer, mask);
}

static gboolean
undo_push_layer_mask (GimpImage     *gimage,
                      const gchar   *undo_desc,
		      GimpUndoType   type,
                      GimpLayer     *layer,
                      GimpLayerMask *mask)
{
  GimpUndo *new;
  gint64    size;

  size = sizeof (LayerMaskUndo);

  if (type == GIMP_UNDO_LAYER_MASK_REMOVE)
    size += gimp_object_get_memsize (GIMP_OBJECT (mask), NULL);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (LayerMaskUndo),
                                   type, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_layer_mask,
                                   undo_free_layer_mask,
                                   "item", layer,
                                   NULL)))
    {
      LayerMaskUndo *lmu = new->data;

      lmu->mask = g_object_ref (mask);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer_mask (GimpUndo            *undo,
                     GimpUndoMode         undo_mode,
                     GimpUndoAccumulator *accum)
{
  LayerMaskUndo *lmu   = undo->data;
  GimpLayer     *layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);

  if ((undo_mode       == GIMP_UNDO_MODE_UNDO &&
       undo->undo_type == GIMP_UNDO_LAYER_MASK_ADD) ||
      (undo_mode       == GIMP_UNDO_MODE_REDO &&
       undo->undo_type == GIMP_UNDO_LAYER_MASK_REMOVE))
    {
      /*  remove layer mask  */

      undo->size += gimp_object_get_memsize (GIMP_OBJECT (lmu->mask), NULL);

      gimp_layer_apply_mask (layer, GIMP_MASK_DISCARD, FALSE);
    }
  else
    {
      /*  restore layer  */

      undo->size -= gimp_object_get_memsize (GIMP_OBJECT (lmu->mask), NULL);

      gimp_layer_add_mask (layer, lmu->mask, FALSE);

      GIMP_ITEM (lmu->mask)->removed = FALSE;
    }

  return TRUE;
}

static void
undo_free_layer_mask (GimpUndo     *undo,
                      GimpUndoMode  undo_mode)
{
  LayerMaskUndo *lmu = undo->data;

  g_object_unref (lmu->mask);
  g_free (lmu);
}


/***************************/
/* Layer re-position Undo  */
/***************************/

typedef struct _LayerRepositionUndo LayerRepositionUndo;

struct _LayerRepositionUndo
{
  gint old_position;
};

static gboolean undo_pop_layer_reposition  (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode,
                                            GimpUndoAccumulator *accum);
static void     undo_free_layer_reposition (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_layer_reposition (GimpImage   *gimage,
                                       const gchar *undo_desc,
                                       GimpLayer   *layer)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (LayerRepositionUndo),
                                   sizeof (LayerRepositionUndo),
                                   GIMP_UNDO_LAYER_REPOSITION, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_layer_reposition,
                                   undo_free_layer_reposition,
                                   "item", layer,
                                   NULL)))
    {
      LayerRepositionUndo *lru = new->data;

      lru->old_position = gimp_image_get_layer_index (gimage, layer);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer_reposition (GimpUndo            *undo,
                           GimpUndoMode         undo_mode,
                           GimpUndoAccumulator *accum)
{
  LayerRepositionUndo *lru   = undo->data;
  GimpLayer           *layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);
  gint                 pos;

  /* what's the layer's current index? */
  pos = gimp_image_get_layer_index (undo->gimage, layer);
  gimp_image_position_layer (undo->gimage, layer, lru->old_position,
                             FALSE, NULL);
  lru->old_position = pos;

  return TRUE;
}

static void
undo_free_layer_reposition (GimpUndo     *undo,
                            GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/***************************/
/*  Layer properties Undo  */
/***************************/

typedef struct _LayerPropertiesUndo LayerPropertiesUndo;

struct _LayerPropertiesUndo
{
  GimpLayerModeEffects old_mode;
  gdouble              old_opacity;
  gboolean             old_preserve_trans;
};

static gboolean undo_push_layer_properties (GimpImage           *gimage,
                                            GimpUndoType         undo_type,
                                            const gchar         *undo_desc,
                                            GimpLayer           *layer);
static gboolean undo_pop_layer_properties  (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode,
                                            GimpUndoAccumulator *accum);
static void     undo_free_layer_properties (GimpUndo            *undo,
                                            GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_layer_mode (GimpImage   *gimage,
                                 const gchar *undo_desc,
                                 GimpLayer   *layer)
{
  return undo_push_layer_properties (gimage, GIMP_UNDO_LAYER_MODE,
                                     undo_desc, layer);
}

gboolean
gimp_image_undo_push_layer_opacity (GimpImage   *gimage,
                                    const gchar *undo_desc,
                                    GimpLayer   *layer)
{
  return undo_push_layer_properties (gimage, GIMP_UNDO_LAYER_OPACITY,
                                     undo_desc, layer);
}

gboolean
gimp_image_undo_push_layer_preserve_trans (GimpImage   *gimage,
                                           const gchar *undo_desc,
                                           GimpLayer   *layer)
{
  return undo_push_layer_properties (gimage, GIMP_UNDO_LAYER_PRESERVE_TRANS,
                                     undo_desc, layer);
}

static gboolean
undo_push_layer_properties (GimpImage    *gimage,
                            GimpUndoType  undo_type,
                            const gchar  *undo_desc,
                            GimpLayer    *layer)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (LayerPropertiesUndo),
                                   sizeof (LayerPropertiesUndo),
                                   undo_type, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_layer_properties,
                                   undo_free_layer_properties,
                                   "item", layer,
                                   NULL)))
    {
      LayerPropertiesUndo *lpu = new->data;

      lpu->old_mode           = gimp_layer_get_mode (layer);
      lpu->old_opacity        = gimp_layer_get_opacity (layer);
      lpu->old_preserve_trans = gimp_layer_get_preserve_trans (layer);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer_properties (GimpUndo            *undo,
                           GimpUndoMode         undo_mode,
                           GimpUndoAccumulator *accum)
{
  LayerPropertiesUndo  *lpu   = undo->data;
  GimpLayer            *layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);

  if (undo->undo_type == GIMP_UNDO_LAYER_MODE)
    {
      GimpLayerModeEffects mode;

      mode = gimp_layer_get_mode (layer);
      gimp_layer_set_mode (layer, lpu->old_mode, FALSE);
      lpu->old_mode = mode;
    }
  else if (undo->undo_type == GIMP_UNDO_LAYER_OPACITY)
    {
      gdouble opacity;

      opacity = gimp_layer_get_opacity (layer);
      gimp_layer_set_opacity (layer, lpu->old_opacity, FALSE);
      lpu->old_opacity = opacity;
    }
  else if (undo->undo_type == GIMP_UNDO_LAYER_PRESERVE_TRANS)
    {
      gboolean preserve_trans;

      preserve_trans = gimp_layer_get_preserve_trans (layer);
      gimp_layer_set_preserve_trans (layer, lpu->old_preserve_trans, FALSE);
      lpu->old_preserve_trans = preserve_trans;
    }

  return TRUE;
}

static void
undo_free_layer_properties (GimpUndo     *undo,
                            GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/*********************/
/*  Text Layer Undo  */
/*********************/

gboolean
gimp_image_undo_push_text_layer (GimpImage        *gimage,
                                 const gchar      *undo_desc,
                                 GimpTextLayer    *layer,
                                 const GParamSpec *pspec)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_TEXT_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_TEXT_UNDO,
                                   0, 0,
                                   GIMP_UNDO_TEXT_LAYER, undo_desc,
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE,
                                   NULL,
                                   NULL,
                                   "item",  layer,
                                   "param", pspec,
                                   NULL)))
    {
      return TRUE;
    }

  return FALSE;
}


/******************************/
/*  Text Layer Modified Undo  */
/******************************/

typedef struct _TextLayerModifiedUndo TextLayerModifiedUndo;

struct _TextLayerModifiedUndo
{
  gboolean old_modified;
};

static gboolean undo_pop_text_layer_modified  (GimpUndo            *undo,
                                               GimpUndoMode         undo_mode,
                                               GimpUndoAccumulator *accum);
static void     undo_free_text_layer_modified (GimpUndo            *undo,
                                               GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_text_layer_modified (GimpImage     *gimage,
                                          const gchar   *undo_desc,
                                          GimpTextLayer *layer)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_TEXT_LAYER (layer), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (layer)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (TextLayerModifiedUndo),
                                   sizeof (TextLayerModifiedUndo),
                                   GIMP_UNDO_TEXT_LAYER_MODIFIED, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_text_layer_modified,
                                   undo_free_text_layer_modified,
                                   "item", layer,
                                   NULL)))
    {
      TextLayerModifiedUndo *modified_undo = new->data;

      modified_undo->old_modified = layer->modified;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_text_layer_modified (GimpUndo            *undo,
                              GimpUndoMode         undo_mode,
                              GimpUndoAccumulator *accum)
{
  TextLayerModifiedUndo *modified_undo = undo->data;
  GimpTextLayer         *layer;
  gboolean               modified;

  layer = GIMP_TEXT_LAYER (GIMP_ITEM_UNDO (undo)->item);

#if 0
  g_print ("setting layer->modified from %s to %s\n",
           layer->modified ? "TRUE" : "FALSE",
           modified_undo->old_modified ? "TRUE" : "FALSE");
#endif

  modified = layer->modified;
  g_object_set (layer, "modified", modified_undo->old_modified, NULL);
  modified_undo->old_modified = modified;

  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (layer));

  return TRUE;
}

static void
undo_free_text_layer_modified (GimpUndo     *undo,
                               GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/*****************************/
/*  Add/Remove Channel Undo  */
/*****************************/

typedef struct _ChannelUndo ChannelUndo;

struct _ChannelUndo
{
  gint         prev_position;   /*  former position in list     */
  GimpChannel *prev_channel;    /*  previous active channel     */
};

static gboolean undo_push_channel (GimpImage           *gimage,
                                   const gchar         *undo_desc,
                                   GimpUndoType         type,
                                   GimpChannel         *channel,
                                   gint                 prev_position,
                                   GimpChannel         *prev_channel);
static gboolean undo_pop_channel  (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode,
                                   GimpUndoAccumulator *accum);
static void     undo_free_channel (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_channel_add (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpChannel *channel,
                                  gint         prev_position,
                                  GimpChannel *prev_channel)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (! gimp_item_is_attached (GIMP_ITEM (channel)), FALSE);
  g_return_val_if_fail (prev_channel == NULL || GIMP_IS_CHANNEL (prev_channel),
                        FALSE);

  return undo_push_channel (gimage, undo_desc, GIMP_UNDO_CHANNEL_ADD,
                            channel, prev_position, prev_channel);
}

gboolean
gimp_image_undo_push_channel_remove (GimpImage   *gimage,
                                     const gchar *undo_desc,
                                     GimpChannel *channel,
                                     gint         prev_position,
                                     GimpChannel *prev_channel)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (channel)), FALSE);
  g_return_val_if_fail (prev_channel == NULL || GIMP_IS_CHANNEL (prev_channel),
                        FALSE);

  return undo_push_channel (gimage, undo_desc, GIMP_UNDO_CHANNEL_REMOVE,
                            channel, prev_position, prev_channel);
}

static gboolean
undo_push_channel (GimpImage    *gimage,
                   const gchar  *undo_desc,
		   GimpUndoType  type,
                   GimpChannel  *channel,
                   gint          prev_position,
                   GimpChannel  *prev_channel)
{
  GimpUndo *new;
  gint64    size;

  size = sizeof (ChannelUndo);

  if (type == GIMP_UNDO_CHANNEL_REMOVE)
    size += gimp_object_get_memsize (GIMP_OBJECT (channel), NULL);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (ChannelUndo),
                                   type, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_channel,
                                   undo_free_channel,
                                   "item", channel,
                                   NULL)))
    {
      ChannelUndo *cu = new->data;

      cu->prev_position = prev_position;
      cu->prev_channel  = prev_channel;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_channel (GimpUndo            *undo,
                  GimpUndoMode         undo_mode,
                  GimpUndoAccumulator *accum)
{
  ChannelUndo *cu      = undo->data;
  GimpChannel *channel = GIMP_CHANNEL (GIMP_ITEM_UNDO (undo)->item);

  if ((undo_mode       == GIMP_UNDO_MODE_UNDO &&
       undo->undo_type == GIMP_UNDO_CHANNEL_ADD) ||
      (undo_mode       == GIMP_UNDO_MODE_REDO &&
       undo->undo_type == GIMP_UNDO_CHANNEL_REMOVE))
    {
      /*  remove channel  */

      undo->size += gimp_object_get_memsize (GIMP_OBJECT (channel), NULL);

      /*  record the current position  */
      cu->prev_position = gimp_image_get_channel_index (undo->gimage,
                                                        channel);

      gimp_container_remove (undo->gimage->channels, GIMP_OBJECT (channel));
      gimp_item_removed (GIMP_ITEM (channel));

      if (channel == gimp_image_get_active_channel (undo->gimage))
        {
          if (cu->prev_channel)
            gimp_image_set_active_channel (undo->gimage, cu->prev_channel);
          else
            gimp_image_unset_active_channel (undo->gimage);
        }
    }
  else
    {
      /*  restore channel  */

      undo->size -= gimp_object_get_memsize (GIMP_OBJECT (channel), NULL);

      /*  record the active channel  */
      cu->prev_channel = gimp_image_get_active_channel (undo->gimage);

      gimp_container_insert (undo->gimage->channels,
			     GIMP_OBJECT (channel), cu->prev_position);
      gimp_image_set_active_channel (undo->gimage, channel);

      GIMP_ITEM (channel)->removed = FALSE;
    }

  return TRUE;
}

static void
undo_free_channel (GimpUndo     *undo,
                   GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/******************************/
/*  Channel re-position Undo  */
/******************************/

typedef struct _ChannelRepositionUndo ChannelRepositionUndo;

struct _ChannelRepositionUndo
{
  gint old_position;
};

static gboolean undo_pop_channel_reposition  (GimpUndo            *undo,
                                              GimpUndoMode         undo_mode,
                                              GimpUndoAccumulator *accum);
static void     undo_free_channel_reposition (GimpUndo            *undo,
                                              GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_channel_reposition (GimpImage   *gimage,
                                         const gchar *undo_desc,
                                         GimpChannel *channel)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (channel)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (ChannelRepositionUndo),
                                   sizeof (ChannelRepositionUndo),
                                   GIMP_UNDO_CHANNEL_REPOSITION, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_channel_reposition,
                                   undo_free_channel_reposition,
                                   "item", channel,
                                   NULL)))
    {
      ChannelRepositionUndo *cru = new->data;

      cru->old_position = gimp_image_get_channel_index (gimage, channel);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_channel_reposition (GimpUndo            *undo,
                             GimpUndoMode         undo_mode,
                             GimpUndoAccumulator *accum)
{
  ChannelRepositionUndo *cru     = undo->data;
  GimpChannel           *channel = GIMP_CHANNEL (GIMP_ITEM_UNDO (undo)->item);
  gint                   pos;

  pos = gimp_image_get_channel_index (undo->gimage, channel);
  gimp_image_position_channel (undo->gimage, channel, cru->old_position,
                               FALSE, NULL);
  cru->old_position = pos;

  return TRUE;
}

static void
undo_free_channel_reposition (GimpUndo     *undo,
                              GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/************************/
/*  Channel color Undo  */
/************************/

typedef struct _ChannelColorUndo ChannelColorUndo;

struct _ChannelColorUndo
{
  GimpRGB old_color;
};

static gboolean undo_pop_channel_color  (GimpUndo            *undo,
                                         GimpUndoMode         undo_mode,
                                         GimpUndoAccumulator *accum);
static void     undo_free_channel_color (GimpUndo            *undo,
                                         GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_channel_color (GimpImage   *gimage,
                                    const gchar *undo_desc,
                                    GimpChannel *channel)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (channel)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (ChannelColorUndo),
                                   sizeof (ChannelColorUndo),
                                   GIMP_UNDO_CHANNEL_COLOR, undo_desc,
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_DRAWABLE,
                                   undo_pop_channel_color,
                                   undo_free_channel_color,
                                   "item", channel,
                                   NULL)))
    {
      ChannelColorUndo *ccu = new->data;

      gimp_channel_get_color (channel , &ccu->old_color);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_channel_color (GimpUndo            *undo,
                        GimpUndoMode         undo_mode,
                        GimpUndoAccumulator *accum)
{
  ChannelColorUndo *ccu     = undo->data;
  GimpChannel      *channel = GIMP_CHANNEL (GIMP_ITEM_UNDO (undo)->item);
  GimpRGB           color;

  gimp_channel_get_color (channel, &color);
  gimp_channel_set_color (channel, &ccu->old_color, FALSE);
  ccu->old_color = color;

  return TRUE;
}

static void
undo_free_channel_color (GimpUndo     *undo,
                         GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/*****************************/
/*  Add/Remove Vectors Undo  */
/*****************************/

typedef struct _VectorsUndo VectorsUndo;

struct _VectorsUndo
{
  gint         prev_position;   /*  former position in list     */
  GimpVectors *prev_vectors;    /*  previous active vectors     */
};

static gboolean undo_push_vectors (GimpImage           *gimage,
                                   const gchar         *undo_desc,
                                   GimpUndoType         type,
                                   GimpVectors         *vectors,
                                   gint                 prev_position,
                                   GimpVectors         *prev_vectors);
static gboolean undo_pop_vectors  (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode,
                                   GimpUndoAccumulator *accum);
static void     undo_free_vectors (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_vectors_add (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpVectors *vectors,
                                  gint         prev_position,
                                  GimpVectors *prev_vectors)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_VECTORS (vectors), FALSE);
  g_return_val_if_fail (! gimp_item_is_attached (GIMP_ITEM (vectors)), FALSE);
  g_return_val_if_fail (prev_vectors == NULL || GIMP_IS_VECTORS (prev_vectors),
                        FALSE);

  return undo_push_vectors (gimage, undo_desc, GIMP_UNDO_VECTORS_ADD,
                            vectors, prev_position, prev_vectors);
}

gboolean
gimp_image_undo_push_vectors_remove (GimpImage   *gimage,
                                     const gchar *undo_desc,
                                     GimpVectors *vectors,
                                     gint         prev_position,
                                     GimpVectors *prev_vectors)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_VECTORS (vectors), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (vectors)), FALSE);
  g_return_val_if_fail (prev_vectors == NULL || GIMP_IS_VECTORS (prev_vectors),
                        FALSE);

  return undo_push_vectors (gimage, undo_desc, GIMP_UNDO_VECTORS_REMOVE,
                            vectors, prev_position, prev_vectors);
}

static gboolean
undo_push_vectors (GimpImage    *gimage,
                   const gchar  *undo_desc,
		   GimpUndoType  type,
                   GimpVectors  *vectors,
                   gint          prev_position,
                   GimpVectors  *prev_vectors)
{
  GimpUndo *new;
  gint64    size;

  size = sizeof (VectorsUndo);

  if (type == GIMP_UNDO_VECTORS_REMOVE)
    size += gimp_object_get_memsize (GIMP_OBJECT (vectors), NULL);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (VectorsUndo),
                                   type, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_vectors,
                                   undo_free_vectors,
                                   "item", vectors,
                                   NULL)))
    {
      VectorsUndo *vu = new->data;

      vu->prev_position = prev_position;
      vu->prev_vectors  = prev_vectors;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_vectors (GimpUndo            *undo,
                  GimpUndoMode         undo_mode,
                  GimpUndoAccumulator *accum)
{
  VectorsUndo *vu      = undo->data;
  GimpVectors *vectors = GIMP_VECTORS (GIMP_ITEM_UNDO (undo)->item);

  if ((undo_mode       == GIMP_UNDO_MODE_UNDO &&
       undo->undo_type == GIMP_UNDO_VECTORS_ADD) ||
      (undo_mode       == GIMP_UNDO_MODE_REDO &&
       undo->undo_type == GIMP_UNDO_VECTORS_REMOVE))
    {
      /*  remove vectors  */

      undo->size += gimp_object_get_memsize (GIMP_OBJECT (vectors), NULL);

      /*  record the current position  */
      vu->prev_position = gimp_image_get_vectors_index (undo->gimage,
                                                        vectors);

      gimp_container_remove (undo->gimage->vectors, GIMP_OBJECT (vectors));
      gimp_item_removed (GIMP_ITEM (vectors));

      if (vectors == gimp_image_get_active_vectors (undo->gimage))
        gimp_image_set_active_vectors (undo->gimage, vu->prev_vectors);
    }
  else
    {
      /*  restore vectors  */

      undo->size -= gimp_object_get_memsize (GIMP_OBJECT (vectors), NULL);

      /*  record the active vectors  */
      vu->prev_vectors = gimp_image_get_active_vectors (undo->gimage);

      gimp_container_insert (undo->gimage->vectors,
			     GIMP_OBJECT (vectors), vu->prev_position);
      gimp_image_set_active_vectors (undo->gimage, vectors);

      GIMP_ITEM (vectors)->removed = FALSE;
    }

  return TRUE;
}

static void
undo_free_vectors (GimpUndo     *undo,
                   GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/**********************/
/*  Vectors Mod Undo  */
/**********************/

typedef struct _VectorsModUndo VectorsModUndo;

struct _VectorsModUndo
{
  GimpVectors *vectors;
};

static gboolean undo_pop_vectors_mod  (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode,
                                       GimpUndoAccumulator *accum);
static void     undo_free_vectors_mod (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_vectors_mod (GimpImage   *gimage,
                                  const gchar *undo_desc,
                                  GimpVectors *vectors)
{
  GimpVectors *copy;
  GimpUndo    *new;
  gint64       size;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_VECTORS (vectors), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (vectors)), FALSE);

  copy = GIMP_VECTORS (gimp_item_duplicate (GIMP_ITEM (vectors),
                                            G_TYPE_FROM_INSTANCE (vectors),
                                            FALSE));

  size = (sizeof (VectorsModUndo) +
          gimp_object_get_memsize (GIMP_OBJECT (copy), NULL));

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   size, sizeof (VectorsModUndo),
                                   GIMP_UNDO_VECTORS_MOD, undo_desc,
                                   GIMP_DIRTY_ITEM | GIMP_DIRTY_VECTORS,
                                   undo_pop_vectors_mod,
                                   undo_free_vectors_mod,
                                   "item", vectors,
                                   NULL)))
    {
      VectorsModUndo *vmu = new->data;

      vmu->vectors = copy;

      return TRUE;
    }

  if (copy)
    g_object_unref (copy);

  return FALSE;
}

static gboolean
undo_pop_vectors_mod (GimpUndo            *undo,
                      GimpUndoMode         undo_mode,
                      GimpUndoAccumulator *accum)
{
  VectorsModUndo *vmu     = undo->data;
  GimpVectors    *vectors = GIMP_VECTORS (GIMP_ITEM_UNDO (undo)->item);
  GimpVectors    *temp;

  undo->size -= gimp_object_get_memsize (GIMP_OBJECT (vmu->vectors), NULL);

  temp = vmu->vectors;

  vmu->vectors =
    GIMP_VECTORS (gimp_item_duplicate (GIMP_ITEM (vectors),
                                       G_TYPE_FROM_INSTANCE (vectors),
                                       FALSE));

  gimp_vectors_freeze (vectors);

  gimp_vectors_copy_strokes (temp, vectors);

  GIMP_ITEM (vectors)->width    = GIMP_ITEM (temp)->width;
  GIMP_ITEM (vectors)->height   = GIMP_ITEM (temp)->height;
  GIMP_ITEM (vectors)->offset_x = GIMP_ITEM (temp)->offset_x;
  GIMP_ITEM (vectors)->offset_y = GIMP_ITEM (temp)->offset_y;

  g_object_unref (temp);

  gimp_vectors_thaw (vectors);

  undo->size += gimp_object_get_memsize (GIMP_OBJECT (vmu->vectors), NULL);

  return TRUE;
}

static void
undo_free_vectors_mod (GimpUndo     *undo,
                       GimpUndoMode  undo_mode)
{
  VectorsModUndo *vmu = undo->data;

  g_object_unref (vmu->vectors);
  g_free (vmu);
}


/******************************/
/*  Vectors re-position Undo  */
/******************************/

typedef struct _VectorsRepositionUndo VectorsRepositionUndo;

struct _VectorsRepositionUndo
{
  gint old_position;
};

static gboolean undo_pop_vectors_reposition  (GimpUndo            *undo,
                                              GimpUndoMode         undo_mode,
                                              GimpUndoAccumulator *accum);
static void     undo_free_vectors_reposition (GimpUndo            *undo,
                                              GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_vectors_reposition (GimpImage   *gimage,
                                         const gchar *undo_desc,
                                         GimpVectors *vectors)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_VECTORS (vectors), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (GIMP_ITEM (vectors)), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   sizeof (VectorsRepositionUndo),
                                   sizeof (VectorsRepositionUndo),
                                   GIMP_UNDO_VECTORS_REPOSITION, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_vectors_reposition,
                                   undo_free_vectors_reposition,
                                   "item", vectors,
                                   NULL)))
    {
      VectorsRepositionUndo *vru = new->data;

      vru->old_position = gimp_image_get_vectors_index (gimage, vectors);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_vectors_reposition (GimpUndo            *undo,
                             GimpUndoMode         undo_mode,
                             GimpUndoAccumulator *accum)
{
  VectorsRepositionUndo *vru     = undo->data;
  GimpVectors           *vectors = GIMP_VECTORS (GIMP_ITEM_UNDO (undo)->item);
  gint                   pos;

  /* what's the vectors's current index? */
  pos = gimp_image_get_vectors_index (undo->gimage, vectors);
  gimp_image_position_vectors (undo->gimage, vectors, vru->old_position,
                               FALSE, NULL);
  vru->old_position = pos;

  return TRUE;
}

static void
undo_free_vectors_reposition (GimpUndo     *undo,
                              GimpUndoMode  undo_mode)
{
  g_free (undo->data);
}


/**************************************/
/*  Floating Selection to Layer Undo  */
/**************************************/

typedef struct _FStoLayerUndo FStoLayerUndo;

struct _FStoLayerUndo
{
  GimpLayer    *floating_layer; /*  the floating layer        */
  GimpDrawable *drawable;       /*  drawable of floating sel  */
};

static gboolean undo_pop_fs_to_layer  (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode,
                                       GimpUndoAccumulator *accum);
static void     undo_free_fs_to_layer (GimpUndo            *undo,
                                       GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_fs_to_layer (GimpImage    *gimage,
                                  const gchar  *undo_desc,
                                  GimpLayer    *floating_layer,
                                  GimpDrawable *drawable)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (floating_layer), FALSE);
  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (FStoLayerUndo),
                                   sizeof (FStoLayerUndo),
                                   GIMP_UNDO_FS_TO_LAYER, undo_desc,
                                   GIMP_DIRTY_IMAGE_STRUCTURE,
                                   undo_pop_fs_to_layer,
                                   undo_free_fs_to_layer,
                                   NULL)))
    {
      FStoLayerUndo *fsu = new->data;

      fsu->floating_layer = floating_layer;
      fsu->drawable       = drawable;

      return TRUE;
    }

  tile_manager_unref (floating_layer->fs.backing_store);
  floating_layer->fs.backing_store = NULL;

  return FALSE;
}

static gboolean
undo_pop_fs_to_layer (GimpUndo            *undo,
                      GimpUndoMode         undo_mode,
                      GimpUndoAccumulator *accum)
{
  FStoLayerUndo *fsu = undo->data;

  switch (undo_mode)
    {
    case GIMP_UNDO_MODE_UNDO:
      /*  Update the preview for the floating sel  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));

      fsu->floating_layer->fs.drawable = fsu->drawable;
      gimp_image_set_active_layer (undo->gimage, fsu->floating_layer);
      undo->gimage->floating_sel = fsu->floating_layer;

      /*  store the contents of the drawable  */
      floating_sel_store (fsu->floating_layer,
                          GIMP_ITEM (fsu->floating_layer)->offset_x,
                          GIMP_ITEM (fsu->floating_layer)->offset_y,
                          GIMP_ITEM (fsu->floating_layer)->width,
                          GIMP_ITEM (fsu->floating_layer)->height);
      fsu->floating_layer->fs.initial = TRUE;

      /*  clear the selection  */
      gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (fsu->floating_layer));
      break;

    case GIMP_UNDO_MODE_REDO:
      /*  restore the contents of the drawable  */
      floating_sel_restore (fsu->floating_layer,
                            GIMP_ITEM (fsu->floating_layer)->offset_x,
                            GIMP_ITEM (fsu->floating_layer)->offset_y,
                            GIMP_ITEM (fsu->floating_layer)->width,
                            GIMP_ITEM (fsu->floating_layer)->height);

      /*  Update the preview for the underlying drawable  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));

      /*  clear the selection  */
      gimp_drawable_invalidate_boundary (GIMP_DRAWABLE (fsu->floating_layer));

      /*  update the pointers  */
      fsu->floating_layer->fs.drawable = NULL;
      undo->gimage->floating_sel       = NULL;
      break;
    }

  gimp_object_name_changed (GIMP_OBJECT (fsu->floating_layer));

  gimp_drawable_update (GIMP_DRAWABLE (fsu->floating_layer),
			0, 0,
			GIMP_ITEM (fsu->floating_layer)->width,
			GIMP_ITEM (fsu->floating_layer)->height);

  gimp_image_floating_selection_changed (undo->gimage);

  return TRUE;
}

static void
undo_free_fs_to_layer (GimpUndo     *undo,
                       GimpUndoMode  undo_mode)
{
  FStoLayerUndo *fsu = undo->data;

  if (undo_mode == GIMP_UNDO_MODE_UNDO)
    {
      tile_manager_unref (fsu->floating_layer->fs.backing_store);
      fsu->floating_layer->fs.backing_store = NULL;
    }

  g_free (fsu);
}


/***********************************/
/*  Floating Selection Rigor Undo  */
/***********************************/

static gboolean undo_pop_fs_rigor (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode,
                                   GimpUndoAccumulator *accum);

gboolean
gimp_image_undo_push_fs_rigor (GimpImage    *gimage,
                               const gchar  *undo_desc,
                               GimpLayer    *floating_layer)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (floating_layer), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   0, 0,
                                   GIMP_UNDO_FS_RIGOR, undo_desc,
                                   GIMP_DIRTY_NONE,
                                   undo_pop_fs_rigor,
                                   NULL,
                                   "item", floating_layer,
                                   NULL)))
    {
      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_fs_rigor (GimpUndo            *undo,
                   GimpUndoMode         undo_mode,
                   GimpUndoAccumulator *accum)
{
  GimpLayer *floating_layer;

  floating_layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);

  if (! gimp_layer_is_floating_sel (floating_layer))
    return FALSE;

  switch (undo_mode)
    {
    case GIMP_UNDO_MODE_UNDO:
      floating_sel_relax (floating_layer, FALSE);
      break;

    case GIMP_UNDO_MODE_REDO:
      floating_sel_rigor (floating_layer, FALSE);
      break;
    }

  return TRUE;
}


/***********************************/
/*  Floating Selection Relax Undo  */
/***********************************/

static gboolean undo_pop_fs_relax (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode,
                                   GimpUndoAccumulator *accum);

gboolean
gimp_image_undo_push_fs_relax (GimpImage   *gimage,
                               const gchar *undo_desc,
                               GimpLayer   *floating_layer)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_LAYER (floating_layer), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_ITEM_UNDO,
                                   0, 0,
                                   GIMP_UNDO_FS_RELAX, undo_desc,
                                   GIMP_DIRTY_NONE,
                                   undo_pop_fs_relax,
                                   NULL,
                                   "item", floating_layer,
                                   NULL)))
    {
      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_fs_relax (GimpUndo            *undo,
                   GimpUndoMode         undo_mode,
                   GimpUndoAccumulator *accum)
{
  GimpLayer *floating_layer;

  floating_layer = GIMP_LAYER (GIMP_ITEM_UNDO (undo)->item);

  if (! gimp_layer_is_floating_sel (floating_layer))
    return FALSE;

  switch (undo_mode)
    {
    case GIMP_UNDO_MODE_UNDO:
      floating_sel_rigor (floating_layer, FALSE);
      break;

    case GIMP_UNDO_MODE_REDO:
      floating_sel_relax (floating_layer, FALSE);
      break;
    }

  return TRUE;
}


/*******************/
/*  Parasite Undo  */
/*******************/

typedef struct _ParasiteUndo ParasiteUndo;

struct _ParasiteUndo
{
  GimpImage    *gimage;
  GimpItem     *item;
  GimpParasite *parasite;
  gchar        *name;
};

static gboolean undo_pop_parasite  (GimpUndo            *undo,
                                    GimpUndoMode         undo_mode,
                                    GimpUndoAccumulator *accum);
static void     undo_free_parasite (GimpUndo            *undo,
                                    GimpUndoMode         undo_mode);

gboolean
gimp_image_undo_push_image_parasite (GimpImage   *gimage,
                                     const gchar *undo_desc,
                                     gpointer     parasite)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ParasiteUndo),
                                   sizeof (ParasiteUndo),
                                   GIMP_UNDO_PARASITE_ATTACH, undo_desc,
                                   GIMP_DIRTY_IMAGE_META,
                                   undo_pop_parasite,
                                   undo_free_parasite,
                                   NULL)))
    {
      ParasiteUndo *pu = new->data;

      pu->gimage   = gimage;
      pu->item     = NULL;
      pu->name     = g_strdup (gimp_parasite_name (parasite));
      pu->parasite = gimp_parasite_copy (gimp_image_parasite_find (gimage,
                                                                   pu->name));

      return TRUE;
    }

  return FALSE;
}

gboolean
gimp_image_undo_push_image_parasite_remove (GimpImage   *gimage,
                                            const gchar *undo_desc,
                                            const gchar *name)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ParasiteUndo),
                                   sizeof (ParasiteUndo),
                                   GIMP_UNDO_PARASITE_REMOVE, undo_desc,
                                   GIMP_DIRTY_IMAGE_META,
                                   undo_pop_parasite,
                                   undo_free_parasite,
                                   NULL)))
    {
      ParasiteUndo *pu = new->data;

      pu->gimage   = gimage;
      pu->item     = NULL;
      pu->name     = g_strdup (name);
      pu->parasite = gimp_parasite_copy (gimp_image_parasite_find (gimage,
                                                                   pu->name));

      return TRUE;
    }

  return FALSE;
}

gboolean
gimp_image_undo_push_item_parasite (GimpImage   *gimage,
                                    const gchar *undo_desc,
                                    GimpItem    *item,
                                    gpointer     parasite)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ParasiteUndo),
                                   sizeof (ParasiteUndo),
                                   GIMP_UNDO_PARASITE_ATTACH, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_parasite,
                                   undo_free_parasite,
                                   NULL)))
    {
      ParasiteUndo *pu = new->data;

      pu->gimage   = NULL;
      pu->item     = item;
      pu->name     = g_strdup (gimp_parasite_name (parasite));
      pu->parasite = gimp_parasite_copy (gimp_item_parasite_find (item,
                                                                  pu->name));
      return TRUE;
    }

  return FALSE;
}

gboolean
gimp_image_undo_push_item_parasite_remove (GimpImage   *gimage,
                                           const gchar *undo_desc,
                                           GimpItem    *item,
                                           const gchar *name)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);
  g_return_val_if_fail (GIMP_IS_ITEM (item), FALSE);
  g_return_val_if_fail (gimp_item_is_attached (item), FALSE);

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   sizeof (ParasiteUndo),
                                   sizeof (ParasiteUndo),
                                   GIMP_UNDO_PARASITE_REMOVE, undo_desc,
                                   GIMP_DIRTY_ITEM_META,
                                   undo_pop_parasite,
                                   undo_free_parasite,
                                   NULL)))
    {
      ParasiteUndo *pu = new->data;

      pu->gimage   = NULL;
      pu->item     = item;
      pu->name     = g_strdup (name);
      pu->parasite = gimp_parasite_copy (gimp_item_parasite_find (item,
                                                                  pu->name));
      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_parasite (GimpUndo            *undo,
                   GimpUndoMode         undo_mode,
                   GimpUndoAccumulator *accum)
{
  ParasiteUndo *pu = undo->data;
  GimpParasite *tmp;

  tmp = pu->parasite;

  if (pu->gimage)
    {
      pu->parasite = gimp_parasite_copy (gimp_image_parasite_find (undo->gimage,
                                                                   pu->name));
      if (tmp)
	gimp_parasite_list_add (pu->gimage->parasites, tmp);
      else
	gimp_parasite_list_remove (pu->gimage->parasites, pu->name);
    }
  else if (pu->item)
    {
      pu->parasite = gimp_parasite_copy (gimp_item_parasite_find (pu->item,
                                                                  pu->name));
      if (tmp)
	gimp_parasite_list_add (pu->item->parasites, tmp);
      else
	gimp_parasite_list_remove (pu->item->parasites, pu->name);
    }
  else
    {
      pu->parasite = gimp_parasite_copy (gimp_parasite_find (undo->gimage->gimp,
                                                             pu->name));
      if (tmp)
	gimp_parasite_attach (undo->gimage->gimp, tmp);
      else
	gimp_parasite_detach (undo->gimage->gimp, pu->name);
    }

  if (tmp)
    gimp_parasite_free (tmp);

  return TRUE;
}

static void
undo_free_parasite (GimpUndo     *undo,
                    GimpUndoMode  undo_mode)
{
  ParasiteUndo *pu = undo->data;

  if (pu->parasite)
    gimp_parasite_free (pu->parasite);
  if (pu->name)
    g_free (pu->name);

  g_free (pu);
}


/******************************************************************************/
/*  Something for which programmer is too lazy to write an undo function for  */
/******************************************************************************/

static gboolean undo_pop_cantundo (GimpUndo            *undo,
                                   GimpUndoMode         undo_mode,
                                   GimpUndoAccumulator *accum);

gboolean
gimp_image_undo_push_cantundo (GimpImage   *gimage,
                               const gchar *undo_desc)
{
  GimpUndo *new;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  /* This is the sole purpose of this type of undo: the ability to
   * mark an image as having been mutated, without really providing
   * any adequate undo facility.
   */

  if ((new = gimp_image_undo_push (gimage, GIMP_TYPE_UNDO,
                                   0, 0,
                                   GIMP_UNDO_CANT, undo_desc,
                                   GIMP_DIRTY_ALL,
                                   undo_pop_cantundo,
                                   NULL,
                                   NULL)))
    {
      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_cantundo (GimpUndo            *undo,
                   GimpUndoMode         undo_mode,
                   GimpUndoAccumulator *accum)
{
  switch (undo_mode)
    {
    case GIMP_UNDO_MODE_UNDO:
      g_message (_("Can't undo %s"), GIMP_OBJECT (undo)->name);
      break;

    case GIMP_UNDO_MODE_REDO:
      break;
    }

  return TRUE;
}