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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "appenv.h"
#include "callbacks.h"
#include "colormaps.h"
#include "fileops.h"
#include "menus.h"
#include "scale.h"
#include "tools.h"

/* local list of menu classes */
static char *menus[] =
{
  "fileMenu",
  "editMainMenu",
  "toolMenu",
  "brushMenu",
  "colorMenu",
};

/* file menu */
static MenuItem file_menu[] =
  {
    {"file_new", &xmPushButtonGadgetClass, 0, NULL, NULL, new_callback, NULL, NULL, NULL},
    {"file_open", &xmPushButtonGadgetClass, 0, NULL, NULL, file_open_callback, NULL, NULL, NULL},
    {"file_save", &xmPushButtonGadgetClass, 0, NULL, NULL, file_save_callback, NULL, NULL, NULL},
    {"file_exit", &xmPushButtonGadgetClass, 0, NULL, NULL, quit_callback, NULL, NULL, NULL},
    { NULL },
  };

/* edit menu */
static MenuItem edit_menu[] =
  {
    {"edit_cut", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_cut_callback, NULL, NULL, NULL},
    {"edit_copy", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_copy_callback, NULL, NULL, NULL},
    {"edit_paste", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_paste_callback, NULL, NULL, NULL},
    {"edit_clear", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_clear_callback, NULL, NULL, NULL},
    {"edit_undo", &xmPushButtonGadgetClass, 0, NULL, NULL, 
       edit_undo_callback, NULL, NULL, NULL},
    {"edit_redo", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_redo_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    {"named_cut", &xmPushButtonGadgetClass, 0, NULL, NULL,
       named_cut_callback, NULL, NULL, NULL},
    {"named_copy", &xmPushButtonGadgetClass, 0, NULL, NULL,
       named_copy_callback, NULL, NULL, NULL},
    {"named_paste", &xmPushButtonGadgetClass, 0, NULL, NULL,
       named_paste_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    {"edit_options", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_options_callback, NULL, NULL, NULL},
    { NULL },
  };

/* selection menu */
static MenuItem select_menu[] =
{
    {"select_toggle", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_hide_callback, NULL, NULL, NULL},
    {"select_invert", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_invert_callback, NULL, NULL, NULL},
    {"select_all", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_all_callback, NULL, NULL, NULL},
    {"select_none", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_none_callback, NULL, NULL, NULL},    
    {"select_anchor", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_anchor_callback, NULL, NULL, NULL},    
    {"select_sharpen", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_sharpen_callback, NULL, NULL, NULL},    
    {"select_opacity", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_opacity_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    {"select_to_gdisp", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_to_gdisp_callback, NULL, NULL, NULL},
    {"select_from_gdisp", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_from_gdisp_callback, NULL, NULL, NULL},
    { NULL },
  };

/* edit for main menu */
static MenuItem edit_main_menu[] =
  {
    {"edit_undo", &xmPushButtonGadgetClass, 0, NULL, NULL, 
       edit_undo_callback, NULL, NULL, NULL},
    {"edit_redo", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_redo_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    {"edit_options", &xmPushButtonGadgetClass, 0, NULL, NULL,
       edit_options_callback, NULL, NULL, NULL},
    { NULL} ,
  };

static MenuItem view_menu[] =
  {
    {"zoom_in", &xmPushButtonGadgetClass, 0, NULL, NULL,
       change_scale, (XtPointer) ZOOMIN, NULL, NULL},
    {"zoom_out", &xmPushButtonGadgetClass, 0, NULL, NULL,
       change_scale, (XtPointer) ZOOMOUT, NULL, NULL},
    {"view_options", &xmPushButtonGadgetClass, 0, NULL, NULL,
       view_options_dialog_callback, NULL, NULL, NULL},
    {"info_window", &xmPushButtonGadgetClass, 0, NULL, NULL,
       window_info_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    {"new_view", &xmPushButtonGadgetClass, 0, NULL, NULL,
       create_new_view_callback, NULL, NULL, NULL},
    {"shrink_wrap", &xmPushButtonGadgetClass, 0, NULL, NULL,
       shrink_wrap_callback, NULL, NULL, NULL},
    {"close_window", &xmPushButtonGadgetClass, 0, NULL, NULL,
       close_window_callback, NULL, NULL, NULL},
    { NULL} ,
  };

static MenuItem tool_menu[] =
  {
    {"rect_select", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) RECT_SELECT, NULL, NULL},
    {"ellipse_select", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) ELLIPSE_SELECT, NULL, NULL},
    {"free_select", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) FREE_SELECT, NULL, NULL},
    {"fuzzy_select", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) FUZZY_SELECT, NULL, NULL},
    {"bezier_select", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) BEZIER_SELECT, NULL, NULL},
    {"iscissors", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) ISCISSORS, NULL, NULL},
    {"crop", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) CROP, NULL, NULL},
    {"transform", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) TRANSFORM_TOOL, NULL, NULL},
    {"flip_horz", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) FLIP_HTOOL, NULL, NULL},
    {"flip_vert", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) FLIP_VTOOL, NULL, NULL},
    {"color_picker", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) COLOR_PICKER, NULL, NULL},
    {"bucket_fill", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) BUCKET_FILL, NULL, NULL},
    {"paintbrush", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) PAINTBRUSH, NULL, NULL},
    {"airbrush", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) AIRBRUSH, NULL, NULL},
    {"clone", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) CLONE, NULL, NULL},
    {"convolve", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) CONVOLVE, NULL, NULL},
    {"blend", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) BLEND, NULL, NULL},
    {"text", &xmPushButtonGadgetClass, 0, NULL, NULL,
       select_tool_callback, (XtPointer) TEXT, NULL, NULL},
    { NULL} ,
  };

static MenuItem brush_menu[] =
  {
    {"brushSelect", &xmPushButtonGadgetClass, 0, NULL, NULL,
       brush_dialog_callback, NULL, NULL, NULL},
    {"", &xmSeparatorGadgetClass, 0, NULL, NULL,
       NULL, NULL, NULL, NULL},
    { NULL },
  };

static MenuItem color_menu[] =
{
  {"paletteDialog", &xmPushButtonGadgetClass, 0, NULL, NULL,
     palette_dialog_callback, NULL, NULL, NULL},
  { NULL },
};

static MenuItem *mainmenu[] =
{
  file_menu,
  edit_main_menu,
  tool_menu,
  brush_menu,
  color_menu,
  NULL,
};

int num_menus = 5;
Widget mainmenu_widgets [] =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

static MenuItem image_menu[] =
{
  {"fileMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, file_menu, NULL},
  {"editMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, edit_menu, NULL},
  {"selectMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, select_menu, NULL},
  {"viewMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, view_menu, NULL},
  {"toolMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, tool_menu, NULL},
  {"filterMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL},
  {"colorMenu", &xmPushButtonGadgetClass, 0, NULL, NULL, NULL, NULL, color_menu, NULL},
  { NULL },
};

/**************  USE THESE FUNCTIONS TO CONFIGURE MENUS  **************/

MenuItem *
get_menu_options (menu_num)
     int menu_num;
{
  return mainmenu[menu_num];
}

char *
get_menu_class (menu_num)
     int menu_num;
{
  return menus[menu_num];
}

MenuItem *
get_image_menu_options ()
{
  return image_menu;
}

void
set_filter_menu (menu)
     MenuItem *menu;
{
  image_menu[5].subitems = menu;
}


