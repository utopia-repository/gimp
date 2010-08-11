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
#ifndef __GDISPLAY_H__
#define __GDISPLAY_H__

#include "gdither.h"
#include "gimage.h"
#include "gximage.h"
#include "selection.h"
#include "timer.h"

/*
 *  Global variables
 *
 */

/*  some useful macros  */

#define  bounds(a,x,y)  ((a < x) ? x : ((a > y) ? y : a))

#define  SCALESRC(g)    (g->scale & 0x00ff)
#define  SCALEDEST(g)   (g->scale >> 8)
#define  SCALE(g,x)     ((x * SCALEDEST(g)) / SCALESRC(g))
#define  UNSCALE(g,x)   ((x * SCALESRC(g)) / SCALEDEST(g))

/*  Two important macros  */
#define MINIMUM(x,y) ((x < y) ? x : y)
#define MAXIMUM(x,y) ((x > y) ? x : y)

#define LOWPASS(x) ((x>0) ? x : 0)
#define HIGHPASS(x,y) ((x>y) ? y : x)


typedef struct _GDisplay GDisplay;
struct _GDisplay
  {
    int ID;                     /*  unique identifier for this gdisplay     */
    Widget hsb, vsb;            /*  widgets for scroll bars                 */
    Widget popup;               /*  widget for popup menu                   */
    Widget shell;               /*  shell widget for this gdisplay          */

    Widget view_options_dialog; /*  dialog box for viewing options          */
    Widget window_info_dialog;  /*  dialog box for image information        */

    GImage *gimage;		/*  pointer to the associated gimage struct */
    int instance;               /*  the instance # of this gdisplay as      */
                                /*  taken from the gimage at creation       */

    GXImage *disp_image;	/*  offscreen image buffer                  */

    unsigned int depth;		/*  depth of our drawables                  */
    int dither_type;		/*  type of dithering requested             */
    long offset_x, offset_y;	/*  offset of display image into raw image  */
    unsigned int scale; 	/*  scale factor from original raw image    */

    Colormap colormap;          /*  colormap of this window                 */
    Window win;			/*  window to draw to, set colormap of, etc */

    TIMER main_timer;
    TIMER line_timer;

    GDither *gdither;           /*  Dither object pointer                   */

    Selection *select;          /*  Selection object                        */

    XtWorkProcId dither_work_proc_id;
    XtWorkProcId load_work_proc_id;
  };


/* member function declarations */

void       gdisplay_remove_and_delete   (GDisplay *);
void       gdisplay_create_image        (GDisplay *, Widget, Widget);
void       gdisplay_paint               (GDisplay *);
void       gdisplay_repaint             (GDisplay *, long, long, long, long, long);
void       gdisplay_remove_processes    (GDisplay *);
void       gdisplay_remove_selection    (GDisplay *);
void       gdisplay_convert             (GDisplay *, long, long, long, long);
void       gdisplay_dither              (GDisplay *, long, long, long, long, long);
void       gdisplay_selection           (GDisplay *);
void       gdisplay_put_image           (GDisplay *, long, long, long, long);
void       gdisplay_disp_region         (GDisplay *, long, long, long, long, long);
void       gdisplay_gimage_region       (GDisplay *, long, long, long, long, long);
void       gdisplay_transform_coords    (GDisplay *, int, int, int *, int *);
void       gdisplay_untransform_coords  (GDisplay *, int, int, int *, int *, int);
Boolean    gdisplay_find_bounds         (GDisplay *, int *, int *, int *, int *);
void       gdisplay_reset_colormap      (GDisplay *);


/*  function declarations  */

GDisplay * gdisplay_gimage              (GImage *, unsigned int);
GDisplay * gdisplay_manage              (Widget, Widget, Widget, Widget, Widget,
					 int, GImage *);
GDisplay * gdisplay_active              (Widget);
GDisplay * gdisplay_get_ID              (int);
void       gdisplay_update              (int, long, long, long, long, long);
void       gdisplay_update_full         (int, long);
void       gdisplay_update_title        (int);
void       gdisplay_install_tool_cursor (int);
void       gdisplay_remove_tool_cursor  (void);
int        gdisplays_dirty              (void);
void       gdisplays_delete             (void);


/*  these functions defined in this include file is an unsightly kludge  */

void       dither_method_change       (GDisplay *, int);
GDither *  dither_create              (GDither_Func, GDisplay *, long, long, long, long, long);

GDisplay * gdisplay_get (void);

#endif /*  __GDISPLAY_H__  */
