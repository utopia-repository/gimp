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

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimplist.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"

#include "config/gimpcoreconfig.h"

#include "display/gimpdisplay.h"

#include "gimpdrawtool.h"
#include "gimptoolcontrol.h"
#include "tool_manager.h"

#include "gimp-intl.h"


typedef struct _GimpToolManager GimpToolManager;

struct _GimpToolManager
{
  GimpTool *active_tool;
  GSList   *tool_stack;

  GQuark    image_clean_handler_id;
  GQuark    image_dirty_handler_id;
};


/*  local function prototypes  */

static GimpToolManager * tool_manager_get    (Gimp            *gimp);
static void              tool_manager_set    (Gimp            *gimp,
                                              GimpToolManager *tool_manager);
static void   tool_manager_tool_changed      (GimpContext     *user_context,
                                              GimpToolInfo    *tool_info,
                                              gpointer         data);
static void   tool_manager_image_clean_dirty (GimpImage       *gimage,
                                              GimpDirtyMask    dirty_mask,
                                              GimpToolManager *tool_manager);


/*  public functions  */

void
tool_manager_init (Gimp *gimp)
{
  GimpToolManager *tool_manager;
  GimpContext     *user_context;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = g_new0 (GimpToolManager, 1);

  tool_manager->active_tool            = NULL;
  tool_manager->tool_stack             = NULL;
  tool_manager->image_clean_handler_id = 0;
  tool_manager->image_dirty_handler_id = 0;

  tool_manager_set (gimp, tool_manager);

  tool_manager->image_clean_handler_id =
    gimp_container_add_handler (gimp->images, "clean",
				G_CALLBACK (tool_manager_image_clean_dirty),
				tool_manager);

  tool_manager->image_dirty_handler_id =
    gimp_container_add_handler (gimp->images, "dirty",
				G_CALLBACK (tool_manager_image_clean_dirty),
				tool_manager);

  user_context = gimp_get_user_context (gimp);

  g_signal_connect (user_context, "tool_changed",
		    G_CALLBACK (tool_manager_tool_changed),
		    tool_manager);
}

void
tool_manager_exit (Gimp *gimp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);
  tool_manager_set (gimp, NULL);

  gimp_container_remove_handler (gimp->images,
				 tool_manager->image_clean_handler_id);
  gimp_container_remove_handler (gimp->images,
				 tool_manager->image_dirty_handler_id);

  if (tool_manager->active_tool)
    g_object_unref (tool_manager->active_tool);

  g_free (tool_manager);
}

GimpTool *
tool_manager_get_active (Gimp *gimp)
{
  GimpToolManager *tool_manager;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  tool_manager = tool_manager_get (gimp);

  return tool_manager->active_tool;
}

void
tool_manager_select_tool (Gimp     *gimp,
			  GimpTool *tool)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_TOOL (tool));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      GimpDisplay *gdisp = tool_manager->active_tool->gdisp;

      if (! gdisp && GIMP_IS_DRAW_TOOL (tool_manager->active_tool))
        gdisp = GIMP_DRAW_TOOL (tool_manager->active_tool)->gdisp;

      if (gdisp)
        tool_manager_control_active (gimp, HALT, gdisp);

      tool_manager_focus_display_active (gimp, NULL);

      g_object_unref (tool_manager->active_tool);
    }

  tool_manager->active_tool = g_object_ref (tool);
}

void
tool_manager_push_tool (Gimp     *gimp,
			GimpTool *tool)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (GIMP_IS_TOOL (tool));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      tool_manager->tool_stack = g_slist_prepend (tool_manager->tool_stack,
						  tool_manager->active_tool);

      g_object_ref (tool_manager->tool_stack->data);
    }

  tool_manager_select_tool (gimp, tool);
}

void
tool_manager_pop_tool (Gimp *gimp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->tool_stack)
    {
      tool_manager_select_tool (gimp,
				GIMP_TOOL (tool_manager->tool_stack->data));

      g_object_unref (tool_manager->tool_stack->data);

      tool_manager->tool_stack = g_slist_remove (tool_manager->tool_stack,
						 tool_manager->active_tool);
    }
}

