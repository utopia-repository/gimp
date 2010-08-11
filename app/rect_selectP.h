#ifndef __RECT_SELECTP_H__
#define __RECT_SELECTP_H__

#include "draw_core.h"

typedef struct _rect_select RectSelect, EllipseSelect;

struct _rect_select
{
  DrawCore *      core;       /*  Core select object                      */
  
  int             x, y;       /*  upper left hand coordinate              */
  int             w, h;       /*  width and height                        */

  int             op;         /*  selection operation (ADD, SUB, etc)     */
  int             replace;    /*  replace current selection?              */

};

#endif  /*  __RECT_SELECTP_H__  */
