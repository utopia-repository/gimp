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

#include "gimpcloneoptions.h"


#define CLONE_DEFAULT_TYPE       GIMP_IMAGE_CLONE
#define CLONE_DEFAULT_ALIGN_MODE GIMP_CLONE_ALIGN_NO


enum
{
  PROP_0,
  PROP_CLONE_TYPE,
  PROP_ALIGN_MODE
};



static void   gimp_clone_options_class_init   (GimpCloneOptionsClass *klass);

static void   gimp_clone_options_set_property (GObject      *object,
                                               guint         property_id,
                                               const GValue *value,
                                               GParamSpec   *pspec);
static void   gimp_clone_options_get_property (GObject      *object,
                                               guint         property_id,
                                               GValue       *value,
                                               GParamSpec   *pspec);


static GimpPaintOptionsClass *parent_class = NULL;


GType
gimp_clone_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpCloneOptionsClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_clone_options_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpCloneOptions),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) NULL
      };

      type = g_type_register_static (GIMP_TYPE_PAINT_OPTIONS,
                                     "GimpCloneOptions",
                                     &info, 0);
    }

  return type;
}

static void
gimp_clone_options_class_init (GimpCloneOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_clone_options_set_property;
  object_class->get_property = gimp_clone_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_CLONE_TYPE,
                                 "clone-type", NULL,
                                 GIMP_TYPE_CLONE_TYPE,
                                 CLONE_DEFAULT_TYPE,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_ALIGN_MODE,
                                 "align-mode", NULL,
                                 GIMP_TYPE_CLONE_ALIGN_MODE,
                                 CLONE_DEFAULT_ALIGN_MODE,
                                 0);
}

static void
gimp_clone_options_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GimpCloneOptions *options = GIMP_CLONE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_CLONE_TYPE:
      options->clone_type = g_value_get_enum (value);
      break;
    case PROP_ALIGN_MODE:
      options->align_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_clone_options_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GimpCloneOptions *options = GIMP_CLONE_OPTIONS (object);

  switch (property_id)
    {
    case PROP_CLONE_TYPE:
      g_value_set_enum (value, options->clone_type);
      break;
    case PROP_ALIGN_MODE:
      g_value_set_enum (value, options->align_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}