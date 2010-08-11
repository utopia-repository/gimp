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

#include "base/gimplut.h"
#include "base/lut-funcs.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimagemap.h"

#include "widgets/gimphelp-ids.h"

#include "display/gimpdisplay.h"

#include "gimpimagemapoptions.h"
#include "gimpposterizetool.h"

#include "gimp-intl.h"


#define POSTERIZE_DEFAULT_LEVELS 3

#define SLIDER_WIDTH 200


static void     gimp_posterize_tool_class_init (GimpPosterizeToolClass *klass);
static void     gimp_posterize_tool_init       (GimpPosterizeTool      *bc_tool);

static void     gimp_posterize_tool_finalize   (GObject          *object);

static gboolean gimp_posterize_tool_initialize (GimpTool         *tool,
                                                GimpDisplay      *gdisp);

static void     gimp_posterize_tool_map        (GimpImageMapTool *image_map_tool);
static void     gimp_posterize_tool_dialog     (GimpImageMapTool *image_map_tool);
static void     gimp_posterize_tool_reset      (GimpImageMapTool *image_map_tool);

static void     posterize_levels_adjustment_update (GtkAdjustment     *adjustment,
                                                    GimpPosterizeTool *posterize_tool);


static GimpImageMapToolClass *parent_class = NULL;


/*  functions  */

void
gimp_posterize_tool_register (GimpToolRegisterCallback  callback,
                              gpointer                  data)
{
  (* callback) (GIMP_TYPE_POSTERIZE_TOOL,
                GIMP_TYPE_IMAGE_MAP_OPTIONS, NULL,
                0,
                "gimp-posterize-tool",
                _("Posterize"),
                _("Reduce image to a fixed number of colors"),
                N_("_Posterize..."), NULL,
                NULL, GIMP_HELP_TOOL_POSTERIZE,
                GIMP_STOCK_TOOL_POSTERIZE,
                data);
}

GType
gimp_posterize_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpPosterizeToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_posterize_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpPosterizeTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_posterize_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_IMAGE_MAP_TOOL,
					  "GimpPosterizeTool",
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_posterize_tool_class_init (GimpPosterizeToolClass *klass)
{
  GObjectClass          *object_class;
  GimpToolClass         *tool_class;
  GimpImageMapToolClass *image_map_tool_class;

  object_class         = G_OBJECT_CLASS (klass);
  tool_class           = GIMP_TOOL_CLASS (klass);
  image_map_tool_class = GIMP_IMAGE_MAP_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize       = gimp_posterize_tool_finalize;

  tool_class->initialize       = gimp_posterize_tool_initialize;

  image_map_tool_class->shell_desc = _("Posterize (Reduce Number of Colors)");

  image_map_tool_class->map    = gimp_posterize_tool_map;
  image_map_tool_class->dialog = gimp_posterize_tool_dialog;
  image_map_tool_class->reset  = gimp_posterize_tool_reset;
}

static void
gimp_posterize_tool_init (GimpPosterizeTool *posterize_tool)
{
  posterize_tool->levels = POSTERIZE_DEFAULT_LEVELS;
  posterize_tool->lut    = gimp_lut_new ();
}

static void
gimp_posterize_tool_finalize (GObject *object)
{
  GimpPosterizeTool *posterize_tool = GIMP_POSTERIZE_TOOL (object);

  if (posterize_tool->lut)
    {
      gimp_lut_free (posterize_tool->lut);
      posterize_tool->lut = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gimp_posterize_tool_initialize (GimpTool    *tool,
				GimpDisplay *gdisp)
{
  GimpPosterizeTool *posterize_tool = GIMP_POSTERIZE_TOOL (tool);
  GimpDrawable      *drawable;

  drawable = gimp_image_active_drawable (gdisp->gimage);

  if (! drawable)
    return FALSE;

  if (gimp_drawable_is_indexed (drawable))
    {
      g_message (_("Posterize does not operate on indexed layers."));
      return FALSE;
    }

  posterize_tool->levels = POSTERIZE_DEFAULT_LEVELS;

  GIMP_TOOL_CLASS (parent_class)->initialize (tool, gdisp);

  gtk_adjustment_set_value (GTK_ADJUSTMENT (posterize_tool->levels_data),
                            posterize_tool->levels);

  gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (posterize_tool));

  return TRUE;
}

static void
gimp_posterize_tool_map (GimpImageMapTool *image_map_tool)
{
  GimpPosterizeTool *posterize_tool = GIMP_POSTERIZE_TOOL (image_map_tool);

  posterize_lut_setup (posterize_tool->lut,
                       posterize_tool->levels,
                       gimp_drawable_bytes (image_map_tool->drawable));
  gimp_image_map_apply (image_map_tool->image_map,
                        (GimpImageMapApplyFunc) gimp_lut_process_2,
                        posterize_tool->lut);
}


/**********************/
/*  Posterize dialog  */
/**********************/

static void
gimp_posterize_tool_dialog (GimpImageMapTool *image_map_tool)
{
  GimpPosterizeTool *posterize_tool = GIMP_POSTERIZE_TOOL (image_map_tool);
  GtkWidget         *table;
  GtkWidget         *slider;
  GtkObject         *data;

  /*  The table containing sliders  */
  table = gtk_table_new (1, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (image_map_tool->main_vbox), table,
                      FALSE, FALSE, 0);
  gtk_widget_show (table);

  data = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                               _("Posterize _levels:"), SLIDER_WIDTH, -1,
                               posterize_tool->levels,
                               2.0, 256.0, 1.0, 10.0, 0,
                               TRUE, 0.0, 0.0,
                               NULL, NULL);
  posterize_tool->levels_data = GTK_ADJUSTMENT (data);
  slider = GIMP_SCALE_ENTRY_SCALE (data);
  gtk_range_set_update_policy (GTK_RANGE (slider), GTK_UPDATE_DELAYED);

  g_signal_connect (posterize_tool->levels_data, "value_changed",
                    G_CALLBACK (posterize_levels_adjustment_update),
                    posterize_tool);
}

static void
gimp_posterize_tool_reset (GimpImageMapTool *image_map_tool)
{
  GimpPosterizeTool *posterize_tool = GIMP_POSTERIZE_TOOL (image_map_tool);

  posterize_tool->levels = POSTERIZE_DEFAULT_LEVELS;

  gtk_adjustment_set_value (GTK_ADJUSTMENT (posterize_tool->levels_data),
                            posterize_tool->levels);
}

static void
posterize_levels_adjustment_update (GtkAdjustment     *adjustment,
				    GimpPosterizeTool *posterize_tool)
{
  if (posterize_tool->levels != adjustment->value)
    {
      posterize_tool->levels = adjustment->value;

      gimp_image_map_tool_preview (GIMP_IMAGE_MAP_TOOL (posterize_tool));
    }
}
