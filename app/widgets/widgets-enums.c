
/* Generated data (by gimp-mkenums) */

#include "config.h"
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets-enums.h"
#include "gimp-intl.h"

/* enumerations from "./widgets-enums.h" */
GType
gimp_active_color_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ACTIVE_COLOR_FOREGROUND, "GIMP_ACTIVE_COLOR_FOREGROUND", "foreground" },
    { GIMP_ACTIVE_COLOR_BACKGROUND, "GIMP_ACTIVE_COLOR_BACKGROUND", "background" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ACTIVE_COLOR_FOREGROUND, N_("Foreground"), NULL },
    { GIMP_ACTIVE_COLOR_BACKGROUND, N_("Background"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpActiveColor", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_aspect_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ASPECT_SQUARE, "GIMP_ASPECT_SQUARE", "square" },
    { GIMP_ASPECT_PORTRAIT, "GIMP_ASPECT_PORTRAIT", "portrait" },
    { GIMP_ASPECT_LANDSCAPE, "GIMP_ASPECT_LANDSCAPE", "landscape" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ASPECT_SQUARE, "GIMP_ASPECT_SQUARE", NULL },
    { GIMP_ASPECT_PORTRAIT, N_("Portrait"), NULL },
    { GIMP_ASPECT_LANDSCAPE, N_("Landscape"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpAspectType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_dialog_state_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_DIALOG_OK, "GIMP_COLOR_DIALOG_OK", "ok" },
    { GIMP_COLOR_DIALOG_CANCEL, "GIMP_COLOR_DIALOG_CANCEL", "cancel" },
    { GIMP_COLOR_DIALOG_UPDATE, "GIMP_COLOR_DIALOG_UPDATE", "update" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_DIALOG_OK, "GIMP_COLOR_DIALOG_OK", NULL },
    { GIMP_COLOR_DIALOG_CANCEL, "GIMP_COLOR_DIALOG_CANCEL", NULL },
    { GIMP_COLOR_DIALOG_UPDATE, "GIMP_COLOR_DIALOG_UPDATE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpColorDialogState", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_frame_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_FRAME_MODE_PIXEL, "GIMP_COLOR_FRAME_MODE_PIXEL", "pixel" },
    { GIMP_COLOR_FRAME_MODE_RGB, "GIMP_COLOR_FRAME_MODE_RGB", "rgb" },
    { GIMP_COLOR_FRAME_MODE_HSV, "GIMP_COLOR_FRAME_MODE_HSV", "hsv" },
    { GIMP_COLOR_FRAME_MODE_CMYK, "GIMP_COLOR_FRAME_MODE_CMYK", "cmyk" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_FRAME_MODE_PIXEL, N_("Pixel values"), NULL },
    { GIMP_COLOR_FRAME_MODE_RGB, N_("RGB"), NULL },
    { GIMP_COLOR_FRAME_MODE_HSV, N_("HSV"), NULL },
    { GIMP_COLOR_FRAME_MODE_CMYK, N_("CMYK"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpColorFrameMode", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_pick_state_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_PICK_STATE_NEW, "GIMP_COLOR_PICK_STATE_NEW", "new" },
    { GIMP_COLOR_PICK_STATE_UPDATE, "GIMP_COLOR_PICK_STATE_UPDATE", "update" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_PICK_STATE_NEW, "GIMP_COLOR_PICK_STATE_NEW", NULL },
    { GIMP_COLOR_PICK_STATE_UPDATE, "GIMP_COLOR_PICK_STATE_UPDATE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpColorPickState", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_cursor_format_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_CURSOR_FORMAT_BITMAP, "GIMP_CURSOR_FORMAT_BITMAP", "bitmap" },
    { GIMP_CURSOR_FORMAT_PIXBUF, "GIMP_CURSOR_FORMAT_PIXBUF", "pixbuf" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_CURSOR_FORMAT_BITMAP, N_("Black & white"), NULL },
    { GIMP_CURSOR_FORMAT_PIXBUF, N_("Fancy"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpCursorFormat", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_help_browser_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_HELP_BROWSER_GIMP, "GIMP_HELP_BROWSER_GIMP", "gimp" },
    { GIMP_HELP_BROWSER_WEB_BROWSER, "GIMP_HELP_BROWSER_WEB_BROWSER", "web-browser" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_HELP_BROWSER_GIMP, N_("GIMP help browser"), NULL },
    { GIMP_HELP_BROWSER_WEB_BROWSER, N_("Web browser"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpHelpBrowserType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_histogram_scale_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_HISTOGRAM_SCALE_LINEAR, "GIMP_HISTOGRAM_SCALE_LINEAR", "linear" },
    { GIMP_HISTOGRAM_SCALE_LOGARITHMIC, "GIMP_HISTOGRAM_SCALE_LOGARITHMIC", "logarithmic" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_HISTOGRAM_SCALE_LINEAR, N_("Linear"), NULL },
    { GIMP_HISTOGRAM_SCALE_LOGARITHMIC, N_("Logarithmic"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpHistogramScale", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_tab_style_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TAB_STYLE_ICON, "GIMP_TAB_STYLE_ICON", "icon" },
    { GIMP_TAB_STYLE_PREVIEW, "GIMP_TAB_STYLE_PREVIEW", "preview" },
    { GIMP_TAB_STYLE_NAME, "GIMP_TAB_STYLE_NAME", "name" },
    { GIMP_TAB_STYLE_BLURB, "GIMP_TAB_STYLE_BLURB", "blurb" },
    { GIMP_TAB_STYLE_ICON_NAME, "GIMP_TAB_STYLE_ICON_NAME", "icon-name" },
    { GIMP_TAB_STYLE_ICON_BLURB, "GIMP_TAB_STYLE_ICON_BLURB", "icon-blurb" },
    { GIMP_TAB_STYLE_PREVIEW_NAME, "GIMP_TAB_STYLE_PREVIEW_NAME", "preview-name" },
    { GIMP_TAB_STYLE_PREVIEW_BLURB, "GIMP_TAB_STYLE_PREVIEW_BLURB", "preview-blurb" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TAB_STYLE_ICON, N_("Icon"), NULL },
    { GIMP_TAB_STYLE_PREVIEW, N_("Current status"), NULL },
    { GIMP_TAB_STYLE_NAME, N_("Text"), NULL },
    { GIMP_TAB_STYLE_BLURB, N_("Description"), NULL },
    { GIMP_TAB_STYLE_ICON_NAME, N_("Icon & text"), NULL },
    { GIMP_TAB_STYLE_ICON_BLURB, N_("Icon & desc"), NULL },
    { GIMP_TAB_STYLE_PREVIEW_NAME, N_("Status & text"), NULL },
    { GIMP_TAB_STYLE_PREVIEW_BLURB, N_("Status & desc"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpTabStyle", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_view_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_VIEW_TYPE_LIST, "GIMP_VIEW_TYPE_LIST", "list" },
    { GIMP_VIEW_TYPE_GRID, "GIMP_VIEW_TYPE_GRID", "grid" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_VIEW_TYPE_LIST, N_("View as list"), NULL },
    { GIMP_VIEW_TYPE_GRID, N_("View as grid"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpViewType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_window_hint_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_WINDOW_HINT_NORMAL, "GIMP_WINDOW_HINT_NORMAL", "normal" },
    { GIMP_WINDOW_HINT_UTILITY, "GIMP_WINDOW_HINT_UTILITY", "utility" },
    { GIMP_WINDOW_HINT_KEEP_ABOVE, "GIMP_WINDOW_HINT_KEEP_ABOVE", "keep-above" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_WINDOW_HINT_NORMAL, N_("Normal window"), NULL },
    { GIMP_WINDOW_HINT_UTILITY, N_("Utility window"), NULL },
    { GIMP_WINDOW_HINT_KEEP_ABOVE, N_("Keep above"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpWindowHint", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_zoom_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ZOOM_IN, "GIMP_ZOOM_IN", "in" },
    { GIMP_ZOOM_OUT, "GIMP_ZOOM_OUT", "out" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ZOOM_IN, N_("Zoom in"), NULL },
    { GIMP_ZOOM_OUT, N_("Zoom out"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (! type)
    {
      type = g_enum_register_static ("GimpZoomType", values);
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}


/* Generated data ends here */
