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

#include "paint-types.h"

#include "config/gimpconfig-params.h"

#include "gimperaseroptions.h"


#define ERASER_DEFAULT_ANTI_ERASE FALSE


enum
{
  PROP_0,
  PROP_ANTI_ERASE
};


static void   gimp_eraser_options_class_init   (GimpEraserOptionsClass *klass);

static void   gimp_eraser_options_set_property (GObject         *object,
                                                guint            property_id,
                                                const GValue    *value,
                                                GParamSpec      *pspec);
static void   gimp_eraser_options_get_property (GObject         *object,
                                                guint            property_id,
                                                GValue          *value,
                                                GParamSpec      *pspec);


static GimpPaintOptionsClass *parent_class = NULL;


GType
gimp_eraser_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpEraserOptionsClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_eraser_options_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpEraserOptions),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) NULL
      };

      type = g_type_register_static (GIMP_TYPE_PAINT_OPTIONS,
                                     "GimpEraserOptions",
                                     &info, 0);
    }

  return type;
}

static void
gimp_eraser_options_class_init (GimpEraserOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_eraser_options_set_property;
  object_class->get_property = gimp_eraser_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_ANTI_ERASE,
                                    "anti-erase", NULL,
                                    ERASER_DEFAULT_ANTI_ERASE,
                                    0);
}

static void
gimp_eraser_options_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GimpEraserOptions *options = GIMP_ERASER_OPTIONS (object);

  switch (property_id)
    {
    case PROP_ANTI_ERASE:
      options->anti_erase = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_eraser_options_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GimpEraserOptions *options = GIMP_ERASER_OPTIONS (object);

  switch (property_id)
    {
    case PROP_ANTI_ERASE:
      g_value_set_boolean (value, options->anti_erase);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}