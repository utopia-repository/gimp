/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * file-open.h
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

#ifndef __FILE_OPEN_H__
#define __FILE_OPEN_H__


GimpImage * file_open_image                 (Gimp               *gimp,
                                             GimpContext        *context,
                                             GimpProgress       *progress,
                                             const gchar        *uri,
                                             const gchar        *entered_filename,
                                             PlugInProcDef      *file_proc,
                                             GimpRunMode         run_mode,
                                             GimpPDBStatusType  *status,
                                             const gchar       **mime_type,
                                             GError            **error);

GimpImage * file_open_thumbnail             (Gimp               *gimp,
                                             GimpContext        *context,
                                             GimpProgress       *progress,
                                             const gchar        *uri,
                                             gint                size,
                                             const gchar       **mime_type,
                                             gint               *image_width,
                                             gint               *image_height);
GimpImage * file_open_with_display          (Gimp               *gimp,
                                             GimpContext        *context,
                                             GimpProgress       *progress,
                                             const gchar        *uri,
                                             GimpPDBStatusType  *status,
                                             GError            **error);

GimpImage * file_open_with_proc_and_display (Gimp               *gimp,
                                             GimpContext        *context,
                                             GimpProgress       *progress,
                                             const gchar        *uri,
                                             const gchar        *entered_filename,
                                             PlugInProcDef      *file_proc,
                                             GimpPDBStatusType  *status,
                                             GError            **error);

GimpLayer * file_open_layer                 (Gimp               *gimp,
                                             GimpContext        *context,
                                             GimpProgress       *progress,
                                             GimpImage          *dest_image,
                                             const gchar        *uri,
                                             GimpRunMode         run_mode,
                                             GimpPDBStatusType  *status,
                                             GError            **error);


#endif /* __FILE_OPEN_H__ */
