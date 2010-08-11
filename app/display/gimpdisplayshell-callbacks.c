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

#include "config.h"

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"
#include "tools/tools-types.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-sample-points.h"
#include "core/gimpimage-quick-mask.h"
#include "core/gimplayer.h"
#include "core/gimptoolinfo.h"

#include "tools/gimpcolortool.h"
#include "tools/gimpmovetool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/tool_manager.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimpcontrollers.h"
#include "widgets/gimpcontrollerkeyboard.h"
#include "widgets/gimpcontrollerwheel.h"
#include "widgets/gimpcursor.h"
#include "widgets/gimpdevices.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpcanvas.h"
#include "gimpdisplay.h"
#include "gimpdisplayoptions.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-appearance.h"
#include "gimpdisplayshell-autoscroll.h"
#include "gimpdisplayshell-callbacks.h"
#include "gimpdisplayshell-coords.h"
#include "gimpdisplayshell-cursor.h"
#include "gimpdisplayshell-draw.h"
#include "gimpdisplayshell-layer-select.h"
#include "gimpdisplayshell-preview.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-selection.h"
#include "gimpdisplayshell-title.h"
#include "gimpdisplayshell-transform.h"
#include "gimpnavigationeditor.h"

#include "gimp-intl.h"


/* #define DEBUG_MOVE_PUSH 1 */


/*  local function prototypes  */

static void       gimp_display_shell_vscrollbar_update (GtkAdjustment    *adjustment,
                                                        GimpDisplayShell *shell);
static void       gimp_display_shell_hscrollbar_update (GtkAdjustment    *adjustment,
                                                        GimpDisplayShell *shell);

static GdkModifierType
                  gimp_display_shell_key_to_state      (gint              key);

static GdkEvent * gimp_display_shell_compress_motion   (GimpDisplayShell *shell);


/*  public functions  */

gboolean
gimp_display_shell_events (GtkWidget        *widget,
                           GdkEvent         *event,
                           GimpDisplayShell *shell)
{
  Gimp     *gimp;
  gboolean  set_display = FALSE;

  /*  are we in destruction?  */
  if (! shell->display || ! shell->display->shell)
    return TRUE;

  gimp = shell->display->image->gimp;

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      {
        GdkEventKey *kevent = (GdkEventKey *) event;

        if (gimp->busy)
          return TRUE;

        /*  do not process any key events while BUTTON1 is down. We do this
         *  so tools keep the modifier state they were in when BUTTON1 was
         *  pressed and to prevent accelerators from being invoked.
         */
        if (kevent->state & GDK_BUTTON1_MASK)
          {
            if (event->type == GDK_KEY_PRESS)
              {
                if (kevent->keyval == GDK_space && shell->space_release_pending)
                  {
                    shell->space_pressed         = TRUE;
                    shell->space_release_pending = FALSE;
                  }
              }
            else
              {
                if (kevent->keyval == GDK_space && shell->space_pressed)
                  {
                    shell->space_pressed         = FALSE;
                    shell->space_release_pending = TRUE;
                  }
              }

            return TRUE;
          }

        switch (kevent->keyval)
          {
          case GDK_Left:      case GDK_Right:
          case GDK_Up:        case GDK_Down:
          case GDK_space:
          case GDK_Tab:
          case GDK_ISO_Left_Tab:
          case GDK_Alt_L:     case GDK_Alt_R:
          case GDK_Shift_L:   case GDK_Shift_R:
          case GDK_Control_L: case GDK_Control_R:
          case GDK_Return:    case GDK_KP_Enter:
          case GDK_BackSpace: case GDK_Delete:
            break;

          case GDK_Escape:
            if (event->type == GDK_KEY_PRESS)
              gimp_display_shell_set_fullscreen (shell, FALSE);
            break;

          default:
            if (shell->space_pressed)
              return TRUE;
            break;
          }

        set_display = TRUE;
        break;
      }

    case GDK_BUTTON_PRESS:
    case GDK_SCROLL:
      set_display = TRUE;
      break;

    case GDK_FOCUS_CHANGE:
      {
        GdkEventFocus *fevent = (GdkEventFocus *) event;

        if (fevent->in && GIMP_DISPLAY_CONFIG (gimp->config)->activate_on_focus)
          set_display = TRUE;
      }
      break;

    case GDK_WINDOW_STATE:
      {
        GdkEventWindowState *sevent = (GdkEventWindowState *) event;
        GimpDisplayOptions  *options;
        gboolean             fullscreen;
        GimpActionGroup     *group;

        shell->window_state = sevent->new_window_state;

        if (! (sevent->changed_mask & GDK_WINDOW_STATE_FULLSCREEN))
          break;

        fullscreen = gimp_display_shell_get_fullscreen (shell);

        gtk_widget_set_name (GTK_WIDGET (shell->menubar),
                             fullscreen ? "gimp-menubar-fullscreen" : NULL);

        options = fullscreen ? shell->fullscreen_options : shell->options;

        gimp_display_shell_set_show_menubar       (shell,
                                                   options->show_menubar);
        gimp_display_shell_set_show_rulers        (shell,
                                                   options->show_rulers);
        gimp_display_shell_set_show_scrollbars    (shell,
                                                   options->show_scrollbars);
        gimp_display_shell_set_show_statusbar     (shell,
                                                   options->show_statusbar);
        gimp_display_shell_set_show_selection     (shell,
                                                   options->show_selection);
        gimp_display_shell_set_show_layer         (shell,
                                                   options->show_layer_boundary);
        gimp_display_shell_set_show_guides        (shell,
                                                   options->show_guides);
        gimp_display_shell_set_show_grid          (shell,
                                                   options->show_grid);
        gimp_display_shell_set_show_sample_points (shell,
                                                   options->show_sample_points);
        gimp_display_shell_set_padding            (shell,
                                                   options->padding_mode,
                                                   &options->padding_color);

        group = gimp_ui_manager_get_action_group (shell->menubar_manager,
                                                  "view");
        gimp_action_group_set_action_active (group, "view-fullscreen",
                                             fullscreen);

        if (shell->display ==
            gimp_context_get_display (gimp_get_user_context (gimp)))
          {
            group = gimp_ui_manager_get_action_group (shell->popup_manager,
                                                      "view");
            gimp_action_group_set_action_active (group, "view-fullscreen",
                                                 fullscreen);
          }
      }
      break;

    default:
      break;
    }

  if (set_display)
    {
      Gimp *gimp = shell->display->image->gimp;

      /*  Setting the context's display automatically sets the image, too  */
      gimp_context_set_display (gimp_get_user_context (gimp), shell->display);
    }

  return FALSE;
}

