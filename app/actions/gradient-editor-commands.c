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

#include "actions-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpgradient.h"

#include "widgets/gimpcolordialog.h"
#include "widgets/gimpgradienteditor.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpviewabledialog.h"

#include "dialogs/dialogs.h"

#include "gradient-editor-commands.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   gradient_editor_left_color_update      (GimpColorDialog     *dialog,
                                                      const GimpRGB       *color,
                                                      GimpColorDialogState state,
                                                      GimpGradientEditor  *editor);
static void   gradient_editor_right_color_update     (GimpColorDialog     *dialog,
                                                      const GimpRGB       *color,
                                                      GimpColorDialogState state,
                                                      GimpGradientEditor  *editor);

static GimpGradientSegment *
              gradient_editor_save_selection         (GimpGradientEditor  *editor);
static void   gradient_editor_replace_selection      (GimpGradientEditor  *editor,
                                                      GimpGradientSegment *replace_seg);

static void   gradient_editor_split_uniform_response (GtkWidget           *widget,
                                                      gint                 response_id,
                                                      GimpGradientEditor  *editor);
static void   gradient_editor_replicate_response     (GtkWidget           *widget,
                                                      gint                 response_id,
                                                      GimpGradientEditor  *editor);


/*  public functionss */

void
gradient_editor_left_color_cmd_callback (GtkAction *action,
                                         gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  editor->left_saved_dirty    = GIMP_DATA (gradient)->dirty;
  editor->left_saved_segments = gradient_editor_save_selection (editor);

  editor->color_dialog =
    gimp_color_dialog_new (GIMP_VIEWABLE (gradient),
                           _("Left Endpoint Color"),
                           GIMP_STOCK_GRADIENT,
                           _("Gradient Segment's Left Endpoint Color"),
                           GTK_WIDGET (editor),
                           global_dialog_factory,
                           "gimp-gradient-editor-color-dialog",
                           &editor->control_sel_l->left_color,
                           editor->instant_update, TRUE);

  g_signal_connect (editor->color_dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &editor->color_dialog);

  g_signal_connect (editor->color_dialog, "update",
                    G_CALLBACK (gradient_editor_left_color_update),
                    editor);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  gtk_window_present (GTK_WINDOW (editor->color_dialog));
}

void
gradient_editor_load_left_cmd_callback (GtkAction *action,
                                        gint       value,
                                        gpointer   data)
{
  GimpGradientEditor  *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient        *gradient;
  GimpContext         *context;
  GimpGradientSegment *seg;
  GimpRGB              color;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  context =
    gimp_get_user_context (GIMP_DATA_EDITOR (editor)->data_factory->gimp);

  switch (value)
    {
    case 0: /* Fetch from left neighbor's right endpoint */
      if (editor->control_sel_l->prev != NULL)
	seg = editor->control_sel_l->prev;
      else
	seg = gimp_gradient_segment_get_last (editor->control_sel_l);

      color = seg->right_color;
      break;

    case 1: /* Fetch from right endpoint */
      color = editor->control_sel_r->right_color;
      break;

    case 2: /* Fetch from FG color */
      gimp_context_get_foreground (context, &color);
      break;

    case 3: /* Fetch from BG color */
      gimp_context_get_background (context, &color);
      break;

    default: /* Load a color */
      color = editor->saved_colors[value - 4];
      break;
    }

  gimp_gradient_segment_range_blend (gradient,
                                     editor->control_sel_l,
                                     editor->control_sel_r,
                                     &color,
                                     &editor->control_sel_r->right_color,
                                     TRUE, TRUE);
}

void
gradient_editor_save_left_cmd_callback (GtkAction *action,
                                        gint       value,
                                        gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_get_left_color (gradient, editor->control_sel_l,
                                        &editor->saved_colors[value]);
}

void
gradient_editor_right_color_cmd_callback (GtkAction *action,
                                          gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  editor->right_saved_dirty    = GIMP_DATA (gradient)->dirty;
  editor->right_saved_segments = gradient_editor_save_selection (editor);

  editor->color_dialog =
    gimp_color_dialog_new (GIMP_VIEWABLE (gradient),
                           _("Right Endpoint Color"),
                           GIMP_STOCK_GRADIENT,
                           _("Gradient Segment's Right Endpoint Color"),
                           GTK_WIDGET (editor),
                           global_dialog_factory,
                           "gimp-gradient-editor-color-dialog",
                           &editor->control_sel_l->right_color,
                           editor->instant_update, TRUE);

  g_signal_connect (editor->color_dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &editor->color_dialog);

  g_signal_connect (editor->color_dialog, "update",
                    G_CALLBACK (gradient_editor_right_color_update),
                    editor);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  gtk_window_present (GTK_WINDOW (editor->color_dialog));
}

