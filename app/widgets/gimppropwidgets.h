/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * gimppropwidgets.h
 * Copyright (C) 2002 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_APP_PROP_WIDGETS_H__
#define __GIMP_APP_PROP_WIDGETS_H__


/*  GParamEnum  */

GtkWidget     * gimp_prop_paint_mode_menu_new     (GObject     *config,
                                                   const gchar *property_name,
                                                   gboolean     with_behind_mode);


/*  GimpParamColor  */

GtkWidget     * gimp_prop_color_button_new        (GObject     *config,
                                                   const gchar *property_name,
                                                   const gchar *title,
                                                   gint         width,
                                                   gint         height,
                                                   GimpColorAreaType  type);


/*  GParamObject (GimpViewable)  */

GtkWidget     * gimp_prop_preview_new             (GObject     *config,
                                                   const gchar *property_name,
                                                   gint         size);


#endif /* __GIMP_APP_PROP_WIDGETS_H__ */