void
gimp_display_shell_canvas_realize (GtkWidget        *canvas,
                                   GimpDisplayShell *shell)
{
  GimpDisplayConfig     *config;
  GimpDisplay           *display;
  GimpCanvasPaddingMode  padding_mode;
  GimpRGB                padding_color;

  display = shell->display;
  config  = GIMP_DISPLAY_CONFIG (display->image->gimp->config);

  gtk_widget_grab_focus (shell->canvas);

  gimp_display_shell_get_padding (shell, &padding_mode, &padding_color);
  gimp_display_shell_set_padding (shell, padding_mode, &padding_color);

  gimp_display_shell_title_update (shell);

  shell->disp_width  = canvas->allocation.width;
  shell->disp_height = canvas->allocation.height;

  /*  set up the scrollbar observers  */
  g_signal_connect (shell->hsbdata, "value-changed",
                    G_CALLBACK (gimp_display_shell_hscrollbar_update),
                    shell);
  g_signal_connect (shell->vsbdata, "value-changed",
                    G_CALLBACK (gimp_display_shell_vscrollbar_update),
                    shell);

  /*  set the initial cursor  */
  gimp_display_shell_set_cursor (shell,
                                 GDK_TOP_LEFT_ARROW,
                                 GIMP_TOOL_CURSOR_NONE,
                                 GIMP_CURSOR_MODIFIER_NONE);

  /*  allow shrinking  */
  gtk_widget_set_size_request (GTK_WIDGET (shell), 0, 0);

  gimp_display_shell_draw_vectors (shell);
}

void
gimp_display_shell_canvas_size_allocate (GtkWidget        *widget,
                                         GtkAllocation    *allocation,
                                         GimpDisplayShell *shell)
{
  /*  are we in destruction?  */
  if (! shell->display || ! shell->display->shell)
    return;

  if ((shell->disp_width  != allocation->width) ||
      (shell->disp_height != allocation->height))
    {
      if (shell->zoom_on_resize   &&
          shell->disp_width  > 64 &&
          shell->disp_height > 64 &&
          allocation->width  > 64 &&
          allocation->height > 64)
        {
          gdouble scale = gimp_zoom_model_get_factor (shell->zoom);
          gint    offset_x;
          gint    offset_y;

          /*  multiply the zoom_factor with the ratio of the new and
           *  old canvas diagonals
           */
          scale *= (sqrt (SQR (allocation->width) +
                          SQR (allocation->height)) /
                    sqrt (SQR (shell->disp_width) +
                          SQR (shell->disp_height)));

          offset_x = UNSCALEX (shell, shell->offset_x);
          offset_y = UNSCALEX (shell, shell->offset_y);

          gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, scale);

          shell->offset_x = SCALEX (shell, offset_x);
          shell->offset_y = SCALEY (shell, offset_y);
        }

      shell->disp_width  = allocation->width;
      shell->disp_height = allocation->height;

      gimp_display_shell_scroll_clamp_offsets (shell);
      gimp_display_shell_scale_setup (shell);
      gimp_display_shell_scaled (shell);
    }
}

gboolean
gimp_display_shell_canvas_expose (GtkWidget        *widget,
                                  GdkEventExpose   *eevent,
                                  GimpDisplayShell *shell)
{
  GdkRegion    *region = NULL;
  GdkRectangle *rects;
  gint          n_rects;
  gint          i;

  /*  are we in destruction?  */
  if (! shell->display || ! shell->display->shell)
    return TRUE;

  /*  If the call to gimp_display_shell_pause() would cause a redraw,
   *  we need to make sure that no XOR drawing happens on areas that
   *  have already been cleared by the windowing system.
   */
  if (shell->paused_count == 0)
    {
      GdkRectangle  area;

      area.x      = 0;
      area.y      = 0;
      area.width  = shell->disp_width;
      area.height = shell->disp_height;

      region = gdk_region_rectangle (&area);

      gdk_region_subtract (region, eevent->region);

      gimp_canvas_set_clip_region (GIMP_CANVAS (shell->canvas),
                                   GIMP_CANVAS_STYLE_XOR, region);
      gimp_canvas_set_clip_region (GIMP_CANVAS (shell->canvas),
                                   GIMP_CANVAS_STYLE_XOR_DASHED, region);
    }

  gimp_display_shell_pause (shell);

  if (region)
    {
      gimp_canvas_set_clip_region (GIMP_CANVAS (shell->canvas),
                                   GIMP_CANVAS_STYLE_XOR, NULL);
      gimp_canvas_set_clip_region (GIMP_CANVAS (shell->canvas),
                                   GIMP_CANVAS_STYLE_XOR_DASHED, NULL);
      gdk_region_destroy (region);
    }

  gdk_region_get_rectangles (eevent->region, &rects, &n_rects);

  for (i = 0; i < n_rects; i++)
    gimp_display_shell_draw_area (shell,
                                  rects[i].x,
                                  rects[i].y,
                                  rects[i].width,
                                  rects[i].height);

  g_free (rects);

  /* draw the transform tool preview */
  gimp_display_shell_preview_transform (shell);

  /* draw the grid */
  gimp_display_shell_draw_grid (shell, &eevent->area);

  /* draw the guides */
  gimp_display_shell_draw_guides (shell);

  /* draw the sample points */
  gimp_display_shell_draw_sample_points (shell);

  /* and the cursor (if we have a software cursor) */
  gimp_display_shell_draw_cursor (shell);

  /* restart (and recalculate) the selection boundaries */
  gimp_display_shell_selection_start (shell->select, TRUE);

  gimp_display_shell_resume (shell);

  return TRUE;
}

