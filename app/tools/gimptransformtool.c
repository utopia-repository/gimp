/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2001 Spencer Kimball, Peter Mattis, and others
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

#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/tile-manager.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable-transform.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpitem-linked.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimppickable.h"
#include "core/gimpprogress.h"
#include "core/gimptoolinfo.h"

#include "vectors/gimpvectors.h"
#include "vectors/gimpstroke.h"

#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpviewabledialog.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-appearance.h"
#include "display/gimpdisplayshell-transform.h"

#ifdef __GNUC__
#warning FIXME #include "dialogs/dialogs-types.h"
#endif
#include "dialogs/dialogs-types.h"
#include "dialogs/info-dialog.h"

#include "gimptoolcontrol.h"
#include "gimptransformoptions.h"
#include "gimptransformtool.h"
#include "gimptransformtool-undo.h"

#include "gimp-intl.h"


#define HANDLE_SIZE 10


/*  local function prototypes  */

static GObject * gimp_transform_tool_constructor   (GType              type,
                                                    guint              n_params,
                                                    GObjectConstructParam *params);
static void     gimp_transform_tool_finalize       (GObject           *object);

static gboolean gimp_transform_tool_initialize     (GimpTool          *tool,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_control        (GimpTool          *tool,
                                                    GimpToolAction     action,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_button_press   (GimpTool          *tool,
                                                    GimpCoords        *coords,
                                                    guint32            time,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_button_release (GimpTool          *tool,
                                                    GimpCoords        *coords,
                                                    guint32            time,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_motion         (GimpTool          *tool,
                                                    GimpCoords        *coords,
                                                    guint32            time,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);
static gboolean gimp_transform_tool_key_press      (GimpTool          *tool,
                                                    GdkEventKey       *kevent,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_modifier_key   (GimpTool          *tool,
                                                    GdkModifierType    key,
                                                    gboolean           press,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_oper_update    (GimpTool          *tool,
                                                    GimpCoords        *coords,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_cursor_update  (GimpTool          *tool,
                                                    GimpCoords        *coords,
                                                    GdkModifierType    state,
                                                    GimpDisplay       *gdisp);

static void     gimp_transform_tool_draw           (GimpDrawTool      *draw_tool);

static TileManager *
                gimp_transform_tool_real_transform (GimpTransformTool *tr_tool,
                                                    GimpItem          *item,
                                                    gboolean           mask_empty,
                                                    GimpDisplay       *gdisp);

static void     gimp_transform_tool_halt           (GimpTransformTool *tr_tool);
static void     gimp_transform_tool_bounds         (GimpTransformTool *tr_tool,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_dialog         (GimpTransformTool *tr_tool);
static void     gimp_transform_tool_prepare        (GimpTransformTool *tr_tool,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_doit           (GimpTransformTool *tr_tool,
                                                    GimpDisplay       *gdisp);
static void     gimp_transform_tool_transform_bounding_box (GimpTransformTool *tr_tool);
static void     gimp_transform_tool_grid_recalc    (GimpTransformTool *tr_tool);

static void     gimp_transform_tool_force_expose_preview (GimpTransformTool *tr_tool);

static void     gimp_transform_tool_response       (GtkWidget         *widget,
                                                    gint               response_id,
                                                    GimpTransformTool *tr_tool);

static void     gimp_transform_tool_notify_type    (GimpTransformOptions *options,
                                                    GParamSpec           *pspec,
                                                    GimpTransformTool    *tr_tool);
static void     gimp_transform_tool_notify_preview (GimpTransformOptions *options,
                                                    GParamSpec           *pspec,
                                                    GimpTransformTool    *tr_tool);


G_DEFINE_TYPE (GimpTransformTool, gimp_transform_tool, GIMP_TYPE_DRAW_TOOL);

#define parent_class gimp_transform_tool_parent_class


static void
gimp_transform_tool_class_init (GimpTransformToolClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GimpToolClass     *tool_class   = GIMP_TOOL_CLASS (klass);
  GimpDrawToolClass *draw_class   = GIMP_DRAW_TOOL_CLASS (klass);

  object_class->constructor  = gimp_transform_tool_constructor;
  object_class->finalize     = gimp_transform_tool_finalize;

  tool_class->initialize     = gimp_transform_tool_initialize;
  tool_class->control        = gimp_transform_tool_control;
  tool_class->button_press   = gimp_transform_tool_button_press;
  tool_class->button_release = gimp_transform_tool_button_release;
  tool_class->motion         = gimp_transform_tool_motion;
  tool_class->key_press      = gimp_transform_tool_key_press;
  tool_class->modifier_key   = gimp_transform_tool_modifier_key;
  tool_class->oper_update    = gimp_transform_tool_oper_update;
  tool_class->cursor_update  = gimp_transform_tool_cursor_update;

  draw_class->draw           = gimp_transform_tool_draw;

  klass->dialog              = NULL;
  klass->dialog_update       = NULL;
  klass->prepare             = NULL;
  klass->motion              = NULL;
  klass->recalc              = NULL;
  klass->transform           = gimp_transform_tool_real_transform;
}

static void
gimp_transform_tool_init (GimpTransformTool *tr_tool)
{
  GimpTool *tool;
  gint      i;

  tool = GIMP_TOOL (tr_tool);

  gimp_tool_control_set_scroll_lock (tool->control, TRUE);
  gimp_tool_control_set_preserve    (tool->control, FALSE);
  gimp_tool_control_set_dirty_mask  (tool->control,
                                     GIMP_DIRTY_IMAGE_SIZE |
                                     GIMP_DIRTY_DRAWABLE   |
                                     GIMP_DIRTY_SELECTION);

  tr_tool->function = TRANSFORM_CREATING;
  tr_tool->original = NULL;

  for (i = 0; i < TRAN_INFO_SIZE; i++)
    {
      tr_tool->trans_info[i]     = 0.0;
      tr_tool->old_trans_info[i] = 0.0;
    }

  gimp_matrix3_identity (&tr_tool->transform);

  tr_tool->use_grid         = TRUE;
  tr_tool->use_center       = TRUE;
  tr_tool->ngx              = 0;
  tr_tool->ngy              = 0;
  tr_tool->grid_coords      = NULL;
  tr_tool->tgrid_coords     = NULL;

  tr_tool->type             = GIMP_TRANSFORM_TYPE_LAYER;
  tr_tool->direction        = GIMP_TRANSFORM_FORWARD;

  tr_tool->shell_desc       = NULL;
  tr_tool->progress_text    = _("Transforming");
  tr_tool->info_dialog      = NULL;
}

static GObject *
gimp_transform_tool_constructor (GType                  type,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObject           *object;
  GimpTool          *tool;
  GimpTransformTool *tr_tool;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  tool    = GIMP_TOOL (object);
  tr_tool = GIMP_TRANSFORM_TOOL (object);

  g_assert (GIMP_IS_TOOL_INFO (tool->tool_info));

  if (tr_tool->use_grid)
    {
      tr_tool->type =
        GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options)->type;
      tr_tool->direction =
        GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options)->direction;

      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::type",
                               G_CALLBACK (gimp_transform_tool_notify_type),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::type",
                               G_CALLBACK (gimp_transform_tool_notify_preview),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::direction",
                               G_CALLBACK (gimp_transform_tool_notify_type),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::direction",
                               G_CALLBACK (gimp_transform_tool_notify_preview),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::preview-type",
                               G_CALLBACK (gimp_transform_tool_notify_preview),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::grid-type",
                               G_CALLBACK (gimp_transform_tool_notify_preview),
                               tr_tool, 0);
      g_signal_connect_object (tool->tool_info->tool_options,
                               "notify::grid-size",
                               G_CALLBACK (gimp_transform_tool_notify_preview),
                               tr_tool, 0);
    }

  return object;
}

static void
gimp_transform_tool_finalize (GObject *object)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (object);

  if (tr_tool->original)
    {
      tile_manager_unref (tr_tool->original);
      tr_tool->original = NULL;
    }

  if (tr_tool->info_dialog)
    {
      info_dialog_free (tr_tool->info_dialog);
      tr_tool->info_dialog = NULL;
    }

  if (tr_tool->grid_coords)
    {
      g_free (tr_tool->grid_coords);
      tr_tool->grid_coords = NULL;
    }

  if (tr_tool->tgrid_coords)
    {
      g_free (tr_tool->tgrid_coords);
      tr_tool->tgrid_coords = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gimp_transform_tool_initialize (GimpTool    *tool,
                                GimpDisplay *gdisp)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (tool);

  if (gdisp != tool->gdisp)
    {
      gint i;

      /*  Set the pointer to the active display  */
      tool->gdisp    = gdisp;
      tool->drawable = gimp_image_active_drawable (gdisp->gimage);

      /*  Initialize the transform tool dialog */
      if (! tr_tool->info_dialog)
        gimp_transform_tool_dialog (tr_tool);

      /*  Find the transform bounds for some tools (like scale,
       *  perspective) that actually need the bounds for
       *  initializing
       */
      gimp_transform_tool_bounds (tr_tool, gdisp);

      gimp_transform_tool_prepare (tr_tool, gdisp);

      /*  Recalculate the transform tool  */
      gimp_transform_tool_recalc (tr_tool, gdisp);

      /*  start drawing the bounding box and handles...  */
      gimp_draw_tool_start (GIMP_DRAW_TOOL (tool), gdisp);

      tr_tool->function = TRANSFORM_CREATING;

      /*  Save the current transformation info  */
      for (i = 0; i < TRAN_INFO_SIZE; i++)
        tr_tool->old_trans_info[i] = tr_tool->trans_info[i];
    }

  return TRUE;
}

static void
gimp_transform_tool_control (GimpTool       *tool,
                             GimpToolAction  action,
                             GimpDisplay    *gdisp)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (tool);

  switch (action)
    {
    case PAUSE:
      break;

    case RESUME:
      gimp_transform_tool_bounds (tr_tool, gdisp);
      gimp_transform_tool_recalc (tr_tool, gdisp);
      break;

    case HALT:
      gimp_transform_tool_halt (tr_tool);
      return; /* don't upchain */
      break;

    default:
      break;
    }

  GIMP_TOOL_CLASS (parent_class)->control (tool, action, gdisp);
}

static void
gimp_transform_tool_button_press (GimpTool        *tool,
                                  GimpCoords      *coords,
                                  guint32          time,
                                  GdkModifierType  state,
                                  GimpDisplay     *gdisp)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (tool);

  if (tr_tool->function == TRANSFORM_CREATING && tr_tool->use_grid)
    gimp_transform_tool_oper_update (tool, coords, state, gdisp);

  tr_tool->lastx = tr_tool->startx = coords->x;
  tr_tool->lasty = tr_tool->starty = coords->y;

  gimp_tool_control_activate (tool->control);
}

static void
gimp_transform_tool_button_release (GimpTool        *tool,
                                    GimpCoords      *coords,
                                    guint32          time,
                                    GdkModifierType  state,
                                    GimpDisplay     *gdisp)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (tool);
  gint               i;

  /*  if we are creating, there is nothing to be done...exit  */
  if (tr_tool->function == TRANSFORM_CREATING && tr_tool->use_grid)
    return;

  /*  if the 3rd button isn't pressed, transform the selected mask  */
  if (! (state & GDK_BUTTON3_MASK))
    {
      /* Shift-clicking is another way to approve the transform  */
      if ((state & GDK_SHIFT_MASK) || ! tr_tool->use_grid)
        {
          gimp_transform_tool_doit (tr_tool, gdisp);
        }
    }
  else
    {
      gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

      /* get rid of preview artifacts left outside the drawable's area */
      gimp_transform_tool_expose_preview (tr_tool);

      /*  Restore the previous transformation info  */
      for (i = 0; i < TRAN_INFO_SIZE; i++)
        tr_tool->trans_info[i] = tr_tool->old_trans_info[i];

      /*  reget the selection bounds  */
      gimp_transform_tool_bounds (tr_tool, gdisp);

      /*  recalculate the tool's transformation matrix  */
      gimp_transform_tool_recalc (tr_tool, gdisp);

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
    }

  gimp_tool_control_halt (tool->control);
}

static void
gimp_transform_tool_motion (GimpTool        *tool,
                            GimpCoords      *coords,
                            guint32          time,
                            GdkModifierType  state,
                            GimpDisplay     *gdisp)
{
  GimpTransformTool      *tr_tool = GIMP_TRANSFORM_TOOL (tool);
  GimpTransformToolClass *tr_tool_class;

  /*  if we are creating, there is nothing to be done so exit.  */
  if (tr_tool->function == TRANSFORM_CREATING || ! tr_tool->use_grid)
    return;

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

  tr_tool->curx  = coords->x;
  tr_tool->cury  = coords->y;
  tr_tool->state = state;

  /*  recalculate the tool's transformation matrix  */
  tr_tool_class = GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool);

  if (tr_tool_class->motion)
    {
      tr_tool_class->motion (tr_tool, gdisp);

      gimp_transform_tool_expose_preview (tr_tool);

      gimp_transform_tool_recalc (tr_tool, gdisp);
    }

  tr_tool->lastx = tr_tool->curx;
  tr_tool->lasty = tr_tool->cury;

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

#define RESPONSE_RESET 1

static gboolean
gimp_transform_tool_key_press (GimpTool    *tool,
                               GdkEventKey *kevent,
                               GimpDisplay *gdisp)
{
  GimpTransformTool *trans_tool = GIMP_TRANSFORM_TOOL (tool);
  GimpDrawTool      *draw_tool  = GIMP_DRAW_TOOL (tool);

  if (gdisp == draw_tool->gdisp)
    {
      switch (kevent->keyval)
        {
        case GDK_KP_Enter:
        case GDK_Return:
          gimp_transform_tool_response (NULL, GTK_RESPONSE_OK, trans_tool);
          return TRUE;

        case GDK_Delete:
        case GDK_BackSpace:
          gimp_transform_tool_response (NULL, RESPONSE_RESET, trans_tool);
          return TRUE;

        case GDK_Escape:
          gimp_transform_tool_response (NULL, GTK_RESPONSE_CANCEL, trans_tool);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gimp_transform_tool_modifier_key (GimpTool        *tool,
                                  GdkModifierType  key,
                                  gboolean         press,
                                  GdkModifierType  state,
                                  GimpDisplay     *gdisp)
{
  GimpTransformOptions *options;

  options = GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options);

  if (key == GDK_CONTROL_MASK)
    {
      g_object_set (options,
                    "constrain-1", ! options->constrain_1,
                    NULL);
    }
  else if (key == GDK_MOD1_MASK)
    {
      g_object_set (options,
                    "constrain-2", ! options->constrain_2,
                    NULL);
    }
}

static void
gimp_transform_tool_oper_update (GimpTool        *tool,
                                 GimpCoords      *coords,
                                 GdkModifierType  state,
                                 GimpDisplay     *gdisp)
{
  GimpTransformTool *tr_tool   = GIMP_TRANSFORM_TOOL (tool);
  GimpDrawTool      *draw_tool = GIMP_DRAW_TOOL (tool);

  if (! tr_tool->use_grid)
    return;

  if (gdisp == tool->gdisp)
    {
      gdouble closest_dist;
      gdouble dist;

      closest_dist = gimp_draw_tool_calc_distance (draw_tool, gdisp,
                                                   coords->x, coords->y,
                                                   tr_tool->tx1, tr_tool->ty1);
      tr_tool->function = TRANSFORM_HANDLE_1;

      dist = gimp_draw_tool_calc_distance (draw_tool, gdisp,
                                           coords->x, coords->y,
                                           tr_tool->tx2, tr_tool->ty2);
      if (dist < closest_dist)
        {
          closest_dist = dist;
          tr_tool->function = TRANSFORM_HANDLE_2;
        }

      dist = gimp_draw_tool_calc_distance (draw_tool, gdisp,
                                           coords->x, coords->y,
                                           tr_tool->tx3, tr_tool->ty3);
      if (dist < closest_dist)
        {
          closest_dist = dist;
          tr_tool->function = TRANSFORM_HANDLE_3;
        }

      dist = gimp_draw_tool_calc_distance (draw_tool, gdisp,
                                           coords->x, coords->y,
                                           tr_tool->tx4, tr_tool->ty4);
      if (dist < closest_dist)
        {
          closest_dist = dist;
          tr_tool->function = TRANSFORM_HANDLE_4;
        }

      if (gimp_draw_tool_on_handle (draw_tool, gdisp,
                                    coords->x, coords->y,
                                    GIMP_HANDLE_CIRCLE,
                                    tr_tool->tcx, tr_tool->tcy,
                                    HANDLE_SIZE, HANDLE_SIZE,
                                    GTK_ANCHOR_CENTER,
                                    FALSE))
        {
          tr_tool->function = TRANSFORM_HANDLE_CENTER;
        }
    }
}

static void
gimp_transform_tool_cursor_update (GimpTool        *tool,
                                   GimpCoords      *coords,
                                   GdkModifierType  state,
                                   GimpDisplay     *gdisp)
{
  GimpTransformTool    *tr_tool = GIMP_TRANSFORM_TOOL (tool);
  GimpTransformOptions *options;

  options = GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options);

  if (tr_tool->use_grid)
    {
      GimpChannel        *selection = gimp_image_get_mask (gdisp->gimage);
      GimpCursorType      cursor    = GIMP_CURSOR_MOUSE;
      GimpCursorModifier  modifier  = GIMP_CURSOR_MODIFIER_NONE;

      switch (options->type)
        {
        case GIMP_TRANSFORM_TYPE_LAYER:
          if (gimp_image_coords_in_active_drawable (gdisp->gimage, coords))
            {
              if (gimp_channel_is_empty (selection) ||
                  gimp_pickable_get_opacity_at (GIMP_PICKABLE (selection),
                                                coords->x, coords->y))
                {
                  cursor = GIMP_CURSOR_MOUSE;
                }
            }
          break;

        case GIMP_TRANSFORM_TYPE_SELECTION:
          if (gimp_channel_is_empty (selection) ||
              gimp_pickable_get_opacity_at (GIMP_PICKABLE (selection),
                                            coords->x, coords->y))
            {
              cursor = GIMP_CURSOR_MOUSE;
            }
          break;

        case GIMP_TRANSFORM_TYPE_PATH:
          if (gimp_image_get_active_vectors (gdisp->gimage))
            cursor = GIMP_CURSOR_MOUSE;
          else
            cursor = GIMP_CURSOR_BAD;
          break;
        }

      if (tr_tool->use_center && tr_tool->function == TRANSFORM_HANDLE_CENTER)
        {
          modifier = GIMP_CURSOR_MODIFIER_MOVE;
        }

      gimp_tool_control_set_cursor          (tool->control, cursor);
      gimp_tool_control_set_cursor_modifier (tool->control, modifier);
    }

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static void
gimp_transform_tool_draw (GimpDrawTool *draw_tool)
{
  GimpTool             *tool    = GIMP_TOOL (draw_tool);
  GimpTransformTool    *tr_tool = GIMP_TRANSFORM_TOOL (draw_tool);
  GimpTransformOptions *options;
  gdouble               z1, z2, z3, z4;

  if (! tr_tool->use_grid)
    return;

  options = GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options);

  /*  draw the bounding box  */
  gimp_draw_tool_draw_line (draw_tool,
                            tr_tool->tx1, tr_tool->ty1,
                            tr_tool->tx2, tr_tool->ty2,
                            FALSE);
  gimp_draw_tool_draw_line (draw_tool,
                            tr_tool->tx2, tr_tool->ty2,
                            tr_tool->tx4, tr_tool->ty4,
                            FALSE);
  gimp_draw_tool_draw_line (draw_tool,
                            tr_tool->tx3, tr_tool->ty3,
                            tr_tool->tx4, tr_tool->ty4,
                            FALSE);
  gimp_draw_tool_draw_line (draw_tool,
                            tr_tool->tx3, tr_tool->ty3,
                            tr_tool->tx1, tr_tool->ty1,
                            FALSE);

  /* We test if the transformed polygon is convex.
   * if z1 and z2 have the same sign as well as z3 and z4
   * the polygon is convex.
   */
  z1 = ((tr_tool->tx2 - tr_tool->tx1) * (tr_tool->ty4 - tr_tool->ty1) -
        (tr_tool->tx4 - tr_tool->tx1) * (tr_tool->ty2 - tr_tool->ty1));
  z2 = ((tr_tool->tx4 - tr_tool->tx1) * (tr_tool->ty3 - tr_tool->ty1) -
        (tr_tool->tx3 - tr_tool->tx1) * (tr_tool->ty4 - tr_tool->ty1));
  z3 = ((tr_tool->tx4 - tr_tool->tx2) * (tr_tool->ty3 - tr_tool->ty2) -
        (tr_tool->tx3 - tr_tool->tx2) * (tr_tool->ty4 - tr_tool->ty2));
  z4 = ((tr_tool->tx3 - tr_tool->tx2) * (tr_tool->ty1 - tr_tool->ty2) -
        (tr_tool->tx1 - tr_tool->tx2) * (tr_tool->ty3 - tr_tool->ty2));

  /*  Draw the grid (not for path transform since it looks ugly)  */

  if (tr_tool->type != GIMP_TRANSFORM_TYPE_PATH &&
      tr_tool->grid_coords                      &&
      tr_tool->tgrid_coords                     &&
      z1 * z2 > 0                               &&
      z3 * z4 > 0)
    {
      gint gci, i, k;

      k = tr_tool->ngx + tr_tool->ngy;

      for (i = 0, gci = 0; i < k; i++, gci += 4)
        {
          gimp_draw_tool_draw_line (draw_tool,
                                    tr_tool->tgrid_coords[gci],
                                    tr_tool->tgrid_coords[gci + 1],
                                    tr_tool->tgrid_coords[gci + 2],
                                    tr_tool->tgrid_coords[gci + 3],
                                    FALSE);
        }
    }

  /*  draw the tool handles  */
  gimp_draw_tool_draw_handle (draw_tool,
                              GIMP_HANDLE_SQUARE,
                              tr_tool->tx1, tr_tool->ty1,
                              HANDLE_SIZE, HANDLE_SIZE,
                              GTK_ANCHOR_CENTER,
                              FALSE);
  gimp_draw_tool_draw_handle (draw_tool,
                              GIMP_HANDLE_SQUARE,
                              tr_tool->tx2, tr_tool->ty2,
                              HANDLE_SIZE, HANDLE_SIZE,
                              GTK_ANCHOR_CENTER,
                              FALSE);
  gimp_draw_tool_draw_handle (draw_tool,
                              GIMP_HANDLE_SQUARE,
                              tr_tool->tx3, tr_tool->ty3,
                              HANDLE_SIZE, HANDLE_SIZE,
                              GTK_ANCHOR_CENTER,
                              FALSE);
  gimp_draw_tool_draw_handle (draw_tool,
                              GIMP_HANDLE_SQUARE,
                              tr_tool->tx4, tr_tool->ty4,
                              HANDLE_SIZE, HANDLE_SIZE,
                              GTK_ANCHOR_CENTER,
                              FALSE);

  /*  draw the center  */
  if (tr_tool->use_center)
    {
      gimp_draw_tool_draw_handle (draw_tool,
                                  GIMP_HANDLE_FILLED_CIRCLE,
                                  tr_tool->tcx, tr_tool->tcy,
                                  HANDLE_SIZE, HANDLE_SIZE,
                                  GTK_ANCHOR_CENTER,
                                  FALSE);
    }

  if (tr_tool->type == GIMP_TRANSFORM_TYPE_PATH)
    {
      GimpVectors *vectors;
      GimpStroke  *stroke = NULL;
      GimpMatrix3  matrix = tr_tool->transform;

      vectors = gimp_image_get_active_vectors (tool->gdisp->gimage);

      if (vectors)
        {
          if (tr_tool->direction == GIMP_TRANSFORM_BACKWARD)
            gimp_matrix3_invert (&matrix);

          while ((stroke = gimp_vectors_stroke_get_next (vectors, stroke)))
            {
              GArray   *coords;
              gboolean  closed;

              coords = gimp_stroke_interpolate (stroke, 1.0, &closed);

              if (coords && coords->len)
                {
                  gint i;

                  for (i = 0; i < coords->len; i++)
                    {
                      GimpCoords *curr = &g_array_index (coords, GimpCoords, i);

                      gimp_matrix3_transform_point (&matrix,
                                                    curr->x, curr->y,
                                                    &curr->x, &curr->y);
                    }

                  gimp_draw_tool_draw_strokes (draw_tool,
                                               &g_array_index (coords,
                                                               GimpCoords, 0),
                                               coords->len, FALSE, FALSE);
                }

              if (coords)
                g_array_free (coords, TRUE);
            }
        }
    }
}

static TileManager *
gimp_transform_tool_real_transform (GimpTransformTool *tr_tool,
                                    GimpItem          *active_item,
                                    gboolean           mask_empty,
                                    GimpDisplay       *gdisp)
{
  GimpTool             *tool = GIMP_TOOL (tr_tool);
  GimpTransformOptions *options;
  GimpContext          *context;
  GimpProgress         *progress;
  TileManager          *ret  = NULL;

  options = GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options);
  context = GIMP_CONTEXT (options);

  if (tr_tool->info_dialog)
    gtk_widget_set_sensitive (GTK_WIDGET (tr_tool->info_dialog->shell), FALSE);

  progress = gimp_progress_start (GIMP_PROGRESS (gdisp),
                                  tr_tool->progress_text, FALSE);

  if (gimp_item_get_linked (active_item))
    gimp_item_linked_transform (active_item, context,
                                &tr_tool->transform,
                                options->direction,
                                options->interpolation,
                                options->supersample,
                                options->recursion_level,
                                options->clip,
                                progress);

  if (GIMP_IS_LAYER (active_item) &&
      gimp_layer_get_mask (GIMP_LAYER (active_item)) &&
      mask_empty)
    {
      GimpLayerMask *mask = gimp_layer_get_mask (GIMP_LAYER (active_item));

      gimp_item_transform (GIMP_ITEM (mask), context,
                           &tr_tool->transform,
                           options->direction,
                           options->interpolation,
                           options->supersample,
                           options->recursion_level,
                           options->clip,
                           progress);
    }

  switch (options->type)
    {
    case GIMP_TRANSFORM_TYPE_LAYER:
    case GIMP_TRANSFORM_TYPE_SELECTION:
      {
        gboolean clip_result = options->clip;

        /*  always clip the selction and unfloated channels
         *  so they keep their size
         */
        if (tr_tool->original)
          {
            if (GIMP_IS_CHANNEL (active_item) &&
                tile_manager_bpp (tr_tool->original) == 1)
              clip_result = TRUE;

            ret =
              gimp_drawable_transform_tiles_affine (GIMP_DRAWABLE (active_item),
                                                    context,
                                                    tr_tool->original,
                                                    &tr_tool->transform,
                                                    options->direction,
                                                    options->interpolation,
                                                    options->supersample,
                                                    options->recursion_level,
                                                    clip_result,
                                                    progress);
          }
      }
      break;

    case GIMP_TRANSFORM_TYPE_PATH:
      gimp_item_transform (active_item, context,
                           &tr_tool->transform,
                           options->direction,
                           options->interpolation,
                           options->supersample,
                           options->recursion_level,
                           options->clip,
                           progress);
      break;
    }

  if (progress)
    gimp_progress_end (progress);

  return ret;
}

static void
gimp_transform_tool_doit (GimpTransformTool *tr_tool,
                          GimpDisplay       *gdisp)
{
  GimpTool             *tool        = GIMP_TOOL (tr_tool);
  GimpTransformOptions *options;
  GimpContext          *context;
  GimpItem             *active_item = NULL;
  TileManager          *new_tiles;
  gboolean              new_layer;
  gboolean              mask_empty;

  options = GIMP_TRANSFORM_OPTIONS (tool->tool_info->tool_options);
  context = GIMP_CONTEXT (options);

  switch (options->type)
    {
    case GIMP_TRANSFORM_TYPE_LAYER:
      active_item = (GimpItem *) gimp_image_active_drawable (gdisp->gimage);
      break;

    case GIMP_TRANSFORM_TYPE_SELECTION:
      active_item = (GimpItem *) gimp_image_get_mask (gdisp->gimage);
      break;

    case GIMP_TRANSFORM_TYPE_PATH:
      active_item = (GimpItem *) gimp_image_get_active_vectors (gdisp->gimage);
      break;
    }

  if (! active_item)
    return;

  mask_empty = gimp_channel_is_empty (gimp_image_get_mask (gdisp->gimage));

  if (gimp_display_shell_get_show_transform (GIMP_DISPLAY_SHELL (gdisp->shell)))
    {
      gimp_display_shell_set_show_transform (GIMP_DISPLAY_SHELL (gdisp->shell),
                                             FALSE);

      /* get rid of preview artifacts left outside the drawable's area */
      gimp_transform_tool_expose_preview (tr_tool);
    }

  gimp_set_busy (gdisp->gimage->gimp);

  /* undraw the tool before we muck around with the transform matrix */
  gimp_draw_tool_stop (GIMP_DRAW_TOOL (tr_tool));

  /*  We're going to dirty this image, but we want to keep the tool around  */
  gimp_tool_control_set_preserve (tool->control, TRUE);

  /*  Start a transform undo group  */
  gimp_image_undo_group_start (gdisp->gimage, GIMP_UNDO_GROUP_TRANSFORM,
                               tool->tool_info->blurb);

  /* With the old UI, if original is NULL, then this is the
   * first transformation. In the new UI, it is always so, right?
   */
  g_assert (tr_tool->original == NULL);

  /*  Copy the current selection to the transform tool's private
   *  selection pointer, so that the original source can be repeatedly
   *  modified.
   */
  tool->drawable = gimp_image_active_drawable (gdisp->gimage);

  switch (options->type)
    {
    case GIMP_TRANSFORM_TYPE_LAYER:
      tr_tool->original = gimp_drawable_transform_cut (tool->drawable,
                                                       context,
                                                       &new_layer);
      break;

    case GIMP_TRANSFORM_TYPE_SELECTION:
      tr_tool->original = tile_manager_ref (GIMP_DRAWABLE (active_item)->tiles);
      tile_manager_set_offsets (tr_tool->original, 0, 0);
      break;

    case GIMP_TRANSFORM_TYPE_PATH:
      tr_tool->original = NULL;
      break;
    }

  /*  Send the request for the transformation to the tool...
   */
  new_tiles = GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->transform (tr_tool,
                                                                  active_item,
                                                                  mask_empty,
                                                                  gdisp);

  gimp_transform_tool_prepare (tr_tool, gdisp);
  gimp_transform_tool_bounds (tr_tool, gdisp);
  gimp_transform_tool_recalc (tr_tool, gdisp);

  switch (options->type)
    {
    case GIMP_TRANSFORM_TYPE_LAYER:
      if (new_tiles)
        {
          /*  paste the new transformed image to the gimage...also implement
           *  undo...
           */
          gimp_drawable_transform_paste (tool->drawable,
                                         new_tiles,
                                         new_layer);
          tile_manager_unref (new_tiles);
        }
      break;

     case GIMP_TRANSFORM_TYPE_SELECTION:
      if (new_tiles)
        {
          gimp_channel_push_undo (GIMP_CHANNEL (active_item), NULL);

          gimp_drawable_set_tiles (GIMP_DRAWABLE (active_item),
                                   FALSE, NULL, new_tiles);
          tile_manager_unref (new_tiles);
        }

      tile_manager_unref (tr_tool->original);
      tr_tool->original = NULL;
      break;

    case GIMP_TRANSFORM_TYPE_PATH:
      /*  Nothing to be done  */
      break;
    }

  /*  Make a note of the new current drawable (since we may have
   *  a floating selection, etc now.
   */
  tool->drawable = gimp_image_active_drawable (gdisp->gimage);

  gimp_transform_tool_push_undo (gdisp->gimage, NULL,
                                 tool->ID,
                                 G_TYPE_FROM_INSTANCE (tool),
                                 tr_tool->old_trans_info,
                                 NULL);

  /*  push the undo group end  */
  gimp_image_undo_group_end (gdisp->gimage);

  /*  We're done dirtying the image, and would like to be restarted
   *  if the image gets dirty while the tool exists
   */
  gimp_tool_control_set_preserve (tool->control, FALSE);

  gimp_unset_busy (gdisp->gimage->gimp);

  gimp_image_flush (gdisp->gimage);

  gimp_transform_tool_halt (tr_tool);
}

static void
gimp_transform_tool_transform_bounding_box (GimpTransformTool *tr_tool)
{
  g_return_if_fail (GIMP_IS_TRANSFORM_TOOL (tr_tool));

  gimp_matrix3_transform_point (&tr_tool->transform,
                                tr_tool->x1, tr_tool->y1,
                                &tr_tool->tx1, &tr_tool->ty1);
  gimp_matrix3_transform_point (&tr_tool->transform,
                                tr_tool->x2, tr_tool->y1,
                                &tr_tool->tx2, &tr_tool->ty2);
  gimp_matrix3_transform_point (&tr_tool->transform,
                                tr_tool->x1, tr_tool->y2,
                                &tr_tool->tx3, &tr_tool->ty3);
  gimp_matrix3_transform_point (&tr_tool->transform,
                                tr_tool->x2, tr_tool->y2,
                                &tr_tool->tx4, &tr_tool->ty4);

  gimp_matrix3_transform_point (&tr_tool->transform,
                                tr_tool->cx, tr_tool->cy,
                                &tr_tool->tcx, &tr_tool->tcy);

  if (tr_tool->grid_coords && tr_tool->tgrid_coords)
    {
      gint i, k;
      gint gci;

      gci = 0;
      k   = (tr_tool->ngx + tr_tool->ngy) * 2;

      for (i = 0; i < k; i++)
        {
          gimp_matrix3_transform_point (&tr_tool->transform,
                                        tr_tool->grid_coords[gci],
                                        tr_tool->grid_coords[gci + 1],
                                        &tr_tool->tgrid_coords[gci],
                                        &tr_tool->tgrid_coords[gci + 1]);
          gci += 2;
        }
    }
}

void
gimp_transform_tool_expose_preview (GimpTransformTool *tr_tool)
{
  GimpTransformOptions *options;

  options =
    GIMP_TRANSFORM_OPTIONS (GIMP_TOOL (tr_tool)->tool_info->tool_options);

  if ((options->preview_type == GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE ||
       options->preview_type == GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID) &&
      options->type         == GIMP_TRANSFORM_TYPE_LAYER &&
      options->direction    == GIMP_TRANSFORM_FORWARD)
    gimp_transform_tool_force_expose_preview (tr_tool);
}

static void
gimp_transform_tool_force_expose_preview (GimpTransformTool *tr_tool)
{
  static gint       last_x = 0;
  static gint       last_y = 0;
  static gint       last_w = 0;
  static gint       last_h = 0;

  GimpDisplayShell *shell;
  gdouble           dx [4], dy [4];
  gint              area_x, area_y, area_w, area_h;
  gint              i;

  g_return_if_fail (GIMP_IS_TRANSFORM_TOOL (tr_tool));

  if (! tr_tool->use_grid)
    return;

  g_return_if_fail (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tr_tool)));

  shell = GIMP_DISPLAY_SHELL (GIMP_DRAW_TOOL (tr_tool)->gdisp->shell);

  gimp_display_shell_transform_xy_f (shell, tr_tool->tx1, tr_tool->ty1,
                                     dx + 0, dy + 0, FALSE);
  gimp_display_shell_transform_xy_f (shell, tr_tool->tx2, tr_tool->ty2,
                                     dx + 1, dy + 1, FALSE);
  gimp_display_shell_transform_xy_f (shell, tr_tool->tx3, tr_tool->ty3,
                                     dx + 2, dy + 2, FALSE);
  gimp_display_shell_transform_xy_f (shell, tr_tool->tx4, tr_tool->ty4,
                                     dx + 3, dy + 3, FALSE);

  /* find bounding box around preview */
  area_x = area_w = (gint) dx [0];
  area_y = area_h = (gint) dy [0];

  for (i = 1; i < 4; i++)
    {
      if (dx [i] < area_x)
        area_x = (gint) dx [i];
      else if (dx [i] > area_w)
        area_w = (gint) dx [i];

      if (dy [i] < area_y)
        area_y = (gint) dy [i];
      else if (dy [i] > area_h)
        area_h = (gint) dy [i];
    }

  area_w -= area_x;
  area_h -= area_y;

  gimp_display_shell_expose_area (shell,
                                  MIN (area_x, last_x),
                                  MIN (area_y, last_y),
                                  MAX (area_w, last_w) + ABS (last_x - area_x),
                                  MAX (area_h, last_h) + ABS (last_y - area_y));

  /* area of last preview must be re-exposed to avoid leaving artifacts */
  last_x = area_x;
  last_y = area_y;
  last_w = area_w;
  last_h = area_h;
}

static void
gimp_transform_tool_halt (GimpTransformTool *tr_tool)
{
  GimpTool *tool = GIMP_TOOL (tr_tool);

  if (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tr_tool)))
    {
      GimpDisplayShell *shell;

      shell = GIMP_DISPLAY_SHELL (GIMP_DRAW_TOOL (tr_tool)->gdisp->shell);

      if (gimp_display_shell_get_show_transform (shell))
        {
          gimp_display_shell_set_show_transform (shell, FALSE);

          /* get rid of preview artifacts left outside the drawable's area */
          gimp_transform_tool_expose_preview (tr_tool);
        }
    }

  if (tr_tool->original)
    {
      tile_manager_unref (tr_tool->original);
      tr_tool->original = NULL;
    }

  /*  inactivate the tool  */
  tr_tool->function = TRANSFORM_CREATING;

  if (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tr_tool)))
    gimp_draw_tool_stop (GIMP_DRAW_TOOL (tr_tool));

  if (tr_tool->info_dialog)
    info_dialog_hide (tr_tool->info_dialog);

  tool->gdisp    = NULL;
  tool->drawable = NULL;
}

