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

#ifndef __PLUG_IN_ENUMS_H__
#define __PLUG_IN_ENUMS_H__


typedef enum /*< pdb-skip >*/ /*< skip >*/
{
  PLUG_IN_RGB_IMAGE      = 1 << 0,
  PLUG_IN_GRAY_IMAGE     = 1 << 1,
  PLUG_IN_INDEXED_IMAGE  = 1 << 2,
  PLUG_IN_RGBA_IMAGE     = 1 << 3,
  PLUG_IN_GRAYA_IMAGE    = 1 << 4,
  PLUG_IN_INDEXEDA_IMAGE = 1 << 5
} PlugInImageType;


#endif /* __PLUG_IN_ENUMS_H__ */