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

#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "core/gimpimage.h"
#include "core/gimpunit.h"
#include "core/gimpmarshal.h"
#include "core/gimpprogress.h"

#include "widgets/gimpunitstore.h"
#include "widgets/gimpunitcombobox.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-scale.h"
#include "gimpscalecombobox.h"
#include "gimpstatusbar.h"

#include "gimp-intl.h"


typedef struct _GimpStatusbarMsg GimpStatusbarMsg;

struct _GimpStatusbarMsg
{
  guint  context_id;
  gchar *text;
};

/* maximal width of the string holding the cursor-coordinates for
 * the status line
 */
#define CURSOR_LEN 256


static void     gimp_statusbar_class_init     (GimpStatusbarClass *klass);
static void     gimp_statusbar_init           (GimpStatusbar      *statusbar);
static void     gimp_statusbar_progress_iface_init (GimpProgressInterface *progress_iface);

static void     gimp_statusbar_destroy            (GtkObject         *object);

static GimpProgress *
                gimp_statusbar_progress_start     (GimpProgress      *progress,
                                                   const gchar       *message,
                                                   gboolean           cancelable);
static void     gimp_statusbar_progress_end       (GimpProgress      *progress);
static gboolean gimp_statusbar_progress_is_active (GimpProgress      *progress);
static void     gimp_statusbar_progress_set_text  (GimpProgress      *progress,
                                                   const gchar       *message);
static void     gimp_statusbar_progress_set_value (GimpProgress      *progress,
                                                   gdouble            percentage);
static gdouble  gimp_statusbar_progress_get_value (GimpProgress      *progress);
static void     gimp_statusbar_progress_canceled  (GtkWidget         *button,
                                                   GimpStatusbar     *statusbar);

static void     gimp_statusbar_update             (GimpStatusbar     *statusbar);
static void     gimp_statusbar_unit_changed       (GimpUnitComboBox  *combo,
                                                   GimpStatusbar     *statusbar);
static void     gimp_statusbar_scale_changed      (GimpScaleComboBox *combo,
                                                   GimpStatusbar     *statusbar);
static void     gimp_statusbar_shell_scaled       (GimpDisplayShell  *shell,
                                                   GimpStatusbar     *statusbar);
static guint    gimp_statusbar_get_context_id     (GimpStatusbar     *statusbar,
                                                   const gchar       *context);


static GtkHBoxClass *parent_class = NULL;


GType
gimp_statusbar_get_type (void)
{
  static GType statusbar_type = 0;

  if (! statusbar_type)
    {
      static const GTypeInfo statusbar_info =
      {
        sizeof (GimpStatusbarClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_statusbar_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpStatusbar),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_statusbar_init,
      };

      static const GInterfaceInfo progress_iface_info =
      {
        (GInterfaceInitFunc) gimp_statusbar_progress_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      statusbar_type = g_type_register_static (GTK_TYPE_HBOX,
                                               "GimpStatusbar",
                                               &statusbar_info, 0);

      g_type_add_interface_static (statusbar_type, GIMP_TYPE_PROGRESS,
                                   &progress_iface_info);
    }

  return statusbar_type;
}

static void
gimp_statusbar_class_init (GimpStatusbarClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->destroy = gimp_statusbar_destroy;

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                           _("Shadow type"),
                                           _("Style of bevel around the statusbar text"),
                                           GTK_TYPE_SHADOW_TYPE,
                                           GTK_SHADOW_IN,
                                           G_PARAM_READABLE));
}