static void
gimp_transform_tool_bounds (GimpTransformTool *tr_tool,
                            GimpDisplay       *gdisp)
{
  GimpTransformOptions *options;

  options =
    GIMP_TRANSFORM_OPTIONS (GIMP_TOOL (tr_tool)->tool_info->tool_options);

  /*  find the boundaries  */
  if (tr_tool->original)
    {
      tile_manager_get_offsets (tr_tool->original, &tr_tool->x1, &tr_tool->y1);

      tr_tool->x2 = tr_tool->x1 + tile_manager_width (tr_tool->original);
      tr_tool->y2 = tr_tool->y1 + tile_manager_height (tr_tool->original);
    }
  else
    {
      switch (options->type)
        {
        case GIMP_TRANSFORM_TYPE_LAYER:
          {
            GimpDrawable *drawable = gimp_image_active_drawable (gdisp->gimage);
            gint          offset_x;
            gint          offset_y;

            gimp_item_offsets (GIMP_ITEM (drawable), &offset_x, &offset_y);

            gimp_drawable_mask_bounds (drawable,
                                       &tr_tool->x1, &tr_tool->y1,
                                       &tr_tool->x2, &tr_tool->y2);
            tr_tool->x1 += offset_x;
            tr_tool->y1 += offset_y;
            tr_tool->x2 += offset_x;
            tr_tool->y2 += offset_y;
          }
          break;

        case GIMP_TRANSFORM_TYPE_SELECTION:
        case GIMP_TRANSFORM_TYPE_PATH:
          gimp_channel_bounds (gimp_image_get_mask (gdisp->gimage),
                               &tr_tool->x1, &tr_tool->y1,
                               &tr_tool->x2, &tr_tool->y2);
          break;
        }
    }

  tr_tool->cx = (gdouble) (tr_tool->x1 + tr_tool->x2) / 2.0;
  tr_tool->cy = (gdouble) (tr_tool->y1 + tr_tool->y2) / 2.0;

  /*  changing the bounds invalidates any grid we may have  */
  if (tr_tool->use_grid)
    gimp_transform_tool_grid_recalc (tr_tool);
}

