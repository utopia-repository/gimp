/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimppluginmanager-menu-branch.h
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

#ifndef __GIMP_PLUG_IN_MANAGER_MENU_BRANCH_H__
#define __GIMP_PLUG_IN_MANAGER_MENU_BRANCH_H__


struct _GimpPlugInMenuBranch
{
  gchar *prog_name;
  gchar *menu_path;
  gchar *menu_label;
};


void   gimp_plug_in_manager_menu_branch_exit (GimpPlugInManager *manager);

/* Add a menu branch */
void   gimp_plug_in_manager_add_menu_branch  (GimpPlugInManager *manager,
                                              const gchar       *prog_name,
                                              const gchar       *menu_path,
                                              const gchar       *menu_label);


#endif /* __GIMP_PLUG_IN_MANAGER_MENU_BRANCH_H__ */