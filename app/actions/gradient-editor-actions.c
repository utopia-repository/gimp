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

#include "widgets/gimpactiongroup.h"
#include "widgets/gimpdataeditor.h"
#include "widgets/gimpgradienteditor.h"
#include "widgets/gimphelp-ids.h"

#include "gradient-editor-actions.h"
#include "gradient-editor-commands.h"

#include "gimp-intl.h"


static GimpActionEntry gradient_editor_actions[] =
{
  { "gradient-editor-popup", GIMP_STOCK_GRADIENT,
    N_("Gradient Editor Menu"), NULL, NULL, NULL,
    GIMP_HELP_GRADIENT_EDITOR_DIALOG },

  { "gradient-editor-load-left-color", GTK_STOCK_REVERT_TO_SAVED,
    N_("_Load Left Color From") },
  { "gradient-editor-save-left-color", GTK_STOCK_SAVE,
    N_("_Save Left Color To") },

  { "gradient-editor-load-right-color", GTK_STOCK_REVERT_TO_SAVED,
    N_("Load Right Color Fr_om") },
  { "gradient-editor-save-right-color", GTK_STOCK_SAVE,
    N_("Sa_ve Right Color To") },

  { "gradient-editor-blending-func", NULL, "blending-function" },
  { "gradient-editor-coloring-type", NULL, "coloring-type"     },

  { "gradient-editor-left-color", NULL,
    N_("L_eft Endpoint's Color..."), NULL, NULL,
    G_CALLBACK (gradient_editor_left_color_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_LEFT_COLOR },

  { "gradient-editor-right-color", NULL,
    N_("R_ight Endpoint's Color..."), NULL, NULL,
    G_CALLBACK (gradient_editor_right_color_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_COLOR },

  { "gradient-editor-flip", GIMP_STOCK_FLIP_HORIZONTAL,
    "flip", NULL, NULL,
    G_CALLBACK (gradient_editor_flip_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_FLIP },

  { "gradient-editor-replicate", GIMP_STOCK_DUPLICATE,
    "replicate", NULL, NULL,
    G_CALLBACK (gradient_editor_replicate_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_FLIP },

  { "gradient-editor-split-midpoint", NULL,
    "splitmidpoint", NULL, NULL,
    G_CALLBACK (gradient_editor_split_midpoint_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_SPLIT_MIDPOINT },

  { "gradient-editor-split-uniform", NULL,
    "splituniform", NULL, NULL,
    G_CALLBACK (gradient_editor_split_uniformly_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_SPLIT_UNIFORM },

  { "gradient-editor-delete", GTK_STOCK_DELETE,
    "delete", "", NULL,
    G_CALLBACK (gradient_editor_delete_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_DELETE },

  { "gradient-editor-recenter", NULL,
    "recenter", NULL, NULL,
    G_CALLBACK (gradient_editor_recenter_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_RECENTER },

  { "gradient-editor-redistribute", NULL,
    "redistribute", NULL, NULL,
    G_CALLBACK (gradient_editor_redistribute_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_REDISTRIBUTE },

  { "gradient-editor-blend-color", NULL,
    N_("Ble_nd Endpoints' Colors"), NULL, NULL,
    G_CALLBACK (gradient_editor_blend_color_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_BLEND_COLOR },

  { "gradient-editor-blend-opacity", NULL,
    N_("Blend Endpoints' Opacit_y"), NULL, NULL,
    G_CALLBACK (gradient_editor_blend_opacity_cmd_callback),
    GIMP_HELP_GRADIENT_EDITOR_BLEND_OPACITY }
};


#define LOAD_LEFT_FROM(num,magic) \
  { "gradient-editor-load-left-" num, NULL, \
    num, NULL, NULL, \
    (magic), FALSE, \
    GIMP_HELP_GRADIENT_EDITOR_LEFT_LOAD }
#define SAVE_LEFT_TO(num,magic) \
  { "gradient-editor-save-left-" num, NULL, \
    num, NULL, NULL, \
    (magic), FALSE, \
    GIMP_HELP_GRADIENT_EDITOR_LEFT_SAVE }
#define LOAD_RIGHT_FROM(num,magic) \
  { "gradient-editor-load-right-" num, NULL, \
    num, NULL, NULL, \
    (magic), FALSE, \
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_LOAD }
#define SAVE_RIGHT_TO(num,magic) \
  { "gradient-editor-save-right-" num, NULL, \
    num, NULL, NULL, \
    (magic), FALSE, \
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_SAVE }

static GimpEnumActionEntry gradient_editor_load_left_actions[] =
{
  { "gradient-editor-load-left-left-neighbor", NULL,
    N_("_Left Neighbor's Right Endpoint"), NULL, NULL,
    0, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_LEFT_LOAD },

  { "gradient-editor-load-left-right-endpoint", NULL,
    N_("_Right Endpoint"), NULL, NULL,
    1, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_LEFT_LOAD },

  { "gradient-editor-load-left-fg", NULL,
    N_("_FG Color"), NULL, NULL,
    2, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_LEFT_LOAD },

  { "gradient-editor-load-left-bg", NULL,
    N_("_BG Color"), NULL, NULL,
    3, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_LEFT_LOAD },

  LOAD_LEFT_FROM ("01", 4),
  LOAD_LEFT_FROM ("02", 5),
  LOAD_LEFT_FROM ("03", 6),
  LOAD_LEFT_FROM ("04", 7),
  LOAD_LEFT_FROM ("05", 8),
  LOAD_LEFT_FROM ("06", 9),
  LOAD_LEFT_FROM ("07", 10),
  LOAD_LEFT_FROM ("08", 11),
  LOAD_LEFT_FROM ("09", 12),
  LOAD_LEFT_FROM ("10", 13)
};

static GimpEnumActionEntry gradient_editor_save_left_actions[] =
{
  SAVE_LEFT_TO ("01", 0),
  SAVE_LEFT_TO ("02", 1),
  SAVE_LEFT_TO ("03", 2),
  SAVE_LEFT_TO ("04", 3),
  SAVE_LEFT_TO ("05", 4),
  SAVE_LEFT_TO ("06", 5),
  SAVE_LEFT_TO ("07", 6),
  SAVE_LEFT_TO ("08", 7),
  SAVE_LEFT_TO ("09", 8),
  SAVE_LEFT_TO ("10", 9)
};

static GimpEnumActionEntry gradient_editor_load_right_actions[] =
{
  { "gradient-editor-load-right-right-neighbor", NULL,
    N_("_Right Neighbor's Left Endpoint"), NULL, NULL,
    0, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_LOAD },

  { "gradient-editor-load-right-left-endpoint", NULL,
    N_("_Left Endpoint"), NULL, NULL,
    1, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_LOAD },

  { "gradient-editor-load-right-fg", NULL,
    N_("_FG Color"), NULL, NULL,
    2, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_LOAD },

  { "gradient-editor-load-right-bg", NULL,
    N_("_BG Color"), NULL, NULL,
    3, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_RIGHT_LOAD },

  LOAD_RIGHT_FROM ("01", 4),
  LOAD_RIGHT_FROM ("02", 5),
  LOAD_RIGHT_FROM ("03", 6),
  LOAD_RIGHT_FROM ("04", 7),
  LOAD_RIGHT_FROM ("05", 8),
  LOAD_RIGHT_FROM ("06", 9),
  LOAD_RIGHT_FROM ("07", 10),
  LOAD_RIGHT_FROM ("08", 11),
  LOAD_RIGHT_FROM ("09", 12),
  LOAD_RIGHT_FROM ("10", 13)
};

static GimpEnumActionEntry gradient_editor_save_right_actions[] =
{
  SAVE_RIGHT_TO ("01", 0),
  SAVE_RIGHT_TO ("02", 1),
  SAVE_RIGHT_TO ("03", 2),
  SAVE_RIGHT_TO ("04", 3),
  SAVE_RIGHT_TO ("05", 4),
  SAVE_RIGHT_TO ("06", 5),
  SAVE_RIGHT_TO ("07", 6),
  SAVE_RIGHT_TO ("08", 7),
  SAVE_RIGHT_TO ("09", 8),
  SAVE_RIGHT_TO ("10", 9)
};

#undef LOAD_LEFT_FROM
#undef SAVE_LEFT_TO
#undef LOAD_RIGHT_FROM
#undef SAVE_RIGHT_TO


static GimpRadioActionEntry gradient_editor_blending_actions[] =
{
  { "gradient-editor-blending-linear", NULL,
    N_("_Linear"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_LINEAR,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING },

  { "gradient-editor-blending-curved", NULL,
    N_("_Curved"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_CURVED,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING },

  { "gradient-editor-blending-sine", NULL,
    N_("_Sinusoidal"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_SINE,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING },

  { "gradient-editor-blending-sphere-increasing", NULL,
    N_("Spherical (i_ncreasing)"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING },

  { "gradient-editor-blending-sphere-decreasing", NULL,
    N_("Spherical (_decreasing)"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING },

  { "gradient-editor-blending-varies", NULL,
    N_("(Varies)"), NULL, NULL,
    -1,
    GIMP_HELP_GRADIENT_EDITOR_BLENDING }
};

static GimpRadioActionEntry gradient_editor_coloring_actions[] =
{
  { "gradient-editor-coloring-rgb", NULL,
    N_("_RGB"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_RGB,
    GIMP_HELP_GRADIENT_EDITOR_COLORING },

  { "gradient-editor-coloring-hsv-ccw", NULL,
    N_("HSV (_counter-clockwise hue)"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_HSV_CCW,
    GIMP_HELP_GRADIENT_EDITOR_COLORING },

  { "gradient-editor-coloring-hsv-cw", NULL,
    N_("HSV (clockwise _hue)"), NULL, NULL,
    GIMP_GRADIENT_SEGMENT_HSV_CW,
    GIMP_HELP_GRADIENT_EDITOR_COLORING },

  { "gradient-editor-coloring-varies", NULL,
    N_("(Varies)"), NULL, NULL,
    -1,
    GIMP_HELP_GRADIENT_EDITOR_COLORING }
};

static GimpEnumActionEntry gradient_editor_zoom_actions[] =
{
  { "gradient-editor-zoom-in", GTK_STOCK_ZOOM_IN,
    N_("Zoom In"), NULL,
    N_("Zoom in"),
    GIMP_ZOOM_IN, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_ZOOM_IN },

  { "gradient-editor-zoom-out", GTK_STOCK_ZOOM_OUT,
    N_("Zoom Out"), NULL,
    N_("Zoom out"),
    GIMP_ZOOM_OUT, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_ZOOM_OUT },

  { "gradient-editor-zoom-all", GTK_STOCK_ZOOM_FIT,
    N_("Zoom All"), NULL,
    N_("Zoom all"),
    GIMP_ZOOM_TO /* abused */, FALSE,
    GIMP_HELP_GRADIENT_EDITOR_ZOOM_ALL }
};


void
gradient_editor_actions_setup (GimpActionGroup *group)
{
  gimp_action_group_add_actions (group,
                                 gradient_editor_actions,
                                 G_N_ELEMENTS (gradient_editor_actions));

  gimp_action_group_add_enum_actions (group,
                                      gradient_editor_load_left_actions,
                                      G_N_ELEMENTS (gradient_editor_load_left_actions),
                                      G_CALLBACK (gradient_editor_load_left_cmd_callback));

  gimp_action_group_add_enum_actions (group,
                                      gradient_editor_save_left_actions,
                                      G_N_ELEMENTS (gradient_editor_save_left_actions),
                                      G_CALLBACK (gradient_editor_save_left_cmd_callback));

  gimp_action_group_add_enum_actions (group,
                                      gradient_editor_load_right_actions,
                                      G_N_ELEMENTS (gradient_editor_load_right_actions),
                                      G_CALLBACK (gradient_editor_load_right_cmd_callback));


  gimp_action_group_add_enum_actions (group,
                                      gradient_editor_save_right_actions,
                                      G_N_ELEMENTS (gradient_editor_save_right_actions),
                                      G_CALLBACK (gradient_editor_save_right_cmd_callback));

  gimp_action_group_add_radio_actions (group,
                                       gradient_editor_blending_actions,
                                       G_N_ELEMENTS (gradient_editor_blending_actions),
                                       0,
                                       G_CALLBACK (gradient_editor_blending_func_cmd_callback));

  gimp_action_group_add_radio_actions (group,
                                       gradient_editor_coloring_actions,
                                       G_N_ELEMENTS (gradient_editor_coloring_actions),
                                       0,
                                       G_CALLBACK (gradient_editor_coloring_type_cmd_callback));

  gimp_action_group_add_enum_actions (group,
                                      gradient_editor_zoom_actions,
                                      G_N_ELEMENTS (gradient_editor_zoom_actions),
                                      G_CALLBACK (gradient_editor_zoom_cmd_callback));
}

void
gradient_editor_actions_update (GimpActionGroup *group,
                                gpointer         data)
{
  GimpGradientEditor  *editor         = GIMP_GRADIENT_EDITOR (data);
  GimpDataEditor      *data_editor    = GIMP_DATA_EDITOR (data);
  GimpGradient        *gradient;
  GimpContext         *context;
  gboolean             editable       = FALSE;
  GimpGradientSegment *left_seg       = NULL;
  GimpGradientSegment *right_seg      = NULL;
  GimpRGB              fg;
  GimpRGB              bg;
  gboolean             blending_equal = TRUE;
  gboolean             coloring_equal = TRUE;
  gboolean             selection      = FALSE;
  gboolean             delete         = FALSE;

  gradient = GIMP_GRADIENT (data_editor->data);

  context = gimp_get_user_context (data_editor->data_factory->gimp);

  if (gradient)
    {
      GimpGradientSegmentType  type;
      GimpGradientSegmentColor color;
      GimpGradientSegment     *seg, *aseg;

      if (data_editor->data_editable)
        editable = TRUE;

      if (editor->control_sel_l->prev)
        left_seg = editor->control_sel_l->prev;
      else
        left_seg = gimp_gradient_segment_get_last (editor->control_sel_l);

      if (editor->control_sel_r->next)
        right_seg = editor->control_sel_r->next;
      else
        right_seg = gimp_gradient_segment_get_first (editor->control_sel_r);

      type  = editor->control_sel_l->type;
      color = editor->control_sel_l->color;

      seg = editor->control_sel_l;

      do
        {
          blending_equal = blending_equal && (seg->type == type);
          coloring_equal = coloring_equal && (seg->color == color);

          aseg = seg;
          seg  = seg->next;
        }
      while (aseg != editor->control_sel_r);

      selection = (editor->control_sel_l != editor->control_sel_r);
      delete    = (editor->control_sel_l->prev || editor->control_sel_r->next);
    }

  if (context)
    {
      gimp_context_get_foreground (context, &fg);
      gimp_context_get_background (context, &bg);
    }

  /*  pretend the gradient not being editable while the dialog is
   *  insensitive. prevents the gradient from being modified while a
   *  dialog is running. bug #161411 --mitch
   */
  if (! GTK_WIDGET_SENSITIVE (editor))
    editable = FALSE;

#define SET_ACTIVE(action,condition) \
        gimp_action_group_set_action_active (group, action, (condition) != 0)
#define SET_COLOR(action,color,set_label) \
        gimp_action_group_set_action_color (group, action, (color), (set_label))
#define SET_LABEL(action,label) \
        gimp_action_group_set_action_label (group, action, (label))
#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)
#define SET_VISIBLE(action,condition) \
        gimp_action_group_set_action_visible (group, action, (condition) != 0)

  SET_SENSITIVE ("gradient-editor-left-color",               editable);
  SET_SENSITIVE ("gradient-editor-load-left-left-neighbor",  editable);
  SET_SENSITIVE ("gradient-editor-load-left-right-endpoint", editable);

  if (gradient)
    {
      SET_COLOR ("gradient-editor-left-color",
                 &editor->control_sel_l->left_color, FALSE);
      SET_COLOR ("gradient-editor-load-left-left-neighbor",
                 &left_seg->right_color, FALSE);
      SET_COLOR ("gradient-editor-load-left-right-endpoint",
                 &editor->control_sel_r->right_color, FALSE);
    }

  SET_SENSITIVE ("gradient-editor-load-left-fg", editable);
  SET_SENSITIVE ("gradient-editor-load-left-bg", editable);

  SET_COLOR ("gradient-editor-load-left-fg", context ? &fg : NULL, FALSE);
  SET_COLOR ("gradient-editor-load-left-bg", context ? &bg : NULL, FALSE);

  SET_SENSITIVE ("gradient-editor-load-left-01", editable);
  SET_SENSITIVE ("gradient-editor-load-left-02", editable);
  SET_SENSITIVE ("gradient-editor-load-left-03", editable);
  SET_SENSITIVE ("gradient-editor-load-left-04", editable);
  SET_SENSITIVE ("gradient-editor-load-left-05", editable);
  SET_SENSITIVE ("gradient-editor-load-left-06", editable);
  SET_SENSITIVE ("gradient-editor-load-left-07", editable);
  SET_SENSITIVE ("gradient-editor-load-left-08", editable);
  SET_SENSITIVE ("gradient-editor-load-left-09", editable);
  SET_SENSITIVE ("gradient-editor-load-left-10", editable);

  SET_COLOR ("gradient-editor-load-left-01", &editor->saved_colors[0], TRUE);
  SET_COLOR ("gradient-editor-load-left-02", &editor->saved_colors[1], TRUE);
  SET_COLOR ("gradient-editor-load-left-03", &editor->saved_colors[2], TRUE);
  SET_COLOR ("gradient-editor-load-left-04", &editor->saved_colors[3], TRUE);
  SET_COLOR ("gradient-editor-load-left-05", &editor->saved_colors[4], TRUE);
  SET_COLOR ("gradient-editor-load-left-06", &editor->saved_colors[5], TRUE);
  SET_COLOR ("gradient-editor-load-left-07", &editor->saved_colors[6], TRUE);
  SET_COLOR ("gradient-editor-load-left-08", &editor->saved_colors[7], TRUE);
  SET_COLOR ("gradient-editor-load-left-09", &editor->saved_colors[8], TRUE);
  SET_COLOR ("gradient-editor-load-left-10", &editor->saved_colors[9], TRUE);

  SET_SENSITIVE ("gradient-editor-save-left-01", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-02", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-03", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-04", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-05", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-06", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-07", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-08", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-09", gradient);
  SET_SENSITIVE ("gradient-editor-save-left-10", gradient);

  SET_COLOR ("gradient-editor-save-left-01", &editor->saved_colors[0], TRUE);
  SET_COLOR ("gradient-editor-save-left-02", &editor->saved_colors[1], TRUE);
  SET_COLOR ("gradient-editor-save-left-03", &editor->saved_colors[2], TRUE);
  SET_COLOR ("gradient-editor-save-left-04", &editor->saved_colors[3], TRUE);
  SET_COLOR ("gradient-editor-save-left-05", &editor->saved_colors[4], TRUE);
  SET_COLOR ("gradient-editor-save-left-06", &editor->saved_colors[5], TRUE);
  SET_COLOR ("gradient-editor-save-left-07", &editor->saved_colors[6], TRUE);
  SET_COLOR ("gradient-editor-save-left-08", &editor->saved_colors[7], TRUE);
  SET_COLOR ("gradient-editor-save-left-09", &editor->saved_colors[8], TRUE);
  SET_COLOR ("gradient-editor-save-left-10", &editor->saved_colors[9], TRUE);

  SET_SENSITIVE ("gradient-editor-right-color",               editable);
  SET_SENSITIVE ("gradient-editor-load-right-right-neighbor", editable);
  SET_SENSITIVE ("gradient-editor-load-right-left-endpoint",  editable);

  if (gradient)
    {
      SET_COLOR ("gradient-editor-right-color",
                 &editor->control_sel_r->right_color, FALSE);
      SET_COLOR ("gradient-editor-load-right-right-neighbor",
                 &right_seg->left_color, FALSE);
      SET_COLOR ("gradient-editor-load-right-left-endpoint",
                 &editor->control_sel_l->left_color, FALSE);
    }

  SET_SENSITIVE ("gradient-editor-load-right-fg", editable);
  SET_SENSITIVE ("gradient-editor-load-right-bg", editable);

  SET_COLOR ("gradient-editor-load-right-fg", context ? &fg : NULL, FALSE);
  SET_COLOR ("gradient-editor-load-right-bg", context ? &bg : NULL, FALSE);

  SET_SENSITIVE ("gradient-editor-load-right-01", editable);
  SET_SENSITIVE ("gradient-editor-load-right-02", editable);
  SET_SENSITIVE ("gradient-editor-load-right-03", editable);
  SET_SENSITIVE ("gradient-editor-load-right-04", editable);
  SET_SENSITIVE ("gradient-editor-load-right-05", editable);
  SET_SENSITIVE ("gradient-editor-load-right-06", editable);
  SET_SENSITIVE ("gradient-editor-load-right-07", editable);
  SET_SENSITIVE ("gradient-editor-load-right-08", editable);
  SET_SENSITIVE ("gradient-editor-load-right-09", editable);
  SET_SENSITIVE ("gradient-editor-load-right-10", editable);

  SET_COLOR ("gradient-editor-load-right-01", &editor->saved_colors[0], TRUE);
  SET_COLOR ("gradient-editor-load-right-02", &editor->saved_colors[1], TRUE);
  SET_COLOR ("gradient-editor-load-right-03", &editor->saved_colors[2], TRUE);
  SET_COLOR ("gradient-editor-load-right-04", &editor->saved_colors[3], TRUE);
  SET_COLOR ("gradient-editor-load-right-05", &editor->saved_colors[4], TRUE);
  SET_COLOR ("gradient-editor-load-right-06", &editor->saved_colors[5], TRUE);
  SET_COLOR ("gradient-editor-load-right-07", &editor->saved_colors[6], TRUE);
  SET_COLOR ("gradient-editor-load-right-08", &editor->saved_colors[7], TRUE);
  SET_COLOR ("gradient-editor-load-right-09", &editor->saved_colors[8], TRUE);
  SET_COLOR ("gradient-editor-load-right-10", &editor->saved_colors[9], TRUE);

  SET_SENSITIVE ("gradient-editor-save-right-01", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-02", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-03", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-04", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-05", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-06", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-07", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-08", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-09", gradient);
  SET_SENSITIVE ("gradient-editor-save-right-10", gradient);

  SET_COLOR ("gradient-editor-save-right-01", &editor->saved_colors[0], TRUE);
  SET_COLOR ("gradient-editor-save-right-02", &editor->saved_colors[1], TRUE);
  SET_COLOR ("gradient-editor-save-right-03", &editor->saved_colors[2], TRUE);
  SET_COLOR ("gradient-editor-save-right-04", &editor->saved_colors[3], TRUE);
  SET_COLOR ("gradient-editor-save-right-05", &editor->saved_colors[4], TRUE);
  SET_COLOR ("gradient-editor-save-right-06", &editor->saved_colors[5], TRUE);
  SET_COLOR ("gradient-editor-save-right-07", &editor->saved_colors[6], TRUE);
  SET_COLOR ("gradient-editor-save-right-08", &editor->saved_colors[7], TRUE);
  SET_COLOR ("gradient-editor-save-right-09", &editor->saved_colors[8], TRUE);
  SET_COLOR ("gradient-editor-save-right-10", &editor->saved_colors[9], TRUE);

  SET_SENSITIVE ("gradient-editor-flip",           editable);
  SET_SENSITIVE ("gradient-editor-replicate",      editable);
  SET_SENSITIVE ("gradient-editor-split-midpoint", editable);
  SET_SENSITIVE ("gradient-editor-split-uniform",  editable);
  SET_SENSITIVE ("gradient-editor-delete",         editable && delete);
  SET_SENSITIVE ("gradient-editor-recenter",       editable);
  SET_SENSITIVE ("gradient-editor-redistribute",   editable);

  if (! selection)
    {
      SET_LABEL ("gradient-editor-blending-func",
                 _("_Blending Function for Segment"));
      SET_LABEL ("gradient-editor-coloring-type",
                 _("Coloring _Type for Segment"));

      SET_LABEL ("gradient-editor-flip",
                 _("_Flip Segment"));
      SET_LABEL ("gradient-editor-replicate",
                 _("_Replicate Segment..."));
      SET_LABEL ("gradient-editor-split-midpoint",
                 _("Split Segment at _Midpoint"));
      SET_LABEL ("gradient-editor-split-uniform",
                 _("Split Segment _Uniformly..."));
      SET_LABEL ("gradient-editor-delete",
                 _("_Delete Segment"));
      SET_LABEL ("gradient-editor-recenter",
                 _("Re-_center Segment's Midpoint"));
      SET_LABEL ("gradient-editor-redistribute",
                 _("Re-distribute _Handles in Segment"));
    }
  else
    {
      SET_LABEL ("gradient-editor-blending-func",
                 _("_Blending Function for Selection"));
      SET_LABEL ("gradient-editor-coloring-type",
                 _("Coloring _Type for Selection"));

      SET_LABEL ("gradient-editor-flip",
                 _("_Flip Selection"));
      SET_LABEL ("gradient-editor-replicate",
                 _("_Replicate Selection..."));
      SET_LABEL ("gradient-editor-split-midpoint",
                 _("Split Segments at _Midpoints"));
      SET_LABEL ("gradient-editor-split-uniform",
                 _("Split Segments _Uniformly..."));
      SET_LABEL ("gradient-editor-delete",
                 _("_Delete Selection"));
      SET_LABEL ("gradient-editor-recenter",
                 _("Re-_center Midpoints in Selection"));
      SET_LABEL ("gradient-editor-redistribute",
                 _("Re-distribute _Handles in Selection"));
    }

  SET_SENSITIVE ("gradient-editor-blending-varies", FALSE);
  SET_VISIBLE   ("gradient-editor-blending-varies", ! blending_equal);

  SET_SENSITIVE ("gradient-editor-blending-linear",            editable);
  SET_SENSITIVE ("gradient-editor-blending-curved",            editable);
  SET_SENSITIVE ("gradient-editor-blending-sine",              editable);
  SET_SENSITIVE ("gradient-editor-blending-sphere-increasing", editable);
  SET_SENSITIVE ("gradient-editor-blending-sphere-decreasing", editable);

  if (blending_equal && gradient)
    {
      switch (editor->control_sel_l->type)
        {
        case GIMP_GRADIENT_SEGMENT_LINEAR:
          SET_ACTIVE ("gradient-editor-blending-linear", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_CURVED:
          SET_ACTIVE ("gradient-editor-blending-curved", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_SINE:
          SET_ACTIVE ("gradient-editor-blending-sine", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_SPHERE_INCREASING:
          SET_ACTIVE ("gradient-editor-blending-sphere-increasing", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_SPHERE_DECREASING:
          SET_ACTIVE ("gradient-editor-blending-sphere-decreasing", TRUE);
          break;
        }
    }
  else
    {
      SET_ACTIVE ("gradient-editor-blending-varies", TRUE);
    }

  SET_SENSITIVE ("gradient-editor-coloring-varies", FALSE);
  SET_VISIBLE   ("gradient-editor-coloring-varies", ! coloring_equal);

  SET_SENSITIVE ("gradient-editor-coloring-rgb",     editable);
  SET_SENSITIVE ("gradient-editor-coloring-hsv-ccw", editable);
  SET_SENSITIVE ("gradient-editor-coloring-hsv-cw",  editable);

  if (coloring_equal && gradient)
    {
      switch (editor->control_sel_l->color)
        {
        case GIMP_GRADIENT_SEGMENT_RGB:
          SET_ACTIVE ("gradient-editor-coloring-rgb", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_HSV_CCW:
          SET_ACTIVE ("gradient-editor-coloring-hsv-ccw", TRUE);
          break;
        case GIMP_GRADIENT_SEGMENT_HSV_CW:
          SET_ACTIVE ("gradient-editor-coloring-hsv-cw", TRUE);
          break;
        }
    }
  else
    {
      SET_ACTIVE ("gradient-editor-coloring-varies", TRUE);
    }

  SET_SENSITIVE ("gradient-editor-blend-color",   editable && selection);
  SET_SENSITIVE ("gradient-editor-blend-opacity", editable && selection);

  SET_SENSITIVE ("gradient-editor-zoom-out", gradient);
  SET_SENSITIVE ("gradient-editor-zoom-in",  gradient);
  SET_SENSITIVE ("gradient-editor-zoom-all", gradient);

#undef SET_ACTIVE
#undef SET_COLOR
#undef SET_LABEL
#undef SET_SENSITIVE
#undef SET_VISIBLE
}