static void
gimp_transform_tool_grid_recalc (GimpTransformTool *tr_tool)
{
  GimpTransformOptions *options;

  options =
    GIMP_TRANSFORM_OPTIONS (GIMP_TOOL (tr_tool)->tool_info->tool_options);

  if (tr_tool->grid_coords != NULL)
    {
      g_free (tr_tool->grid_coords);
      tr_tool->grid_coords = NULL;
    }

  if (tr_tool->tgrid_coords != NULL)
    {
      g_free (tr_tool->tgrid_coords);
      tr_tool->tgrid_coords = NULL;
    }

  if (options->preview_type != GIMP_TRANSFORM_PREVIEW_TYPE_GRID &&
      options->preview_type != GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID)
    return;

  switch (options->grid_type)
    {
    case GIMP_TRANSFORM_GRID_TYPE_N_LINES:
    case GIMP_TRANSFORM_GRID_TYPE_SPACING:
      {
        GimpTool *tool;
        gint      i, gci;
        gdouble  *coords;
        gint      width, height;

        width  = MAX (1, tr_tool->x2 - tr_tool->x1);
        height = MAX (1, tr_tool->y2 - tr_tool->y1);

        tool = GIMP_TOOL (tr_tool);

        if (options->grid_type == GIMP_TRANSFORM_GRID_TYPE_N_LINES)
          {
            if (width <= height)
              {
                tr_tool->ngx = options->grid_size;
                tr_tool->ngy = tr_tool->ngx * MAX (1, height / width);
              }
            else
              {
                tr_tool->ngy = options->grid_size;
                tr_tool->ngx = tr_tool->ngy * MAX (1, width / height);
              }
          }
        else /* GIMP_TRANSFORM_GRID_TYPE_SPACING */
          {
            gint grid_size = MAX (2, options->grid_size);

            tr_tool->ngx = width  / grid_size;
            tr_tool->ngy = height / grid_size;
          }

        tr_tool->grid_coords = coords =
          g_new (gdouble, (tr_tool->ngx + tr_tool->ngy) * 4);

        tr_tool->tgrid_coords =
          g_new (gdouble, (tr_tool->ngx + tr_tool->ngy) * 4);

        gci = 0;

        for (i = 1; i <= tr_tool->ngx; i++)
          {
            coords[gci]     = tr_tool->x1 + (((gdouble) i) / (tr_tool->ngx + 1) *
                                             (tr_tool->x2 - tr_tool->x1));
            coords[gci + 1] = tr_tool->y1;
            coords[gci + 2] = coords[gci];
            coords[gci + 3] = tr_tool->y2;

            gci += 4;
          }

        for (i = 1; i <= tr_tool->ngy; i++)
          {
            coords[gci]     = tr_tool->x1;
            coords[gci + 1] = tr_tool->y1 + (((gdouble) i) / (tr_tool->ngy + 1) *
                                             (tr_tool->y2 - tr_tool->y1));
            coords[gci + 2] = tr_tool->x2;
            coords[gci + 3] = coords[gci + 1];

            gci += 4;
          }
      }

    default:
      break;
    }
}

