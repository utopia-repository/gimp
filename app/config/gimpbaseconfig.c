/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpBaseConfig class
 * Copyright (C) 2001  Sven Neumann <sven@gimp.org>
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

#include "libgimpbase/gimpbase.h"

#include "config-types.h"

#include "gimpconfig-params.h"

#include "gimprc-blurbs.h"
#include "gimpbaseconfig.h"

#include "gimp-intl.h"


static void  gimp_base_config_class_init   (GimpBaseConfigClass *klass);
static void  gimp_base_config_finalize     (GObject             *object);
static void  gimp_base_config_set_property (GObject             *object,
                                            guint                property_id,
                                            const GValue        *value,
                                            GParamSpec          *pspec);
static void  gimp_base_config_get_property (GObject             *object,
                                            guint                property_id,
                                            GValue              *value,
                                            GParamSpec          *pspec);


enum
{
  PROP_0,
  PROP_TEMP_PATH,
  PROP_SWAP_PATH,
  PROP_STINGY_MEMORY_USE,
  PROP_NUM_PROCESSORS,
  PROP_TILE_CACHE_SIZE
};

static GObjectClass *parent_class = NULL;


GType
gimp_base_config_get_type (void)
{
  static GType config_type = 0;

  if (! config_type)
    {
      static const GTypeInfo config_info =
      {
        sizeof (GimpBaseConfigClass),
	NULL,           /* base_init      */
        NULL,           /* base_finalize  */
	(GClassInitFunc) gimp_base_config_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpBaseConfig),
	0,              /* n_preallocs    */
	NULL            /* instance_init  */
      };

      config_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GimpBaseConfig",
                                            &config_info, 0);
    }

  return config_type;
}

static void
gimp_base_config_class_init (GimpBaseConfigClass *klass)
{
  GObjectClass *object_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = gimp_base_config_finalize;
  object_class->set_property = gimp_base_config_set_property;
  object_class->get_property = gimp_base_config_get_property;

  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_TEMP_PATH,
                                 "temp-path", TEMP_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR,
                                 "${gimp_dir}" G_DIR_SEPARATOR_S "tmp",
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_SWAP_PATH,
                                 "swap-path", SWAP_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR,
                                 "${gimp_dir}",
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_STINGY_MEMORY_USE,
                                    "stingy-memory-use", STINGY_MEMORY_USE_BLURB,
                                    FALSE,
                                    GIMP_PARAM_IGNORE);
  GIMP_CONFIG_INSTALL_PROP_UINT (object_class, PROP_NUM_PROCESSORS,
                                 "num-processors", NUM_PROCESSORS_BLURB,
                                 1, 30, 1,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_MEMSIZE (object_class, PROP_TILE_CACHE_SIZE,
                                    "tile-cache-size", TILE_CACHE_SIZE_BLURB,
                                    0, MIN (G_MAXULONG, GIMP_MAX_MEMSIZE),
                                    1 << 27, /* 128MB */
                                    GIMP_PARAM_CONFIRM);
}

static void
gimp_base_config_finalize (GObject *object)
{
  GimpBaseConfig *base_config = GIMP_BASE_CONFIG (object);

  g_free (base_config->temp_path);
  g_free (base_config->swap_path);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_base_config_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GimpBaseConfig *base_config = GIMP_BASE_CONFIG (object);

  switch (property_id)
    {
    case PROP_TEMP_PATH:
      g_free (base_config->temp_path);
      base_config->temp_path = g_value_dup_string (value);
      break;
    case PROP_SWAP_PATH:
      g_free (base_config->swap_path);
      base_config->swap_path = g_value_dup_string (value);
      break;
    case PROP_STINGY_MEMORY_USE:
      base_config->stingy_memory_use = g_value_get_boolean (value);
      break;
    case PROP_NUM_PROCESSORS:
      base_config->num_processors = g_value_get_uint (value);
      break;
    case PROP_TILE_CACHE_SIZE:
      base_config->tile_cache_size = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_base_config_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GimpBaseConfig *base_config = GIMP_BASE_CONFIG (object);

  switch (property_id)
    {
    case PROP_TEMP_PATH:
      g_value_set_string (value, base_config->temp_path);
      break;
    case PROP_SWAP_PATH:
      g_value_set_string (value, base_config->swap_path);
      break;
    case PROP_STINGY_MEMORY_USE:
      g_value_set_boolean (value, base_config->stingy_memory_use);
      break;
    case PROP_NUM_PROCESSORS:
      g_value_set_uint (value, base_config->num_processors);
      break;
    case PROP_TILE_CACHE_SIZE:
      g_value_set_uint64 (value, base_config->tile_cache_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}