static void
gimp_statusbar_init (GimpStatusbar *statusbar)
{
  GtkBox        *box = GTK_BOX (statusbar);
  GtkWidget     *hbox;
  GtkWidget     *frame;
  GimpUnitStore *store;
  GtkShadowType  shadow_type;
  gboolean       has_focus_on_click;

  box->spacing     = 2;
  box->homogeneous = FALSE;

  statusbar->shell                = NULL;
  statusbar->cursor_format_str[0] = '\0';
  statusbar->length_format_str[0] = '\0';
  statusbar->progress_active      = FALSE;

  gtk_box_set_spacing (box, 1);

  gtk_widget_style_get (GTK_WIDGET (statusbar),
                        "shadow_type", &shadow_type,
                        NULL);

  statusbar->cursor_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->cursor_frame), shadow_type);
  gtk_box_pack_start (box, statusbar->cursor_frame, FALSE, FALSE, 0);
  gtk_box_reorder_child (box, statusbar->cursor_frame, 0);
  gtk_widget_show (statusbar->cursor_frame);

  hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (statusbar->cursor_frame), hbox);
  gtk_widget_show (hbox);

  statusbar->cursor_label = gtk_label_new ("0, 0");
  gtk_misc_set_alignment (GTK_MISC (statusbar->cursor_label), 0.5, 0.5);
  gtk_container_add (GTK_CONTAINER (hbox), statusbar->cursor_label);
  gtk_widget_show (statusbar->cursor_label);

  store = gimp_unit_store_new (2);
  statusbar->unit_combo = gimp_unit_combo_box_new_with_model (store);
  g_object_unref (store);

  has_focus_on_click =
    g_object_class_find_property (G_OBJECT_GET_CLASS (statusbar->unit_combo),
                                  "focus-on-click") != NULL;

  GTK_WIDGET_UNSET_FLAGS (statusbar->unit_combo, GTK_CAN_FOCUS);
  if (has_focus_on_click)
    g_object_set (statusbar->unit_combo, "focus-on-click", FALSE, NULL);
  gtk_container_add (GTK_CONTAINER (hbox), statusbar->unit_combo);
  gtk_widget_show (statusbar->unit_combo);

  g_signal_connect (statusbar->unit_combo, "changed",
                    G_CALLBACK (gimp_statusbar_unit_changed),
                    statusbar);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), shadow_type);
  gtk_box_pack_start (box, frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  statusbar->scale_combo = gimp_scale_combo_box_new ();
  GTK_WIDGET_UNSET_FLAGS (statusbar->scale_combo, GTK_CAN_FOCUS);
  if (has_focus_on_click)
    g_object_set (statusbar->scale_combo, "focus-on-click", FALSE, NULL);
  gtk_container_add (GTK_CONTAINER (frame), statusbar->scale_combo);
  gtk_widget_show (statusbar->scale_combo);

  g_signal_connect (statusbar->scale_combo, "changed",
                    G_CALLBACK (gimp_statusbar_scale_changed),
                    statusbar);

  statusbar->progressbar = gtk_progress_bar_new ();
  gtk_box_pack_start (box, statusbar->progressbar, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->progressbar);

  GTK_PROGRESS_BAR (statusbar->progressbar)->progress.x_align = 0.0;
  GTK_PROGRESS_BAR (statusbar->progressbar)->progress.y_align = 0.5;

  statusbar->cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_set_sensitive (statusbar->cancel_button, FALSE);
  gtk_box_pack_start (box, statusbar->cancel_button, FALSE, FALSE, 0);
  GTK_WIDGET_UNSET_FLAGS (statusbar->cancel_button, GTK_CAN_FOCUS);
  gtk_widget_show (statusbar->cancel_button);

  g_signal_connect (statusbar->cancel_button, "clicked",
                    G_CALLBACK (gimp_statusbar_progress_canceled),
                    statusbar);

  /* Update the statusbar once to work around a canvas size problem:
   *
   *  The first update of the statusbar used to queue a resize which
   *  in term caused the canvas to be resized. That made it shrink by
   *  one pixel in height resulting in the last row not being displayed.
   *  Shrink-wrapping the display used to fix this reliably. With the
   *  next call the resize doesn't seem to happen any longer.
   */

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (statusbar->progressbar),
                             "GIMP");

  statusbar->seq_context_id = 1;
  statusbar->messages       = NULL;
  statusbar->keys           = NULL;
}

