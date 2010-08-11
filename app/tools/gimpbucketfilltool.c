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

#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpdrawable-bucket-fill.h"
#include "core/gimpimage.h"
#include "core/gimppickable.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimphelp-ids.h"

#include "display/gimpdisplay.h"

#include "gimpbucketfilloptions.h"
#include "gimpbucketfilltool.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   gimp_bucket_fill_tool_button_press   (GimpTool        *tool,
                                                    GimpCoords      *coords,
                                                    guint32          time,
                                                    GdkModifierType  state,
                                                    GimpDisplay     *display);
static void   gimp_bucket_fill_tool_button_release (GimpTool        *tool,
                                                    GimpCoords      *coords,
                                                    guint32          time,
                                                    GdkModifierType  state,
                                                    GimpDisplay     *display);
static void   gimp_bucket_fill_tool_modifier_key   (GimpTool        *tool,
                                                    GdkModifierType  key,
                                                    gboolean         press,
                                                    GdkModifierType  state,
                                                    GimpDisplay     *display);
static void   gimp_bucket_fill_tool_cursor_update  (GimpTool        *tool,
                                                    GimpCoords      *coords,
                                                    GdkModifierType  state,
                                                    GimpDisplay     *display);


G_DEFINE_TYPE (GimpBucketFillTool, gimp_bucket_fill_tool, GIMP_TYPE_TOOL);

#define parent_class gimp_bucket_fill_tool_parent_class


void
gimp_bucket_fill_tool_register (GimpToolRegisterCallback  callback,
                                gpointer                  data)
{
  (* callback) (GIMP_TYPE_BUCKET_FILL_TOOL,
                GIMP_TYPE_BUCKET_FILL_OPTIONS,
                gimp_bucket_fill_options_gui,
                GIMP_CONTEXT_FOREGROUND_MASK |
                GIMP_CONTEXT_BACKGROUND_MASK |
                GIMP_CONTEXT_OPACITY_MASK    |
                GIMP_CONTEXT_PAINT_MODE_MASK |
                GIMP_CONTEXT_PATTERN_MASK,
                "gimp-bucket-fill-tool",
                _("Bucket Fill"),
                _("Fill with a color or pattern"),
                N_("_Bucket Fill"), "<shift>B",
                NULL, GIMP_HELP_TOOL_BUCKET_FILL,
                GIMP_STOCK_TOOL_BUCKET_FILL,
                data);
}

static void
gimp_bucket_fill_tool_class_init (GimpBucketFillToolClass *klass)
{
  GimpToolClass *tool_class = GIMP_TOOL_CLASS (klass);

  tool_class->button_press   = gimp_bucket_fill_tool_button_press;
  tool_class->button_release = gimp_bucket_fill_tool_button_release;
  tool_class->modifier_key   = gimp_bucket_fill_tool_modifier_key;
  tool_class->cursor_update  = gimp_bucket_fill_tool_cursor_update;
}

static void
gimp_bucket_fill_tool_init (GimpBucketFillTool *bucket_fill_tool)
{
  GimpTool *tool = GIMP_TOOL (bucket_fill_tool);

  gimp_tool_control_set_scroll_lock     (tool->control, TRUE);
  gimp_tool_control_set_tool_cursor     (tool->control,
                                         GIMP_TOOL_CURSOR_BUCKET_FILL);
  gimp_tool_control_set_action_value_1  (tool->control,
                                         "context/context-opacity-set");
  gimp_tool_control_set_action_object_1 (tool->control,
                                         "context/context-pattern-select-set");
}

static void
gimp_bucket_fill_tool_button_press (GimpTool        *tool,
                                    GimpCoords      *coords,
                                    guint32          time,
                                    GdkModifierType  state,
                                    GimpDisplay     *display)
{
  GimpBucketFillTool    *bucket_tool = GIMP_BUCKET_FILL_TOOL (tool);
  GimpBucketFillOptions *options;

  options = GIMP_BUCKET_FILL_OPTIONS (tool->tool_info->tool_options);

  bucket_tool->target_x = coords->x;
  bucket_tool->target_y = coords->y;

  if (! options->sample_merged)
    {
      gint off_x, off_y;

      gimp_item_offsets (GIMP_ITEM (gimp_image_active_drawable (display->image)),
                         &off_x, &off_y);

      bucket_tool->target_x -= off_x;
      bucket_tool->target_y -= off_y;
    }

  tool->display = display;
  gimp_tool_control_activate (tool->control);
}

