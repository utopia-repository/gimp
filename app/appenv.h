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
#ifndef __APPENV_H__
#define __APPENV_H__

#include <Xm/XmAll.h>
#include "memutils.h"        /* all gimp files should use xfree and xmalloc */

#define STATIC static

#define DISPLAY              (XtDisplay (toplevel))


typedef struct _appdata
  {
    int depth;               /* depth of color visual */
    int dither_type;	     /* type of dithering algorithm */
    int threshold;           /* sensitivity value for soft seed fills--magic wand */
    int levels_of_undo;      /* number of undo levels */
    int bytes_of_undo;       /* max number of bytes allowed in undo stack */
    int arrow_accel;         /* increment to move selections with arrow keys when shift key is down */
    char *colorcube;         /* string of the form: "8.6.4" specifying RGB colorcube */
    char *insignia;          /* application insignia */
    char *bitmap_dir;        /* bitmap directory for tool icons, etc */
    char *gimprc_search_path;/* paths in which to search for gimprc file  */
    Boolean mit_shm;         /* used shared memory for XImages? */
    Boolean stingy;          /* implement stingy memory management  */
    Boolean quick_16bit;     /* implement faster 16 bit drawing at memory penalty */
    Boolean use_bbox;        /* draw bounding box instead of full outline when moving selections */
  }
appdata;

/* toplevel widget */
extern appdata app_data;
extern Widget toplevel;
extern XtAppContext app_context;


#endif /*  APPENV_H  */