static void
gimp_display_shell_check_device_cursor (GimpDisplayShell *shell)
{
  GdkDevice *current_device;

  current_device = gimp_devices_get_current (shell->display->image->gimp);

  shell->draw_cursor = ! current_device->has_cursor;
}

gboolean
gimp_display_shell_canvas_tool_events (GtkWidget        *canvas,
                                       GdkEvent         *event,
                                       GimpDisplayShell *shell)
{
  GimpDisplay         *display;
  GimpImage           *image;
  Gimp                *gimp;
  GdkDisplay          *gdk_display;
  GimpTool            *active_tool;
  GimpCoords           display_coords;
  GimpCoords           image_coords;
  GdkModifierType      state;
  guint32              time;
  gboolean             return_val        = FALSE;
  gboolean             update_sw_cursor  = FALSE;

  static GimpToolInfo *space_shaded_tool = NULL;

  g_return_val_if_fail (GTK_WIDGET_REALIZED (canvas), FALSE);

  /*  are we in destruction?  */
  if (! shell->display || ! shell->display->shell)
    return TRUE;

  /*  set the active display before doing any other canvas event processing  */
  if (gimp_display_shell_events (canvas, event, shell))
    return TRUE;

  display = shell->display;
  image   = display->image;
  gimp    = image->gimp;

  gdk_display = gtk_widget_get_display (canvas);

  /*  Find out what device the event occurred upon  */
  if (! gimp->busy && gimp_devices_check_change (gimp, event))
    {
      gimp_display_shell_check_device_cursor (shell);
    }

  gimp_display_shell_get_event_coords (shell, event,
                                       gimp_devices_get_current (gimp),
                                       &display_coords);
  gimp_display_shell_get_event_state (shell, event,
                                      gimp_devices_get_current (gimp),
                                      &state);
  time = gdk_event_get_time (event);

  /*  GimpCoords passed to tools are ALWAYS in image coordinates  */
  gimp_display_shell_untransform_coords (shell,
                                         &display_coords,
                                         &image_coords);

  active_tool = tool_manager_get_active (gimp);

  if (active_tool && gimp_tool_control_get_snap_to (active_tool->control))
    {
      gint x, y, width, height;

      gimp_tool_control_get_snap_offsets (active_tool->control,
                                          &x, &y, &width, &height);

      if (gimp_display_shell_snap_coords (shell,
                                          &image_coords,
                                          &image_coords,
                                          x, y, width, height))
        {
          update_sw_cursor = TRUE;
        }
    }

  switch (event->type)
    {
    case GDK_ENTER_NOTIFY:
      {
        GdkEventCrossing *cevent = (GdkEventCrossing *) event;

        if (cevent->mode != GDK_CROSSING_NORMAL)
          return TRUE;

        update_sw_cursor = TRUE;

        tool_manager_oper_update_active (gimp,
                                         &image_coords, state,
                                         shell->proximity,
                                         display);
      }
      break;

    case GDK_LEAVE_NOTIFY:
      {
        GdkEventCrossing *cevent = (GdkEventCrossing *) event;

        if (cevent->mode != GDK_CROSSING_NORMAL)
          return TRUE;

        shell->proximity = FALSE;
        gimp_display_shell_clear_cursor (shell);

        tool_manager_oper_update_active (gimp,
                                         &image_coords, state,
                                         shell->proximity,
                                         display);
      }
      break;

    case GDK_PROXIMITY_IN:
      tool_manager_oper_update_active (gimp,
                                       &image_coords, state,
                                       shell->proximity,
                                       display);
      break;

    case GDK_PROXIMITY_OUT:
      shell->proximity = FALSE;
      gimp_display_shell_clear_cursor (shell);

      tool_manager_oper_update_active (gimp,
                                       &image_coords, state,
                                       shell->proximity,
                                       display);
      break;

    case GDK_FOCUS_CHANGE:
      {
        GdkEventFocus *fevent = (GdkEventFocus *) event;

        if (fevent->in)
          {
            GTK_WIDGET_SET_FLAGS (canvas, GTK_HAS_FOCUS);

            /*  press modifier keys when the canvas gets the focus
             *
             *  in "click to focus" mode, we did this on BUTTON_PRESS, so
             *  do it here only if button_press_before_focus is FALSE
             */
            if (! shell->button_press_before_focus)
              {
                tool_manager_focus_display_active (gimp, display);
                tool_manager_modifier_state_active (gimp, state, display);

                tool_manager_oper_update_active (gimp,
                                                 &image_coords, state,
                                                 shell->proximity,
                                                 display);
              }
          }
        else
          {
            GTK_WIDGET_UNSET_FLAGS (canvas, GTK_HAS_FOCUS);

            /*  reset it here to be prepared for the next
             *  FOCUS_IN / BUTTON_PRESS confusion
             */
            shell->button_press_before_focus = FALSE;

            /*  release modifier keys when the canvas loses the focus  */
            tool_manager_focus_display_active (gimp, NULL);

            tool_manager_oper_update_active (gimp,
                                             &image_coords, 0,
                                             shell->proximity,
                                             display);
          }

        /*  stop the signal because otherwise gtk+ exposes the whole
         *  canvas to get the non-existant focus indicator drawn
         */
        return_val = TRUE;
      }
      break;

    case GDK_BUTTON_PRESS:
      {
        GdkEventButton *bevent = (GdkEventButton *) event;
        GdkEventMask    event_mask;

        if (! GTK_WIDGET_HAS_FOCUS (canvas))
          {
            /*  in "click to focus" mode, the BUTTON_PRESS arrives before
             *  FOCUS_IN, so we have to update the tool's modifier state here
             */
            tool_manager_focus_display_active (gimp, display);
            tool_manager_modifier_state_active (gimp, state, display);

            tool_manager_oper_update_active (gimp,
                                             &image_coords, state,
                                             shell->proximity,
                                             display);

            active_tool = tool_manager_get_active (gimp);

            if (active_tool)
              {
                if ((! gimp_image_is_empty (image) ||
                     gimp_tool_control_get_handle_empty_image (active_tool->control)) &&
                    (bevent->button == 1 ||
                     bevent->button == 2 ||
                     bevent->button == 3))
                  {
                    tool_manager_cursor_update_active (gimp,
                                                       &image_coords, state,
                                                       display);
                  }
                else if (gimp_image_is_empty (image) &&
                         ! gimp_tool_control_get_handle_empty_image (active_tool->control))
                  {
                    gimp_display_shell_set_cursor (shell,
                                                   GIMP_CURSOR_MOUSE,
                                                   gimp_tool_control_get_tool_cursor (active_tool->control),
                                                   GIMP_CURSOR_MODIFIER_BAD);
                  }
              }
            else
              {
                gimp_display_shell_set_cursor (shell,
                                               GIMP_CURSOR_MOUSE,
                                               GIMP_TOOL_CURSOR_NONE,
                                               GIMP_CURSOR_MODIFIER_BAD);
              }

            shell->button_press_before_focus = TRUE;

            /*  we expect a FOCUS_IN event to follow, but can't rely
             *  on it, so force one
             */
            gdk_window_focus (canvas->window, time);
          }

        /*  ignore new mouse events  */
        if (gimp->busy)
          return TRUE;

        active_tool = tool_manager_get_active (gimp);

        switch (bevent->button)
          {
          case 1:
            state |= GDK_BUTTON1_MASK;

            event_mask = (GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

            if (active_tool &&
                (! GIMP_DISPLAY_CONFIG (gimp->config)->perfect_mouse ||
                 (gimp_tool_control_get_motion_mode (active_tool->control) !=
                  GIMP_MOTION_MODE_EXACT)))
              {
                /*  don't request motion hins for XInput devices because
                 *  the wacom driver is known to report crappy hints
                 *  (#6901) --mitch
                 */
                if (gimp_devices_get_current (gimp) ==
                    gdk_display_get_core_pointer (gdk_display))
                  {
                    event_mask |= (GDK_POINTER_MOTION_HINT_MASK);
                  }
              }

            gdk_pointer_grab (canvas->window,
                              FALSE, event_mask, NULL, NULL, time);

            if (! shell->space_pressed && ! shell->space_release_pending)
              gdk_keyboard_grab (canvas->window, FALSE, time);

            if (active_tool &&
                (! gimp_image_is_empty (image) ||
                 gimp_tool_control_get_handle_empty_image (active_tool->control)))
              {
                gboolean initialized = TRUE;

                /*  initialize the current tool if it has no drawable
                 */
                if (! active_tool->drawable)
                  {
                    initialized = tool_manager_initialize_active (gimp,
                                                                  display);
                  }
                else if ((active_tool->drawable !=
                          gimp_image_active_drawable (image)) &&
                         ! gimp_tool_control_get_preserve (active_tool->control))
                  {
                    /*  create a new one, deleting the current
                     */
                    gimp_context_tool_changed (gimp_get_user_context (gimp));

                    initialized = tool_manager_initialize_active (gimp, display);
                  }

                if (initialized)
                  {
                    tool_manager_button_press_active (gimp,
                                                      &image_coords, time, state,
                                                      display);

                    shell->last_motion_time = bevent->time;
                  }
              }
            break;

          case 2:
            state |= GDK_BUTTON2_MASK;

            shell->scrolling      = TRUE;
            shell->scroll_start_x = bevent->x + shell->offset_x;
            shell->scroll_start_y = bevent->y + shell->offset_y;

            gtk_grab_add (canvas);

            gimp_display_shell_set_override_cursor (shell, GDK_FLEUR);
            break;

          case 3:
            state |= GDK_BUTTON3_MASK;
            gimp_ui_manager_ui_popup (shell->popup_manager,
                                      "/dummy-menubar/image-popup",
                                      GTK_WIDGET (shell),
                                      NULL, NULL, NULL, NULL);
            return_val = TRUE;
            break;

          default:
            break;
          }
      }
      break;

    case GDK_BUTTON_RELEASE:
      {
        GdkEventButton *bevent = (GdkEventButton *) event;

        gimp_display_shell_autoscroll_stop (shell);

        if (gimp->busy)
          return TRUE;

        active_tool = tool_manager_get_active (gimp);

        switch (bevent->button)
          {
          case 1:
            state &= ~GDK_BUTTON1_MASK;

            if (! shell->space_pressed && ! shell->space_release_pending)
              gdk_display_keyboard_ungrab (gdk_display, time);

            gdk_display_pointer_ungrab (gdk_display, time);

            gtk_grab_add (canvas);

            if (active_tool &&
                (! gimp_image_is_empty (image) ||
                 gimp_tool_control_get_handle_empty_image (active_tool->control)))
              {
                if (gimp_tool_control_is_active (active_tool->control))
                  {
                    tool_manager_button_release_active (gimp,
                                                        &image_coords,
                                                        time, state,
                                                        display);
                  }
              }

            /*  update the tool's modifier state because it didn't get
             *  key events while BUTTON1 was down
             */
            tool_manager_focus_display_active (gimp, display);
            tool_manager_modifier_state_active (gimp, state, display);

            tool_manager_oper_update_active (gimp,
                                             &image_coords, state,
                                             shell->proximity,
                                             display);

            gtk_grab_remove (canvas);

            if (shell->space_release_pending)
              {
#ifdef DEBUG_MOVE_PUSH
                g_printerr ("%s: popping move tool\n", G_STRFUNC);
#endif

                gimp_context_set_tool (gimp_get_user_context (gimp),
                                       space_shaded_tool);
                space_shaded_tool = NULL;

                tool_manager_focus_display_active (gimp, display);
                tool_manager_modifier_state_active (gimp, state, display);

                tool_manager_oper_update_active (gimp,
                                                 &image_coords, state,
                                                 shell->proximity,
                                                 display);

                shell->space_release_pending = FALSE;

                gdk_display_keyboard_ungrab (gdk_display, time);
              }
            break;

          case 2:
            state &= ~GDK_BUTTON2_MASK;

            shell->scrolling      = FALSE;
            shell->scroll_start_x = 0;
            shell->scroll_start_y = 0;

            gtk_grab_remove (canvas);

            gimp_display_shell_unset_override_cursor (shell);
            break;

          case 3:
            state &= ~GDK_BUTTON3_MASK;
            break;

          default:
            break;
          }
      }
      break;

    case GDK_SCROLL:
      {
        GdkEventScroll     *sevent = (GdkEventScroll *) event;
        GdkScrollDirection  direction;
        GimpController     *wheel;

        wheel = gimp_controllers_get_wheel (gimp);

        if (wheel &&
            gimp_controller_wheel_scroll (GIMP_CONTROLLER_WHEEL (wheel),
                                          sevent))
          return TRUE;

        direction = sevent->direction;

        if (state & GDK_CONTROL_MASK)
          {
            switch (direction)
              {
              case GDK_SCROLL_UP:
                gimp_display_shell_scale_to (shell, GIMP_ZOOM_IN, 0.0,
                                             sevent->x, sevent->y);
                break;

              case GDK_SCROLL_DOWN:
                gimp_display_shell_scale_to (shell, GIMP_ZOOM_OUT, 0.0,
                                             sevent->x, sevent->y);
                break;

              default:
                break;
              }
          }
        else
          {
            GtkAdjustment *adj = NULL;
            gdouble        value;

            if (state & GDK_SHIFT_MASK)
              switch (direction)
                {
                case GDK_SCROLL_UP:    direction = GDK_SCROLL_LEFT;  break;
                case GDK_SCROLL_DOWN:  direction = GDK_SCROLL_RIGHT; break;
                case GDK_SCROLL_LEFT:  direction = GDK_SCROLL_UP;    break;
                case GDK_SCROLL_RIGHT: direction = GDK_SCROLL_DOWN;  break;
                }

            switch (direction)
              {
              case GDK_SCROLL_LEFT:
              case GDK_SCROLL_RIGHT:
                adj = shell->hsbdata;
                break;

              case GDK_SCROLL_UP:
              case GDK_SCROLL_DOWN:
                adj = shell->vsbdata;
                break;
              }

            value = adj->value + ((direction == GDK_SCROLL_UP ||
                                   direction == GDK_SCROLL_LEFT) ?
                                  -adj->page_increment / 2 :
                                  adj->page_increment / 2);
            value = CLAMP (value, adj->lower, adj->upper - adj->page_size);

            gtk_adjustment_set_value (adj, value);
          }

        /*  GimpCoords passed to tools are ALWAYS in image coordinates  */
        gimp_display_shell_untransform_coords (shell,
                                               &display_coords,
                                               &image_coords);

        active_tool = tool_manager_get_active (gimp);

        if (active_tool &&
            gimp_tool_control_get_snap_to (active_tool->control))
          {
            gint x, y, width, height;

            gimp_tool_control_get_snap_offsets (active_tool->control,
                                                &x, &y, &width, &height);

            if (gimp_display_shell_snap_coords (shell,
                                                &image_coords,
                                                &image_coords,
                                                x, y, width, height))
              {
                update_sw_cursor = TRUE;
              }
          }

        tool_manager_oper_update_active (gimp,
                                         &image_coords, state,
                                         shell->proximity,
                                         display);

        return_val = TRUE;
      }
      break;

    case GDK_MOTION_NOTIFY:
      {
        GdkEventMotion *mevent            = (GdkEventMotion *) event;
        GdkEvent       *compressed_motion = NULL;

        if (gimp->busy)
          return TRUE;

        active_tool = tool_manager_get_active (gimp);

        if (active_tool &&
            gimp_tool_control_get_motion_mode (active_tool->control) ==
            GIMP_MOTION_MODE_COMPRESS)
          {
            compressed_motion = gimp_display_shell_compress_motion (shell);
          }

        if (compressed_motion)
          {
            GdkDevice *device = gimp_devices_get_current (gimp);

            gimp_display_shell_get_event_coords (shell, compressed_motion,
                                                 device, &display_coords);
            gimp_display_shell_get_event_state (shell, compressed_motion,
                                                device, &state);
            time = gdk_event_get_time (event);

            /*  GimpCoords passed to tools are ALWAYS in image coordinates  */
            gimp_display_shell_untransform_coords (shell,
                                                   &display_coords,
                                                   &image_coords);

            if (active_tool &&
                gimp_tool_control_get_snap_to (active_tool->control))
              {
                gint x, y, width, height;

                gimp_tool_control_get_snap_offsets (active_tool->control,
                                                    &x, &y, &width, &height);

                gimp_display_shell_snap_coords (shell,
                                                &image_coords,
                                                &image_coords,
                                                x, y, width, height);
              }
          }

        /* Ask for the pointer position, but ignore it except for cursor
         * handling, so motion events sync with the button press/release events
         */
        if (mevent->is_hint)
          {
            gimp_display_shell_get_device_coords (shell,
                                                  mevent->device,
                                                  &display_coords);
          }

        update_sw_cursor = TRUE;

        if (! shell->proximity)
          {
            shell->proximity = TRUE;
            gimp_display_shell_check_device_cursor (shell);
          }

        if (state & GDK_BUTTON1_MASK)
          {
            if (active_tool                                        &&
                gimp_tool_control_is_active (active_tool->control) &&
                (! gimp_image_is_empty (image) ||
                 gimp_tool_control_get_handle_empty_image (active_tool->control)))
              {
                GdkTimeCoord **history_events;
                gint           n_history_events;

               /*  if the first mouse button is down, check for automatic
                 *  scrolling...
                 */
                if ((mevent->x < 0                 ||
                     mevent->y < 0                 ||
                     mevent->x > shell->disp_width ||
                     mevent->y > shell->disp_height) &&
                    ! gimp_tool_control_get_scroll_lock (active_tool->control))
                  {
                    gimp_display_shell_autoscroll_start (shell, state, mevent);
                  }

                if (gimp_tool_control_get_motion_mode (active_tool->control) ==
                    GIMP_MOTION_MODE_EXACT &&
                    gdk_device_get_history (mevent->device, mevent->window,
                                            shell->last_motion_time,
                                            mevent->time,
                                            &history_events,
                                            &n_history_events))
                  {
                    gint i;

                    for (i = 0; i < n_history_events; i++)
                      {
                        gimp_display_shell_get_time_coords (shell,
                                                            mevent->device,
                                                            history_events[i],
                                                            &display_coords);

                        /*  GimpCoords passed to tools are ALWAYS in
                         *  image coordinates
                         */
                        gimp_display_shell_untransform_coords (shell,
                                                               &display_coords,
                                                               &image_coords);

                        if (gimp_tool_control_get_snap_to (active_tool->control))
                          {
                            gint x, y, width, height;

                            gimp_tool_control_get_snap_offsets (active_tool->control,
                                                                &x, &y, &width, &height);

                            gimp_display_shell_snap_coords (shell,
                                                            &image_coords,
                                                            &image_coords,
                                                            x, y, width, height);
                          }

                        tool_manager_motion_active (gimp,
                                                    &image_coords,
                                                    history_events[i]->time,
                                                    state,
                                                    display);
                      }

                    gdk_device_free_history (history_events, n_history_events);
                  }
                else
                  {
                    tool_manager_motion_active (gimp,
                                                &image_coords, time, state,
                                                display);
                  }

                shell->last_motion_time = mevent->time;
              }
          }
        else if (state & GDK_BUTTON2_MASK)
          {
            if (shell->scrolling)
              {
                gimp_display_shell_scroll (shell,
                                           (shell->scroll_start_x - mevent->x -
                                            shell->offset_x),
                                           (shell->scroll_start_y - mevent->y -
                                            shell->offset_y));
              }
          }

        if (! (state &
               (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)))
          {
            tool_manager_oper_update_active (gimp,
                                             &image_coords, state,
                                             shell->proximity,
                                             display);
          }
      }
      break;

    case GDK_KEY_PRESS:
      {
        GdkEventKey *kevent = (GdkEventKey *) event;

        tool_manager_focus_display_active (gimp, display);

        switch (kevent->keyval)
          {
          case GDK_Return:
          case GDK_KP_Enter:
          case GDK_BackSpace:
          case GDK_Delete:
          case GDK_Escape:
          case GDK_Left:
          case GDK_Right:
          case GDK_Up:
          case GDK_Down:
            if (gimp_image_is_empty (image) ||
                ! tool_manager_key_press_active (gimp,
                                                 kevent,
                                                 display))
              {
                GimpController *keyboard;

                keyboard = gimp_controllers_get_keyboard (gimp);

                if (keyboard)
                  gimp_controller_keyboard_key_press (GIMP_CONTROLLER_KEYBOARD (keyboard),
                                                      kevent);
              }

            return_val = TRUE;
            break;

          case GDK_space:
            {
              active_tool = tool_manager_get_active (gimp);

              if (active_tool &&
                  ! shell->space_pressed && ! GIMP_IS_MOVE_TOOL (active_tool))
                {
                  GimpToolInfo *move_tool_info;

                  move_tool_info = (GimpToolInfo *)
                    gimp_container_get_child_by_name (gimp->tool_info_list,
                                                      "gimp-move-tool");

                  if (GIMP_IS_TOOL_INFO (move_tool_info))
                    {
#ifdef DEBUG_MOVE_PUSH
                      g_printerr ("%s: pushing move tool\n", G_STRFUNC);
#endif

                      space_shaded_tool = active_tool->tool_info;

                      gdk_keyboard_grab (canvas->window, FALSE, time);

                      gimp_context_set_tool (gimp_get_user_context (gimp),
                                             move_tool_info);

                      tool_manager_focus_display_active (gimp, display);
                      tool_manager_modifier_state_active (gimp, state, display);

                      shell->space_pressed = TRUE;
                    }
                }
            }

            return_val = TRUE;
            break;

          case GDK_Tab:
          case GDK_ISO_Left_Tab:
            if (state & GDK_CONTROL_MASK)
              {
                if (! gimp_image_is_empty (image))
                  {
                    if (kevent->keyval == GDK_Tab)
                      gimp_display_shell_layer_select_init (shell,
                                                            1, kevent->time);
                    else
                      gimp_display_shell_layer_select_init (shell,
                                                            -1, kevent->time);
                  }
              }
            else
              {
                gimp_dialog_factories_toggle ();
              }

            return_val = TRUE;
            break;

            /*  Update the state based on modifiers being pressed  */
          case GDK_Alt_L:     case GDK_Alt_R:
          case GDK_Shift_L:   case GDK_Shift_R:
          case GDK_Control_L: case GDK_Control_R:
            {
              GdkModifierType key;

              key = gimp_display_shell_key_to_state (kevent->keyval);
              state |= key;

              /*  For all modifier keys: call the tools modifier_state *and*
               *  oper_update method so tools can choose if they are interested
               *  in the release itself or only in the resulting state
               */
              if (! gimp_image_is_empty (image))
                tool_manager_modifier_state_active (gimp, state, display);
            }

            break;
          }

        tool_manager_oper_update_active (gimp,
                                         &image_coords, state,
                                         shell->proximity,
                                         display);
      }
      break;

    case GDK_KEY_RELEASE:
      {
        GdkEventKey *kevent = (GdkEventKey *) event;

        tool_manager_focus_display_active (gimp, display);

        switch (kevent->keyval)
          {
          case GDK_space:
            if (shell->space_pressed)
              {
#ifdef DEBUG_MOVE_PUSH
                g_printerr ("%s: popping move tool\n", G_STRFUNC);
#endif

                gimp_context_set_tool (gimp_get_user_context (gimp),
                                       space_shaded_tool);
                space_shaded_tool = NULL;

                tool_manager_focus_display_active (gimp, display);
                tool_manager_modifier_state_active (gimp, state, display);

                shell->space_pressed = FALSE;

                gdk_display_keyboard_ungrab (gdk_display, time);
             }

            return_val = TRUE;
            break;

            /*  Update the state based on modifiers being pressed  */
          case GDK_Alt_L:     case GDK_Alt_R:
          case GDK_Shift_L:   case GDK_Shift_R:
          case GDK_Control_L: case GDK_Control_R:
            {
              GdkModifierType key;

              key = gimp_display_shell_key_to_state (kevent->keyval);
              state &= ~key;

              /*  For all modifier keys: call the tools modifier_state *and*
               *  oper_update method so tools can choose if they are interested
               *  in the press itself or only in the resulting state
               */
              if (! gimp_image_is_empty (image))
                tool_manager_modifier_state_active (gimp, state, display);
            }

            break;
          }

        tool_manager_oper_update_active (gimp,
                                         &image_coords, state,
                                         shell->proximity,
                                         display);
      }
      break;

    default:
      break;
    }

  /*  if we reached this point in gimp_busy mode, return now  */
  if (gimp->busy)
    return return_val;

  /*  cursor update support  */

  if (GIMP_DISPLAY_CONFIG (gimp->config)->cursor_updating)
    {
      active_tool = tool_manager_get_active (gimp);

      if (active_tool)
        {
          if ((! gimp_image_is_empty (image) ||
               gimp_tool_control_get_handle_empty_image (active_tool->control)) &&
              ! (state & (GDK_BUTTON1_MASK |
                          GDK_BUTTON2_MASK |
                          GDK_BUTTON3_MASK)))
            {
              tool_manager_cursor_update_active (gimp,
                                                 &image_coords, state,
                                                 display);
            }
          else if (gimp_image_is_empty (image) &&
                   ! gimp_tool_control_get_handle_empty_image (active_tool->control))
            {
              gimp_display_shell_set_cursor (shell,
                                             GIMP_CURSOR_MOUSE,
                                             gimp_tool_control_get_tool_cursor (active_tool->control),
                                             GIMP_CURSOR_MODIFIER_BAD);
            }
        }
      else
        {
          gimp_display_shell_set_cursor (shell,
                                         GIMP_CURSOR_MOUSE,
                                         GIMP_TOOL_CURSOR_NONE,
                                         GIMP_CURSOR_MODIFIER_BAD);
        }
    }

  if (update_sw_cursor)
    gimp_display_shell_update_cursor (shell,
                                      (gint) display_coords.x,
                                      (gint) display_coords.y,
                                      (gint) image_coords.x,
                                      (gint) image_coords.y);

  return return_val;
}

gboolean
gimp_display_shell_ruler_button_press (GtkWidget        *widget,
                                       GdkEventButton   *event,
                                       GimpDisplayShell *shell,
                                       gboolean          horizontal)
{
  GimpDisplay *display = shell->display;

  if (display->image->gimp->busy)
    return TRUE;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      GimpTool *active_tool;
      gboolean  sample_point;

      active_tool  = tool_manager_get_active (display->image->gimp);
      sample_point = (event->state & GDK_CONTROL_MASK);

      if (! ((sample_point   && GIMP_IS_COLOR_TOOL (active_tool)) ||
             (! sample_point && GIMP_IS_MOVE_TOOL (active_tool))))
        {
          GimpToolInfo *tool_info;

          tool_info = (GimpToolInfo *)
            gimp_container_get_child_by_name (display->image->gimp->tool_info_list,
                                              sample_point ?
                                              "gimp-color-picker-tool" :
                                              "gimp-move-tool");

          if (tool_info)
            gimp_context_set_tool (gimp_get_user_context (display->image->gimp),
                                   tool_info);
        }

      active_tool = tool_manager_get_active (display->image->gimp);

      if (active_tool)
        {
          gdk_pointer_grab (shell->canvas->window, FALSE,
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_BUTTON_RELEASE_MASK,
                            NULL, NULL, event->time);

          gdk_keyboard_grab (shell->canvas->window, FALSE, event->time);

          if (sample_point)
            gimp_color_tool_start_sample_point (active_tool, display);
          else if (horizontal)
            gimp_move_tool_start_hguide (active_tool, display);
          else
            gimp_move_tool_start_vguide (active_tool, display);
        }
    }

  return FALSE;
}

