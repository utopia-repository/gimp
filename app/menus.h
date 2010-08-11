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
#ifndef __MENUS_H__
#define __MENUS_H__


#include "gtk/gtk.h"


void menus_get_toolbox_menubar (GtkWidget           **menubar,
				GtkAccelGroup       **accel_group);
void menus_get_image_menu      (GtkWidget           **menu,
				GtkAccelGroup       **accel_group);
void menus_get_load_menu       (GtkWidget           **menu,
				GtkAccelGroup       **accel_group);
void menus_get_save_menu       (GtkWidget           **menu,
				GtkAccelGroup       **accel_group);
void menus_create              (GtkMenuEntry         *entries,
				int                   nmenu_entries);
void menus_set_sensitive       (char                 *path,
				int                   sensitive);
void menus_set_state           (char                 *path,
				int                   state);
void menus_destroy             (char                 *path);
void menus_quit                (void);


#endif /* MENUS_H */
