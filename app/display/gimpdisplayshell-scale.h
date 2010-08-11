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

#ifndef __GIMP_DISPLAY_SHELL_SCALE_H__
#define __GIMP_DISPLAY_SHELL_SCALE_H__


void   gimp_display_shell_scale_setup           (GimpDisplayShell *shell);

void   gimp_display_shell_scale_set_dot_for_dot (GimpDisplayShell *shell,
                                                 gboolean          dot_for_dot);

void   gimp_display_shell_scale                 (GimpDisplayShell *shell,
                                                 GimpZoomType      zoom_type,
                                                 gdouble           scale);
void   gimp_display_shell_scale_to              (GimpDisplayShell *shell,
                                                 GimpZoomType      zoom_type,
                                                 gdouble           scale,
                                                 gdouble           x,
                                                 gdouble           y);
void   gimp_display_shell_scale_fit_in          (GimpDisplayShell *shell);
void   gimp_display_shell_scale_fit_to          (GimpDisplayShell *shell);
void   gimp_display_shell_scale_by_values       (GimpDisplayShell *shell,
                                                 gdouble           scale,
                                                 gint              offset_x,
                                                 gint              offset_y,
                                                 gboolean          resize_window);
void   gimp_display_shell_scale_shrink_wrap     (GimpDisplayShell *shell);

void   gimp_display_shell_scale_resize          (GimpDisplayShell *shell,
                                                 gboolean          resize_window,
                                                 gboolean          redisplay);
void   gimp_display_shell_scale_dialog          (GimpDisplayShell *shell);


#endif  /*  __GIMP_DISPLAY_SHELL_SCALE_H__  */
