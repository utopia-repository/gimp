/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpviewrenderer-utils.c
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#include <gtk/gtk.h>

#include "widgets-types.h"

#include "core/gimpbrush.h"
#include "core/gimpbuffer.h"
#include "core/gimpdrawable.h"
#include "core/gimpgradient.h"
#include "core/gimplayer.h"
#include "core/gimpimage.h"
#include "core/gimpimagefile.h"
#include "vectors/gimpvectors.h"

#include "gimpviewrendererbrush.h"
#include "gimpviewrendererbuffer.h"
#include "gimpviewrendererlayer.h"
#include "gimpviewrenderergradient.h"
#include "gimpviewrendererimage.h"
#include "gimpviewrendererimagefile.h"
#include "gimpviewrenderervectors.h"


GType
gimp_view_renderer_type_from_viewable_type (GType viewable_type)
{
  GType type = GIMP_TYPE_VIEW_RENDERER;

  g_return_val_if_fail (g_type_is_a (viewable_type, GIMP_TYPE_VIEWABLE),
                        G_TYPE_NONE);

  if (g_type_is_a (viewable_type, GIMP_TYPE_BRUSH))
    {
      type = GIMP_TYPE_VIEW_RENDERER_BRUSH;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_BUFFER))
    {
      type = GIMP_TYPE_VIEW_RENDERER_BUFFER;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_IMAGE))
    {
      type = GIMP_TYPE_VIEW_RENDERER_IMAGE;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_LAYER))
    {
      type = GIMP_TYPE_VIEW_RENDERER_LAYER;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_DRAWABLE))
    {
      type = GIMP_TYPE_VIEW_RENDERER_DRAWABLE;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_GRADIENT))
    {
      type = GIMP_TYPE_VIEW_RENDERER_GRADIENT;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_VECTORS))
    {
      type = GIMP_TYPE_VIEW_RENDERER_VECTORS;
    }
  else if (g_type_is_a (viewable_type, GIMP_TYPE_IMAGEFILE))
    {
      type = GIMP_TYPE_VIEW_RENDERER_IMAGEFILE;
    }

  return type;
}