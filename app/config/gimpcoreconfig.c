/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpCoreConfig class
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

#include "core/core-types.h"
#include "core/gimpgrid.h"
#include "core/gimptemplate.h"

#include "gimpconfig.h"
#include "gimpconfig-params.h"
#include "gimpconfig-types.h"
#include "gimpconfig-utils.h"

#include "gimprc-blurbs.h"
#include "gimpcoreconfig.h"

#include "gimp-intl.h"


static void  gimp_core_config_class_init   (GimpCoreConfigClass *klass);
static void  gimp_core_config_init         (GimpCoreConfig      *config);
static void  gimp_core_config_finalize            (GObject      *object);
static void  gimp_core_config_set_property        (GObject      *object,
                                                   guint         property_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec);
static void  gimp_core_config_get_property        (GObject      *object,
                                                   guint         property_id,
                                                   GValue       *value,
                                                   GParamSpec   *pspec);
static void gimp_core_config_default_image_notify (GObject      *object,
                                                   GParamSpec   *pspec,
                                                   gpointer      data);
static void gimp_core_config_default_grid_notify  (GObject      *object,
                                                   GParamSpec   *pspec,
                                                   gpointer      data);


#define DEFAULT_BRUSH     "Circle (11)"
#define DEFAULT_PATTERN   "Pine"
#define DEFAULT_PALETTE   "Default"
#define DEFAULT_GRADIENT  "FG to BG (RGB)"
#define DEFAULT_FONT      "Sans"
#define DEFAULT_COMMENT   "Created with The GIMP"

enum
{
  PROP_0,
  PROP_INTERPOLATION_TYPE,
  PROP_PLUG_IN_PATH,
  PROP_MODULE_PATH,
  PROP_ENVIRON_PATH,
  PROP_BRUSH_PATH,
  PROP_BRUSH_PATH_WRITABLE,
  PROP_PATTERN_PATH,
  PROP_PATTERN_PATH_WRITABLE,
  PROP_PALETTE_PATH,
  PROP_PALETTE_PATH_WRITABLE,
  PROP_GRADIENT_PATH,
  PROP_GRADIENT_PATH_WRITABLE,
  PROP_FONT_PATH,
  PROP_FONT_PATH_WRITABLE,
  PROP_DEFAULT_BRUSH,
  PROP_DEFAULT_PATTERN,
  PROP_DEFAULT_PALETTE,
  PROP_DEFAULT_GRADIENT,
  PROP_DEFAULT_FONT,
  PROP_GLOBAL_BRUSH,
  PROP_GLOBAL_PATTERN,
  PROP_GLOBAL_PALETTE,
  PROP_GLOBAL_GRADIENT,
  PROP_GLOBAL_FONT,
  PROP_DEFAULT_IMAGE,
  PROP_DEFAULT_GRID,
  PROP_UNDO_LEVELS,
  PROP_UNDO_SIZE,
  PROP_UNDO_PREVIEW_SIZE,
  PROP_PLUGINRC_PATH,
  PROP_LAYER_PREVIEWS,
  PROP_LAYER_PREVIEW_SIZE,
  PROP_THUMBNAIL_SIZE,
  PROP_THUMBNAIL_FILESIZE_LIMIT,
  PROP_INSTALL_COLORMAP,
  PROP_MIN_COLORS
};

static GObjectClass *parent_class = NULL;


GType
gimp_core_config_get_type (void)
{
  static GType config_type = 0;

  if (! config_type)
    {
      static const GTypeInfo config_info =
      {
        sizeof (GimpCoreConfigClass),
	NULL,           /* base_init      */
        NULL,           /* base_finalize  */
	(GClassInitFunc) gimp_core_config_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpCoreConfig),
	0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_core_config_init
      };

      config_type = g_type_register_static (GIMP_TYPE_BASE_CONFIG,
                                            "GimpCoreConfig",
                                            &config_info, 0);
    }

  return config_type;
}

