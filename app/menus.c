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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "appenv.h"
#include "colormaps.h"
#include "commands.h"
#include "fileops.h"
#include "gimprc.h"
#include "interface.h"
#include "menus.h"
#include "paint_funcs.h"
#include "procedural_db.h"
#include "scale.h"
#include "tools.h"
#include "gdisplay.h"

static void menus_init (void);

static GtkItemFactoryEntry toolbox_entries[] =
{
  { "/File/New", "<control>N", file_new_cmd_callback, 0 },
  { "/File/Open", "<control>O", file_open_cmd_callback, 0 },
  { "/File/About...", NULL, about_dialog_cmd_callback, 0 },
  { "/File/Preferences...", NULL, file_pref_cmd_callback, 0 },
  { "/File/Tip of the day", NULL, tips_dialog_cmd_callback, 0 },
  { "/File/---", NULL, NULL, 0, "<Separator>" },
  { "/File/Dialogs/Brushes...", "<control><shift>B", dialogs_brushes_cmd_callback, 0 },
  { "/File/Dialogs/Patterns...", "<control><shift>P", dialogs_patterns_cmd_callback, 0 },
  { "/File/Dialogs/Palette...", "<control>P", dialogs_palette_cmd_callback, 0 },
  { "/File/Dialogs/Gradient Editor...", "<control>G", dialogs_gradient_editor_cmd_callback, 0 },
  { "/File/Dialogs/Tool Options...", "<control><shift>T", dialogs_tools_options_cmd_callback, 0 },
  { "/File/---", NULL, NULL, 0, "<Separator>" },
  { "/File/Quit", "<control>Q", file_quit_cmd_callback, 0 },
};
static guint n_toolbox_entries = sizeof (toolbox_entries) / sizeof (toolbox_entries[0]);
static GtkItemFactory *toolbox_factory = NULL;
 
