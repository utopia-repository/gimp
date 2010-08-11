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
#ifndef __BRUSH_HEADER_H__
#define __BRUSH_HEADER_H__

typedef struct _BrushHeader BrushHeader;

#define FILE_VERSION 1
#define sz_BrushHeader 20

/*  All field entries are MSB  */

struct _BrushHeader
{
  unsigned long   header_size;   /*  header_size = sizeof (BrushHeader) + brush name  */
  unsigned long   version;       /*  brush file version #  */
  unsigned long   width;         /*  width of brush  */
  unsigned long   height;        /*  height of brush  */
  unsigned long   bytes;         /*  depth of brush in bytes--always 1 */
  
};

/*  In a brush file, next comes the brush name, null-terminated.  After that
 *  comes the brush data--width * height * bytes bytes of it...
 */

#endif  /*  __BRUSH_HEADER_H__  */
