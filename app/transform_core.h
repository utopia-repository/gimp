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
#ifndef __TRANSFORM_CORE_H__
#define __TRANSFORM_CORE_H__

#include "info_dialog.h"
#include "draw_core.h"

/* possible scaling functions */
#define CREATING        0
#define HANDLE_1        1
#define HANDLE_2        2
#define HANDLE_3        3
#define HANDLE_4        4

/* the different states that the transformation function can be called with  */
#define INIT            0
#define MOTION          1
#define RECALC          2
#define FINISH          3

/* buffer sizes for scaling information strings (for the info dialog) */
#define MAX_INFO_BUF    12
#define TRAN_INFO_SIZE  4

/* control whether the transform tool draws a bounding box */
#define NON_INTERACTIVE 0
#define INTERACTIVE     1


typedef double  Vector[3];
typedef Vector  Matrix[3];
typedef double  TranInfo[TRAN_INFO_SIZE];

typedef void * (* TransformFunc)   (Tool *, void *, int);
typedef struct _transform_core TransformCore;

struct _transform_core
{
  DrawCore *      core;         /*  Core select object          */
  
  int             startx;       /*  starting x coord            */
  int             starty;       /*  starting y coord            */

  int             curx;         /*  current x coord             */
  int             cury;         /*  current y coord             */

  int             lastx;        /*  last x coord                */
  int             lasty;        /*  last y coord                */

  int             state;        /*  state of buttons and keys   */

  int             x1, y1;       /*  upper left hand coordinate  */
  int             x2, y2;       /*  lower right hand coords     */

  double          tx1, ty1;     /*  transformed coords          */
  double          tx2, ty2;     /*                              */
  double          tx3, ty3;     /*                              */
  double          tx4, ty4;     /*                              */

  int             sx1, sy1;     /*  transformed screen coords   */
  int             sx2, sy2;     /*  position of four handles    */
  int             sx3, sy3;     /*                              */
  int             sx4, sy4;     /*                              */

  Matrix          transform;    /*  transformation matrix       */
  TranInfo        trans_info;   /*  transformation info         */

  void *          select_ptr;   /*  pointer to selection struct */
  int             gregion_id;   /*  ID of current gdisplay reg. */

  int             srw, srh;     /*  width and height of handles */

  TransformFunc   trans_func;   /*  transformation function     */

  int             function;     /*  current tool activity       */

  int             interactive;  /*  tool is interactive         */
};


/*  make this variable available to all  */
extern        InfoDialog * transform_info;

/*  transform tool action functions  */
void          transform_core_button_press      (Tool *, XButtonEvent *, XtPointer);
void          transform_core_button_release    (Tool *, XButtonEvent *, XtPointer);
void          transform_core_motion            (Tool *, XMotionEvent *, XtPointer);
void          transform_core_control           (Tool *, int, void *);

/*  transform tool functions  */
void          transform_core_draw         (Tool *);
void          transform_core_no_draw      (Tool *);
Tool *        transform_core_new          (int, int);
void          transform_core_free         (Tool *);
void          transform_core_reset        (Tool *, void *);

/*  matrix functions  */
void          transform_bounding_box      (Tool *);
void          transform_point             (Matrix, double, double, double *, double *);
void          mult_matrix                 (Matrix, Matrix);
void          identity_matrix             (Matrix);
void          translate_matrix            (Matrix, double, double);
void          scale_matrix                (Matrix, double, double);
void          rotate_matrix               (Matrix, double);
void          xshear_matrix               (Matrix, double);
void          yshear_matrix               (Matrix, double);


/*  Something to help the transform_undo deal with lots of variables...  */

typedef struct _transform_undo TransformUndo;

struct _transform_undo
{
  void *          old_select;
  int             tool_type;
  TranInfo        trans_info;
  Boolean         first;
  int             initial_move;
};


#endif  /*  __TRANSFORM_CORE_H__  */