gboolean
gimp_display_shell_hruler_button_press (GtkWidget        *widget,
                                        GdkEventButton   *event,
                                        GimpDisplayShell *shell)
{
  return gimp_display_shell_ruler_button_press (widget, event, shell, TRUE);
}

gboolean
gimp_display_shell_vruler_button_press (GtkWidget        *widget,
                                        GdkEventButton   *event,
                                        GimpDisplayShell *shell)
{
  return gimp_display_shell_ruler_button_press (widget, event, shell, FALSE);
}

gboolean
gimp_display_shell_origin_button_press (GtkWidget        *widget,
                                        GdkEventButton   *event,
                                        GimpDisplayShell *shell)
{
  if (! shell->display->image->gimp->busy)
    {
      if (event->button == 1)
        {
          gboolean unused;

          g_signal_emit_by_name (shell, "popup-menu", &unused);
        }
    }

  /* Return TRUE to stop signal emission so the button doesn't grab the
   * pointer away from us.
   */
  return TRUE;
}

gboolean
gimp_display_shell_quick_mask_button_press (GtkWidget        *widget,
                                            GdkEventButton   *bevent,
                                            GimpDisplayShell *shell)
{
  if ((bevent->type == GDK_BUTTON_PRESS) && (bevent->button == 3))
    {
      gimp_ui_manager_ui_popup (shell->menubar_manager, "/quick-mask-popup",
                                GTK_WIDGET (shell),
                                NULL, NULL, NULL, NULL);

      return TRUE;
    }

  return FALSE;
}