static void
gimp_transform_tool_dialog (GimpTransformTool *tr_tool)
{
  GimpTool     *tool = GIMP_TOOL (tr_tool);
  GimpToolInfo *tool_info;
  const gchar  *stock_id;
  gchar        *identifier;

  if (! GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->dialog)
    return;

  tool_info = tool->tool_info;

  stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_info));

  tr_tool->info_dialog = info_dialog_new (NULL,
                                          tool_info->blurb,
                                          GIMP_OBJECT (tool_info)->name,
                                          stock_id,
                                          tr_tool->shell_desc,
                                          NULL /* tool->gdisp->shell */,
                                          gimp_standard_help_func,
                                          tool_info->help_id);

  gtk_dialog_add_buttons (GTK_DIALOG (tr_tool->info_dialog->shell),
                          GIMP_STOCK_RESET, RESPONSE_RESET,
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          stock_id,         GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (tr_tool->info_dialog->shell),
                                   GTK_RESPONSE_OK);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (tr_tool->info_dialog->shell),
                                           RESPONSE_RESET,
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_signal_connect (tr_tool->info_dialog->shell, "response",
                    G_CALLBACK (gimp_transform_tool_response),
                    tr_tool);

  GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->dialog (tr_tool);

  identifier = g_strconcat (GIMP_OBJECT (tool_info)->name, "-dialog", NULL);

  gimp_dialog_factory_add_foreign (gimp_dialog_factory_from_name ("toplevel"),
                                   identifier,
                                   tr_tool->info_dialog->shell);

  g_free (identifier);
}

