
/* Generated data (by gimp-mkenums) */

#include "config.h"
#include <glib-object.h>
#include "libgimpbase/gimpbase.h"
#include "gimpenums.h"

/* enumerations from "./gimpenums.h" */
GType
gimp_add_mask_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ADD_WHITE_MASK, "GIMP_ADD_WHITE_MASK", "white-mask" },
    { GIMP_ADD_BLACK_MASK, "GIMP_ADD_BLACK_MASK", "black-mask" },
    { GIMP_ADD_ALPHA_MASK, "GIMP_ADD_ALPHA_MASK", "alpha-mask" },
    { GIMP_ADD_ALPHA_TRANSFER_MASK, "GIMP_ADD_ALPHA_TRANSFER_MASK", "alpha-transfer-mask" },
    { GIMP_ADD_SELECTION_MASK, "GIMP_ADD_SELECTION_MASK", "selection-mask" },
    { GIMP_ADD_COPY_MASK, "GIMP_ADD_COPY_MASK", "copy-mask" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ADD_WHITE_MASK, "GIMP_ADD_WHITE_MASK", NULL },
    { GIMP_ADD_BLACK_MASK, "GIMP_ADD_BLACK_MASK", NULL },
    { GIMP_ADD_ALPHA_MASK, "GIMP_ADD_ALPHA_MASK", NULL },
    { GIMP_ADD_ALPHA_TRANSFER_MASK, "GIMP_ADD_ALPHA_TRANSFER_MASK", NULL },
    { GIMP_ADD_SELECTION_MASK, "GIMP_ADD_SELECTION_MASK", NULL },
    { GIMP_ADD_COPY_MASK, "GIMP_ADD_COPY_MASK", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpAddMaskType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_blend_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_FG_BG_RGB_MODE, "GIMP_FG_BG_RGB_MODE", "fg-bg-rgb-mode" },
    { GIMP_FG_BG_HSV_MODE, "GIMP_FG_BG_HSV_MODE", "fg-bg-hsv-mode" },
    { GIMP_FG_TRANSPARENT_MODE, "GIMP_FG_TRANSPARENT_MODE", "fg-transparent-mode" },
    { GIMP_CUSTOM_MODE, "GIMP_CUSTOM_MODE", "custom-mode" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_FG_BG_RGB_MODE, "GIMP_FG_BG_RGB_MODE", NULL },
    { GIMP_FG_BG_HSV_MODE, "GIMP_FG_BG_HSV_MODE", NULL },
    { GIMP_FG_TRANSPARENT_MODE, "GIMP_FG_TRANSPARENT_MODE", NULL },
    { GIMP_CUSTOM_MODE, "GIMP_CUSTOM_MODE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpBlendMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_brush_application_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_BRUSH_HARD, "GIMP_BRUSH_HARD", "hard" },
    { GIMP_BRUSH_SOFT, "GIMP_BRUSH_SOFT", "soft" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_BRUSH_HARD, "GIMP_BRUSH_HARD", NULL },
    { GIMP_BRUSH_SOFT, "GIMP_BRUSH_SOFT", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpBrushApplicationMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_bucket_fill_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_FG_BUCKET_FILL, "GIMP_FG_BUCKET_FILL", "fg-bucket-fill" },
    { GIMP_BG_BUCKET_FILL, "GIMP_BG_BUCKET_FILL", "bg-bucket-fill" },
    { GIMP_PATTERN_BUCKET_FILL, "GIMP_PATTERN_BUCKET_FILL", "pattern-bucket-fill" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_FG_BUCKET_FILL, "GIMP_FG_BUCKET_FILL", NULL },
    { GIMP_BG_BUCKET_FILL, "GIMP_BG_BUCKET_FILL", NULL },
    { GIMP_PATTERN_BUCKET_FILL, "GIMP_PATTERN_BUCKET_FILL", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpBucketFillMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_channel_ops_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_CHANNEL_OP_ADD, "GIMP_CHANNEL_OP_ADD", "add" },
    { GIMP_CHANNEL_OP_SUBTRACT, "GIMP_CHANNEL_OP_SUBTRACT", "subtract" },
    { GIMP_CHANNEL_OP_REPLACE, "GIMP_CHANNEL_OP_REPLACE", "replace" },
    { GIMP_CHANNEL_OP_INTERSECT, "GIMP_CHANNEL_OP_INTERSECT", "intersect" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_CHANNEL_OP_ADD, "GIMP_CHANNEL_OP_ADD", NULL },
    { GIMP_CHANNEL_OP_SUBTRACT, "GIMP_CHANNEL_OP_SUBTRACT", NULL },
    { GIMP_CHANNEL_OP_REPLACE, "GIMP_CHANNEL_OP_REPLACE", NULL },
    { GIMP_CHANNEL_OP_INTERSECT, "GIMP_CHANNEL_OP_INTERSECT", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpChannelOps", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_channel_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_RED_CHANNEL, "GIMP_RED_CHANNEL", "red-channel" },
    { GIMP_GREEN_CHANNEL, "GIMP_GREEN_CHANNEL", "green-channel" },
    { GIMP_BLUE_CHANNEL, "GIMP_BLUE_CHANNEL", "blue-channel" },
    { GIMP_GRAY_CHANNEL, "GIMP_GRAY_CHANNEL", "gray-channel" },
    { GIMP_INDEXED_CHANNEL, "GIMP_INDEXED_CHANNEL", "indexed-channel" },
    { GIMP_ALPHA_CHANNEL, "GIMP_ALPHA_CHANNEL", "alpha-channel" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_RED_CHANNEL, "GIMP_RED_CHANNEL", NULL },
    { GIMP_GREEN_CHANNEL, "GIMP_GREEN_CHANNEL", NULL },
    { GIMP_BLUE_CHANNEL, "GIMP_BLUE_CHANNEL", NULL },
    { GIMP_GRAY_CHANNEL, "GIMP_GRAY_CHANNEL", NULL },
    { GIMP_INDEXED_CHANNEL, "GIMP_INDEXED_CHANNEL", NULL },
    { GIMP_ALPHA_CHANNEL, "GIMP_ALPHA_CHANNEL", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpChannelType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_clone_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_IMAGE_CLONE, "GIMP_IMAGE_CLONE", "image-clone" },
    { GIMP_PATTERN_CLONE, "GIMP_PATTERN_CLONE", "pattern-clone" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_IMAGE_CLONE, "GIMP_IMAGE_CLONE", NULL },
    { GIMP_PATTERN_CLONE, "GIMP_PATTERN_CLONE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpCloneType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_convert_dither_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_NO_DITHER, "GIMP_NO_DITHER", "no-dither" },
    { GIMP_FS_DITHER, "GIMP_FS_DITHER", "fs-dither" },
    { GIMP_FSLOWBLEED_DITHER, "GIMP_FSLOWBLEED_DITHER", "fslowbleed-dither" },
    { GIMP_FIXED_DITHER, "GIMP_FIXED_DITHER", "fixed-dither" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_NO_DITHER, "GIMP_NO_DITHER", NULL },
    { GIMP_FS_DITHER, "GIMP_FS_DITHER", NULL },
    { GIMP_FSLOWBLEED_DITHER, "GIMP_FSLOWBLEED_DITHER", NULL },
    { GIMP_FIXED_DITHER, "GIMP_FIXED_DITHER", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpConvertDitherType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_convert_palette_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_MAKE_PALETTE, "GIMP_MAKE_PALETTE", "make-palette" },
    { GIMP_REUSE_PALETTE, "GIMP_REUSE_PALETTE", "reuse-palette" },
    { GIMP_WEB_PALETTE, "GIMP_WEB_PALETTE", "web-palette" },
    { GIMP_MONO_PALETTE, "GIMP_MONO_PALETTE", "mono-palette" },
    { GIMP_CUSTOM_PALETTE, "GIMP_CUSTOM_PALETTE", "custom-palette" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_MAKE_PALETTE, "GIMP_MAKE_PALETTE", NULL },
    { GIMP_REUSE_PALETTE, "GIMP_REUSE_PALETTE", NULL },
    { GIMP_WEB_PALETTE, "GIMP_WEB_PALETTE", NULL },
    { GIMP_MONO_PALETTE, "GIMP_MONO_PALETTE", NULL },
    { GIMP_CUSTOM_PALETTE, "GIMP_CUSTOM_PALETTE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpConvertPaletteType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_convolution_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_NORMAL_CONVOL, "GIMP_NORMAL_CONVOL", "normal-convol" },
    { GIMP_ABSOLUTE_CONVOL, "GIMP_ABSOLUTE_CONVOL", "absolute-convol" },
    { GIMP_NEGATIVE_CONVOL, "GIMP_NEGATIVE_CONVOL", "negative-convol" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_NORMAL_CONVOL, "GIMP_NORMAL_CONVOL", NULL },
    { GIMP_ABSOLUTE_CONVOL, "GIMP_ABSOLUTE_CONVOL", NULL },
    { GIMP_NEGATIVE_CONVOL, "GIMP_NEGATIVE_CONVOL", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpConvolutionType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_convolve_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_BLUR_CONVOLVE, "GIMP_BLUR_CONVOLVE", "blur-convolve" },
    { GIMP_SHARPEN_CONVOLVE, "GIMP_SHARPEN_CONVOLVE", "sharpen-convolve" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_BLUR_CONVOLVE, "GIMP_BLUR_CONVOLVE", NULL },
    { GIMP_SHARPEN_CONVOLVE, "GIMP_SHARPEN_CONVOLVE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpConvolveType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_dodge_burn_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_DODGE, "GIMP_DODGE", "dodge" },
    { GIMP_BURN, "GIMP_BURN", "burn" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_DODGE, "GIMP_DODGE", NULL },
    { GIMP_BURN, "GIMP_BURN", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpDodgeBurnType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_fill_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_FOREGROUND_FILL, "GIMP_FOREGROUND_FILL", "foreground-fill" },
    { GIMP_BACKGROUND_FILL, "GIMP_BACKGROUND_FILL", "background-fill" },
    { GIMP_WHITE_FILL, "GIMP_WHITE_FILL", "white-fill" },
    { GIMP_TRANSPARENT_FILL, "GIMP_TRANSPARENT_FILL", "transparent-fill" },
    { GIMP_PATTERN_FILL, "GIMP_PATTERN_FILL", "pattern-fill" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_FOREGROUND_FILL, "GIMP_FOREGROUND_FILL", NULL },
    { GIMP_BACKGROUND_FILL, "GIMP_BACKGROUND_FILL", NULL },
    { GIMP_WHITE_FILL, "GIMP_WHITE_FILL", NULL },
    { GIMP_TRANSPARENT_FILL, "GIMP_TRANSPARENT_FILL", NULL },
    { GIMP_PATTERN_FILL, "GIMP_PATTERN_FILL", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpFillType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_gradient_segment_color_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_GRADIENT_SEGMENT_RGB, "GIMP_GRADIENT_SEGMENT_RGB", "rgb" },
    { GIMP_GRADIENT_SEGMENT_HSV_CCW, "GIMP_GRADIENT_SEGMENT_HSV_CCW", "hsv-ccw" },
    { GIMP_GRADIENT_SEGMENT_HSV_CW, "GIMP_GRADIENT_SEGMENT_HSV_CW", "hsv-cw" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_GRADIENT_SEGMENT_RGB, "GIMP_GRADIENT_SEGMENT_RGB", NULL },
    { GIMP_GRADIENT_SEGMENT_HSV_CCW, "GIMP_GRADIENT_SEGMENT_HSV_CCW", NULL },
    { GIMP_GRADIENT_SEGMENT_HSV_CW, "GIMP_GRADIENT_SEGMENT_HSV_CW", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpGradientSegmentColor", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_gradient_segment_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_GRADIENT_SEGMENT_LINEAR, "GIMP_GRADIENT_SEGMENT_LINEAR", "linear" },
    { GIMP_GRADIENT_SEGMENT_CURVED, "GIMP_GRADIENT_SEGMENT_CURVED", "curved" },
    { GIMP_GRADIENT_SEGMENT_SINE, "GIMP_GRADIENT_SEGMENT_SINE", "sine" },
    { GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING, "GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING", "sphere-increasing" },
    { GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING, "GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING", "sphere-decreasing" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_GRADIENT_SEGMENT_LINEAR, "GIMP_GRADIENT_SEGMENT_LINEAR", NULL },
    { GIMP_GRADIENT_SEGMENT_CURVED, "GIMP_GRADIENT_SEGMENT_CURVED", NULL },
    { GIMP_GRADIENT_SEGMENT_SINE, "GIMP_GRADIENT_SEGMENT_SINE", NULL },
    { GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING, "GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING", NULL },
    { GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING, "GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpGradientSegmentType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_gradient_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_GRADIENT_LINEAR, "GIMP_GRADIENT_LINEAR", "linear" },
    { GIMP_GRADIENT_BILINEAR, "GIMP_GRADIENT_BILINEAR", "bilinear" },
    { GIMP_GRADIENT_RADIAL, "GIMP_GRADIENT_RADIAL", "radial" },
    { GIMP_GRADIENT_SQUARE, "GIMP_GRADIENT_SQUARE", "square" },
    { GIMP_GRADIENT_CONICAL_SYMMETRIC, "GIMP_GRADIENT_CONICAL_SYMMETRIC", "conical-symmetric" },
    { GIMP_GRADIENT_CONICAL_ASYMMETRIC, "GIMP_GRADIENT_CONICAL_ASYMMETRIC", "conical-asymmetric" },
    { GIMP_GRADIENT_SHAPEBURST_ANGULAR, "GIMP_GRADIENT_SHAPEBURST_ANGULAR", "shapeburst-angular" },
    { GIMP_GRADIENT_SHAPEBURST_SPHERICAL, "GIMP_GRADIENT_SHAPEBURST_SPHERICAL", "shapeburst-spherical" },
    { GIMP_GRADIENT_SHAPEBURST_DIMPLED, "GIMP_GRADIENT_SHAPEBURST_DIMPLED", "shapeburst-dimpled" },
    { GIMP_GRADIENT_SPIRAL_CLOCKWISE, "GIMP_GRADIENT_SPIRAL_CLOCKWISE", "spiral-clockwise" },
    { GIMP_GRADIENT_SPIRAL_ANTICLOCKWISE, "GIMP_GRADIENT_SPIRAL_ANTICLOCKWISE", "spiral-anticlockwise" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_GRADIENT_LINEAR, "GIMP_GRADIENT_LINEAR", NULL },
    { GIMP_GRADIENT_BILINEAR, "GIMP_GRADIENT_BILINEAR", NULL },
    { GIMP_GRADIENT_RADIAL, "GIMP_GRADIENT_RADIAL", NULL },
    { GIMP_GRADIENT_SQUARE, "GIMP_GRADIENT_SQUARE", NULL },
    { GIMP_GRADIENT_CONICAL_SYMMETRIC, "GIMP_GRADIENT_CONICAL_SYMMETRIC", NULL },
    { GIMP_GRADIENT_CONICAL_ASYMMETRIC, "GIMP_GRADIENT_CONICAL_ASYMMETRIC", NULL },
    { GIMP_GRADIENT_SHAPEBURST_ANGULAR, "GIMP_GRADIENT_SHAPEBURST_ANGULAR", NULL },
    { GIMP_GRADIENT_SHAPEBURST_SPHERICAL, "GIMP_GRADIENT_SHAPEBURST_SPHERICAL", NULL },
    { GIMP_GRADIENT_SHAPEBURST_DIMPLED, "GIMP_GRADIENT_SHAPEBURST_DIMPLED", NULL },
    { GIMP_GRADIENT_SPIRAL_CLOCKWISE, "GIMP_GRADIENT_SPIRAL_CLOCKWISE", NULL },
    { GIMP_GRADIENT_SPIRAL_ANTICLOCKWISE, "GIMP_GRADIENT_SPIRAL_ANTICLOCKWISE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpGradientType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_histogram_channel_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_HISTOGRAM_VALUE, "GIMP_HISTOGRAM_VALUE", "value" },
    { GIMP_HISTOGRAM_RED, "GIMP_HISTOGRAM_RED", "red" },
    { GIMP_HISTOGRAM_GREEN, "GIMP_HISTOGRAM_GREEN", "green" },
    { GIMP_HISTOGRAM_BLUE, "GIMP_HISTOGRAM_BLUE", "blue" },
    { GIMP_HISTOGRAM_ALPHA, "GIMP_HISTOGRAM_ALPHA", "alpha" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_HISTOGRAM_VALUE, "GIMP_HISTOGRAM_VALUE", NULL },
    { GIMP_HISTOGRAM_RED, "GIMP_HISTOGRAM_RED", NULL },
    { GIMP_HISTOGRAM_GREEN, "GIMP_HISTOGRAM_GREEN", NULL },
    { GIMP_HISTOGRAM_BLUE, "GIMP_HISTOGRAM_BLUE", NULL },
    { GIMP_HISTOGRAM_ALPHA, "GIMP_HISTOGRAM_ALPHA", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpHistogramChannel", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_hue_range_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ALL_HUES, "GIMP_ALL_HUES", "all-hues" },
    { GIMP_RED_HUES, "GIMP_RED_HUES", "red-hues" },
    { GIMP_YELLOW_HUES, "GIMP_YELLOW_HUES", "yellow-hues" },
    { GIMP_GREEN_HUES, "GIMP_GREEN_HUES", "green-hues" },
    { GIMP_CYAN_HUES, "GIMP_CYAN_HUES", "cyan-hues" },
    { GIMP_BLUE_HUES, "GIMP_BLUE_HUES", "blue-hues" },
    { GIMP_MAGENTA_HUES, "GIMP_MAGENTA_HUES", "magenta-hues" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ALL_HUES, "GIMP_ALL_HUES", NULL },
    { GIMP_RED_HUES, "GIMP_RED_HUES", NULL },
    { GIMP_YELLOW_HUES, "GIMP_YELLOW_HUES", NULL },
    { GIMP_GREEN_HUES, "GIMP_GREEN_HUES", NULL },
    { GIMP_CYAN_HUES, "GIMP_CYAN_HUES", NULL },
    { GIMP_BLUE_HUES, "GIMP_BLUE_HUES", NULL },
    { GIMP_MAGENTA_HUES, "GIMP_MAGENTA_HUES", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpHueRange", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_icon_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ICON_TYPE_STOCK_ID, "GIMP_ICON_TYPE_STOCK_ID", "stock-id" },
    { GIMP_ICON_TYPE_INLINE_PIXBUF, "GIMP_ICON_TYPE_INLINE_PIXBUF", "inline-pixbuf" },
    { GIMP_ICON_TYPE_IMAGE_FILE, "GIMP_ICON_TYPE_IMAGE_FILE", "image-file" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ICON_TYPE_STOCK_ID, "GIMP_ICON_TYPE_STOCK_ID", NULL },
    { GIMP_ICON_TYPE_INLINE_PIXBUF, "GIMP_ICON_TYPE_INLINE_PIXBUF", NULL },
    { GIMP_ICON_TYPE_IMAGE_FILE, "GIMP_ICON_TYPE_IMAGE_FILE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpIconType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_interpolation_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_INTERPOLATION_NONE, "GIMP_INTERPOLATION_NONE", "none" },
    { GIMP_INTERPOLATION_LINEAR, "GIMP_INTERPOLATION_LINEAR", "linear" },
    { GIMP_INTERPOLATION_CUBIC, "GIMP_INTERPOLATION_CUBIC", "cubic" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_INTERPOLATION_NONE, "GIMP_INTERPOLATION_NONE", NULL },
    { GIMP_INTERPOLATION_LINEAR, "GIMP_INTERPOLATION_LINEAR", NULL },
    { GIMP_INTERPOLATION_CUBIC, "GIMP_INTERPOLATION_CUBIC", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpInterpolationType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_layer_mode_effects_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_NORMAL_MODE, "GIMP_NORMAL_MODE", "normal-mode" },
    { GIMP_DISSOLVE_MODE, "GIMP_DISSOLVE_MODE", "dissolve-mode" },
    { GIMP_BEHIND_MODE, "GIMP_BEHIND_MODE", "behind-mode" },
    { GIMP_MULTIPLY_MODE, "GIMP_MULTIPLY_MODE", "multiply-mode" },
    { GIMP_SCREEN_MODE, "GIMP_SCREEN_MODE", "screen-mode" },
    { GIMP_OVERLAY_MODE, "GIMP_OVERLAY_MODE", "overlay-mode" },
    { GIMP_DIFFERENCE_MODE, "GIMP_DIFFERENCE_MODE", "difference-mode" },
    { GIMP_ADDITION_MODE, "GIMP_ADDITION_MODE", "addition-mode" },
    { GIMP_SUBTRACT_MODE, "GIMP_SUBTRACT_MODE", "subtract-mode" },
    { GIMP_DARKEN_ONLY_MODE, "GIMP_DARKEN_ONLY_MODE", "darken-only-mode" },
    { GIMP_LIGHTEN_ONLY_MODE, "GIMP_LIGHTEN_ONLY_MODE", "lighten-only-mode" },
    { GIMP_HUE_MODE, "GIMP_HUE_MODE", "hue-mode" },
    { GIMP_SATURATION_MODE, "GIMP_SATURATION_MODE", "saturation-mode" },
    { GIMP_COLOR_MODE, "GIMP_COLOR_MODE", "color-mode" },
    { GIMP_VALUE_MODE, "GIMP_VALUE_MODE", "value-mode" },
    { GIMP_DIVIDE_MODE, "GIMP_DIVIDE_MODE", "divide-mode" },
    { GIMP_DODGE_MODE, "GIMP_DODGE_MODE", "dodge-mode" },
    { GIMP_BURN_MODE, "GIMP_BURN_MODE", "burn-mode" },
    { GIMP_HARDLIGHT_MODE, "GIMP_HARDLIGHT_MODE", "hardlight-mode" },
    { GIMP_SOFTLIGHT_MODE, "GIMP_SOFTLIGHT_MODE", "softlight-mode" },
    { GIMP_GRAIN_EXTRACT_MODE, "GIMP_GRAIN_EXTRACT_MODE", "grain-extract-mode" },
    { GIMP_GRAIN_MERGE_MODE, "GIMP_GRAIN_MERGE_MODE", "grain-merge-mode" },
    { GIMP_COLOR_ERASE_MODE, "GIMP_COLOR_ERASE_MODE", "color-erase-mode" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_NORMAL_MODE, "GIMP_NORMAL_MODE", NULL },
    { GIMP_DISSOLVE_MODE, "GIMP_DISSOLVE_MODE", NULL },
    { GIMP_BEHIND_MODE, "GIMP_BEHIND_MODE", NULL },
    { GIMP_MULTIPLY_MODE, "GIMP_MULTIPLY_MODE", NULL },
    { GIMP_SCREEN_MODE, "GIMP_SCREEN_MODE", NULL },
    { GIMP_OVERLAY_MODE, "GIMP_OVERLAY_MODE", NULL },
    { GIMP_DIFFERENCE_MODE, "GIMP_DIFFERENCE_MODE", NULL },
    { GIMP_ADDITION_MODE, "GIMP_ADDITION_MODE", NULL },
    { GIMP_SUBTRACT_MODE, "GIMP_SUBTRACT_MODE", NULL },
    { GIMP_DARKEN_ONLY_MODE, "GIMP_DARKEN_ONLY_MODE", NULL },
    { GIMP_LIGHTEN_ONLY_MODE, "GIMP_LIGHTEN_ONLY_MODE", NULL },
    { GIMP_HUE_MODE, "GIMP_HUE_MODE", NULL },
    { GIMP_SATURATION_MODE, "GIMP_SATURATION_MODE", NULL },
    { GIMP_COLOR_MODE, "GIMP_COLOR_MODE", NULL },
    { GIMP_VALUE_MODE, "GIMP_VALUE_MODE", NULL },
    { GIMP_DIVIDE_MODE, "GIMP_DIVIDE_MODE", NULL },
    { GIMP_DODGE_MODE, "GIMP_DODGE_MODE", NULL },
    { GIMP_BURN_MODE, "GIMP_BURN_MODE", NULL },
    { GIMP_HARDLIGHT_MODE, "GIMP_HARDLIGHT_MODE", NULL },
    { GIMP_SOFTLIGHT_MODE, "GIMP_SOFTLIGHT_MODE", NULL },
    { GIMP_GRAIN_EXTRACT_MODE, "GIMP_GRAIN_EXTRACT_MODE", NULL },
    { GIMP_GRAIN_MERGE_MODE, "GIMP_GRAIN_MERGE_MODE", NULL },
    { GIMP_COLOR_ERASE_MODE, "GIMP_COLOR_ERASE_MODE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpLayerModeEffects", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_mask_apply_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_MASK_APPLY, "GIMP_MASK_APPLY", "apply" },
    { GIMP_MASK_DISCARD, "GIMP_MASK_DISCARD", "discard" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_MASK_APPLY, "GIMP_MASK_APPLY", NULL },
    { GIMP_MASK_DISCARD, "GIMP_MASK_DISCARD", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpMaskApplyMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_merge_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_EXPAND_AS_NECESSARY, "GIMP_EXPAND_AS_NECESSARY", "expand-as-necessary" },
    { GIMP_CLIP_TO_IMAGE, "GIMP_CLIP_TO_IMAGE", "clip-to-image" },
    { GIMP_CLIP_TO_BOTTOM_LAYER, "GIMP_CLIP_TO_BOTTOM_LAYER", "clip-to-bottom-layer" },
    { GIMP_FLATTEN_IMAGE, "GIMP_FLATTEN_IMAGE", "flatten-image" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_EXPAND_AS_NECESSARY, "GIMP_EXPAND_AS_NECESSARY", NULL },
    { GIMP_CLIP_TO_IMAGE, "GIMP_CLIP_TO_IMAGE", NULL },
    { GIMP_CLIP_TO_BOTTOM_LAYER, "GIMP_CLIP_TO_BOTTOM_LAYER", NULL },
    { GIMP_FLATTEN_IMAGE, "GIMP_FLATTEN_IMAGE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpMergeType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_offset_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_OFFSET_BACKGROUND, "GIMP_OFFSET_BACKGROUND", "background" },
    { GIMP_OFFSET_TRANSPARENT, "GIMP_OFFSET_TRANSPARENT", "transparent" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_OFFSET_BACKGROUND, "GIMP_OFFSET_BACKGROUND", NULL },
    { GIMP_OFFSET_TRANSPARENT, "GIMP_OFFSET_TRANSPARENT", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpOffsetType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_orientation_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ORIENTATION_HORIZONTAL, "GIMP_ORIENTATION_HORIZONTAL", "horizontal" },
    { GIMP_ORIENTATION_VERTICAL, "GIMP_ORIENTATION_VERTICAL", "vertical" },
    { GIMP_ORIENTATION_UNKNOWN, "GIMP_ORIENTATION_UNKNOWN", "unknown" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ORIENTATION_HORIZONTAL, "GIMP_ORIENTATION_HORIZONTAL", NULL },
    { GIMP_ORIENTATION_VERTICAL, "GIMP_ORIENTATION_VERTICAL", NULL },
    { GIMP_ORIENTATION_UNKNOWN, "GIMP_ORIENTATION_UNKNOWN", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpOrientationType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_paint_application_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_PAINT_CONSTANT, "GIMP_PAINT_CONSTANT", "constant" },
    { GIMP_PAINT_INCREMENTAL, "GIMP_PAINT_INCREMENTAL", "incremental" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_PAINT_CONSTANT, "GIMP_PAINT_CONSTANT", NULL },
    { GIMP_PAINT_INCREMENTAL, "GIMP_PAINT_INCREMENTAL", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpPaintApplicationMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_repeat_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_REPEAT_NONE, "GIMP_REPEAT_NONE", "none" },
    { GIMP_REPEAT_SAWTOOTH, "GIMP_REPEAT_SAWTOOTH", "sawtooth" },
    { GIMP_REPEAT_TRIANGULAR, "GIMP_REPEAT_TRIANGULAR", "triangular" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_REPEAT_NONE, "GIMP_REPEAT_NONE", NULL },
    { GIMP_REPEAT_SAWTOOTH, "GIMP_REPEAT_SAWTOOTH", NULL },
    { GIMP_REPEAT_TRIANGULAR, "GIMP_REPEAT_TRIANGULAR", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRepeatMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_rotation_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ROTATE_90, "GIMP_ROTATE_90", "90" },
    { GIMP_ROTATE_180, "GIMP_ROTATE_180", "180" },
    { GIMP_ROTATE_270, "GIMP_ROTATE_270", "270" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ROTATE_90, "GIMP_ROTATE_90", NULL },
    { GIMP_ROTATE_180, "GIMP_ROTATE_180", NULL },
    { GIMP_ROTATE_270, "GIMP_ROTATE_270", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRotationType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_run_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_RUN_INTERACTIVE, "GIMP_RUN_INTERACTIVE", "interactive" },
    { GIMP_RUN_NONINTERACTIVE, "GIMP_RUN_NONINTERACTIVE", "noninteractive" },
    { GIMP_RUN_WITH_LAST_VALS, "GIMP_RUN_WITH_LAST_VALS", "with-last-vals" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_RUN_INTERACTIVE, "GIMP_RUN_INTERACTIVE", NULL },
    { GIMP_RUN_NONINTERACTIVE, "GIMP_RUN_NONINTERACTIVE", NULL },
    { GIMP_RUN_WITH_LAST_VALS, "GIMP_RUN_WITH_LAST_VALS", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpRunMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_size_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_PIXELS, "GIMP_PIXELS", "pixels" },
    { GIMP_POINTS, "GIMP_POINTS", "points" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_PIXELS, "GIMP_PIXELS", NULL },
    { GIMP_POINTS, "GIMP_POINTS", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpSizeType", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_transfer_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_SHADOWS, "GIMP_SHADOWS", "shadows" },
    { GIMP_MIDTONES, "GIMP_MIDTONES", "midtones" },
    { GIMP_HIGHLIGHTS, "GIMP_HIGHLIGHTS", "highlights" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_SHADOWS, "GIMP_SHADOWS", NULL },
    { GIMP_MIDTONES, "GIMP_MIDTONES", NULL },
    { GIMP_HIGHLIGHTS, "GIMP_HIGHLIGHTS", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTransferMode", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_transform_direction_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TRANSFORM_FORWARD, "GIMP_TRANSFORM_FORWARD", "forward" },
    { GIMP_TRANSFORM_BACKWARD, "GIMP_TRANSFORM_BACKWARD", "backward" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TRANSFORM_FORWARD, "GIMP_TRANSFORM_FORWARD", NULL },
    { GIMP_TRANSFORM_BACKWARD, "GIMP_TRANSFORM_BACKWARD", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTransformDirection", values);
      gimp_type_set_translation_domain (type, GETTEXT_PACKAGE "-libgimp");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}


/* Generated data ends here */


typedef GType (* GimpGetTypeFunc) (void);

static GimpGetTypeFunc get_type_funcs[] =
{
  gimp_add_mask_type_get_type,
  gimp_blend_mode_get_type,
  gimp_brush_application_mode_get_type,
  gimp_bucket_fill_mode_get_type,
  gimp_channel_ops_get_type,
  gimp_channel_type_get_type,
  gimp_clone_type_get_type,
  gimp_convert_dither_type_get_type,
  gimp_convert_palette_type_get_type,
  gimp_convolution_type_get_type,
  gimp_convolve_type_get_type,
  gimp_dodge_burn_type_get_type,
  gimp_fill_type_get_type,
  gimp_gradient_segment_color_get_type,
  gimp_gradient_segment_type_get_type,
  gimp_gradient_type_get_type,
  gimp_histogram_channel_get_type,
  gimp_hue_range_get_type,
  gimp_icon_type_get_type,
  gimp_image_base_type_get_type,
  gimp_image_type_get_type,
  gimp_interpolation_type_get_type,
  gimp_layer_mode_effects_get_type,
  gimp_mask_apply_mode_get_type,
  gimp_merge_type_get_type,
  gimp_message_handler_type_get_type,
  gimp_offset_type_get_type,
  gimp_orientation_type_get_type,
  gimp_pdb_arg_type_get_type,
  gimp_pdb_proc_type_get_type,
  gimp_pdb_status_type_get_type,
  gimp_paint_application_mode_get_type,
  gimp_progress_command_get_type,
  gimp_repeat_mode_get_type,
  gimp_rotation_type_get_type,
  gimp_run_mode_get_type,
  gimp_size_type_get_type,
  gimp_stack_trace_mode_get_type,
  gimp_transfer_mode_get_type,
  gimp_transform_direction_get_type
};

static const gchar *type_names[] =
{
  "GimpAddMaskType",
  "GimpBlendMode",
  "GimpBrushApplicationMode",
  "GimpBucketFillMode",
  "GimpChannelOps",
  "GimpChannelType",
  "GimpCloneType",
  "GimpConvertDitherType",
  "GimpConvertPaletteType",
  "GimpConvolutionType",
  "GimpConvolveType",
  "GimpDodgeBurnType",
  "GimpFillType",
  "GimpGradientSegmentColor",
  "GimpGradientSegmentType",
  "GimpGradientType",
  "GimpHistogramChannel",
  "GimpHueRange",
  "GimpIconType",
  "GimpImageBaseType",
  "GimpImageType",
  "GimpInterpolationType",
  "GimpLayerModeEffects",
  "GimpMaskApplyMode",
  "GimpMergeType",
  "GimpMessageHandlerType",
  "GimpOffsetType",
  "GimpOrientationType",
  "GimpPDBArgType",
  "GimpPDBProcType",
  "GimpPDBStatusType",
  "GimpPaintApplicationMode",
  "GimpProgressCommand",
  "GimpRepeatMode",
  "GimpRotationType",
  "GimpRunMode",
  "GimpSizeType",
  "GimpStackTraceMode",
  "GimpTransferMode",
  "GimpTransformDirection"
};

void
_gimp_enums_init (void)
{
  GimpGetTypeFunc *funcs;
  gint             i;

  for (i = 0, funcs = get_type_funcs;
       i < G_N_ELEMENTS (get_type_funcs);
       i++, funcs++)
    {
      GType type = (*funcs) ();

      g_type_class_ref (type);
    }
}

/**
 * gimp_enums_get_type_names:
 * @n_type_names: return location for the number of names
 *
 * This function gives access to the list of enums registered by libgimp.
 * The returned array is static and must not be modified.
 *
 * Return value: an array with type names
 *
 * Since: GIMP 2.2
 **/
const gchar **
gimp_enums_get_type_names (gint *n_type_names)
{
  g_return_val_if_fail (n_type_names != NULL, NULL);

  *n_type_names = G_N_ELEMENTS (type_names);

  return type_names;
}