void
gradient_editor_load_right_cmd_callback (GtkAction *action,
                                         gint       value,
                                         gpointer   data)
{
  GimpGradientEditor  *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient        *gradient;
  GimpContext         *context;
  GimpGradientSegment *seg;
  GimpRGB              color;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  context =
    gimp_get_user_context (GIMP_DATA_EDITOR (editor)->data_factory->gimp);

  switch (value)
    {
    case 0: /* Fetch from right neighbor's left endpoint */
      if (editor->control_sel_r->next != NULL)
	seg = editor->control_sel_r->next;
      else
	seg = gimp_gradient_segment_get_first (editor->control_sel_r);

      color = seg->left_color;
      break;

    case 1: /* Fetch from left endpoint */
      color = editor->control_sel_l->left_color;
      break;

    case 2: /* Fetch from FG color */
      gimp_context_get_foreground (context, &color);
      break;

    case 3: /* Fetch from BG color */
      gimp_context_get_background (context, &color);
      break;

    default: /* Load a color */
      color = editor->saved_colors[value - 4];
      break;
    }

  gimp_gradient_segment_range_blend (gradient,
                                     editor->control_sel_l,
                                     editor->control_sel_r,
                                     &editor->control_sel_l->left_color,
                                     &color,
                                     TRUE, TRUE);
}

void
gradient_editor_save_right_cmd_callback (GtkAction *action,
                                         gint       value,
                                         gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_get_right_color (gradient, editor->control_sel_r,
                                         &editor->saved_colors[value]);
}

void
gradient_editor_blending_func_cmd_callback (GtkAction *action,
                                            GtkAction *current,
                                            gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;
  gint                value;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

  if (gradient && value >= 0)
    {
      GimpGradientSegmentType type = value;

      gimp_gradient_segment_range_set_blending_function (gradient,
                                                         editor->control_sel_l,
                                                         editor->control_sel_r,
                                                         type);
    }
}

void
gradient_editor_coloring_type_cmd_callback (GtkAction *action,
                                            GtkAction *current,
                                            gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;
  gint                value;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

  if (gradient && value >= 0)
    {
      GimpGradientSegmentColor color = value;

      gimp_gradient_segment_range_set_coloring_type (gradient,
                                                     editor->control_sel_l,
                                                     editor->control_sel_r,
                                                     color);
    }
}

void
gradient_editor_flip_cmd_callback (GtkAction *action,
                                   gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_flip (gradient,
                                    editor->control_sel_l,
                                    editor->control_sel_r,
                                    &editor->control_sel_l,
                                    &editor->control_sel_r);
}

void
gradient_editor_replicate_cmd_callback (GtkAction *action,
                                        gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GtkWidget          *dialog;
  GtkWidget          *vbox;
  GtkWidget          *label;
  GtkWidget          *scale;
  GtkObject          *scale_data;
  const gchar        *title;
  const gchar        *desc;

  if (editor->control_sel_l == editor->control_sel_r)
    {
      title = _("Replicate Segment");
      desc  = _("Replicate Gradient Segment");
    }
  else
    {
      title = _("Replicate Selection");
      desc  = _("Replicate Gradient Selection");
    }

  dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (GIMP_DATA_EDITOR (editor)->data),
                              title, "gimp-gradient-segment-replicate",
                              GIMP_STOCK_GRADIENT, desc,
                              GTK_WIDGET (editor),
                              gimp_standard_help_func,
                              GIMP_HELP_GRADIENT_EDITOR_REPLICATE,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              _("Replicate"),   GTK_RESPONSE_OK,

                              NULL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gradient_editor_replicate_response),
                    editor);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
  gtk_widget_show (vbox);

  /*  Instructions  */
  if (editor->control_sel_l == editor->control_sel_r)
    label = gtk_label_new (_("Select the number of times\n"
                             "to replicate the selected segment."));
  else
    label = gtk_label_new (_("Select the number of times\n"
                             "to replicate the selection."));

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /*  Scale  */
  editor->replicate_times = 2;
  scale_data  = gtk_adjustment_new (2.0, 2.0, 21.0, 1.0, 1.0, 1.0);

  scale = gtk_hscale_new (GTK_ADJUSTMENT (scale_data));
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, TRUE, 4);
  gtk_widget_show (scale);

  g_signal_connect (scale_data, "value_changed",
		    G_CALLBACK (gimp_int_adjustment_update),
		    &editor->replicate_times);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  gtk_widget_show (dialog);
}

void
gradient_editor_split_midpoint_cmd_callback (GtkAction *action,
                                             gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_split_midpoint (gradient,
                                              editor->control_sel_l,
                                              editor->control_sel_r,
                                              &editor->control_sel_l,
                                              &editor->control_sel_r);
}

