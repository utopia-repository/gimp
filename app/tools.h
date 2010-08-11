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
#ifndef __TOOLS_H__
#define __TOOLS_H__

/*  The possible states for tools  */
#define  INACTIVE               0
#define  ACTIVE                 1
#define  PAUSED                 2


/*  Tool control actions  */
#define  PAUSE                  0
#define  RESUME                 1
#define  HALT                   2


/*  The types of tools...  */

#define  RECT_SELECT		0
#define  ELLIPSE_SELECT		1
#define  FREE_SELECT		2
#define  FUZZY_SELECT		3
#define  BEZIER_SELECT		4
#define  ISCISSORS        	5
#define  CROP                   6
#define  TRANSFORM_TOOL         7
#define  FLIP_HTOOL             8
#define  FLIP_VTOOL             9
#define  COLOR_PICKER           10
#define  BUCKET_FILL            11
#define  PAINTBRUSH             12
#define  AIRBRUSH               13
#define  CLONE                  14
#define  CONVOLVE               15
#define  BLEND                  16
#define  TEXT                   17


/*  Structure definitions  */

typedef struct _tool Tool;
typedef void (* ButtonPressFunc)   (Tool *, XButtonEvent *, XtPointer);
typedef void (* ButtonReleaseFunc) (Tool *, XButtonEvent *, XtPointer);
typedef void (* MotionFunc)        (Tool *, XMotionEvent *, XtPointer);
typedef void (* ArrowKeysFunc)     (Tool *, XKeyEvent *, XtPointer);
typedef void (* ToolCtlFunc)       (Tool *, int, void *);

struct _tool
{
  /*  Data  */
  int            type;                 /*  Tool type  */
  int            state;                /*  state of tool activity  */
  int            paused_count;         /*  paused control count  */
  int            scroll_lock;          /*  allow scrolling or not  */
  void *         private;              /*  Tool-specific information  */
  void *         gdisp_ptr;            /*  pointer to currently active gdisp  */

  /*  Action functions  */
  ButtonPressFunc    button_press_func;
  ButtonReleaseFunc  button_release_func;
  MotionFunc         motion_func;
  ArrowKeysFunc      arrow_keys_func;
  ToolCtlFunc        control_func;
  
};


/*  Global Data Structure  */

extern Tool * active_tool;


/*  Function declarations  */

void     tools_select             (int);
void     tools_options            (int);
void     active_tool_control      (int, void *);
void     tools_toggle_activation  (Widget);


/*  Standard member functions  */
void     standard_arrow_keys_func (Tool *, XKeyEvent *, XtPointer);

#endif  /*  __TOOLS_H__  */

