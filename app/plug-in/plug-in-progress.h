/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * plug-in-progress.h
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

#ifndef __PLUG_IN_PROGRESS_H__
#define __PLUG_IN_PROGRESS_H__


void       plug_in_progress_start     (PlugIn      *plug_in,
                                       const gchar *message,
                                       gint         display_ID);
void       plug_in_progress_update    (PlugIn      *plug_in,
                                       gdouble      percentage);
void       plug_in_progress_end       (PlugIn      *plug_in);

gboolean   plug_in_progress_install   (PlugIn      *plug_in,
                                       const gchar *progress_callback);
gboolean   plug_in_progress_uninstall (PlugIn      *plug_in,
                                       const gchar *progress_callback);
gboolean   plug_in_progress_cancel    (PlugIn      *plug_in,
                                       const gchar *progress_callback);

void       plug_in_progress_message   (PlugIn      *plug_in,
                                       const gchar *message);


#endif /* __PLUG_IN_PROGRESS_H__ */