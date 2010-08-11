
/* Generated data (by gimp-mkenums) */

#include "config.h"
#include <glib-object.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-enums.h"
#include "tools-enums.h"
#include "gimp-intl.h"

/* enumerations from "./tools-enums.h" */
GType
gimp_color_pick_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_PICK_MODE_NONE, "GIMP_COLOR_PICK_MODE_NONE", "none" },
    { GIMP_COLOR_PICK_MODE_FOREGROUND, "GIMP_COLOR_PICK_MODE_FOREGROUND", "foreground" },
    { GIMP_COLOR_PICK_MODE_BACKGROUND, "GIMP_COLOR_PICK_MODE_BACKGROUND", "background" },
    { GIMP_COLOR_PICK_MODE_PALETTE, "GIMP_COLOR_PICK_MODE_PALETTE", "palette" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_PICK_MODE_NONE, N_("Pick only"), NULL },
    { GIMP_COLOR_PICK_MODE_FOREGROUND, N_("Set foreground color"), NULL },
    { GIMP_COLOR_PICK_MODE_BACKGROUND, N_("Set background color"), NULL },
    { GIMP_COLOR_PICK_MODE_PALETTE, N_("Add to palette"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpColorPickMode", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_rectangle_guide_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_RECTANGLE_GUIDE_NONE, "GIMP_RECTANGLE_GUIDE_NONE", "none" },
    { GIMP_RECTANGLE_GUIDE_CENTER_LINES, "GIMP_RECTANGLE_GUIDE_CENTER_LINES", "center-lines" },
    { GIMP_RECTANGLE_GUIDE_THIRDS, "GIMP_RECTANGLE_GUIDE_THIRDS", "thirds" },
    { GIMP_RECTANGLE_GUIDE_GOLDEN, "GIMP_RECTANGLE_GUIDE_GOLDEN", "golden" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_RECTANGLE_GUIDE_NONE, N_("No guides"), NULL },
    { GIMP_RECTANGLE_GUIDE_CENTER_LINES, N_("Center lines"), NULL },
    { GIMP_RECTANGLE_GUIDE_THIRDS, N_("Rule of thirds"), NULL },
    { GIMP_RECTANGLE_GUIDE_GOLDEN, N_("Golden sections"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRectangleGuide", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_rectangle_constraint_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_RECTANGLE_CONSTRAIN_NONE, "GIMP_RECTANGLE_CONSTRAIN_NONE", "none" },
    { GIMP_RECTANGLE_CONSTRAIN_IMAGE, "GIMP_RECTANGLE_CONSTRAIN_IMAGE", "image" },
    { GIMP_RECTANGLE_CONSTRAIN_DRAWABLE, "GIMP_RECTANGLE_CONSTRAIN_DRAWABLE", "drawable" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_RECTANGLE_CONSTRAIN_NONE, N_("No constraint"), NULL },
    { GIMP_RECTANGLE_CONSTRAIN_IMAGE, N_("Image bounds"), NULL },
    { GIMP_RECTANGLE_CONSTRAIN_DRAWABLE, N_("Drawable bounds"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRectangleConstraint", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_rect_select_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_RECT_SELECT_MODE_FREE, "GIMP_RECT_SELECT_MODE_FREE", "free" },
    { GIMP_RECT_SELECT_MODE_FIXED_SIZE, "GIMP_RECT_SELECT_MODE_FIXED_SIZE", "fixed-size" },
    { GIMP_RECT_SELECT_MODE_FIXED_RATIO, "GIMP_RECT_SELECT_MODE_FIXED_RATIO", "fixed-ratio" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_RECT_SELECT_MODE_FREE, N_("Free select"), NULL },
    { GIMP_RECT_SELECT_MODE_FIXED_SIZE, N_("Fixed size"), NULL },
    { GIMP_RECT_SELECT_MODE_FIXED_RATIO, N_("Fixed aspect ratio"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRectSelectMode", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_transform_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TRANSFORM_TYPE_LAYER, "GIMP_TRANSFORM_TYPE_LAYER", "layer" },
    { GIMP_TRANSFORM_TYPE_SELECTION, "GIMP_TRANSFORM_TYPE_SELECTION", "selection" },
    { GIMP_TRANSFORM_TYPE_PATH, "GIMP_TRANSFORM_TYPE_PATH", "path" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TRANSFORM_TYPE_LAYER, N_("Transform layer"), NULL },
    { GIMP_TRANSFORM_TYPE_SELECTION, N_("Transform selection"), NULL },
    { GIMP_TRANSFORM_TYPE_PATH, N_("Transform path"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTransformType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_vector_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_VECTOR_MODE_DESIGN, "GIMP_VECTOR_MODE_DESIGN", "design" },
    { GIMP_VECTOR_MODE_EDIT, "GIMP_VECTOR_MODE_EDIT", "edit" },
    { GIMP_VECTOR_MODE_MOVE, "GIMP_VECTOR_MODE_MOVE", "move" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_VECTOR_MODE_DESIGN, N_("Design"), NULL },
    { GIMP_VECTOR_MODE_EDIT, N_("Edit"), NULL },
    { GIMP_VECTOR_MODE_MOVE, N_("Move"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpVectorMode", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_transform_preview_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TRANSFORM_PREVIEW_TYPE_OUTLINE, "GIMP_TRANSFORM_PREVIEW_TYPE_OUTLINE", "outline" },
    { GIMP_TRANSFORM_PREVIEW_TYPE_GRID, "GIMP_TRANSFORM_PREVIEW_TYPE_GRID", "grid" },
    { GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE, "GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE", "image" },
    { GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID, "GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID", "image-grid" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TRANSFORM_PREVIEW_TYPE_OUTLINE, N_("Outline"), NULL },
    { GIMP_TRANSFORM_PREVIEW_TYPE_GRID, N_("Grid"), NULL },
    { GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE, N_("Image"), NULL },
    { GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID, N_("Image + Grid"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTransformPreviewType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_transform_grid_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TRANSFORM_GRID_TYPE_N_LINES, "GIMP_TRANSFORM_GRID_TYPE_N_LINES", "n-lines" },
    { GIMP_TRANSFORM_GRID_TYPE_SPACING, "GIMP_TRANSFORM_GRID_TYPE_SPACING", "spacing" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TRANSFORM_GRID_TYPE_N_LINES, N_("Number of grid lines"), NULL },
    { GIMP_TRANSFORM_GRID_TYPE_SPACING, N_("Grid line spacing"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTransformGridType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}


/* Generated data ends here */

