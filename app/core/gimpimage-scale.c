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

#include "core-types.h"

#include "base/tile-manager.h"

#include "gimp.h"
#include "gimpimage.h"
#include "gimpimage-guides.h"
#include "gimpimage-scale.h"
#include "gimpimage-undo.h"
#include "gimpimage-undo-push.h"
#include "gimplayer.h"
#include "gimplist.h"
#include "gimpprogress.h"

#include "gimp-intl.h"


void
gimp_image_scale (GimpImage             *gimage,
		  gint                   new_width,
		  gint                   new_height,
                  GimpInterpolationType  interpolation_type,
                  GimpProgress          *progress)
{
  GimpItem *item;
  GList    *list;
  GList    *remove = NULL;
  gint      old_width;
  gint      old_height;
  gdouble   img_scale_w = 1.0;
  gdouble   img_scale_h = 1.0;
  gdouble   progress_max;
  gdouble   progress_current = 1.0;

  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (new_width > 0 && new_height > 0);
  g_return_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress));

  gimp_set_busy (gimage->gimp);

  progress_max = (gimage->channels->num_children +
                  gimage->layers->num_children   +
                  gimage->vectors->num_children  +
                  1 /* selection */);

  g_object_freeze_notify (G_OBJECT (gimage));

  gimp_image_undo_group_start (gimage, GIMP_UNDO_GROUP_IMAGE_SCALE,
                               _("Scale Image"));

  /*  Push the image size to the stack  */
  gimp_image_undo_push_image_size (gimage, NULL);

  old_width      = gimage->width;
  old_height     = gimage->height;
  img_scale_w    = (gdouble) new_width  / (gdouble) old_width;
  img_scale_h    = (gdouble) new_height / (gdouble) old_height;

  /*  Set the new width and height  */
  g_object_set (gimage,
                "width",  new_width,
                "height", new_height,
                NULL);

  /*  Scale all channels  */
  for (list = GIMP_LIST (gimage->channels)->list;
       list;
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      gimp_item_scale (item,
                       new_width, new_height, 0, 0,
                       interpolation_type, NULL);

      if (progress)
        gimp_progress_set_value (progress, progress_current++ / progress_max);
    }

  /*  Scale all vectors  */
  for (list = GIMP_LIST (gimage->vectors)->list;
       list;
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      gimp_item_scale (item,
                       new_width, new_height, 0, 0,
                       interpolation_type, NULL);

      if (progress)
        gimp_progress_set_value (progress, progress_current++ / progress_max);
    }

  /*  Don't forget the selection mask!  */
  gimp_item_scale (GIMP_ITEM (gimp_image_get_mask (gimage)),
                   new_width, new_height, 0, 0,
                   interpolation_type, NULL);

  if (progress)
    gimp_progress_set_value (progress, progress_current++ / progress_max);

  /*  Scale all layers  */
  for (list = GIMP_LIST (gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      item = (GimpItem *) list->data;

      if (! gimp_item_scale_by_factors (item,
                                        img_scale_w, img_scale_h,
                                        interpolation_type, NULL))
	{
	  /* Since 0 < img_scale_w, img_scale_h, failure due to one or more
	   * vanishing scaled layer dimensions. Implicit delete implemented
	   * here. Upstream warning implemented in resize_check_layer_scaling(),
	   * which offers the user the chance to bail out.
	   */
          remove = g_list_prepend (remove, item);
        }

      if (progress)
        gimp_progress_set_value (progress, progress_current++ / progress_max);
    }

  /* We defer removing layers lost to scaling until now so as not to mix
   * the operations of iterating over and removal from gimage->layers.
   */
  remove = g_list_reverse (remove);

  for (list = remove; list; list = g_list_next (list))
    {
      GimpLayer *layer = list->data;

      gimp_image_remove_layer (gimage, layer);
    }

  g_list_free (remove);

  /*  Scale all Guides  */
  for (list = gimage->guides; list; list = g_list_next (list))
    {
      GimpGuide *guide = list->data;

      switch (guide->orientation)
	{
	case GIMP_ORIENTATION_HORIZONTAL:
	  gimp_image_undo_push_image_guide (gimage, NULL, guide);
	  guide->position = (guide->position * new_height) / old_height;
	  break;

	case GIMP_ORIENTATION_VERTICAL:
	  gimp_image_undo_push_image_guide (gimage, NULL, guide);
	  guide->position = (guide->position * new_width) / old_width;
	  break;

	default:
          break;
	}
    }

  gimp_image_undo_group_end (gimage);

  gimp_viewable_size_changed (GIMP_VIEWABLE (gimage));
  g_object_thaw_notify (G_OBJECT (gimage));

  gimp_unset_busy (gimage->gimp);
}