static GtkItemFactoryEntry image_entries[] =
{
  { "/File/New", "<control>N", file_new_cmd_callback, 1 },
  { "/File/Open", "<control>O", file_open_cmd_callback, 0 },
  { "/File/Save", "<control>S", file_save_cmd_callback, 0 },
  { "/File/Save as", NULL, file_save_as_cmd_callback, 0 },
  { "/File/Preferences...", NULL, file_pref_cmd_callback, 0 },
  { "/File/---", NULL, NULL, 0, "<Separator>" },
  
  
  { "/File/Close", "<control>W", file_close_cmd_callback, 0 },
  { "/File/Quit", "<control>Q", file_quit_cmd_callback, 0 },
  { "/File/---", NULL, NULL, 0, "<Separator>" },
  
  { "/Edit/Cut", "<control>X", edit_cut_cmd_callback, 0 },
  { "/Edit/Copy", "<control>C", edit_copy_cmd_callback, 0 },
  { "/Edit/Paste", "<control>V", edit_paste_cmd_callback, 0 },
  { "/Edit/Paste Into", NULL, edit_paste_into_cmd_callback, 0 },
  { "/Edit/Clear", "<control>K", edit_clear_cmd_callback, 0 },
  { "/Edit/Fill", "<control>.", edit_fill_cmd_callback, 0 },
  { "/Edit/Stroke", NULL, edit_stroke_cmd_callback, 0 },
  { "/Edit/Undo", "<control>Z", edit_undo_cmd_callback, 0 },
  { "/Edit/Redo", "<control>R", edit_redo_cmd_callback, 0 },
  { "/Edit/---", NULL, NULL, 0, "<Separator>" },
  { "/Edit/Cut Named", "<control><shift>X", edit_named_cut_cmd_callback, 0 },
  { "/Edit/Copy Named", "<control><shift>C", edit_named_copy_cmd_callback, 0 },
  { "/Edit/Paste Named", "<control><shift>V", edit_named_paste_cmd_callback, 0 },
  { "/Edit/---", NULL, NULL, 0, "<Separator>" },
  
  { "/Select/Toggle", "<control>T", select_toggle_cmd_callback, 0 },
  { "/Select/Invert", "<control>I", select_invert_cmd_callback, 0 },
  { "/Select/All", "<control>A", select_all_cmd_callback, 0 },
  { "/Select/None", "<control><shift>A", select_none_cmd_callback, 0 },
  { "/Select/Float", "<control><shift>L", select_float_cmd_callback, 0 },
  { "/Select/Sharpen", "<control><shift>H", select_sharpen_cmd_callback, 0 },
  { "/Select/Border", "<control><shift>B", select_border_cmd_callback, 0 },
  { "/Select/Feather", "<control><shift>F", select_feather_cmd_callback, 0 },
  { "/Select/Grow", NULL, select_grow_cmd_callback, 0 },
  { "/Select/Shrink", NULL, select_shrink_cmd_callback, 0 },
  { "/Select/Save To Channel", NULL, select_save_cmd_callback, 0 },
  { "/Select/By Color...", NULL, select_by_color_cmd_callback, 0 },
  
  { "/View/Zoom In", "equal", view_zoomin_cmd_callback, 0 },
  { "/View/Zoom Out", "minus", view_zoomout_cmd_callback, 0 },
  { "/View/Zoom/16:1", NULL, view_zoom_16_1_callback, 0 },
  { "/View/Zoom/8:1", NULL, view_zoom_8_1_callback, 0 },
  { "/View/Zoom/4:1", NULL, view_zoom_4_1_callback, 0 },
  { "/View/Zoom/2:1", NULL, view_zoom_2_1_callback, 0 },
  { "/View/Zoom/1:1", "1", view_zoom_1_1_callback, 0 },
  { "/View/Zoom/1:2", NULL, view_zoom_1_2_callback, 0 },
  { "/View/Zoom/1:4", NULL, view_zoom_1_4_callback, 0 },
  { "/View/Zoom/1:8", NULL, view_zoom_1_8_callback, 0 },
  { "/View/Zoom/1:16", NULL, view_zoom_1_16_callback, 0 },
  { "/View/Window Info...", "<control><shift>I", view_window_info_cmd_callback, 0 },
  { "/View/Toggle Rulers", "<control><shift>R", view_toggle_rulers_cmd_callback, 0, "<ToggleItem>" },
  { "/View/Toggle Guides", "<control><shift>T", view_toggle_guides_cmd_callback, 0, "<ToggleItem>" },
  { "/View/Snap To Guides", NULL, view_snap_to_guides_cmd_callback, 0, "<ToggleItem>" },
  { "/View/---", NULL, NULL, 0, "<Separator>" },
  { "/View/New View", NULL, view_new_view_cmd_callback, 0 },
  { "/View/Shrink Wrap", "<control>E", view_shrink_wrap_cmd_callback, 0 },
  
  { "/Image/Colors/Equalize", NULL, image_equalize_cmd_callback, 0 },
  { "/Image/Colors/Invert", NULL, image_invert_cmd_callback, 0 },
  { "/Image/Colors/Posterize", NULL, image_posterize_cmd_callback, 0 },
  { "/Image/Colors/Threshold", NULL, image_threshold_cmd_callback, 0 },
  { "/Image/Colors/---", NULL, NULL, 0, "<Separator>" },
  { "/Image/Colors/Color Balance", NULL, image_color_balance_cmd_callback, 0 },
  { "/Image/Colors/Brightness-Contrast", NULL, image_brightness_contrast_cmd_callback, 0 },
  { "/Image/Colors/Hue-Saturation", NULL, image_hue_saturation_cmd_callback, 0 },
  { "/Image/Colors/Curves", NULL, image_curves_cmd_callback, 0 },
  { "/Image/Colors/Levels", NULL, image_levels_cmd_callback, 0 },
  { "/Image/Colors/---", NULL, NULL, 0, "<Separator>" },
  { "/Image/Colors/Desaturate", NULL, image_desaturate_cmd_callback, 0 },
  { "/Image/Channel Ops/Duplicate", "<control>D", channel_ops_duplicate_cmd_callback, 0 },
  { "/Image/Channel Ops/Offset", "<control><shift>O", channel_ops_offset_cmd_callback, 0 },
  { "/Image/Alpha/Add Alpha Channel", NULL, layers_add_alpha_channel_cmd_callback, 0 },
  
  { "/Image/---", NULL, NULL, 0, "<Separator>" },
  { "/Image/RGB", NULL, image_convert_rgb_cmd_callback, 0 },
  { "/Image/Grayscale", NULL, image_convert_grayscale_cmd_callback, 0 },
  { "/Image/Indexed", NULL, image_convert_indexed_cmd_callback, 0 },
  { "/Image/---", NULL, NULL, 0, "<Separator>" },
  { "/Image/Resize", NULL, image_resize_cmd_callback, 0 },
  { "/Image/Scale", NULL, image_scale_cmd_callback, 0 },
  { "/Image/---", NULL, NULL, 0, "<Separator>" },
  { "/Image/Histogram", NULL, image_histogram_cmd_callback, 0 },
  { "/Image/---", NULL, NULL, 0, "<Separator>" },
  
  { "/Layers/Layers & Channels...", "<control>L", dialogs_lc_cmd_callback, 0 },
  { "/Layers/Raise Layer", "<control>F", layers_raise_cmd_callback, 0 },
  { "/Layers/Lower Layer", "<control>B", layers_lower_cmd_callback, 0 },
  { "/Layers/Anchor Layer", "<control>H", layers_anchor_cmd_callback, 0 },
  { "/Layers/Merge Visible Layers", "<control>M", layers_merge_cmd_callback, 0 },
  { "/Layers/Flatten Image", NULL, layers_flatten_cmd_callback, 0 },
  { "/Layers/Alpha To Selection", NULL, layers_alpha_select_cmd_callback, 0 },
  { "/Layers/Mask To Selection", NULL, layers_mask_select_cmd_callback, 0 },
  { "/Layers/Add Alpha Channel", NULL, layers_add_alpha_channel_cmd_callback, 0 },
  
  { "/Tools/Rect Select", "R", tools_select_cmd_callback, RECT_SELECT },
  { "/Tools/Ellipse Select", "E", tools_select_cmd_callback, ELLIPSE_SELECT },
  { "/Tools/Free Select", "F", tools_select_cmd_callback, FREE_SELECT },
  { "/Tools/Fuzzy Select", "Z", tools_select_cmd_callback, FUZZY_SELECT },
  { "/Tools/Bezier Select", "B", tools_select_cmd_callback, BEZIER_SELECT },
  { "/Tools/Intelligent Scissors", "I", tools_select_cmd_callback, ISCISSORS },
  { "/Tools/Move", "M", tools_select_cmd_callback, MOVE },
  { "/Tools/Magnify", "<shift>M", tools_select_cmd_callback, MAGNIFY },
  { "/Tools/Crop", "<shift>C", tools_select_cmd_callback, CROP },
  { "/Tools/Transform", "<shift>T", tools_select_cmd_callback, ROTATE },
  { "/Tools/Flip", "<shift>F", tools_select_cmd_callback, FLIP_HORZ },
  { "/Tools/Text", "T", tools_select_cmd_callback, TEXT },
  { "/Tools/Color Picker", "O", tools_select_cmd_callback, COLOR_PICKER },
  { "/Tools/Bucket Fill", "<shift>B", tools_select_cmd_callback, BUCKET_FILL },
  { "/Tools/Blend", "L", tools_select_cmd_callback, BLEND },
  { "/Tools/Paintbrush", "P", tools_select_cmd_callback, PAINTBRUSH },
  { "/Tools/Pencil", "<shift>P", tools_select_cmd_callback, PENCIL },
  { "/Tools/Eraser", "<shift>E", tools_select_cmd_callback, ERASER },
  { "/Tools/Airbrush", "A", tools_select_cmd_callback, AIRBRUSH },
  { "/Tools/Clone", "C", tools_select_cmd_callback, CLONE },
  { "/Tools/Convolve", "V", tools_select_cmd_callback, CONVOLVE },
  { "/Tools/Default Colors", "D", tools_default_colors_cmd_callback, 0 },
  { "/Tools/Swap Colors", "X", tools_swap_colors_cmd_callback, 0 },  
  { "/Tools/---", NULL, NULL, 0, "<Separator>" },
  { "/Tools/Toolbox", NULL, toolbox_raise_callback, 0 },
  
  { "/Filters/", NULL, NULL, 0 },
  { "/Filters/Repeat last", "<alt>F", filters_repeat_cmd_callback, 0x0 },
  { "/Filters/Re-show last", "<alt><shift>F", filters_repeat_cmd_callback, 0x1 },
  { "/Filters/---", NULL, NULL, 0, "<Separator>" },
  
  { "/Script-Fu/", NULL, NULL, 0 },
  
  { "/Dialogs/Brushes...", "<control><shift>B", dialogs_brushes_cmd_callback, 0 },
  { "/Dialogs/Patterns...", "<control><shift>P", dialogs_patterns_cmd_callback, 0 },
  { "/Dialogs/Palette...", "<control>P", dialogs_palette_cmd_callback, 0 },
  { "/Dialogs/Gradient Editor...", "<control>G", dialogs_gradient_editor_cmd_callback, 0 },
  { "/Dialogs/Layers & Channels...", "<control>L", dialogs_lc_cmd_callback, 0 },
  { "/Dialogs/Indexed Palette...", NULL, dialogs_indexed_palette_cmd_callback, 0 },
  { "/Dialogs/Tool Options...", NULL, dialogs_tools_options_cmd_callback, 0 },
};
static guint n_image_entries = sizeof (image_entries) / sizeof (image_entries[0]);
static GtkItemFactory *image_factory = NULL;