static void
gimp_transform_tool_prepare (GimpTransformTool *tr_tool,
                             GimpDisplay       *gdisp)
{
  GimpTransformOptions *options;

  options =
    GIMP_TRANSFORM_OPTIONS (GIMP_TOOL (tr_tool)->tool_info->tool_options);

  if ((options->preview_type == GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE ||
       options->preview_type == GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID) &&
      options->type         == GIMP_TRANSFORM_TYPE_LAYER &&
      options->direction    == GIMP_TRANSFORM_FORWARD)
    gimp_display_shell_set_show_transform (GIMP_DISPLAY_SHELL (gdisp->shell),
                                           TRUE);
  else
    gimp_display_shell_set_show_transform (GIMP_DISPLAY_SHELL (gdisp->shell),
                                           FALSE);

  if (tr_tool->info_dialog)
    {
      gimp_viewable_dialog_set_viewable (GIMP_VIEWABLE_DIALOG (tr_tool->info_dialog->shell),
                                         GIMP_VIEWABLE (gimp_image_active_drawable (gdisp->gimage)));

      gtk_widget_set_sensitive (GTK_WIDGET (tr_tool->info_dialog->shell), TRUE);
    }

  if (GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->prepare)
    GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->prepare (tr_tool, gdisp);
}

void
gimp_transform_tool_recalc (GimpTransformTool *tr_tool,
                            GimpDisplay       *gdisp)
{
  if (GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->recalc)
    GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->recalc (tr_tool, gdisp);

  gimp_transform_tool_transform_bounding_box (tr_tool);

  if (GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->dialog_update)
    GIMP_TRANSFORM_TOOL_GET_CLASS (tr_tool)->dialog_update (tr_tool);
}