void
gradient_editor_split_uniformly_cmd_callback (GtkAction *action,
                                              gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GtkWidget          *dialog;
  GtkWidget          *vbox;
  GtkWidget          *label;
  GtkWidget          *scale;
  GtkObject          *scale_data;
  const gchar        *title;
  const gchar        *desc;

  if (editor->control_sel_l == editor->control_sel_r)
    {
      title = _("Split Segment Uniformly");
      desc  = _("Split Gradient Segment Uniformly");
    }
  else
    {
      title = _("Split Segments Uniformly");
      desc  = _("Split Gradient Segments Uniformly");
    }

  dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (GIMP_DATA_EDITOR (editor)->data),
                              title, "gimp-gradient-segment-split-uniformly",
                              GIMP_STOCK_GRADIENT, desc,
                              GTK_WIDGET (editor),
                              gimp_standard_help_func,
                              GIMP_HELP_GRADIENT_EDITOR_SPLIT_UNIFORM,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              _("Split"),       GTK_RESPONSE_OK,

                              NULL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gradient_editor_split_uniform_response),
                    editor);

  /*  The main vbox  */
  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), vbox);
  gtk_widget_show (vbox);

  /*  Instructions  */
  if (editor->control_sel_l == editor->control_sel_r)
    label = gtk_label_new (_("Select the number of uniform parts\n"
                             "in which to split the selected segment."));
  else
    label = gtk_label_new (_("Select the number of uniform parts\n"
                             "in which to split the segments in the selection."));

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /*  Scale  */
  editor->split_parts = 2;
  scale_data = gtk_adjustment_new (2.0, 2.0, 21.0, 1.0, 1.0, 1.0);

  scale = gtk_hscale_new (GTK_ADJUSTMENT (scale_data));
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 4);
  gtk_widget_show (scale);

  g_signal_connect (scale_data, "value_changed",
		    G_CALLBACK (gimp_int_adjustment_update),
		    &editor->split_parts);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  gtk_widget_show (dialog);
}

void
gradient_editor_delete_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_delete (gradient,
                                      editor->control_sel_l,
                                      editor->control_sel_r,
                                      &editor->control_sel_l,
                                      &editor->control_sel_r);
}

void
gradient_editor_recenter_cmd_callback (GtkAction *action,
                                       gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_recenter_handles (gradient,
                                                editor->control_sel_l,
                                                editor->control_sel_r);
}

void
gradient_editor_redistribute_cmd_callback (GtkAction *action,
                                           gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_redistribute_handles (gradient,
                                                    editor->control_sel_l,
                                                    editor->control_sel_r);
}

void
gradient_editor_blend_color_cmd_callback (GtkAction *action,
                                          gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_blend (gradient,
                                     editor->control_sel_l,
                                     editor->control_sel_r,
                                     &editor->control_sel_l->left_color,
                                     &editor->control_sel_r->right_color,
                                     TRUE, FALSE);
}

void
gradient_editor_blend_opacity_cmd_callback (GtkAction *action,
                                            gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);
  GimpGradient       *gradient;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  gimp_gradient_segment_range_blend (gradient,
                                     editor->control_sel_l,
                                     editor->control_sel_r,
                                     &editor->control_sel_l->left_color,
                                     &editor->control_sel_r->right_color,
                                     FALSE, TRUE);
}

void
gradient_editor_zoom_cmd_callback (GtkAction *action,
                                   gint       value,
                                   gpointer   data)
{
  GimpGradientEditor *editor = GIMP_GRADIENT_EDITOR (data);

  gimp_gradient_editor_zoom (editor, (GimpZoomType) value);
}


/*  private functions  */

static void
gradient_editor_left_color_update (GimpColorDialog      *dialog,
                                   const GimpRGB        *color,
                                   GimpColorDialogState  state,
                                   GimpGradientEditor   *editor)
{
  GimpGradient *gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  switch (state)
    {
    case GIMP_COLOR_DIALOG_UPDATE:
      gimp_gradient_segment_range_blend (gradient,
                                         editor->control_sel_l,
                                         editor->control_sel_r,
                                         color,
                                         &editor->control_sel_r->right_color,
                                         TRUE, TRUE);
      break;

    case GIMP_COLOR_DIALOG_OK:
      gimp_gradient_segment_range_blend (gradient,
                                         editor->control_sel_l,
                                         editor->control_sel_r,
                                         color,
                                         &editor->control_sel_r->right_color,
                                         TRUE, TRUE);
      gimp_gradient_segments_free (editor->left_saved_segments);
      gtk_widget_destroy (editor->color_dialog);
      editor->color_dialog = NULL;
      gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
      gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                              GIMP_EDITOR (editor)->popup_data);
      break;

    case GIMP_COLOR_DIALOG_CANCEL:
      gradient_editor_replace_selection (editor, editor->left_saved_segments);
      GIMP_DATA (gradient)->dirty = editor->left_saved_dirty;
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gradient));
      gtk_widget_destroy (editor->color_dialog);
      editor->color_dialog = NULL;
      gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
      gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                              GIMP_EDITOR (editor)->popup_data);
      break;
    }
}