static GtkItemFactoryEntry load_entries[] =
{
  { "/Automatic", NULL, file_load_by_extension_callback, 0 },
  { "/---", NULL, NULL, 0, "<Separator>" },
 };
static guint n_load_entries = sizeof (load_entries) / sizeof (load_entries[0]);
static GtkItemFactory *load_factory = NULL;
  
static GtkItemFactoryEntry save_entries[] =
{
  { "/By extension", NULL, file_save_by_extension_callback, 0 },
  { "/---", NULL, NULL, 0, "<Separator>" },
};
static guint n_save_entries = sizeof (save_entries) / sizeof (save_entries[0]);
static GtkItemFactory *save_factory = NULL;

static int initialize = TRUE;


void
menus_get_toolbox_menubar (GtkWidget           **menubar,
			   GtkAccelGroup       **accel_group)
{
  if (initialize)
    menus_init ();

  if (menubar)
    *menubar = toolbox_factory->widget;
  if (accel_group)
    *accel_group = toolbox_factory->accel_group;
}

void
menus_get_image_menu (GtkWidget           **menu,
		      GtkAccelGroup       **accel_group)
{
  if (initialize)
    menus_init ();

  if (menu)
    *menu = image_factory->widget;
  if (accel_group)
    *accel_group = image_factory->accel_group;
}

