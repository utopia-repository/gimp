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

#include "core/gimptoolinfo.h"

#include "paint/gimperaseroptions.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimperasertool.h"
#include "gimppaintoptions-gui.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


static void   gimp_eraser_tool_class_init    (GimpEraserToolClass  *klass);
static void   gimp_eraser_tool_init          (GimpEraserTool       *eraser);

static void   gimp_eraser_tool_modifier_key  (GimpTool             *tool,
                                              GdkModifierType       key,
                                              gboolean              press,
                                              GdkModifierType       state,
                                              GimpDisplay          *gdisp);
static void   gimp_eraser_tool_cursor_update (GimpTool             *tool,
                                              GimpCoords           *coords,
                                              GdkModifierType       state,
                                              GimpDisplay          *gdisp);

static GtkWidget * gimp_eraser_options_gui   (GimpToolOptions      *tool_options);


static GimpPaintToolClass *parent_class = NULL;


void
gimp_eraser_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  (* callback) (GIMP_TYPE_ERASER_TOOL,
                GIMP_TYPE_ERASER_OPTIONS,
                gimp_eraser_options_gui,
                GIMP_PAINT_OPTIONS_CONTEXT_MASK,
                "gimp-eraser-tool",
                _("Eraser"),
                _("Erase to background or transparency"),
                N_("_Eraser"), "<shift>E",
                NULL, GIMP_HELP_TOOL_ERASER,
                GIMP_STOCK_TOOL_ERASER,
                data);
}

GType
gimp_eraser_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpEraserToolClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_eraser_tool_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpEraserTool),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_eraser_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_PAINT_TOOL,
                                          "GimpEraserTool",
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_eraser_tool_class_init (GimpEraserToolClass *klass)
{
  GimpToolClass *tool_class = GIMP_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  tool_class->modifier_key  = gimp_eraser_tool_modifier_key;
  tool_class->cursor_update = gimp_eraser_tool_cursor_update;
}

static void
gimp_eraser_tool_init (GimpEraserTool *eraser)
{
  GimpTool *tool = GIMP_TOOL (eraser);

  gimp_tool_control_set_tool_cursor            (tool->control,
                                                GIMP_TOOL_CURSOR_ERASER);
  gimp_tool_control_set_toggle_tool_cursor     (tool->control,
                                                GIMP_TOOL_CURSOR_ERASER);
  gimp_tool_control_set_toggle_cursor_modifier (tool->control,
                                                GIMP_CURSOR_MODIFIER_MINUS);

  gimp_paint_tool_enable_color_picker (GIMP_PAINT_TOOL (eraser),
                                       GIMP_COLOR_PICK_MODE_BACKGROUND);
}

static void
gimp_eraser_tool_modifier_key (GimpTool        *tool,
                               GdkModifierType  key,
                               gboolean         press,
                               GdkModifierType  state,
                               GimpDisplay     *gdisp)
{
  if (key == GDK_MOD1_MASK)
    {
      GimpEraserOptions *options;

      options = GIMP_ERASER_OPTIONS (tool->tool_info->tool_options);

      g_object_set (options,
                    "anti-erase", ! options->anti_erase,
                    NULL);
    }

  GIMP_TOOL_CLASS (parent_class)->modifier_key (tool, key, press, state, gdisp);
}

static void
gimp_eraser_tool_cursor_update (GimpTool        *tool,
                                GimpCoords      *coords,
                                GdkModifierType  state,
                                GimpDisplay     *gdisp)
{
  GimpEraserOptions *options;

  options = GIMP_ERASER_OPTIONS (tool->tool_info->tool_options);

  gimp_tool_control_set_toggle (tool->control, options->anti_erase);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}


/*  tool options stuff  */

static GtkWidget *
gimp_eraser_options_gui (GimpToolOptions *tool_options)
{
  GObject   *config;
  GtkWidget *vbox;
  GtkWidget *button;
  gchar     *str;

  config = G_OBJECT (tool_options);

  vbox = gimp_paint_options_gui (tool_options);

  /* the anti_erase toggle */
  str = g_strdup_printf (_("Anti erase  %s"),
                         gimp_get_mod_string (GDK_MOD1_MASK));

  button = gimp_prop_check_button_new (config, "anti-erase", str);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_free (str);

  return vbox;
}