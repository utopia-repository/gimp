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
#ifndef __GDISPLAY_H__
#define __GDISPLAY_H__

#include "gimage.h"
#include "info_dialog.h"
#include "selection.h"

/*
 *  Global variables
 *
 */

/*  some useful macros  */

#define  SCALESRC(g)    (g->scale & 0x00ff)
#define  SCALEDEST(g)   (g->scale >> 8)
#define  SCALE(g,x)     ((x * SCALEDEST(g)) / SCALESRC(g))
#define  UNSCALE(g,x)   ((x * SCALESRC(g)) / SCALEDEST(g))

#define LOWPASS(x) ((x>0) ? x : 0)
#define HIGHPASS(x,y) ((x>y) ? y : x)


typedef enum
{
  SelectionOff,
  SelectionLayerOff,
  SelectionOn,
  SelectionPause,
  SelectionResume
} SelectionControl;

typedef struct _GDisplay GDisplay;
struct _GDisplay
{
  int ID;                         /*  unique identifier for this gdisplay     */

  GtkWidget *shell;               /*  shell widget for this gdisplay          */
  GtkWidget *canvas;              /*  canvas widget for this gdisplay         */
  GtkWidget *hsb, *vsb;           /*  widgets for scroll bars                 */
  GtkWidget *hrule, *vrule;       /*  widgets for rulers                      */
  GtkWidget *origin;              /*  widgets for rulers                      */
  GtkWidget *popup;               /*  widget for popup menu                   */

  InfoDialog *window_info_dialog; /*  dialog box for image information        */

  int color_type;                 /*  is this an RGB or GRAY colormap         */

  GtkAdjustment *hsbdata;         /*  horizontal data information             */
  GtkAdjustment *vsbdata;         /*  vertical data information               */

  GImage *gimage;	          /*  pointer to the associated gimage struct */
  int instance;                   /*  the instance # of this gdisplay as      */
                                  /*  taken from the gimage at creation       */

  int depth;   		          /*  depth of our drawables                  */
  int disp_width;                 /*  width of drawing area in the window     */
  int disp_height;                /*  height of drawing area in the window    */
  int disp_xoffset;
  int disp_yoffset;

  int offset_x, offset_y;         /*  offset of display image into raw image  */
  int scale;        	          /*  scale factor from original raw image    */
  short draw_guides;              /*  should the guides be drawn?             */
  short snap_to_guides;           /*  should the guides be snapped to?        */

  Selection *select;              /*  Selection object                        */

  GdkGC *scroll_gc;               /*  GC for scrolling */

  GSList *update_areas;           /*  Update areas list                       */
  GSList *display_areas;          /*  Display areas list                      */

  GdkCursorType current_cursor;   /*  Currently installed cursor              */
};



/* member function declarations */

GDisplay * gdisplay_new                    (GImage *, unsigned int);
void       gdisplay_remove_and_delete      (GDisplay *);
int        gdisplay_mask_value             (GDisplay *, int, int);
int        gdisplay_mask_bounds            (GDisplay *, int *, int *, int *, int *);
void       gdisplay_transform_coords       (GDisplay *, int, int, int *, int *, int);
void       gdisplay_untransform_coords     (GDisplay *, int, int, int *,
					    int *, int, int);
void       gdisplay_transform_coords_f     (GDisplay *, double, double, double *,
					    double *, int);
void       gdisplay_untransform_coords_f   (GDisplay *, double, double, double *,
					    double *, int);
void       gdisplay_install_tool_cursor    (GDisplay *, GdkCursorType);
void       gdisplay_remove_tool_cursor     (GDisplay *);
void       gdisplay_set_menu_sensitivity   (GDisplay *);
void       gdisplay_expose_area            (GDisplay *, int, int, int, int);
void       gdisplay_expose_guide           (GDisplay *, Guide *);
void       gdisplay_expose_full            (GDisplay *);
void       gdisplay_flush                  (GDisplay *);
void       gdisplay_draw_guides            (GDisplay *);
void       gdisplay_draw_guide             (GDisplay *, Guide *, int);
Guide*     gdisplay_find_guide             (GDisplay *, int, int);
void       gdisplay_snap_point             (GDisplay *, int , int, int *, int *);
void       gdisplay_snap_rectangle         (GDisplay *, int, int, int, int, int *, int *);

/*  function declarations  */

GDisplay * gdisplay_active                 (void);
GDisplay * gdisplay_get_ID                 (int);
void       gdisplays_update_title          (int);
void       gdisplays_update_area           (int, int, int, int, int);
void       gdisplays_expose_guides         (int);
void       gdisplays_expose_guide          (int, Guide *);
void       gdisplays_update_full           (int);
void       gdisplays_shrink_wrap           (int);
void       gdisplays_expose_full           (void);
void       gdisplays_selection_visibility  (int, SelectionControl);
int        gdisplays_dirty                 (void);
void       gdisplays_delete                (void);
void       gdisplays_flush                 (void);


#endif /*  __GDISPLAY_H__  */
