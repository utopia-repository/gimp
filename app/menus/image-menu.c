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

#include <string.h>

#include <gtk/gtk.h>

#include "menus-types.h"

#include "file-menu.h"
#include "image-menu.h"
#include "plug-in-menus.h"


void
image_menu_setup (GimpUIManager *manager,
                  const gchar   *ui_path)
{
  if (! strcmp (ui_path, "/dummy-menubar"))
    ui_path = "/dummy-menubar/image-popup";

  file_menu_setup (manager, ui_path);
  plug_in_menus_setup (manager, ui_path);
}