static void
gimp_statusbar_progress_iface_init (GimpProgressInterface *progress_iface)
{
  progress_iface->start     = gimp_statusbar_progress_start;
  progress_iface->end       = gimp_statusbar_progress_end;
  progress_iface->is_active = gimp_statusbar_progress_is_active;
  progress_iface->set_text  = gimp_statusbar_progress_set_text;
  progress_iface->set_value = gimp_statusbar_progress_set_value;
  progress_iface->get_value = gimp_statusbar_progress_get_value;
}

static void
gimp_statusbar_destroy (GtkObject *object)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (object);
  GSList        *list;

  for (list = statusbar->messages; list; list = list->next)
    {
      GimpStatusbarMsg *msg = list->data;

      g_free (msg->text);
      g_free (msg);
    }

  g_slist_free (statusbar->messages);
  statusbar->messages = NULL;

  for (list = statusbar->keys; list; list = list->next)
    g_free (list->data);

  g_slist_free (statusbar->keys);
  statusbar->keys = NULL;

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static GimpProgress *
gimp_statusbar_progress_start (GimpProgress *progress,
                               const gchar  *message,
                               gboolean      cancelable)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  if (! statusbar->progress_active)
    {
      GtkWidget *bar = statusbar->progressbar;

      gimp_statusbar_push (statusbar, "progress", message);
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), 0.0);
      gtk_widget_set_sensitive (statusbar->cancel_button, cancelable);

      statusbar->progress_active = TRUE;

      if (GTK_WIDGET_DRAWABLE (bar))
        gdk_window_process_updates (bar->window, TRUE);

      return progress;
    }

  return NULL;
}

static void
gimp_statusbar_progress_end (GimpProgress *progress)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  if (statusbar->progress_active)
    {
      GtkWidget *bar = statusbar->progressbar;

      gimp_statusbar_pop (statusbar, "progress");
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), 0.0);
      gtk_widget_set_sensitive (statusbar->cancel_button, FALSE);

      statusbar->progress_active = FALSE;
    }
}

static gboolean
gimp_statusbar_progress_is_active (GimpProgress *progress)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  return statusbar->progress_active;
}

static void
gimp_statusbar_progress_set_text (GimpProgress *progress,
                                  const gchar  *message)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  if (statusbar->progress_active)
    {
      GtkWidget *bar = statusbar->progressbar;

      gimp_statusbar_replace (statusbar, "progress", message);

      if (GTK_WIDGET_DRAWABLE (bar))
        gdk_window_process_updates (bar->window, TRUE);
    }
}

static void
gimp_statusbar_progress_set_value (GimpProgress *progress,
                                   gdouble       percentage)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  if (statusbar->progress_active)
    {
      GtkWidget *bar = statusbar->progressbar;

      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), percentage);

      if (GTK_WIDGET_DRAWABLE (bar))
        gdk_window_process_updates (bar->window, TRUE);
    }
}

static gdouble
gimp_statusbar_progress_get_value (GimpProgress *progress)
{
  GimpStatusbar *statusbar = GIMP_STATUSBAR (progress);

  if (statusbar->progress_active)
    {
      GtkWidget *bar = statusbar->progressbar;

      return gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (bar));
    }

  return 0.0;
}

static void
gimp_statusbar_progress_canceled (GtkWidget     *button,
                                  GimpStatusbar *statusbar)
{
  if (statusbar->progress_active)
    gimp_progress_cancel (GIMP_PROGRESS (statusbar));
}

static void
gimp_statusbar_update (GimpStatusbar *statusbar)
{
  gchar *text = NULL;

  if (statusbar->messages)
    {
      GimpStatusbarMsg *msg = statusbar->messages->data;

      text = msg->text;
    }

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (statusbar->progressbar),
                             text ? text : "");
}