static void
gimp_transform_tool_response (GtkWidget         *widget,
                              gint               response_id,
                              GimpTransformTool *tr_tool)
{
  switch (response_id)
    {
    case RESPONSE_RESET:
      {
        GimpTool *tool;
        gint      i;

        tool = GIMP_TOOL (tr_tool);

        gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

        /*  Restore the previous transformation info  */
        for (i = 0; i < TRAN_INFO_SIZE; i++)
          tr_tool->trans_info[i] = tr_tool->old_trans_info[i];

        /*  reget the selection bounds  */
        gimp_transform_tool_bounds (tr_tool, tool->gdisp);

        /*  recalculate the tool's transformation matrix  */
        gimp_transform_tool_recalc (tr_tool, tool->gdisp);

        /* update preview */
        gimp_transform_tool_expose_preview (tr_tool);

        gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
      }
      break;

    case GTK_RESPONSE_OK:
      gimp_transform_tool_doit (tr_tool, GIMP_TOOL (tr_tool)->gdisp);
      break;

    default:
      gimp_transform_tool_halt (tr_tool);
      break;
    }
}

static void
gimp_transform_tool_notify_type (GimpTransformOptions *options,
                                 GParamSpec           *pspec,
                                 GimpTransformTool    *tr_tool)
{
  if (tr_tool->function != TRANSFORM_CREATING)
    gimp_draw_tool_pause (GIMP_DRAW_TOOL (tr_tool));

  tr_tool->type      = options->type;
  tr_tool->direction = options->direction;

  if (tr_tool->function != TRANSFORM_CREATING)
    {
      /*  reget the selection bounds  */
      gimp_transform_tool_bounds (tr_tool, GIMP_TOOL (tr_tool)->gdisp);

      /*  recalculate the tool's transformation matrix  */
      gimp_transform_tool_recalc (tr_tool, GIMP_TOOL (tr_tool)->gdisp);

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (tr_tool));
    }
}