static void
gradient_editor_right_color_update (GimpColorDialog      *dialog,
                                    const GimpRGB        *color,
                                    GimpColorDialogState  state,
                                    GimpGradientEditor   *editor)
{
  GimpGradient *gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  switch (state)
    {
    case GIMP_COLOR_DIALOG_UPDATE:
      gimp_gradient_segment_range_blend (gradient,
                                         editor->control_sel_l,
                                         editor->control_sel_r,
                                         &editor->control_sel_r->left_color,
                                         color,
                                         TRUE, TRUE);
      break;

    case GIMP_COLOR_DIALOG_OK:
      gimp_gradient_segment_range_blend (gradient,
                                         editor->control_sel_l,
                                         editor->control_sel_r,
                                         &editor->control_sel_r->left_color,
                                         color,
                                         TRUE, TRUE);
      gimp_gradient_segments_free (editor->right_saved_segments);
      gtk_widget_destroy (editor->color_dialog);
      editor->color_dialog = NULL;
      gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
      gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                              GIMP_EDITOR (editor)->popup_data);
      break;

    case GIMP_COLOR_DIALOG_CANCEL:
      gradient_editor_replace_selection (editor, editor->right_saved_segments);
      GIMP_DATA (gradient)->dirty = editor->right_saved_dirty;
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (gradient));
      gtk_widget_destroy (editor->color_dialog);
      editor->color_dialog = NULL;
      gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
      gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                              GIMP_EDITOR (editor)->popup_data);
      break;
    }
}

static GimpGradientSegment *
gradient_editor_save_selection (GimpGradientEditor *editor)
{
  GimpGradientSegment *seg, *prev, *tmp;
  GimpGradientSegment *oseg, *oaseg;

  prev = NULL;
  oseg = editor->control_sel_l;
  tmp  = NULL;

  do
    {
      seg = gimp_gradient_segment_new ();

      *seg = *oseg; /* Copy everything */

      if (prev == NULL)
	tmp = seg; /* Remember first segment */
      else
	prev->next = seg;

      seg->prev = prev;
      seg->next = NULL;

      prev  = seg;
      oaseg = oseg;
      oseg  = oseg->next;
    }
  while (oaseg != editor->control_sel_r);

  return tmp;
}

static void
gradient_editor_replace_selection (GimpGradientEditor  *editor,
                                   GimpGradientSegment *replace_seg)
{
  GimpGradient        *gradient;
  GimpGradientSegment *lseg, *rseg;
  GimpGradientSegment *replace_last;

  gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

  /* Remember left and right segments */

  lseg = editor->control_sel_l->prev;
  rseg = editor->control_sel_r->next;

  replace_last = gimp_gradient_segment_get_last (replace_seg);

  /* Free old selection */

  editor->control_sel_r->next = NULL;

  gimp_gradient_segments_free (editor->control_sel_l);

  /* Link in new segments */

  if (lseg)
    lseg->next = replace_seg;
  else
    gradient->segments = replace_seg;

  replace_seg->prev = lseg;

  if (rseg)
    rseg->prev = replace_last;

  replace_last->next = rseg;

  editor->control_sel_l = replace_seg;
  editor->control_sel_r = replace_last;

  gradient->last_visited = NULL; /* Force re-search */
}

static void
gradient_editor_split_uniform_response (GtkWidget          *widget,
                                        gint                response_id,
                                        GimpGradientEditor *editor)
{
  gtk_widget_destroy (widget);
  gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  if (response_id == GTK_RESPONSE_OK)
    {
      GimpGradient *gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

      gimp_gradient_segment_range_split_uniform (gradient,
                                                 editor->control_sel_l,
                                                 editor->control_sel_r,
                                                 editor->split_parts,
                                                 &editor->control_sel_l,
                                                 &editor->control_sel_r);
    }
}

static void
gradient_editor_replicate_response (GtkWidget          *widget,
                                    gint                response_id,
                                    GimpGradientEditor *editor)
{
  gtk_widget_destroy (widget);
  gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
  gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                          GIMP_EDITOR (editor)->popup_data);

  if (response_id == GTK_RESPONSE_OK)
    {
      GimpGradient *gradient = GIMP_GRADIENT (GIMP_DATA_EDITOR (editor)->data);

      gimp_gradient_segment_range_replicate (gradient,
                                             editor->control_sel_l,
                                             editor->control_sel_r,
                                             editor->replicate_times,
                                             &editor->control_sel_l,
                                             &editor->control_sel_r);
    }
}