GtkWidget *
gimp_statusbar_new (GimpDisplayShell *shell)
{
  GimpStatusbar *statusbar;

  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), NULL);

  statusbar = g_object_new (GIMP_TYPE_STATUSBAR, NULL);

  statusbar->shell = shell;

  g_signal_connect_object (shell, "scaled",
                           G_CALLBACK (gimp_statusbar_shell_scaled),
                           statusbar, 0);

  return GTK_WIDGET (statusbar);
}

void
gimp_statusbar_push (GimpStatusbar *statusbar,
                     const gchar   *context,
                     const gchar   *message)
{
  GimpStatusbarMsg *msg;
  guint             context_id;
  GSList           *list;

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));
  g_return_if_fail (message != NULL);

  context_id = gimp_statusbar_get_context_id (statusbar, context);

  for (list = statusbar->messages; list; list = g_slist_next (list))
    {
      msg = list->data;

      if (msg->context_id == context_id)
        {
          statusbar->messages = g_slist_remove (statusbar->messages, msg);
          g_free (msg->text);
          g_free (msg);

          break;
        }
    }

  msg = g_new0 (GimpStatusbarMsg, 1);

  msg->context_id = context_id;
  msg->text       = g_strdup (message);

  statusbar->messages = g_slist_prepend (statusbar->messages, msg);

  gimp_statusbar_update (statusbar);
}

void
gimp_statusbar_push_coords (GimpStatusbar *statusbar,
                            const gchar   *context,
                            const gchar   *title,
                            gdouble        x,
                            const gchar   *separator,
                            gdouble        y)
{
  GimpDisplayShell *shell;
  gchar             buf[CURSOR_LEN];

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));
  g_return_if_fail (title != NULL);
  g_return_if_fail (separator != NULL);

  shell = statusbar->shell;

  if (shell->unit == GIMP_UNIT_PIXEL)
    {
      g_snprintf (buf, sizeof (buf), statusbar->cursor_format_str,
                  title,
                  (gint) RINT (x),
                  separator,
                  (gint) RINT (y));
    }
  else /* show real world units */
    {
      GimpImage *image       = shell->gdisp->gimage;
      gdouble    unit_factor = _gimp_unit_get_factor (image->gimp,
                                                      shell->unit);

      g_snprintf (buf, sizeof (buf), statusbar->cursor_format_str,
                  title,
                  x * unit_factor / image->xresolution,
                  separator,
                  y * unit_factor / image->yresolution);
    }

  gimp_statusbar_push (statusbar, context, buf);
}

void
gimp_statusbar_push_length (GimpStatusbar       *statusbar,
                            const gchar         *context,
                            const gchar         *title,
                            GimpOrientationType  axis,
                            gdouble              value)
{
  GimpDisplayShell *shell;
  gchar             buf[CURSOR_LEN];

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));
  g_return_if_fail (title != NULL);

  shell = statusbar->shell;

  if (shell->unit == GIMP_UNIT_PIXEL)
    {
      g_snprintf (buf, sizeof (buf), statusbar->length_format_str,
                  title,
                  (gint) RINT (value));
    }
  else /* show real world units */
    {
      GimpImage *image       = shell->gdisp->gimage;
      gdouble    resolution;
      gdouble    unit_factor = _gimp_unit_get_factor (image->gimp,
                                                      shell->unit);

      switch (axis)
        {
        case GIMP_ORIENTATION_HORIZONTAL:
          resolution = image->xresolution;
          break;

        case GIMP_ORIENTATION_VERTICAL:
          resolution = image->yresolution;
          break;

        default:
          g_return_if_reached ();
          break;
        }

      g_snprintf (buf, sizeof (buf), statusbar->length_format_str,
                  title,
                  value * unit_factor / resolution);
    }

  gimp_statusbar_push (statusbar, context, buf);
}

