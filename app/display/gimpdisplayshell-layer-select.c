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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "display-types.h"

#include "config/gimpcoreconfig.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"

#include "widgets/gimpview.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-layer-select.h"

#include "gimp-intl.h"


typedef struct _LayerSelect LayerSelect;

struct _LayerSelect
{
  GtkWidget *shell;
  GtkWidget *preview;
  GtkWidget *label;

  GimpImage *gimage;
  GimpLayer *orig_layer;
};


/*  local function prototypes  */

static LayerSelect * layer_select_new       (GimpImage   *gimage,
                                             GimpLayer   *layer,
                                             gint         preview_size);
static void          layer_select_destroy   (LayerSelect *layer_select,
                                             guint32      time);
static void          layer_select_advance   (LayerSelect *layer_select,
                                             gint         move);
static gboolean      layer_select_events    (GtkWidget   *widget,
                                             GdkEvent    *event,
                                             LayerSelect *layer_select);


/*  public functions  */

void
gimp_display_shell_layer_select_init (GimpDisplayShell *shell,
                                      gint              move,
                                      guint32           time)
{
  LayerSelect *layer_select;
  GimpImage   *gimage;
  GimpLayer   *layer;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimage = shell->gdisp->gimage;

  layer = gimp_image_get_active_layer (gimage);

  if (! layer)
    return;

  layer_select = layer_select_new (gimage, layer,
                                   gimage->gimp->config->layer_preview_size);
  layer_select_advance (layer_select, move);

  gtk_window_set_screen (GTK_WINDOW (layer_select->shell),
                         gtk_widget_get_screen (GTK_WIDGET (shell)));

  gtk_widget_show (layer_select->shell);

  gdk_keyboard_grab (layer_select->shell->window, FALSE, time);
}


/*  private functions  */

static LayerSelect *
layer_select_new (GimpImage *gimage,
                  GimpLayer *layer,
                  gint       preview_size)
{
  LayerSelect *layer_select;
  GtkWidget   *frame1;
  GtkWidget   *frame2;
  GtkWidget   *hbox;
  GtkWidget   *alignment;

  layer_select = g_new0 (LayerSelect, 1);

  layer_select->gimage     = gimage;
  layer_select->orig_layer = layer;

  layer_select->shell = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_role (GTK_WINDOW (layer_select->shell), "gimp-layer-select");
  gtk_window_set_title (GTK_WINDOW (layer_select->shell), _("Layer Select"));
  gtk_window_set_position (GTK_WINDOW (layer_select->shell), GTK_WIN_POS_MOUSE);
  gtk_widget_set_events (layer_select->shell, (GDK_KEY_PRESS_MASK   |
                                               GDK_KEY_RELEASE_MASK |
                                               GDK_BUTTON_PRESS_MASK));

  g_signal_connect (layer_select->shell, "event",
                    G_CALLBACK (layer_select_events),
                    layer_select);

  frame1 = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame1), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (layer_select->shell), frame1);
  gtk_widget_show (frame1);

  frame2 = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame1), frame2);
  gtk_widget_show (frame2);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (frame2), hbox);
  gtk_widget_show (hbox);

  /*  The preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);
  gtk_widget_show (alignment);

  layer_select->preview = gimp_view_new_by_types (GIMP_TYPE_VIEW,
                                                  GIMP_TYPE_LAYER,
                                                  preview_size, 1, FALSE);
  gimp_view_set_viewable (GIMP_VIEW (layer_select->preview),
                          GIMP_VIEWABLE (layer));
  gtk_container_add (GTK_CONTAINER (alignment), layer_select->preview);
  gtk_widget_show (layer_select->preview);
  gtk_widget_show (alignment);

  /*  the layer name label */
  layer_select->label = gtk_label_new (GIMP_OBJECT (layer)->name);
  gtk_box_pack_start (GTK_BOX (hbox), layer_select->label, FALSE, FALSE, 0);
  gtk_widget_show (layer_select->label);

  return layer_select;
}

static void
layer_select_destroy (LayerSelect *layer_select,
                      guint32      time)
{
  gdk_display_keyboard_ungrab (gtk_widget_get_display (layer_select->shell),
                                                       time);

  gtk_widget_destroy (layer_select->shell);

  if (layer_select->orig_layer !=
      gimp_image_get_active_layer (layer_select->gimage))
    {
      gimp_image_flush (layer_select->gimage);
    }

  g_free (layer_select);
}

static void
layer_select_advance (LayerSelect *layer_select,
		      gint         move)
{
  GimpLayer *current_layer;
  GimpLayer *next_layer;
  gint       index;

  if (move == 0)
    return;

  /*  If there is a floating selection, allow no advancement  */
  if (gimp_image_floating_sel (layer_select->gimage))
    return;

  current_layer = gimp_image_get_active_layer (layer_select->gimage);

  index = gimp_container_get_child_index (layer_select->gimage->layers,
                                          GIMP_OBJECT (current_layer));

  index += move;

  if (index < 0)
    index = gimp_container_num_children (layer_select->gimage->layers) - 1;
  else if (index >= gimp_container_num_children (layer_select->gimage->layers))
    index = 0;

  next_layer = (GimpLayer *)
    gimp_container_get_child_by_index (layer_select->gimage->layers, index);

  if (next_layer && next_layer != current_layer)
    {
      current_layer = gimp_image_set_active_layer (layer_select->gimage,
                                                   next_layer);

      if (current_layer)
        {
          gimp_view_set_viewable (GIMP_VIEW (layer_select->preview),
                                  GIMP_VIEWABLE (current_layer));
          gtk_label_set_text (GTK_LABEL (layer_select->label),
                              GIMP_OBJECT (current_layer)->name);
        }
    }
}

static gboolean
layer_select_events (GtkWidget   *widget,
		     GdkEvent    *event,
                     LayerSelect *layer_select)
{
  GdkEventKey    *kevent;
  GdkEventButton *bevent;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;

      layer_select_destroy (layer_select, bevent->time);
      break;

    case GDK_KEY_PRESS:
      kevent = (GdkEventKey *) event;

      switch (kevent->keyval)
	{
	case GDK_Tab:
          layer_select_advance (layer_select, 1);
          break;
	case GDK_ISO_Left_Tab:
          layer_select_advance (layer_select, -1);
	  break;
	}
      return TRUE;
      break;

    case GDK_KEY_RELEASE:
      kevent = (GdkEventKey *) event;

      switch (kevent->keyval)
	{
	case GDK_Alt_L: case GDK_Alt_R:
	  kevent->state &= ~GDK_MOD1_MASK;
	  break;
	case GDK_Control_L: case GDK_Control_R:
	  kevent->state &= ~GDK_CONTROL_MASK;
	  break;
	case GDK_Shift_L: case GDK_Shift_R:
	  kevent->state &= ~GDK_SHIFT_MASK;
	  break;
	}

      if (! (kevent->state & GDK_CONTROL_MASK))
	layer_select_destroy (layer_select, kevent->time);

      return TRUE;
      break;

    default:
      break;
    }

  return FALSE;
}