void
gimp_display_shell_quick_mask_toggled (GtkWidget        *widget,
                                       GimpDisplayShell *shell)
{
  if (GTK_TOGGLE_BUTTON (widget)->active !=
      gimp_image_get_quick_mask_state (shell->display->image))
    {
      gimp_image_set_quick_mask_state (shell->display->image,
                                       GTK_TOGGLE_BUTTON (widget)->active);

      gimp_image_flush (shell->display->image);
    }
}

gboolean
gimp_display_shell_nav_button_press (GtkWidget        *widget,
                                     GdkEventButton   *bevent,
                                     GimpDisplayShell *shell)
{
  if ((bevent->type == GDK_BUTTON_PRESS) && (bevent->button == 1))
    {
      gimp_navigation_editor_popup (shell, widget, bevent->x, bevent->y);
    }

  return TRUE;
}


/*  private functions  */

static void
gimp_display_shell_vscrollbar_update (GtkAdjustment    *adjustment,
                                      GimpDisplayShell *shell)
{
  gimp_display_shell_scroll (shell, 0, (adjustment->value - shell->offset_y));
}

static void
gimp_display_shell_hscrollbar_update (GtkAdjustment    *adjustment,
                                      GimpDisplayShell *shell)
{
  gimp_display_shell_scroll (shell, (adjustment->value - shell->offset_x), 0);
}