static void
gimp_bucket_fill_tool_button_release (GimpTool        *tool,
                                      GimpCoords      *coords,
                                      guint32          time,
                                      GdkModifierType  state,
                                      GimpDisplay     *display)
{
  GimpBucketFillTool    *bucket_tool = GIMP_BUCKET_FILL_TOOL (tool);
  GimpBucketFillOptions *options;
  GimpContext           *context;

  options = GIMP_BUCKET_FILL_OPTIONS (tool->tool_info->tool_options);
  context = GIMP_CONTEXT (options);

  /*  if the 3rd button isn't pressed, fill the selected region  */
  if (! (state & GDK_BUTTON3_MASK))
    {
      gimp_drawable_bucket_fill (gimp_image_active_drawable (display->image),
                                 context,
                                 options->fill_mode,
                                 gimp_context_get_paint_mode (context),
                                 gimp_context_get_opacity (context),
                                 ! options->fill_selection,
                                 options->fill_transparent,
                                 options->threshold,
                                 options->sample_merged,
                                 bucket_tool->target_x,
                                 bucket_tool->target_y);

      gimp_image_flush (display->image);
    }

  gimp_tool_control_halt (tool->control);
}

static void
gimp_bucket_fill_tool_modifier_key (GimpTool        *tool,
                                    GdkModifierType  key,
                                    gboolean         press,
                                    GdkModifierType  state,
                                    GimpDisplay     *display)
{
  GimpBucketFillOptions *options;

  options = GIMP_BUCKET_FILL_OPTIONS (tool->tool_info->tool_options);

  if (key == GDK_CONTROL_MASK)
    {
      switch (options->fill_mode)
        {
        case GIMP_FG_BUCKET_FILL:
          g_object_set (options, "fill-mode", GIMP_BG_BUCKET_FILL, NULL);
          break;

        case GIMP_BG_BUCKET_FILL:
          g_object_set (options, "fill-mode", GIMP_FG_BUCKET_FILL, NULL);
          break;

        default:
          break;
        }
    }
  else if (key == GDK_SHIFT_MASK)
    {
      g_object_set (options, "fill-selection", ! options->fill_selection, NULL);
    }
}

static void
gimp_bucket_fill_tool_cursor_update (GimpTool        *tool,
                                     GimpCoords      *coords,
                                     GdkModifierType  state,
                                     GimpDisplay     *display)
{
  GimpBucketFillOptions *options;
  GimpCursorModifier     cmodifier = GIMP_CURSOR_MODIFIER_NONE;

  options = GIMP_BUCKET_FILL_OPTIONS (tool->tool_info->tool_options);

  if (gimp_image_coords_in_active_drawable (display->image, coords))
    {
      GimpChannel *selection = gimp_image_get_mask (display->image);

      /*  One more test--is there a selected region?
       *  if so, is cursor inside?
       */
      if (gimp_channel_is_empty (selection) ||
          gimp_pickable_get_opacity_at (GIMP_PICKABLE (selection),
                                        coords->x, coords->y))
        {
          switch (options->fill_mode)
            {
            case GIMP_FG_BUCKET_FILL:
              cmodifier = GIMP_CURSOR_MODIFIER_FOREGROUND;
              break;

            case GIMP_BG_BUCKET_FILL:
              cmodifier = GIMP_CURSOR_MODIFIER_BACKGROUND;
              break;

            case GIMP_PATTERN_BUCKET_FILL:
              cmodifier = GIMP_CURSOR_MODIFIER_PATTERN;
              break;
            }
        }
    }

  gimp_tool_control_set_cursor_modifier (tool->control, cmodifier);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, display);
}