void
gimp_statusbar_replace (GimpStatusbar *statusbar,
                        const gchar   *context,
                        const gchar   *message)
{
  GimpStatusbarMsg *msg;
  guint             context_id;
  GSList           *list;

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));
  g_return_if_fail (message != NULL);

  if (! statusbar->messages)
    {
      gimp_statusbar_push (statusbar, context, message);
      return;
    }

  context_id = gimp_statusbar_get_context_id (statusbar, context);

  msg = statusbar->messages->data;

  if (msg->context_id == context_id)
    {
      gimp_statusbar_pop (statusbar, context);
      gimp_statusbar_push (statusbar, context, message);

      return;
    }

  for (list = statusbar->messages; list; list = g_slist_next (list))
    {
      msg = list->data;

      if (msg->context_id == context_id)
        {
          g_free (msg->text);
          msg->text = g_strdup (message);

          return;
        }
    }

  msg = g_new0 (GimpStatusbarMsg, 1);

  msg->context_id = context_id;
  msg->text       = g_strdup (message);

  statusbar->messages = g_slist_prepend (statusbar->messages, msg);

  gimp_statusbar_update (statusbar);
}

void
gimp_statusbar_pop (GimpStatusbar *statusbar,
                    const gchar   *context)
{
  guint   context_id;
  GSList *list;

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));

  context_id = gimp_statusbar_get_context_id (statusbar, context);

  for (list = statusbar->messages; list; list = list->next)
    {
      GimpStatusbarMsg *msg = list->data;

      if (msg->context_id == context_id)
        {
          statusbar->messages = g_slist_remove (statusbar->messages, msg);
          g_free (msg->text);
          g_free (msg);

          break;
        }
    }

  gimp_statusbar_update (statusbar);
}

void
gimp_statusbar_set_cursor (GimpStatusbar *statusbar,
                           gdouble        x,
                           gdouble        y)
{
  GimpDisplayShell *shell;
  GtkTreeModel     *model;
  GimpUnitStore    *store;
  gchar             buffer[CURSOR_LEN];

  g_return_if_fail (GIMP_IS_STATUSBAR (statusbar));

  shell = statusbar->shell;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (statusbar->unit_combo));
  store = GIMP_UNIT_STORE (model);

  gimp_unit_store_set_pixel_values (store, x, y);

  if (shell->unit == GIMP_UNIT_PIXEL)
    {
      g_snprintf (buffer, sizeof (buffer),
                  statusbar->cursor_format_str,
                  "", (gint) RINT (x), ", ", (gint) RINT (y));
    }
  else /* show real world units */
    {
      gimp_unit_store_get_values (store, shell->unit, &x, &y);

      g_snprintf (buffer, sizeof (buffer),
                  statusbar->cursor_format_str,
                  "", x, ", ", y);
    }

  gtk_label_set_text (GTK_LABEL (statusbar->cursor_label), buffer);

  if (x <  0 ||
      y <  0 ||
      x >= statusbar->shell->gdisp->gimage->width ||
      y >= statusbar->shell->gdisp->gimage->height)
    {
      gtk_widget_set_sensitive (statusbar->cursor_label, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (statusbar->cursor_label, TRUE);
    }
}

void
gimp_statusbar_clear_cursor (GimpStatusbar *statusbar)
{
  gtk_label_set_text (GTK_LABEL (statusbar->cursor_label), "");
  gtk_widget_set_sensitive (statusbar->cursor_label, TRUE);
}