void
menus_get_load_menu (GtkWidget           **menu,
		     GtkAccelGroup       **accel_group)
{
  if (initialize)
    menus_init ();

  if (menu)
    *menu = load_factory->widget;
  if (accel_group)
    *accel_group = load_factory->accel_group;
}

void
menus_get_save_menu (GtkWidget           **menu,
		     GtkAccelGroup       **accel_group)
{
  if (initialize)
    menus_init ();

  if (menu)
    *menu = save_factory->widget;
  if (accel_group)
    *accel_group = save_factory->accel_group;
}

void
menus_create (GtkMenuEntry *entries,
	      int           nmenu_entries)
{
  if (initialize)
    menus_init ();

  gtk_item_factory_create_menu_entries (nmenu_entries, entries);
}

void
menus_set_sensitive (char *path,
		     int   sensitive)
{
  GtkItemFactory *ifactory;
  GtkWidget *widget = NULL;

  if (initialize)
    menus_init ();

  ifactory = gtk_item_factory_from_path (path);

  if (ifactory)
    {
      widget = gtk_item_factory_get_widget (ifactory, path);
      
      gtk_widget_set_sensitive (widget, sensitive);
    }
  if (!ifactory || !widget)
    printf ("Unable to set sensitivity for menu which doesn't exist:\n%s", path);
}

void
menus_set_state (char *path,
		 int   state)
{
  GtkItemFactory *ifactory;
  GtkWidget *widget = NULL;

  if (initialize)
    menus_init ();

  ifactory = gtk_item_factory_from_path (path);

  if (ifactory)
    {
      widget = gtk_item_factory_get_widget (ifactory, path);

      if (widget && GTK_IS_CHECK_MENU_ITEM (widget))
	gtk_check_menu_item_set_state (GTK_CHECK_MENU_ITEM (widget), state);
      else
	widget = NULL;
    }
  if (!ifactory || !widget)
    printf ("Unable to set state for menu which doesn't exist:\n%s", path);
}

void
menus_destroy (char *path)
{
  if (initialize)
    menus_init ();

  gtk_item_factories_path_delete (NULL, path);
}

void
menus_quit ()
{
  gchar *filename;

  filename = g_strconcat (gimp_directory (), "/menurc", NULL);
  gtk_item_factory_dump_rc (filename, NULL, TRUE);
  g_free (filename);
  
  if (!initialize)
    {
      gtk_object_unref (GTK_OBJECT (toolbox_factory));
      gtk_object_unref (GTK_OBJECT (image_factory));
      gtk_object_unref (GTK_OBJECT (load_factory));
      gtk_object_unref (GTK_OBJECT (save_factory));
    }
}


static void
menus_init ()
{
  if (initialize)
    {
      gchar *filename;

      initialize = FALSE;      

      toolbox_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<Toolbox>", NULL);
      gtk_item_factory_create_items_ac (toolbox_factory,
					n_toolbox_entries,
					toolbox_entries,
					NULL, 2);
      image_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<Image>", NULL);
      gtk_item_factory_create_items_ac (image_factory,
					n_image_entries,
					image_entries,
					NULL, 2);
      load_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<Load>", NULL);
      gtk_item_factory_create_items_ac (load_factory,
					n_load_entries,
					load_entries,
					NULL, 2);
      save_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<Save>", NULL);
      gtk_item_factory_create_items_ac (save_factory,
					n_save_entries,
					save_entries,
					NULL, 2);

      filename = g_strconcat (gimp_directory (), "/menurc", NULL);
      gtk_item_factory_parse_rc (filename);
      g_free (filename);
    }
}

