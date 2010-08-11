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

#ifndef __GIMP_DISPLAY_SHELL_H__
#define __GIMP_DISPLAY_SHELL_H__


#include <gtk/gtkwindow.h>

#include "libgimpwidgets/gimpwidgets.h"


/* Apply to a float the same rounding mode used in the renderer */
#define  PROJ_ROUND(coord) ((gint) ceil (coord))

/* finding the effective screen resolution (double) */
#define  SCREEN_XRES(s)   ((s)->dot_for_dot ? \
                           (s)->gdisp->gimage->xresolution : (s)->monitor_xres)
#define  SCREEN_YRES(s)   ((s)->dot_for_dot ? \
                           (s)->gdisp->gimage->yresolution : (s)->monitor_yres)

/* calculate scale factors (double) */
#define  SCALEFACTOR_X(s) (gimp_zoom_model_get_factor ((s)->zoom) \
                           * SCREEN_XRES(s) / (s)->gdisp->gimage->xresolution)
#define  SCALEFACTOR_Y(s) (gimp_zoom_model_get_factor ((s)->zoom) \
                           * SCREEN_YRES(s) / (s)->gdisp->gimage->yresolution)

/* scale values */
#define  SCALEX(s,x)      PROJ_ROUND ((x) * SCALEFACTOR_X(s))
#define  SCALEY(s,y)      PROJ_ROUND ((y) * SCALEFACTOR_Y(s))

/* unscale values */
#define  UNSCALEX(s,x)    ((gint) ((x) / SCALEFACTOR_X(s)))
#define  UNSCALEY(s,y)    ((gint) ((y) / SCALEFACTOR_Y(s)))
/* (and float-returning versions) */
#define  FUNSCALEX(s,x)   ((x) / SCALEFACTOR_X(s))
#define  FUNSCALEY(s,y)   ((y) / SCALEFACTOR_Y(s))


#define GIMP_TYPE_DISPLAY_SHELL            (gimp_display_shell_get_type ())
#define GIMP_DISPLAY_SHELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShell))
#define GIMP_DISPLAY_SHELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShellClass))
#define GIMP_IS_DISPLAY_SHELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DISPLAY_SHELL))
#define GIMP_IS_DISPLAY_SHELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DISPLAY_SHELL))
#define GIMP_DISPLAY_SHELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DISPLAY_SHELL, GimpDisplayShellClass))


typedef struct _GimpDisplayShellClass  GimpDisplayShellClass;

struct _GimpDisplayShell
{
  GtkWindow         parent_instance;

  GimpDisplay      *gdisp;

  GimpUIManager    *menubar_manager;
  GimpUIManager    *popup_manager;

  gdouble           monitor_xres;
  gdouble           monitor_yres;

  GimpUnit          unit;

  GimpZoomModel    *zoom;
  gdouble           other_scale;       /*  scale factor entered in Zoom->Other     */
  gboolean          dot_for_dot;       /*  is monitor resolution being ignored?    */

  gint              offset_x;          /*  offset of display image into raw image  */
  gint              offset_y;

  gint              disp_width;        /*  width of drawing area              */
  gint              disp_height;       /*  height of drawing area             */
  gint              disp_xoffset;
  gint              disp_yoffset;

  gboolean          proximity;         /*  is a device in proximity           */
  gboolean          snap_to_guides;    /*  should the guides be snapped to?   */
  gboolean          snap_to_grid;      /*  should the grid be snapped to?     */
  gboolean          snap_to_canvas;    /*  should the canvas be snapped to?   */
  gboolean          snap_to_vectors;   /*  should the active path be snapped  */

  Selection        *select;            /*  Selection object                   */

  GtkWidget        *canvas;            /*  GimpCanvas widget                  */
  GdkGC            *grid_gc;           /*  GC for grid drawing                */
  GdkGC            *pen_gc;            /*  GC for felt pen drawing            */

  GtkAdjustment    *hsbdata;           /*  adjustments                        */
  GtkAdjustment    *vsbdata;
  GtkWidget        *hsb;               /*  scroll bars                        */
  GtkWidget        *vsb;

  GtkWidget        *hrule;             /*  rulers                             */
  GtkWidget        *vrule;

  GtkWidget        *origin_button;     /*  NW: origin button                  */
  GtkWidget        *quick_mask_button; /*  SW: quick mask button              */
  GtkWidget        *zoom_button;       /*  NE: zoom toggle button             */
  GtkWidget        *nav_ebox;          /*  SE: navigation event box           */

  GtkWidget        *menubar;           /*  menubar                            */
  GtkWidget        *statusbar;         /*  statusbar                          */

  guchar           *render_buf;        /*  buffer for rendering the image     */

  guint             title_idle_id;     /*  title update idle ID               */

  gint              icon_size;         /*  size of the icon pixmap            */
  guint             icon_idle_id;      /*  ID of the idle-function            */