static void
gimp_statusbar_shell_scaled (GimpDisplayShell *shell,
                             GimpStatusbar    *statusbar)
{
  static PangoLayout *layout = NULL;

  GimpImage    *image = shell->gdisp->gimage;
  GtkTreeModel *model;
  const gchar  *text;
  gint          width;
  gint          diff;

  g_signal_handlers_block_by_func (statusbar->scale_combo,
                                   gimp_statusbar_scale_changed, statusbar);
  gimp_scale_combo_box_set_scale (GIMP_SCALE_COMBO_BOX (statusbar->scale_combo),
                                  shell->scale);
  g_signal_handlers_unblock_by_func (statusbar->scale_combo,
                                     gimp_statusbar_scale_changed, statusbar);

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (statusbar->unit_combo));
  gimp_unit_store_set_resolutions (GIMP_UNIT_STORE (model),
                                   image->xresolution, image->yresolution);

  g_signal_handlers_block_by_func (statusbar->unit_combo,
                                   gimp_statusbar_unit_changed, statusbar);
  gimp_unit_combo_box_set_active (GIMP_UNIT_COMBO_BOX (statusbar->unit_combo),
                                  shell->unit);
  g_signal_handlers_unblock_by_func (statusbar->unit_combo,
                                     gimp_statusbar_unit_changed, statusbar);

  if (shell->unit == GIMP_UNIT_PIXEL)
    {
      g_snprintf (statusbar->cursor_format_str,
                  sizeof (statusbar->cursor_format_str),
                  "%%s%%d%%s%%d");
      g_snprintf (statusbar->length_format_str,
                  sizeof (statusbar->length_format_str),
                  "%%s%%d");
    }
  else /* show real world units */
    {
      g_snprintf (statusbar->cursor_format_str,
                  sizeof (statusbar->cursor_format_str),
                  "%%s%%.%df%%s%%.%df",
                  _gimp_unit_get_digits (image->gimp, shell->unit),
                  _gimp_unit_get_digits (image->gimp, shell->unit));
      g_snprintf (statusbar->length_format_str,
                  sizeof (statusbar->length_format_str),
                  "%%s%%.%df",
                  _gimp_unit_get_digits (image->gimp, shell->unit));
    }

  gimp_statusbar_set_cursor (statusbar, - image->width, - image->height);

  text = gtk_label_get_text (GTK_LABEL (statusbar->cursor_label));

  /* one static layout for all displays should be fine */
  if (! layout)
    layout = gtk_widget_create_pango_layout (statusbar->cursor_label, text);
  else
    pango_layout_set_text (layout, text, -1);

  pango_layout_get_pixel_size (layout, &width, NULL);

  /*  find out how many pixels the label's parent frame is bigger than
   *  the label itself
   */
  diff = (statusbar->cursor_frame->allocation.width -
          statusbar->cursor_label->allocation.width);

  gtk_widget_set_size_request (statusbar->cursor_label, width, -1);

  /* don't resize if this is a new display */
  if (diff)
    gtk_widget_set_size_request (statusbar->cursor_frame, width + diff, -1);

  gimp_statusbar_clear_cursor (statusbar);
}

static void
gimp_statusbar_unit_changed (GimpUnitComboBox *combo,
                             GimpStatusbar    *statusbar)
{
  gimp_display_shell_set_unit (statusbar->shell,
                               gimp_unit_combo_box_get_active (combo));
}

static void
gimp_statusbar_scale_changed (GimpScaleComboBox *combo,
                              GimpStatusbar     *statusbar)
{
  gimp_display_shell_scale (statusbar->shell,
                            GIMP_ZOOM_TO,
                            gimp_scale_combo_box_get_scale (combo));
}

static guint
gimp_statusbar_get_context_id (GimpStatusbar *statusbar,
                               const gchar   *context)
{
  gchar *string;
  guint *id;

  g_return_val_if_fail (GIMP_IS_STATUSBAR (statusbar), 0);
  g_return_val_if_fail (context != NULL, 0);

  /* we need to preserve namespaces on object datas */
  string = g_strconcat ("gimp-status-bar-context:", context, NULL);

  id = g_object_get_data (G_OBJECT (statusbar), string);
  if (!id)
    {
      id = g_new (guint, 1);
      *id = statusbar->seq_context_id++;
      g_object_set_data_full (G_OBJECT (statusbar), string, id, g_free);
      statusbar->keys = g_slist_prepend (statusbar->keys, string);
    }
  else
    {
      g_free (string);
    }

  return *id;
}
