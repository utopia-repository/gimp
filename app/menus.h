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
#ifndef __MENUS_H__
#define __MENUS_H__

#include "buildmenu.h"

#define FILEMENU    0
#define EDITMENU    1
#define TOOLMENU    2
#define BRUSHMENU   3

extern int num_menus;
extern Widget mainmenu_widgets [];

/* function declarations */

MenuItem * get_menu_options (int);
char *     get_menu_class (int);
MenuItem * get_image_menu_options ();
void       set_filter_menu (MenuItem *);

#endif /* MENUS_H */