  GimpCursorFormat    cursor_format;   /*  Currently used cursor format       */
  GimpCursorType      current_cursor;  /*  Currently installed main cursor    */
  GimpToolCursorType  tool_cursor;     /*  Current Tool cursor                */
  GimpCursorModifier  cursor_modifier; /*  Cursor modifier (plus, minus, ...) */

  GimpCursorType    override_cursor;   /*  Overriding cursor                  */
  gboolean          using_override_cursor; /*  is the cursor overridden?      */

  gboolean          draw_cursor;       /* should we draw software cursor ?    */
  gboolean          have_cursor;       /* is cursor currently drawn ?         */
  gint              cursor_x;          /* software cursor X value             */
  gint              cursor_y;          /* software cursor Y value             */

  GtkWidget        *close_dialog;      /*  close dialog                       */
  GtkWidget        *scale_dialog;      /*  scale (zoom) dialog                */
  GtkWidget        *nav_popup;         /*  navigation popup                   */
  GtkWidget        *grid_dialog;       /*  grid configuration dialog          */

  GimpColorDisplayStack *filter_stack;   /* color display conversion stuff    */
  guint                  filter_idle_id;
  GtkWidget             *filters_dialog; /* color display filter dialog       */

  gint              paused_count;

  GQuark            vectors_freeze_handler;
  GQuark            vectors_thaw_handler;
  GQuark            vectors_visible_handler;

  GdkWindowState    window_state;      /* for fullscreen display              */
  gboolean          zoom_on_resize;
  gboolean          show_transform_preview;

  GimpDisplayOptions *options;
  GimpDisplayOptions *fullscreen_options;

  /*  the state of gimp_display_shell_tool_events()  */
  gboolean          space_pressed;
  gboolean          space_release_pending;
  gboolean          scrolling;
  gint              scroll_start_x;
  gint              scroll_start_y;
  gboolean          button_press_before_focus;
  guint32           last_motion_time;

  GdkRectangle     *highlight;         /* in image coordinates, can be NULL   */
  GimpDrawable     *mask;

  gpointer          scroll_info;
};

struct _GimpDisplayShellClass
{
  GtkWindowClass  parent_class;

  void (* scaled)    (GimpDisplayShell *shell);
  void (* scrolled)  (GimpDisplayShell *shell);
  void (* reconnect) (GimpDisplayShell *shell);
};


GType       gimp_display_shell_get_type              (void) G_GNUC_CONST;

GtkWidget * gimp_display_shell_new                   (GimpDisplay      *gdisp,
                                                      GimpUnit          unit,
                                                      gdouble           scale,
                                                      GimpMenuFactory  *menu_factory,
                                                      GimpUIManager    *popup_manager);

void        gimp_display_shell_reconnect             (GimpDisplayShell *shell);

void        gimp_display_shell_scaled                (GimpDisplayShell *shell);
void        gimp_display_shell_scrolled              (GimpDisplayShell *shell);

void        gimp_display_shell_set_unit              (GimpDisplayShell *shell,
                                                      GimpUnit          unit);
GimpUnit    gimp_display_shell_get_unit              (GimpDisplayShell *shell);

gboolean    gimp_display_shell_snap_coords           (GimpDisplayShell *shell,
                                                      GimpCoords       *coords,
                                                      GimpCoords       *snapped_coords,
                                                      gint              snap_offset_x,
                                                      gint              snap_offset_y,
                                                      gint              snap_width,
                                                      gint              snap_height);

gboolean    gimp_display_shell_mask_bounds           (GimpDisplayShell *shell,
                                                      gint             *x1,
                                                      gint             *y1,
                                                      gint             *x2,
                                                      gint             *y2);

void        gimp_display_shell_expose_area           (GimpDisplayShell *shell,
                                                      gint              x,
                                                      gint              y,
                                                      gint              w,
                                                      gint              h);
void        gimp_display_shell_expose_guide          (GimpDisplayShell *shell,
                                                      GimpGuide        *guide);
void        gimp_display_shell_expose_sample_point   (GimpDisplayShell *shell,
                                                      GimpSamplePoint  *sample_point);
void        gimp_display_shell_expose_full           (GimpDisplayShell *shell);

void        gimp_display_shell_flush                 (GimpDisplayShell *shell,
                                                      gboolean          now);

void        gimp_display_shell_pause                 (GimpDisplayShell *shell);
void        gimp_display_shell_resume                (GimpDisplayShell *shell);

void        gimp_display_shell_update_icon           (GimpDisplayShell *shell);

void        gimp_display_shell_shrink_wrap           (GimpDisplayShell *shell);

void        gimp_display_shell_selection_visibility  (GimpDisplayShell *shell,
                                                      GimpSelectionControl  control);
void        gimp_display_shell_set_highlight         (GimpDisplayShell *shell,
                                                      const GdkRectangle *highlight);
void        gimp_display_shell_set_mask              (GimpDisplayShell *shell,
                                                      GimpDrawable     *mask);


#endif /* __GIMP_DISPLAY_SHELL_H__ */