/**
 * gimp_image_scale_check:
 * @gimage:      A #GimpImage.
 * @new_width:   The new width.
 * @new_height:  The new height.
 * @max_memsize: The maximum new memory size.
 * @new_memsize: The new memory size.
 *
 * Inventory the layer list in gimage and check that it may be
 * scaled to @new_height and @new_width without problems.
 *
 * Return value: #GIMP_IMAGE_SCALE_OK if scaling the image will shrink none
 *               of its layers completely away, and the new image size
 *               is smaller than @max_memsize.
 *               #GIMP_IMAGE_SCALE_TOO_SMALL if scaling would remove some
 *               existing layers.
 *               #GIMP_IMAGE_SCALE_TOO_BIG if the new image size would
 *               exceed the maximum specified in the preferences.
 **/
GimpImageScaleCheckType
gimp_image_scale_check (const GimpImage *gimage,
                        gint             new_width,
                        gint             new_height,
                        gint64           max_memsize,
                        gint64          *new_memsize)
{
  GList  *list;
  gint64  current_size;
  gint64  scalable_size;
  gint64  undo_size;
  gint64  redo_size;
  gint64  fixed_size;
  gint64  new_size;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), GIMP_IMAGE_SCALE_TOO_SMALL);
  g_return_val_if_fail (new_memsize != NULL, GIMP_IMAGE_SCALE_TOO_SMALL);

  current_size = gimp_object_get_memsize (GIMP_OBJECT (gimage), NULL);

  /*  the part of the image's memsize that scales linearly with the image  */
  scalable_size =
    gimp_object_get_memsize (GIMP_OBJECT (gimage->layers), NULL)         +
    gimp_object_get_memsize (GIMP_OBJECT (gimage->channels), NULL)       +
    gimp_object_get_memsize (GIMP_OBJECT (gimage->selection_mask), NULL) +
    gimp_object_get_memsize (GIMP_OBJECT (gimage->projection), NULL);

  undo_size = gimp_object_get_memsize (GIMP_OBJECT (gimage->undo_stack), NULL);
  redo_size = gimp_object_get_memsize (GIMP_OBJECT (gimage->redo_stack), NULL);

  /*  the fixed part of the image's memsize w/o any undo information  */
  fixed_size = current_size - undo_size - redo_size - scalable_size;

  /*  calculate the new size, which is:  */
  new_size = (fixed_size  +    /*  the fixed part                */
              scalable_size *  /*  plus the part that scales...  */
              ((gdouble) new_width  / gimp_image_get_width  (gimage)) *
              ((gdouble) new_height / gimp_image_get_height (gimage)));

  *new_memsize = new_size;

  if (new_size > current_size && new_size > max_memsize)
    return GIMP_IMAGE_SCALE_TOO_BIG;

  for (list = GIMP_LIST (gimage->layers)->list;
       list;
       list = g_list_next (list))
    {
      GimpItem *item = list->data;

      if (! gimp_item_check_scaling (item, new_width, new_height))
	return GIMP_IMAGE_SCALE_TOO_SMALL;
    }

  return GIMP_IMAGE_SCALE_OK;
}
