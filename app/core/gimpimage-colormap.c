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

#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "gimpimage.h"
#include "gimpimage-colormap.h"
#include "gimpimage-undo-push.h"

#include "gimp-intl.h"


guchar *
gimp_image_get_colormap (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);

  return gimage->cmap;
}

gint
gimp_image_get_colormap_size (const GimpImage *gimage)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), 0);

  return gimage->num_cols;
}

void
gimp_image_set_colormap (GimpImage    *gimage,
                         const guchar *cmap,
                         gint          n_colors,
                         gboolean      push_undo)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (cmap != NULL || n_colors == 0);
  g_return_if_fail (n_colors >= 0 && n_colors <= 256);

  if (push_undo)
    gimp_image_undo_push_image_colormap (gimage, _("Set Colormap"));

  if (cmap)
    {
      if (! gimage->cmap)
        gimage->cmap = g_new0 (guchar, GIMP_IMAGE_COLORMAP_SIZE);

      memcpy (gimage->cmap, cmap, n_colors * 3);
    }
  else
    {
      if (gimage->cmap)
        g_free (gimage->cmap);

      gimage->cmap = NULL;
    }

  gimage->num_cols = n_colors;

  gimp_image_colormap_changed (gimage, -1);
}

void
gimp_image_get_colormap_entry (GimpImage *gimage,
                               gint       color_index,
                               GimpRGB   *color)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (gimage->cmap != NULL);
  g_return_if_fail (color_index >= 0 && color_index < gimage->num_cols);
  g_return_if_fail (color != NULL);

  gimp_rgba_set_uchar (color,
                       gimage->cmap[color_index * 3],
                       gimage->cmap[color_index * 3 + 1],
                       gimage->cmap[color_index * 3 + 2],
                       OPAQUE_OPACITY);
}

void
gimp_image_set_colormap_entry (GimpImage     *gimage,
                               gint           color_index,
                               const GimpRGB *color,
                               gboolean       push_undo)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (gimage->cmap != NULL);
  g_return_if_fail (color_index >= 0 && color_index < gimage->num_cols);
  g_return_if_fail (color != NULL);

  if (push_undo)
    gimp_image_undo_push_image_colormap (gimage,
                                         _("Change Colormap entry"));

  gimp_rgb_get_uchar (color,
                      &gimage->cmap[color_index * 3],
                      &gimage->cmap[color_index * 3 + 1],
                      &gimage->cmap[color_index * 3 + 2]);

  gimp_image_colormap_changed (gimage, color_index);
}

void
gimp_image_add_colormap_entry (GimpImage     *gimage,
                               const GimpRGB *color)
{
  g_return_if_fail (GIMP_IS_IMAGE (gimage));
  g_return_if_fail (gimage->cmap != NULL);
  g_return_if_fail (gimage->num_cols < 256);
  g_return_if_fail (color != NULL);

  gimp_image_undo_push_image_colormap (gimage,
                                       _("Add Color to Colormap"));

  gimp_rgb_get_uchar (color,
                      &gimage->cmap[gimage->num_cols * 3],
                      &gimage->cmap[gimage->num_cols * 3 + 1],
                      &gimage->cmap[gimage->num_cols * 3 + 2]);

  gimage->num_cols++;

  gimp_image_colormap_changed (gimage, -1);
}