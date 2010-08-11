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
#ifndef __PAINT_CORE_H__
#define __PAINT_CORE_H__

#include "draw_core.h"
#include "temp_buf.h"

/* the different states that the painting function can be called with  */
#define INIT_PAINT      0
#define MOTION_PAINT    1
#define PAUSE_PAINT     2
#define RESUME_PAINT    3
#define FINISH_PAINT    4


typedef void * (* PaintFunc)   (Tool *, int);
typedef struct _paint_core PaintCore;

struct _paint_core
{
  DrawCore *      core;         /*  Core select object          */
  
  int             startx;       /*  starting x coord            */
  int             starty;       /*  starting y coord            */

  int             curx;         /*  current x coord             */
  int             cury;         /*  current y coord             */

  int             lastx;        /*  last x coord                */
  int             lasty;        /*  last y coord                */

  int             state;        /*  state of buttons and keys   */

  int             num_movements;/*  number of motion events     */

  int             x1, y1;       /*  image space coordinate      */
  int             x2, y2;       /*  image space coords          */

  int             floating;     /*  floating selection?         */

  TempBuf *       brush_mask;   /*  mask for current brush      */

  PaintFunc       paint_func;   /*  painting function           */
};


/*  paint tool action functions  */
void          paint_core_button_press      (Tool *, XButtonEvent *, XtPointer);
void          paint_core_button_release    (Tool *, XButtonEvent *, XtPointer);
void          paint_core_motion            (Tool *, XMotionEvent *, XtPointer);
void          paint_core_control           (Tool *, int, void *);

/*  paint tool functions  */
void          paint_core_no_draw      (Tool *);
Tool *        paint_core_new          (int);
void          paint_core_free         (Tool *);

/*  paint tool painting functions  */
TempBuf *     paint_core_init_discrete     (Tool *, int, int, TempBuf *);
TempBuf *     paint_core_init_stroke       (Tool *, int, int, int, int, TempBuf *);
TempBuf *     paint_core_get_orig_image    (Tool *, int, int, int, int);
void          paint_core_paste_canvas      (Tool *, int, int, int, int);

#endif  /*  __PAINT_CORE_H__  */






