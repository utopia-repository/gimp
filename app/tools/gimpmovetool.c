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

#include "tools-types.h"

#include "config/gimpdisplayconfig.h"
#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-guides.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimplayer-floating-sel.h"
#include "core/gimptoolinfo.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-appearance.h"
#include "display/gimpdisplayshell-draw.h"
#include "display/gimpdisplayshell-transform.h"

#include "widgets/gimphelp-ids.h"

#include "gimpeditselectiontool.h"
#include "gimpmoveoptions.h"
#include "gimpmovetool.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


#define SWAP_ORIENT(orient) ((orient) == GIMP_ORIENTATION_HORIZONTAL ? \
                             GIMP_ORIENTATION_VERTICAL : \
                             GIMP_ORIENTATION_HORIZONTAL)



/*  local function prototypes  */

static void   gimp_move_tool_class_init     (GimpMoveToolClass *klass);
static void   gimp_move_tool_init           (GimpMoveTool      *move_tool);

static void   gimp_move_tool_control        (GimpTool          *tool,
                                             GimpToolAction     action,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_button_press   (GimpTool          *tool,
                                             GimpCoords        *coords,
                                             guint32            time,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_button_release (GimpTool          *tool,
                                             GimpCoords        *coords,
                                             guint32            time,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_motion         (GimpTool          *tool,
                                             GimpCoords        *coords,
                                             guint32            time,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_modifier_key   (GimpTool          *tool,
                                             GdkModifierType    key,
                                             gboolean           press,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_oper_update    (GimpTool          *tool,
                                             GimpCoords        *coords,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);
static void   gimp_move_tool_cursor_update  (GimpTool          *tool,
                                             GimpCoords        *coords,
                                             GdkModifierType    state,
                                             GimpDisplay       *gdisp);

static void   gimp_move_tool_draw           (GimpDrawTool      *draw_tool);

static void   gimp_move_tool_start_guide    (GimpMoveTool      *move,
                                             GimpDisplay       *gdisp,
                                             GimpOrientationType  orientation);


static GimpDrawToolClass *parent_class = NULL;


void
gimp_move_tool_register (GimpToolRegisterCallback  callback,
                         gpointer                  data)
{
  (* callback) (GIMP_TYPE_MOVE_TOOL,
                GIMP_TYPE_MOVE_OPTIONS,
                gimp_move_options_gui,
                0,
                "gimp-move-tool",
                _("Move"),
                _("Move layers & selections"),
                N_("_Move"), "M",
                NULL, GIMP_HELP_TOOL_MOVE,
                GIMP_STOCK_TOOL_MOVE,
                data);
}

GType
gimp_move_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpMoveToolClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_move_tool_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpMoveTool),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_move_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_DRAW_TOOL,
                                          "GimpMoveTool",
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_move_tool_class_init (GimpMoveToolClass *klass)
{
  GimpToolClass     *tool_class      = GIMP_TOOL_CLASS (klass);
  GimpDrawToolClass *draw_tool_class = GIMP_DRAW_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  tool_class->control        = gimp_move_tool_control;
  tool_class->button_press   = gimp_move_tool_button_press;
  tool_class->button_release = gimp_move_tool_button_release;
  tool_class->motion         = gimp_move_tool_motion;
  tool_class->key_press      = gimp_edit_selection_tool_key_press;
  tool_class->modifier_key   = gimp_move_tool_modifier_key;
  tool_class->oper_update    = gimp_move_tool_oper_update;
  tool_class->cursor_update  = gimp_move_tool_cursor_update;

  draw_tool_class->draw      = gimp_move_tool_draw;
}

static void
gimp_move_tool_init (GimpMoveTool *move_tool)
{
  GimpTool *tool = GIMP_TOOL (move_tool);

  move_tool->floating_layer     = NULL;
  move_tool->guide              = NULL;

  move_tool->moving_guide       = FALSE;
  move_tool->guide_position     = -1;
  move_tool->guide_orientation  = GIMP_ORIENTATION_UNKNOWN;

  move_tool->saved_type         = GIMP_TRANSFORM_TYPE_LAYER;

  move_tool->old_active_layer   = NULL;
  move_tool->old_active_vectors = NULL;

  gimp_tool_control_set_snap_to             (tool->control, FALSE);
  gimp_tool_control_set_handles_empty_image (tool->control, TRUE);
  gimp_tool_control_set_tool_cursor         (tool->control,
                                             GIMP_TOOL_CURSOR_MOVE);
}

static void
gimp_move_tool_control (GimpTool       *tool,
                        GimpToolAction  action,
                        GimpDisplay    *gdisp)
{
  GimpMoveTool     *move  = GIMP_MOVE_TOOL (tool);
  GimpDisplayShell *shell = GIMP_DISPLAY_SHELL (gdisp->shell);

  switch (action)
    {
    case PAUSE:
      break;

    case RESUME:
      if (move->guide &&
          gimp_display_shell_get_show_guides (GIMP_DISPLAY_SHELL (shell)))
	gimp_display_shell_draw_guide (GIMP_DISPLAY_SHELL (shell),
                                       move->guide, TRUE);
      break;

    case HALT:
      if (move->guide &&
          gimp_display_shell_get_show_guides (GIMP_DISPLAY_SHELL (shell)))
        gimp_display_shell_draw_guide (GIMP_DISPLAY_SHELL (shell),
                                       move->guide, FALSE);
      break;

    default:
      break;
    }

  GIMP_TOOL_CLASS (parent_class)->control (tool, action, gdisp);
}

static void
gimp_move_tool_button_press (GimpTool        *tool,
                             GimpCoords      *coords,
                             guint32          time,
                             GdkModifierType  state,
                             GimpDisplay     *gdisp)
{
  GimpMoveTool     *move    = GIMP_MOVE_TOOL (tool);
  GimpDisplayShell *shell   = GIMP_DISPLAY_SHELL (gdisp->shell);
  GimpMoveOptions  *options = GIMP_MOVE_OPTIONS (tool->tool_info->tool_options);

  tool->gdisp = gdisp;

  move->floating_layer     = NULL;
  move->guide              = NULL;
  move->moving_guide       = FALSE;
  move->old_active_layer   = NULL;
  move->old_active_vectors = NULL;

  if (! options->move_current)
    {
      if (options->move_type == GIMP_TRANSFORM_TYPE_PATH)
        {
          GimpVectors *vectors;

          if (gimp_draw_tool_on_vectors (GIMP_DRAW_TOOL (tool), gdisp,
                                         coords, 7, 7,
                                         NULL, NULL, NULL, NULL, NULL,
                                         &vectors))
            {
              move->old_active_vectors =
                gimp_image_get_active_vectors (gdisp->gimage);

              gimp_image_set_active_vectors (gdisp->gimage, vectors);
            }
          else
            {
              /*  no path picked  */
              return;
            }
        }
      else if (options->move_type == GIMP_TRANSFORM_TYPE_LAYER)
        {
          GimpGuide *guide;
          GimpLayer *layer;
          gint       snap_distance;

          snap_distance =
            GIMP_DISPLAY_CONFIG (gdisp->gimage->gimp->config)->snap_distance;

          if (gimp_display_shell_get_show_guides (shell) &&
              (guide = gimp_image_find_guide (gdisp->gimage,
                                              coords->x, coords->y,
                                              FUNSCALEX (shell, snap_distance),
                                              FUNSCALEY (shell, snap_distance))))
            {
              move->guide             = guide;
              move->moving_guide      = TRUE;
              move->guide_position    = guide->position;
              move->guide_orientation = guide->orientation;

              gimp_tool_control_set_scroll_lock (tool->control, TRUE);
              gimp_tool_control_activate (tool->control);

              gimp_display_shell_selection_visibility (shell,
                                                       GIMP_SELECTION_PAUSE);

              gimp_draw_tool_start (GIMP_DRAW_TOOL (tool), gdisp);

              gimp_tool_push_status_length (tool, _("Move Guide: "),
                                            SWAP_ORIENT (move->guide_orientation),
                                            move->guide_position);

              return;
            }
          else if ((layer = gimp_image_pick_correlate_layer (gdisp->gimage,
                                                             coords->x,
                                                             coords->y)))
            {
              if (gimp_image_floating_sel (gdisp->gimage) &&
                  ! gimp_layer_is_floating_sel (layer))
                {
                  /*  If there is a floating selection, and this aint it,
                   *  use the move tool to anchor it.
                   */
                  move->floating_layer = gimp_image_floating_sel (gdisp->gimage);
                  gimp_tool_control_activate (tool->control);

                  return;
                }
              else
                {
                  move->old_active_layer =
                    gimp_image_get_active_layer (gdisp->gimage);

                  gimp_image_set_active_layer (gdisp->gimage, layer);
                }
            }
          else
            {
              /*  no guide and no layer picked  */

              return;
            }
        }
    }

  switch (options->move_type)
    {
    case GIMP_TRANSFORM_TYPE_PATH:
      if (gimp_image_get_active_vectors (gdisp->gimage))
        gimp_edit_selection_tool_start (tool, gdisp, coords,
                                        GIMP_TRANSLATE_MODE_VECTORS, TRUE);
      break;

    case GIMP_TRANSFORM_TYPE_SELECTION:
      if (! gimp_channel_is_empty (gimp_image_get_mask (gdisp->gimage)))
        gimp_edit_selection_tool_start (tool, gdisp, coords,
                                        GIMP_TRANSLATE_MODE_MASK, TRUE);
      break;

    case GIMP_TRANSFORM_TYPE_LAYER:
      {
        GimpDrawable *drawable = gimp_image_active_drawable (gdisp->gimage);

        if (GIMP_IS_LAYER_MASK (drawable))
          gimp_edit_selection_tool_start (tool, gdisp, coords,
                                          GIMP_TRANSLATE_MODE_LAYER_MASK, TRUE);
        else if (GIMP_IS_CHANNEL (drawable))
          gimp_edit_selection_tool_start (tool, gdisp, coords,
                                          GIMP_TRANSLATE_MODE_CHANNEL, TRUE);
        else if (GIMP_IS_LAYER (drawable))
          gimp_edit_selection_tool_start (tool, gdisp, coords,
                                          GIMP_TRANSLATE_MODE_LAYER, TRUE);
      }
      break;
    }
}

static void
gimp_move_tool_button_release (GimpTool        *tool,
                               GimpCoords      *coords,
                               guint32          time,
                               GdkModifierType  state,
                               GimpDisplay     *gdisp)
{
  GimpMoveTool     *move   = GIMP_MOVE_TOOL (tool);
  GimpDisplayShell *shell  = GIMP_DISPLAY_SHELL (gdisp->shell);
  GimpGuiConfig    *config = GIMP_GUI_CONFIG (gdisp->gimage->gimp->config);

  if (gimp_tool_control_is_active (tool->control))
    gimp_tool_control_halt (tool->control);

  if (move->moving_guide)
    {
      gboolean delete_guide = FALSE;
      gint     x, y, width, height;

      gimp_tool_pop_status (tool);

      gimp_tool_control_set_scroll_lock (tool->control, FALSE);
      gimp_draw_tool_stop (GIMP_DRAW_TOOL (tool));

      if (state & GDK_BUTTON3_MASK)
        {
          move->moving_guide      = FALSE;
          move->guide_position    = -1;
          move->guide_orientation = GIMP_ORIENTATION_UNKNOWN;

          gimp_display_shell_selection_visibility (shell,
                                                   GIMP_SELECTION_RESUME);
          return;
        }

      gimp_display_shell_untransform_viewport (shell, &x, &y, &width, &height);

      switch (move->guide_orientation)
	{
	case GIMP_ORIENTATION_HORIZONTAL:
	  if ((move->guide_position < y) ||
              (move->guide_position > (y + height)))
	    delete_guide = TRUE;
	  break;

	case GIMP_ORIENTATION_VERTICAL:
	  if ((move->guide_position < x) ||
              (move->guide_position > (x + width)))
	    delete_guide = TRUE;
	  break;

	default:
	  break;
	}

      if (delete_guide)
	{
          if (move->guide)
            {
              gimp_image_remove_guide (gdisp->gimage, move->guide, TRUE);
              move->guide = NULL;
            }
	}
      else
        {
          if (move->guide)
            {
              gimp_image_move_guide (gdisp->gimage, move->guide,
                                     move->guide_position, TRUE);
            }
          else
            {
              switch (move->guide_orientation)
                {
                case GIMP_ORIENTATION_HORIZONTAL:
                  move->guide = gimp_image_add_hguide (gdisp->gimage,
                                                       move->guide_position,
                                                       TRUE);
                  break;

                case GIMP_ORIENTATION_VERTICAL:
                  move->guide = gimp_image_add_vguide (gdisp->gimage,
                                                       move->guide_position,
                                                       TRUE);
                  break;

                default:
                  g_assert_not_reached ();
                }
            }
        }

      gimp_display_shell_selection_visibility (shell, GIMP_SELECTION_RESUME);
      gimp_image_flush (gdisp->gimage);

      if (move->guide)
	gimp_display_shell_draw_guide (shell, move->guide, TRUE);

      move->moving_guide      = FALSE;
      move->guide_position    = -1;
      move->guide_orientation = GIMP_ORIENTATION_UNKNOWN;
    }
  else
    {
      if (! config->move_tool_changes_active || (state & GDK_BUTTON3_MASK))
        {
          if (move->old_active_layer)
            {
              gimp_image_set_active_layer (gdisp->gimage,
                                           move->old_active_layer);
              move->old_active_layer = NULL;
            }

          if (move->old_active_vectors)
            {
              gimp_image_set_active_vectors (gdisp->gimage,
                                             move->old_active_vectors);
              move->old_active_vectors = NULL;
            }
        }

      /*  Take care of the case where the user "cancels" the action  */
      if (! (state & GDK_BUTTON3_MASK))
	{
	  if (move->floating_layer)
	    {
	      floating_sel_anchor (move->floating_layer);
	      gimp_image_flush (gdisp->gimage);
	    }
	}
    }
}

static void
gimp_move_tool_motion (GimpTool        *tool,
                       GimpCoords      *coords,
                       guint32          time,
                       GdkModifierType  state,
                       GimpDisplay     *gdisp)

{
  GimpMoveTool     *move  = GIMP_MOVE_TOOL (tool);
  GimpDisplayShell *shell = GIMP_DISPLAY_SHELL (gdisp->shell);

  if (move->moving_guide)
    {
      gint      tx, ty;
      gboolean  delete_guide = FALSE;

      gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

      gimp_display_shell_transform_xy (shell,
                                       coords->x, coords->y,
                                       &tx, &ty,
                                       FALSE);

      if (tx < 0 || tx >= shell->disp_width ||
          ty < 0 || ty >= shell->disp_height)
	{
	  move->guide_position = -1;

          delete_guide = TRUE;
	}
      else
        {
          gint x, y, width, height;

          if (move->guide_orientation == GIMP_ORIENTATION_HORIZONTAL)
            move->guide_position = RINT (coords->y);
          else
            move->guide_position = RINT (coords->x);

          gimp_display_shell_untransform_viewport (shell, &x, &y,
                                                   &width, &height);

          switch (move->guide_orientation)
            {
            case GIMP_ORIENTATION_HORIZONTAL:
              if ((move->guide_position < y) ||
                  (move->guide_position > (y + height)))
                delete_guide = TRUE;
              break;

            case GIMP_ORIENTATION_VERTICAL:
              if ((move->guide_position < x) ||
                  (move->guide_position > (x + width)))
                delete_guide = TRUE;
              break;

            default:
              break;
            }
        }

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));

      gimp_tool_pop_status (tool);

      if (delete_guide)
        {
          gimp_tool_push_status (tool,
                                 move->guide ?
                                 _("Remove Guide") : _("Cancel Guide"));
        }
      else
        {
          gimp_tool_push_status_length (tool,
                                        move->guide ?
                                        _("Move Guide: ") : _("Add Guide: "),
                                        SWAP_ORIENT (move->guide_orientation),
                                        move->guide_position);
        }
    }
}

static void
gimp_move_tool_modifier_key (GimpTool        *tool,
                             GdkModifierType  key,
                             gboolean         press,
			     GdkModifierType  state,
			     GimpDisplay     *gdisp)
{
  GimpMoveTool    *move    = GIMP_MOVE_TOOL (tool);
  GimpMoveOptions *options = GIMP_MOVE_OPTIONS (tool->tool_info->tool_options);

  if (key == GDK_SHIFT_MASK)
    {
      g_object_set (options, "move-current", ! options->move_current, NULL);
    }
  else if (key == GDK_MOD1_MASK || key == GDK_CONTROL_MASK)
    {
      GimpTransformType button_type;

      button_type = options->move_type;

      if (press)
        {
          if (key == (state & (GDK_MOD1_MASK | GDK_CONTROL_MASK)))
            {
              /*  first modifier pressed  */

              move->saved_type = options->move_type;
            }
        }
      else
        {
          if (! (state & (GDK_MOD1_MASK | GDK_CONTROL_MASK)))
            {
              /*  last modifier released  */

              button_type = move->saved_type;
            }
        }

      if (state & GDK_MOD1_MASK)
        {
          button_type = GIMP_TRANSFORM_TYPE_SELECTION;
        }
      else if (state & GDK_CONTROL_MASK)
        {
          button_type = GIMP_TRANSFORM_TYPE_PATH;
        }

      if (button_type != options->move_type)
        {
          g_object_set (options, "move-type", button_type, NULL);
        }
    }
}

static void
gimp_move_tool_oper_update (GimpTool        *tool,
                            GimpCoords      *coords,
                            GdkModifierType  state,
                            GimpDisplay     *gdisp)
{
  GimpMoveTool     *move    = GIMP_MOVE_TOOL (tool);
  GimpDisplayShell *shell   = GIMP_DISPLAY_SHELL (gdisp->shell);
  GimpMoveOptions  *options = GIMP_MOVE_OPTIONS (tool->tool_info->tool_options);
  GimpGuide        *guide   = NULL;

  if (options->move_type == GIMP_TRANSFORM_TYPE_LAYER &&
      ! options->move_current                         &&
      gimp_display_shell_get_show_guides (shell)      &&
      shell->proximity)
    {
      gint snap_distance;

      snap_distance =
        GIMP_DISPLAY_CONFIG (gdisp->gimage->gimp->config)->snap_distance;

      guide = gimp_image_find_guide (gdisp->gimage, coords->x, coords->y,
                                     FUNSCALEX (shell, snap_distance),
                                     FUNSCALEY (shell, snap_distance));
    }

  if (move->guide && move->guide != guide)
    gimp_display_shell_draw_guide (shell, move->guide, FALSE);

  move->guide = guide;

  if (move->guide)
    gimp_display_shell_draw_guide (shell, move->guide, TRUE);
}

static void
gimp_move_tool_cursor_update (GimpTool        *tool,
                              GimpCoords      *coords,
                              GdkModifierType  state,
                              GimpDisplay     *gdisp)
{
  GimpDisplayShell *shell   = GIMP_DISPLAY_SHELL (gdisp->shell);
  GimpMoveOptions  *options = GIMP_MOVE_OPTIONS (tool->tool_info->tool_options);

  GimpCursorType     cursor      = GIMP_CURSOR_BAD;
  GimpToolCursorType tool_cursor = GIMP_TOOL_CURSOR_MOVE;
  GimpCursorModifier modifier    = GIMP_CURSOR_MODIFIER_NONE;

  if (options->move_type == GIMP_TRANSFORM_TYPE_PATH)
    {
      tool_cursor = GIMP_TOOL_CURSOR_PATHS;
      modifier    = GIMP_CURSOR_MODIFIER_MOVE;

      if (options->move_current)
        {
          if (gimp_image_get_active_vectors (gdisp->gimage))
            cursor = GIMP_CURSOR_MOUSE;
        }
      else
        {
          if (gimp_draw_tool_on_vectors (GIMP_DRAW_TOOL (tool), gdisp,
                                         coords, 7, 7,
                                         NULL, NULL, NULL, NULL, NULL, NULL))
            {
              cursor      = GDK_HAND2;
              tool_cursor = GIMP_TOOL_CURSOR_HAND;
            }
        }
    }
  else if (options->move_type == GIMP_TRANSFORM_TYPE_SELECTION)
    {
      tool_cursor = GIMP_TOOL_CURSOR_RECT_SELECT;
      modifier    = GIMP_CURSOR_MODIFIER_MOVE;

      if (! gimp_channel_is_empty (gimp_image_get_mask (gdisp->gimage)))
        cursor = GIMP_CURSOR_MOUSE;
    }
  else if (options->move_current)
    {
      if (gimp_image_active_drawable (gdisp->gimage))
        cursor = GIMP_CURSOR_MOUSE;
    }
  else
    {
      GimpGuide *guide;
      GimpLayer *layer;
      gint       snap_distance;

      snap_distance =
        GIMP_DISPLAY_CONFIG (gdisp->gimage->gimp->config)->snap_distance;

      if (gimp_display_shell_get_show_guides (shell) &&
          (guide = gimp_image_find_guide (gdisp->gimage, coords->x, coords->y,
                                          FUNSCALEX (shell, snap_distance),
                                          FUNSCALEY (shell, snap_distance))))
        {
          cursor      = GDK_HAND2;
          tool_cursor = GIMP_TOOL_CURSOR_HAND;
          modifier    = GIMP_CURSOR_MODIFIER_MOVE;
	}
      else if ((layer = gimp_image_pick_correlate_layer (gdisp->gimage,
                                                         coords->x, coords->y)))
	{
	  /*  if there is a floating selection, and this aint it...  */
	  if (gimp_image_floating_sel (gdisp->gimage) &&
	      ! gimp_layer_is_floating_sel (layer))
	    {
              cursor      = GIMP_CURSOR_MOUSE;
              tool_cursor = GIMP_TOOL_CURSOR_MOVE;
              modifier    = GIMP_CURSOR_MODIFIER_ANCHOR;
	    }
	  else if (layer == gimp_image_get_active_layer (gdisp->gimage))
	    {
              cursor = GIMP_CURSOR_MOUSE;
	    }
	  else
	    {
              cursor      = GDK_HAND2;
              tool_cursor = GIMP_TOOL_CURSOR_HAND;
              modifier    = GIMP_CURSOR_MODIFIER_MOVE;
	    }
	}
    }

  gimp_tool_control_set_cursor          (tool->control, cursor);
  gimp_tool_control_set_tool_cursor     (tool->control, tool_cursor);
  gimp_tool_control_set_cursor_modifier (tool->control, modifier);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static void
gimp_move_tool_draw (GimpDrawTool *draw_tool)
{
  GimpMoveTool *move = GIMP_MOVE_TOOL (draw_tool);

  if (move->moving_guide && move->guide_position != -1)
    {
      switch (move->guide_orientation)
        {
        case GIMP_ORIENTATION_HORIZONTAL:
          gimp_draw_tool_draw_line (draw_tool,
                                    0, move->guide_position,
                                    draw_tool->gdisp->gimage->width,
                                    move->guide_position,
                                    FALSE);
          break;

        case GIMP_ORIENTATION_VERTICAL:
          gimp_draw_tool_draw_line (draw_tool,
                                    move->guide_position, 0,
                                    move->guide_position,
                                    draw_tool->gdisp->gimage->height,
                                    FALSE);
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

void
gimp_move_tool_start_hguide (GimpTool    *tool,
			     GimpDisplay *gdisp)
{
  g_return_if_fail (GIMP_IS_MOVE_TOOL (tool));
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  gimp_move_tool_start_guide (GIMP_MOVE_TOOL (tool), gdisp,
                              GIMP_ORIENTATION_HORIZONTAL);
}

void
gimp_move_tool_start_vguide (GimpTool    *tool,
			     GimpDisplay *gdisp)
{
  g_return_if_fail (GIMP_IS_MOVE_TOOL (tool));
  g_return_if_fail (GIMP_IS_DISPLAY (gdisp));

  gimp_move_tool_start_guide (GIMP_MOVE_TOOL (tool), gdisp,
                              GIMP_ORIENTATION_VERTICAL);
}

static void
gimp_move_tool_start_guide (GimpMoveTool        *move,
                            GimpDisplay         *gdisp,
                            GimpOrientationType  orientation)
{
  GimpTool *tool = GIMP_TOOL (move);

  gimp_display_shell_selection_visibility (GIMP_DISPLAY_SHELL (gdisp->shell),
                                           GIMP_SELECTION_PAUSE);

  tool->gdisp = gdisp;
  gimp_tool_control_activate (tool->control);
  gimp_tool_control_set_scroll_lock (tool->control, TRUE);

  if (move->guide)
    gimp_display_shell_draw_guide (GIMP_DISPLAY_SHELL (gdisp->shell),
                                   move->guide, FALSE);

  move->guide             = NULL;
  move->moving_guide      = TRUE;
  move->guide_position    = -1;
  move->guide_orientation = orientation;

  gimp_tool_set_cursor (tool, gdisp,
                        GDK_HAND2,
                        GIMP_TOOL_CURSOR_HAND,
                        GIMP_CURSOR_MODIFIER_MOVE);

  gimp_draw_tool_start (GIMP_DRAW_TOOL (move), gdisp);
}