static void
gimp_transform_tool_notify_preview (GimpTransformOptions *options,
                                    GParamSpec           *pspec,
                                    GimpTransformTool    *tr_tool)
{
  GimpDisplayShell *shell = NULL;

  if (gimp_draw_tool_is_active (GIMP_DRAW_TOOL (tr_tool)))
    shell = GIMP_DISPLAY_SHELL (GIMP_DRAW_TOOL (tr_tool)->gdisp->shell);

  switch (options->preview_type)
    {
    default:
    case GIMP_TRANSFORM_PREVIEW_TYPE_OUTLINE:
      if (shell)
        {
          gimp_display_shell_set_show_transform (shell, FALSE);
          gimp_transform_tool_force_expose_preview (tr_tool);
        }
      break;

    case GIMP_TRANSFORM_PREVIEW_TYPE_GRID:
      if (shell)
        {
          gimp_display_shell_set_show_transform (shell, FALSE);
          gimp_transform_tool_force_expose_preview (tr_tool);
        }

      if (tr_tool->function != TRANSFORM_CREATING)
        {
          gimp_draw_tool_pause (GIMP_DRAW_TOOL (tr_tool));

          gimp_transform_tool_grid_recalc (tr_tool);
          gimp_transform_tool_transform_bounding_box (tr_tool);

          gimp_draw_tool_resume (GIMP_DRAW_TOOL (tr_tool));
        }
      break;

    case GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE:
      if (shell)
        {
          if (options->type      == GIMP_TRANSFORM_TYPE_LAYER &&
              options->direction == GIMP_TRANSFORM_FORWARD)
            gimp_display_shell_set_show_transform (shell, TRUE);
          else
            gimp_display_shell_set_show_transform (shell, FALSE);

          gimp_transform_tool_force_expose_preview (tr_tool);
        }
      break;

    case GIMP_TRANSFORM_PREVIEW_TYPE_IMAGE_GRID:
      if (shell)
        {
          if (options->type      == GIMP_TRANSFORM_TYPE_LAYER &&
              options->direction == GIMP_TRANSFORM_FORWARD)
            gimp_display_shell_set_show_transform (shell, TRUE);
          else
            gimp_display_shell_set_show_transform (shell, FALSE);

          gimp_transform_tool_force_expose_preview (tr_tool);
        }

      if (tr_tool->function != TRANSFORM_CREATING)
        {
          gimp_draw_tool_pause (GIMP_DRAW_TOOL (tr_tool));

          gimp_transform_tool_grid_recalc (tr_tool);
          gimp_transform_tool_transform_bounding_box (tr_tool);

          gimp_draw_tool_resume (GIMP_DRAW_TOOL (tr_tool));
        }
      break;
    }
}
