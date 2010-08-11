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
#ifndef __SELECTION_H__
#define __SELECTION_H__

#include "autodialog.h"
#include "temp_buf.h"
#include "gregion.h"

#define DEFAULT_SPEED     75

typedef struct _selection Selection;

struct _selection
{
  /*  This information is for maintaining the selection's appearance  */
  GRegion *     region;     /*  Selected region                   */
  Window        win;        /*  Window to draw to                 */
  GC            gc;         /*  GC for drawing selection outline  */
  void *        gdisp;      /*  GDisplay object pointer           */

  /*  This information is for drawing the marching ants around the border  */
  XSegment *    xelems;     /*  X representation of area          */
  int           num_xelems; /*  number of points in pts array     */
  int           state;      /*  internal drawing state            */
  int           paused;     /*  count of pause requests           */
  int           recalc;     /*  flag to recalculate the selection */
  int           index;      /*  index of current stipple pattern  */
  int           speed;      /*  speed of marching ants            */
  Boolean       hidden;     /*  is the selection hidden?          */
  XtIntervalId  timer;      /*  timer for successive draws        */


  /*  This information is for moving and swapping selections      */
  int           offset_x;   /*  offsets for dynamically           */
  int           offset_y;   /*  moving  the selection             */
  int           fresh_cut;  /*  true if a cut op just occured     */

  /*  This information is for transparent selections              */
  float         opacity;    /*  opacity value of the floating sel */
  float         old_opacity;/*  old opacity value                 */
  AutoDialog    dlg;        /*  opacity autodialog                */
  int           opacity_ID; /*  id of scale widget                */

  TempBuf *     float_buf;  /*  temp buffer for floating selection*/
  TempBuf *     orig_buf;   /*  temp buffer for original image    */
};


/*  Function declarations  */

Selection *  selection_create          (Window, void *, int, int, int);
Selection *  selection_generic_new     (GRegion *, int, int, TempBuf *);
void         selection_generic_free    (Selection *);
void         selection_swap            (Selection *, Selection *);
void         selection_clear           (Selection *, void *);
void         selection_pause           (Selection *);
void         selection_resume          (Selection *);
void         selection_start           (Selection *, long, int);
void         selection_invis           (Selection *);
void         selection_translate       (Selection *, int, int, int, int, void *);
Boolean      selection_find_bounds     (Selection *, int *, int *, int *, int *);
Boolean      selection_point_inside    (Selection *, void *, int, int);
void         selection_stencil         (Selection *, void *, int, int, int, int, int);
void         selection_cut_floating    (Selection *, void *);
void         selection_paste_floating  (Selection *, void *, int);
void         selection_replace         (Selection *, void *, Selection *);
void         selection_anchor          (Selection *, void *);
void         selection_select_all      (Selection *, void *);
void         selection_select_none     (Selection *, void *);
void         selection_invert          (Selection *, void *);
void         selection_sharpen         (Selection *, void *);
void         selection_to_gdisplay     (Selection *, void *);
void         selection_from_gdisplay   (Selection *, void *, void *);
void         selection_hide            (Selection *, void *);
void         selection_set_opacity     (Selection *, double);
void         selection_opacity_dialog  (Selection *, void *);
void         selection_free            (Selection *);


#endif  /*  __SELECTION_H__  */