static void
gimp_core_config_class_init (GimpCoreConfigClass *klass)
{
  GObjectClass *object_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = gimp_core_config_finalize;
  object_class->set_property = gimp_core_config_set_property;
  object_class->get_property = gimp_core_config_get_property;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_INTERPOLATION_TYPE,
                                 "interpolation-type",
                                 INTERPOLATION_TYPE_BLURB,
                                 GIMP_TYPE_INTERPOLATION_TYPE,
                                 GIMP_INTERPOLATION_LINEAR,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PLUG_IN_PATH,
                                 "plug-in-path", PLUG_IN_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_plug_in_path ("plug-ins"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_MODULE_PATH,
                                 "module-path", MODULE_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_plug_in_path ("modules"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_ENVIRON_PATH,
                                 "environ-path", ENVIRON_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_plug_in_path ("environ"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_BRUSH_PATH,
                                 "brush-path", BRUSH_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_data_path ("brushes"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_BRUSH_PATH_WRITABLE,
                                 "brush-path-writable",
                                 BRUSH_PATH_WRITABLE_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_writable_path ("brushes"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PATTERN_PATH,
                                 "pattern-path", PATTERN_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_data_path ("patterns"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PATTERN_PATH_WRITABLE,
                                 "pattern-path-writable",
                                 PATTERN_PATH_WRITABLE_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_writable_path ("patterns"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PALETTE_PATH,
                                 "palette-path", PALETTE_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_data_path ("palettes"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_PALETTE_PATH_WRITABLE,
                                 "palette-path-writable",
                                 PALETTE_PATH_WRITABLE_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_writable_path ("palettes"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_GRADIENT_PATH,
                                 "gradient-path", GRADIENT_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_data_path ("gradients"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_GRADIENT_PATH_WRITABLE,
                                 "gradient-path-writable",
                                 GRADIENT_PATH_WRITABLE_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_writable_path ("gradients"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_FONT_PATH,
                                 "font-path", FONT_PATH_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_data_path ("fonts"),
                                 0);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class, PROP_FONT_PATH_WRITABLE,
                                 "font-path-writable",
                                 FONT_PATH_WRITABLE_BLURB,
				 GIMP_PARAM_PATH_DIR_LIST,
                                 gimp_config_build_writable_path ("fonts"),
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_DEFAULT_BRUSH,
                                   "default-brush", DEFAULT_BRUSH_BLURB,
                                   DEFAULT_BRUSH,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_DEFAULT_PATTERN,
                                   "default-pattern", DEFAULT_PATTERN_BLURB,
                                   DEFAULT_PATTERN,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_DEFAULT_PALETTE,
                                   "default-palette", DEFAULT_PALETTE_BLURB,
                                   DEFAULT_PALETTE,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_DEFAULT_GRADIENT,
                                   "default-gradient", DEFAULT_GRADIENT_BLURB,
                                   DEFAULT_GRADIENT,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_DEFAULT_FONT,
                                   "default-font", DEFAULT_FONT_BLURB,
                                   DEFAULT_FONT,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_GLOBAL_BRUSH,
                                    "global-brush", GLOBAL_BRUSH_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_GLOBAL_PATTERN,
                                    "global-pattern", GLOBAL_PATTERN_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_GLOBAL_PALETTE,
                                    "global-palette", GLOBAL_PALETTE_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_GLOBAL_GRADIENT,
                                    "global-gradient", GLOBAL_GRADIENT_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_GLOBAL_FONT,
                                    "global-font", GLOBAL_FONT_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_DEFAULT_IMAGE,
                                   "default-image", DEFAULT_IMAGE_BLURB,
                                   GIMP_TYPE_TEMPLATE,
                                   GIMP_PARAM_AGGREGATE);
  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_DEFAULT_GRID,
                                   "default-grid", DEFAULT_GRID_BLURB,
                                   GIMP_TYPE_GRID,
                                   GIMP_PARAM_AGGREGATE);
  GIMP_CONFIG_INSTALL_PROP_INT (object_class, PROP_UNDO_LEVELS,
                                "undo-levels", UNDO_LEVELS_BLURB,
                                0, G_MAXINT, 5,
                                GIMP_PARAM_CONFIRM);
  GIMP_CONFIG_INSTALL_PROP_MEMSIZE (object_class, PROP_UNDO_SIZE,
                                    "undo-size", UNDO_SIZE_BLURB,
                                    0, GIMP_MAX_MEMSIZE, 1 << 24, /* 16MB */
                                    GIMP_PARAM_CONFIRM);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_UNDO_PREVIEW_SIZE,
                                 "undo-preview-size", UNDO_PREVIEW_SIZE_BLURB,
                                 GIMP_TYPE_VIEW_SIZE,
                                 GIMP_VIEW_SIZE_LARGE,
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_PATH (object_class,
                                 PROP_PLUGINRC_PATH,
                                 "pluginrc-path", PLUGINRC_PATH_BLURB,
				 GIMP_PARAM_PATH_FILE,
                                 "${gimp_dir}" G_DIR_SEPARATOR_S "pluginrc",
                                 GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_LAYER_PREVIEWS,
                                    "layer-previews", LAYER_PREVIEWS_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_LAYER_PREVIEW_SIZE,
                                 "layer-preview-size", LAYER_PREVIEW_SIZE_BLURB,
                                 GIMP_TYPE_VIEW_SIZE,
                                 GIMP_VIEW_SIZE_MEDIUM,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_THUMBNAIL_SIZE,
                                 "thumbnail-size", THUMBNAIL_SIZE_BLURB,
                                 GIMP_TYPE_THUMBNAIL_SIZE,
                                 GIMP_THUMBNAIL_SIZE_NORMAL,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_MEMSIZE (object_class, PROP_THUMBNAIL_FILESIZE_LIMIT,
                                    "thumbnail-filesize-limit",
                                    THUMBNAIL_FILESIZE_LIMIT_BLURB,
                                    0, GIMP_MAX_MEMSIZE, 1 << 22,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_INSTALL_COLORMAP,
                                    "install-colormap", INSTALL_COLORMAP_BLURB,
                                    FALSE,
                                    GIMP_PARAM_RESTART);
  GIMP_CONFIG_INSTALL_PROP_INT (object_class, PROP_MIN_COLORS,
                                "min-colors", MIN_COLORS_BLURB,
                                27, 256, 144,
                                GIMP_PARAM_RESTART);
}

static void
gimp_core_config_init (GimpCoreConfig *config)
{
  config->default_image = g_object_new (GIMP_TYPE_TEMPLATE,
                                        "comment", DEFAULT_COMMENT,
                                        NULL);
  g_signal_connect (config->default_image, "notify",
                    G_CALLBACK (gimp_core_config_default_image_notify),
                    config);

  config->default_grid = g_object_new (GIMP_TYPE_GRID, NULL);
  g_signal_connect (config->default_grid, "notify",
                    G_CALLBACK (gimp_core_config_default_grid_notify),
                    config);
}

static void
gimp_core_config_finalize (GObject *object)
{
  GimpCoreConfig *core_config = GIMP_CORE_CONFIG (object);

  g_free (core_config->plug_in_path);
  g_free (core_config->module_path);
  g_free (core_config->environ_path);
  g_free (core_config->brush_path);
  g_free (core_config->brush_path_writable);
  g_free (core_config->pattern_path);
  g_free (core_config->pattern_path_writable);
  g_free (core_config->palette_path);
  g_free (core_config->palette_path_writable);
  g_free (core_config->gradient_path);
  g_free (core_config->gradient_path_writable);
  g_free (core_config->font_path);
  g_free (core_config->font_path_writable);
  g_free (core_config->default_brush);
  g_free (core_config->default_pattern);
  g_free (core_config->default_palette);
  g_free (core_config->default_gradient);
  g_free (core_config->default_font);
  g_free (core_config->plug_in_rc_path);

  if (core_config->default_image)
    g_object_unref (core_config->default_image);

  if (core_config->default_grid)
    g_object_unref (core_config->default_grid);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_core_config_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GimpCoreConfig *core_config = GIMP_CORE_CONFIG (object);

  switch (property_id)
    {
    case PROP_INTERPOLATION_TYPE:
      core_config->interpolation_type = g_value_get_enum (value);
      break;
    case PROP_PLUG_IN_PATH:
      g_free (core_config->plug_in_path);
      core_config->plug_in_path = g_value_dup_string (value);
      break;
    case PROP_MODULE_PATH:
      g_free (core_config->module_path);
      core_config->module_path = g_value_dup_string (value);
      break;
    case PROP_ENVIRON_PATH:
      g_free (core_config->environ_path);
      core_config->environ_path = g_value_dup_string (value);
      break;
    case PROP_BRUSH_PATH:
      g_free (core_config->brush_path);
      core_config->brush_path = g_value_dup_string (value);
      break;
    case PROP_BRUSH_PATH_WRITABLE:
      g_free (core_config->brush_path_writable);
      core_config->brush_path_writable = g_value_dup_string (value);
      break;
    case PROP_PATTERN_PATH:
      g_free (core_config->pattern_path);
      core_config->pattern_path = g_value_dup_string (value);
      break;
    case PROP_PATTERN_PATH_WRITABLE:
      g_free (core_config->pattern_path_writable);
      core_config->pattern_path_writable = g_value_dup_string (value);
      break;
    case PROP_PALETTE_PATH:
      g_free (core_config->palette_path);
      core_config->palette_path = g_value_dup_string (value);
      break;
    case PROP_PALETTE_PATH_WRITABLE:
      g_free (core_config->palette_path_writable);
      core_config->palette_path_writable = g_value_dup_string (value);
      break;
    case PROP_GRADIENT_PATH:
      g_free (core_config->gradient_path);
      core_config->gradient_path = g_value_dup_string (value);
      break;
    case PROP_GRADIENT_PATH_WRITABLE:
      g_free (core_config->gradient_path_writable);
      core_config->gradient_path_writable = g_value_dup_string (value);
      break;
    case PROP_FONT_PATH:
      g_free (core_config->font_path);
      core_config->font_path = g_value_dup_string (value);
      break;
    case PROP_FONT_PATH_WRITABLE:
      g_free (core_config->font_path_writable);
      core_config->font_path_writable = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_BRUSH:
      g_free (core_config->default_brush);
      core_config->default_brush = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_PATTERN:
      g_free (core_config->default_pattern);
      core_config->default_pattern = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_PALETTE:
      g_free (core_config->default_palette);
      core_config->default_palette = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_GRADIENT:
      g_free (core_config->default_gradient);
      core_config->default_gradient = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_FONT:
      g_free (core_config->default_font);
      core_config->default_font = g_value_dup_string (value);
      break;
    case PROP_GLOBAL_BRUSH:
      core_config->global_brush = g_value_get_boolean (value);
      break;
    case PROP_GLOBAL_PATTERN:
      core_config->global_pattern = g_value_get_boolean (value);
      break;
    case PROP_GLOBAL_PALETTE:
      core_config->global_palette = g_value_get_boolean (value);
      break;
    case PROP_GLOBAL_GRADIENT:
      core_config->global_gradient = g_value_get_boolean (value);
      break;
    case PROP_GLOBAL_FONT:
      core_config->global_font = g_value_get_boolean (value);
      break;
    case PROP_DEFAULT_IMAGE:
      if (g_value_get_object (value))
        gimp_config_sync (GIMP_CONFIG (g_value_get_object (value)),
                          GIMP_CONFIG (core_config->default_image), 0);
      break;
    case PROP_DEFAULT_GRID:
      if (g_value_get_object (value))
        gimp_config_sync (GIMP_CONFIG (g_value_get_object (value)),
                          GIMP_CONFIG (core_config->default_grid), 0);
      break;
    case PROP_UNDO_LEVELS:
      core_config->levels_of_undo = g_value_get_int (value);
      break;
    case PROP_UNDO_SIZE:
      core_config->undo_size = g_value_get_uint64 (value);
      break;
    case PROP_UNDO_PREVIEW_SIZE:
      core_config->undo_preview_size = g_value_get_enum (value);
      break;
    case PROP_PLUGINRC_PATH:
      g_free (core_config->plug_in_rc_path);
      core_config->plug_in_rc_path = g_value_dup_string (value);
      break;
    case PROP_LAYER_PREVIEWS:
      core_config->layer_previews = g_value_get_boolean (value);
      break;
    case PROP_LAYER_PREVIEW_SIZE:
      core_config->layer_preview_size = g_value_get_enum (value);
      break;
    case PROP_THUMBNAIL_SIZE:
      core_config->thumbnail_size = g_value_get_enum (value);
      break;
    case PROP_THUMBNAIL_FILESIZE_LIMIT:
      core_config->thumbnail_filesize_limit = g_value_get_uint64 (value);
      break;
    case PROP_INSTALL_COLORMAP:
      core_config->install_cmap = g_value_get_boolean (value);
      break;
    case PROP_MIN_COLORS:
      core_config->min_colors = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_core_config_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GimpCoreConfig *core_config = GIMP_CORE_CONFIG (object);

  switch (property_id)
    {
    case PROP_INTERPOLATION_TYPE:
      g_value_set_enum (value, core_config->interpolation_type);
      break;
    case PROP_PLUG_IN_PATH:
      g_value_set_string (value, core_config->plug_in_path);
      break;
    case PROP_MODULE_PATH:
      g_value_set_string (value, core_config->module_path);
      break;
    case PROP_ENVIRON_PATH:
      g_value_set_string (value, core_config->environ_path);
      break;
    case PROP_BRUSH_PATH:
      g_value_set_string (value, core_config->brush_path);
      break;
    case PROP_BRUSH_PATH_WRITABLE:
      g_value_set_string (value, core_config->brush_path_writable);
      break;
    case PROP_PATTERN_PATH:
      g_value_set_string (value, core_config->pattern_path);
      break;
    case PROP_PATTERN_PATH_WRITABLE:
      g_value_set_string (value, core_config->pattern_path_writable);
      break;
    case PROP_PALETTE_PATH:
      g_value_set_string (value, core_config->palette_path);
      break;
    case PROP_PALETTE_PATH_WRITABLE:
      g_value_set_string (value, core_config->palette_path_writable);
      break;
    case PROP_GRADIENT_PATH:
      g_value_set_string (value, core_config->gradient_path);
      break;
    case PROP_GRADIENT_PATH_WRITABLE:
      g_value_set_string (value, core_config->gradient_path_writable);
      break;
    case PROP_FONT_PATH:
      g_value_set_string (value, core_config->font_path);
      break;
    case PROP_FONT_PATH_WRITABLE:
      g_value_set_string (value, core_config->font_path_writable);
      break;
    case PROP_DEFAULT_BRUSH:
      g_value_set_string (value, core_config->default_brush);
      break;
    case PROP_DEFAULT_PATTERN:
      g_value_set_string (value, core_config->default_pattern);
      break;
    case PROP_DEFAULT_PALETTE:
      g_value_set_string (value, core_config->default_palette);
      break;
    case PROP_DEFAULT_GRADIENT:
      g_value_set_string (value, core_config->default_gradient);
      break;
    case PROP_DEFAULT_FONT:
      g_value_set_string (value, core_config->default_font);
      break;
    case PROP_GLOBAL_BRUSH:
      g_value_set_boolean (value, core_config->global_brush);
      break;
    case PROP_GLOBAL_PATTERN:
      g_value_set_boolean (value, core_config->global_pattern);
      break;
    case PROP_GLOBAL_PALETTE:
      g_value_set_boolean (value, core_config->global_palette);
      break;
    case PROP_GLOBAL_GRADIENT:
      g_value_set_boolean (value, core_config->global_gradient);
      break;
    case PROP_GLOBAL_FONT:
      g_value_set_boolean (value, core_config->global_font);
      break;
    case PROP_DEFAULT_IMAGE:
      g_value_set_object (value, core_config->default_image);
      break;
    case PROP_DEFAULT_GRID:
      g_value_set_object (value, core_config->default_grid);
      break;
    case PROP_UNDO_LEVELS:
      g_value_set_int (value, core_config->levels_of_undo);
      break;
    case PROP_UNDO_SIZE:
      g_value_set_uint64 (value, core_config->undo_size);
      break;
    case PROP_UNDO_PREVIEW_SIZE:
      g_value_set_enum (value, core_config->undo_preview_size);
      break;
    case PROP_PLUGINRC_PATH:
      g_value_set_string (value, core_config->plug_in_rc_path);
      break;
    case PROP_LAYER_PREVIEWS:
      g_value_set_boolean (value, core_config->layer_previews);
      break;
    case PROP_LAYER_PREVIEW_SIZE:
      g_value_set_enum (value, core_config->layer_preview_size);
      break;
    case PROP_THUMBNAIL_SIZE:
      g_value_set_enum (value, core_config->thumbnail_size);
      break;
    case PROP_THUMBNAIL_FILESIZE_LIMIT:
      g_value_set_uint64 (value, core_config->thumbnail_filesize_limit);
      break;
    case PROP_INSTALL_COLORMAP:
      g_value_set_boolean (value, core_config->install_cmap);
      break;
    case PROP_MIN_COLORS:
      g_value_set_int (value, core_config->min_colors);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_core_config_default_image_notify (GObject    *object,
                                       GParamSpec *pspec,
                                       gpointer    data)
{
  g_object_notify (G_OBJECT (data), "default-image");
}

static void
gimp_core_config_default_grid_notify (GObject    *object,
                                      GParamSpec *pspec,
                                      gpointer    data)
{
  g_object_notify (G_OBJECT (data), "default-grid");
}