static GdkModifierType
gimp_display_shell_key_to_state (gint key)
{
  switch (key)
    {
    case GDK_Alt_L:
    case GDK_Alt_R:
      return GDK_MOD1_MASK;
    case GDK_Shift_L:
    case GDK_Shift_R:
      return GDK_SHIFT_MASK;
    case GDK_Control_L:
    case GDK_Control_R:
      return GDK_CONTROL_MASK;
    default:
      return 0;
    }
}

/* gimp_display_shell_compress_motion:
 *
 * This function walks the whole GDK event queue seeking motion events
 * corresponding to the widget 'widget'.  If it finds any it will
 * remove them from the queue, and return the most recent motion event.
 * Otherwise it will return NULL.
 *
 * The gimp_display_shell_compress_motion function source may be re-used under
 * the XFree86-style license. <adam@gimp.org>
 */
static GdkEvent *
gimp_display_shell_compress_motion (GimpDisplayShell *shell)
{
  GList    *requeued_events = NULL;
  GList    *list;
  GdkEvent *last_motion = NULL;

  /*  Move the entire GDK event queue to a private list, filtering
   *  out any motion events for the desired widget.
   */
  while (gdk_events_pending ())
    {
      GdkEvent *event = gdk_event_get ();

      if (!event)
        {
          /* Do nothing */
        }
      else if ((gtk_get_event_widget (event) == shell->canvas) &&
               (event->any.type == GDK_MOTION_NOTIFY))
        {
          if (last_motion)
            gdk_event_free (last_motion);

          last_motion = event;
        }
      else
        {
          requeued_events = g_list_prepend (requeued_events, event);
        }
    }

  /* Replay the remains of our private event list back into the
   * event queue in order.
   */
  requeued_events = g_list_reverse (requeued_events);

  for (list = requeued_events; list; list = g_list_next (list))
    {
      GdkEvent *event = list->data;

      gdk_event_put (event);
      gdk_event_free (event);
    }

  g_list_free (requeued_events);

  return last_motion;
}