gboolean
tool_manager_initialize_active (Gimp        *gimp,
                                GimpDisplay *gdisp)
{
  GimpToolManager *tool_manager;
  GimpTool        *tool;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);
  g_return_val_if_fail (GIMP_IS_DISPLAY (gdisp), FALSE);

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      tool = tool_manager->active_tool;

      if (gimp_tool_initialize (tool, gdisp))
        {
          tool->drawable = gimp_image_active_drawable (gdisp->gimage);

          return TRUE;
        }
    }

  return FALSE;
}

void
tool_manager_control_active (Gimp           *gimp,
			     GimpToolAction  action,
			     GimpDisplay    *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (! tool_manager->active_tool)
    return;

  if (gdisp && (tool_manager->active_tool->gdisp == gdisp ||
                (GIMP_IS_DRAW_TOOL (tool_manager->active_tool) &&
                 GIMP_DRAW_TOOL (tool_manager->active_tool)->gdisp == gdisp)))
    {
      gimp_tool_control (tool_manager->active_tool, action, gdisp);
    }
  else if (action == HALT)
    {
      if (gimp_tool_control_is_active (tool_manager->active_tool->control))
        gimp_tool_control_halt (tool_manager->active_tool->control);
    }
}

void
tool_manager_button_press_active (Gimp            *gimp,
                                  GimpCoords      *coords,
                                  guint32          time,
                                  GdkModifierType  state,
                                  GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_button_press (tool_manager->active_tool,
                              coords, time, state,
                              gdisp);
    }
}

void
tool_manager_button_release_active (Gimp            *gimp,
                                    GimpCoords      *coords,
                                    guint32          time,
                                    GdkModifierType  state,
                                    GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_button_release (tool_manager->active_tool,
                                coords, time, state,
                                gdisp);
    }
}

void
tool_manager_motion_active (Gimp            *gimp,
                            GimpCoords      *coords,
                            guint32          time,
                            GdkModifierType  state,
                            GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_motion (tool_manager->active_tool,
                        coords, time, state,
                        gdisp);
    }
}

gboolean
tool_manager_key_press_active (Gimp        *gimp,
                               GdkEventKey *kevent,
                               GimpDisplay *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      return gimp_tool_key_press (tool_manager->active_tool,
                                  kevent,
                                  gdisp);
    }

  return FALSE;
}

void
tool_manager_focus_display_active (Gimp        *gimp,
                                   GimpDisplay *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_set_focus_display (tool_manager->active_tool,
                                   gdisp);
    }
}

void
tool_manager_modifier_state_active (Gimp            *gimp,
                                    GdkModifierType  state,
                                    GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_set_modifier_state (tool_manager->active_tool,
                                    state,
                                    gdisp);
    }
}

void
tool_manager_oper_update_active (Gimp            *gimp,
                                 GimpCoords      *coords,
                                 GdkModifierType  state,
                                 GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_oper_update (tool_manager->active_tool,
                             coords, state,
                             gdisp);
    }
}

void
tool_manager_cursor_update_active (Gimp            *gimp,
                                   GimpCoords      *coords,
                                   GdkModifierType  state,
                                   GimpDisplay     *gdisp)
{
  GimpToolManager *tool_manager;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  tool_manager = tool_manager_get (gimp);

  if (tool_manager->active_tool)
    {
      gimp_tool_cursor_update (tool_manager->active_tool,
                               coords, state,
                               gdisp);
    }
}


/*  private functions  */

static GQuark tool_manager_quark = 0;

static GimpToolManager *
tool_manager_get (Gimp *gimp)
{
  if (! tool_manager_quark)
    tool_manager_quark = g_quark_from_static_string ("gimp-tool-manager");

  return g_object_get_qdata (G_OBJECT (gimp), tool_manager_quark);
}

static void
tool_manager_set (Gimp            *gimp,
		  GimpToolManager *tool_manager)
{
  if (! tool_manager_quark)
    tool_manager_quark = g_quark_from_static_string ("gimp-tool-manager");

  g_object_set_qdata (G_OBJECT (gimp), tool_manager_quark, tool_manager);
}

