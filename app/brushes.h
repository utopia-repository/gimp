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
#ifndef __BRUSHES_H__
#define __BRUSHES_H__

#include "linked.h"
#include "temp_buf.h"


typedef struct _GBrush  GBrush, * GBrushP;

struct _GBrush
{
  char *     filename;    /*  actual filename--brush's location on disk  */
  char *     name;        /*  brush's name--for brush selection dialog   */

  int        index;       /*  brush's index...                           */
  
  TempBuf *  mask;        /*  the actual mask...                         */

};

/*  global variables  */
extern link_ptr     brush_list;
extern int          num_brushes;


/*  function declarations  */
void                brushes_init ();
void                brushes_free ();
void                brush_select_dialog_free ();
void                select_brush (GBrushP);
GBrushP             get_brush_by_index (int);
GBrushP             get_active_brush ();
XmStringTable       get_brush_names ();
void                create_brush_dialog ();

int                 get_brush_interpolate ();
float               get_brush_opacity ();
int                 get_brush_paint_mode ();

void                set_brush_interpolate (int);
void                set_brush_opacity (double);
void                set_brush_paint_mode (int);

#endif  /*  __BRUSHES_H__  */