static void
tool_manager_tool_changed (GimpContext  *user_context,
			   GimpToolInfo *tool_info,
			   gpointer      data)
{
  GimpToolManager *tool_manager = data;
  GimpTool        *new_tool     = NULL;

  if (! tool_info)
    return;

  /* FIXME: gimp_busy HACK */
  if (user_context->gimp->busy)
    {
      /*  there may be contexts waiting for the user_context's "tool_changed"
       *  signal, so stop emitting it.
       */
      g_signal_stop_emission_by_name (user_context, "tool_changed");

      if (G_TYPE_FROM_INSTANCE (tool_manager->active_tool) !=
	  tool_info->tool_type)
	{
	  g_signal_handlers_block_by_func (user_context,
					   tool_manager_tool_changed,
					   data);

	  /*  explicitly set the current tool  */
	  gimp_context_set_tool (user_context,
                                 tool_manager->active_tool->tool_info);

	  g_signal_handlers_unblock_by_func (user_context,
					     tool_manager_tool_changed,
					     data);
	}

      return;
    }

  if (g_type_is_a (tool_info->tool_type, GIMP_TYPE_TOOL))
    {
      new_tool = g_object_new (tool_info->tool_type,
                               "tool-info", tool_info,
                               NULL);
    }
  else
    {
      g_warning ("%s: tool_info->tool_type is no GimpTool subclass",
		 G_STRFUNC);
      return;
    }

  /*  disconnect the old tool's context  */
  if (tool_manager->active_tool            &&
      tool_manager->active_tool->tool_info &&
      tool_manager->active_tool->tool_info->context_props)
    {
      GimpToolInfo *old_tool_info = tool_manager->active_tool->tool_info;

      gimp_context_set_parent (GIMP_CONTEXT (old_tool_info->tool_options), NULL);
    }

  /*  connect the new tool's context  */
  if (tool_info->context_props)
    {
      GimpCoreConfig      *config       = user_context->gimp->config;
      GimpContextPropMask  global_props = 0;

      /*  FG and BG are always shared between all tools  */
      global_props |= GIMP_CONTEXT_FOREGROUND_MASK;
      global_props |= GIMP_CONTEXT_BACKGROUND_MASK;

      if (config->global_brush)
        global_props |= GIMP_CONTEXT_BRUSH_MASK;
      if (config->global_pattern)
        global_props |= GIMP_CONTEXT_PATTERN_MASK;
      if (config->global_palette)
        global_props |= GIMP_CONTEXT_PALETTE_MASK;
      if (config->global_gradient)
        global_props |= GIMP_CONTEXT_GRADIENT_MASK;
      if (config->global_font)
        global_props |= GIMP_CONTEXT_FONT_MASK;

      gimp_context_copy_properties (GIMP_CONTEXT (tool_info->tool_options),
                                    user_context,
                                    tool_info->context_props & ~global_props);
      gimp_context_set_parent (GIMP_CONTEXT (tool_info->tool_options),
                               user_context);
    }

  tool_manager_select_tool (user_context->gimp, new_tool);

  g_object_unref (new_tool);
}

static void
tool_manager_image_clean_dirty (GimpImage       *gimage,
                                GimpDirtyMask    dirty_mask,
                                GimpToolManager *tool_manager)
{
  GimpTool *active_tool = tool_manager->active_tool;

  if (active_tool &&
      ! gimp_tool_control_preserve (active_tool->control) &&
      (gimp_tool_control_dirty_mask (active_tool->control) & dirty_mask))
    {
      GimpDisplay *gdisp = active_tool->gdisp;

      if (! gdisp || gdisp->gimage != gimage)
        if (GIMP_IS_DRAW_TOOL (active_tool))
          gdisp = GIMP_DRAW_TOOL (active_tool)->gdisp;

      if (gdisp && gdisp->gimage == gimage)
        gimp_context_tool_changed (gimp_get_user_context (gimage->gimp));
    }
}