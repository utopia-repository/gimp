/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "config/gimpconfig-params.h"
#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimppropwidgets.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpbycolorselecttool.h"
#include "gimpellipseselecttool.h"
#include "gimpfuzzyselecttool.h"
#include "gimpiscissorstool.h"
#include "gimpselectionoptions.h"
#include "gimptooloptions-gui.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_OPERATION,
  PROP_ANTIALIAS,
  PROP_FEATHER,
  PROP_FEATHER_RADIUS,
  PROP_SELECT_TRANSPARENT,
  PROP_SAMPLE_MERGED,
  PROP_THRESHOLD,
  PROP_AUTO_SHRINK,
  PROP_SHRINK_MERGED,
  PROP_FIXED_MODE,
  PROP_FIXED_WIDTH,
  PROP_FIXED_HEIGHT,
  PROP_FIXED_UNIT,
  PROP_INTERACTIVE
};


static void   gimp_selection_options_class_init (GimpSelectionOptionsClass *options_class);

static void   gimp_selection_options_set_property (GObject         *object,
                                                   guint            property_id,
                                                   const GValue    *value,
                                                   GParamSpec      *pspec);
static void   gimp_selection_options_get_property (GObject         *object,
                                                   guint            property_id,
                                                   GValue          *value,
                                                   GParamSpec      *pspec);

static void   gimp_selection_options_reset        (GimpToolOptions *tool_options);

static void   selection_options_fixed_mode_notify (GimpSelectionOptions *options,
                                                   GParamSpec           *pspec,
                                                   GtkWidget            *fixed_box);


static GimpToolOptionsClass *parent_class = NULL;


GType
gimp_selection_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpSelectionOptionsClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_selection_options_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpSelectionOptions),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) NULL
      };

      type = g_type_register_static (GIMP_TYPE_TOOL_OPTIONS,
                                     "GimpSelectionOptions",
                                     &info, 0);
    }

  return type;
}

static void
gimp_selection_options_class_init (GimpSelectionOptionsClass *klass)
{
  GObjectClass         *object_class  = G_OBJECT_CLASS (klass);
  GimpToolOptionsClass *options_class = GIMP_TOOL_OPTIONS_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_selection_options_set_property;
  object_class->get_property = gimp_selection_options_get_property;

  options_class->reset       = gimp_selection_options_reset;

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_OPERATION,
                                 "operation", NULL,
                                 GIMP_TYPE_CHANNEL_OPS,
                                 GIMP_CHANNEL_OP_REPLACE,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_ANTIALIAS,
                                    "antialias",
                                    N_("Smooth edges"),
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_FEATHER,
                                    "feather", NULL,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_FEATHER_RADIUS,
                                   "feather-radius", NULL,
                                   0.0, 100.0, 10.0,
                                   0);

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SELECT_TRANSPARENT,
                                    "select-transparent",
                                    N_("Allow completely transparent regions "
                                       "to be selected"),
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SAMPLE_MERGED,
                                    "sample-merged",
                                    N_("Base selection on all visible layers"),
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_THRESHOLD,
                                   "threshold",
                                   N_("Maximum color difference"),
                                   0.0, 255.0, 15.0,
                                   0);

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_AUTO_SHRINK,
                                    "auto-shrink", NULL,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHRINK_MERGED,
                                    "shrink-merged",
                                    N_("Use all visible layers when shrinking "
                                       "the selection"),
                                    FALSE,
                                    0);

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_FIXED_MODE,
                                 "fixed-mode", NULL,
                                 GIMP_TYPE_RECT_SELECT_MODE,
                                 GIMP_RECT_SELECT_MODE_FREE,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_FIXED_WIDTH,
                                   "fixed-width", NULL,
                                   0.0, GIMP_MAX_IMAGE_SIZE, 1.0,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_FIXED_HEIGHT,
                                   "fixed-height", NULL,
                                   0.0, GIMP_MAX_IMAGE_SIZE, 1.0,
                                   0);
  GIMP_CONFIG_INSTALL_PROP_UNIT (object_class, PROP_FIXED_UNIT,
                                 "fixed-unit", NULL,
                                 TRUE, TRUE, GIMP_UNIT_PIXEL,
                                 0);

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_INTERACTIVE,
                                    "interactive", NULL,
                                    FALSE,
                                    0);
}

static void
gimp_selection_options_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GimpSelectionOptions *options = GIMP_SELECTION_OPTIONS (object);

  switch (property_id)
    {
    case PROP_OPERATION:
      options->operation = g_value_get_enum (value);
      break;
    case PROP_ANTIALIAS:
      options->antialias = g_value_get_boolean (value);
      break;
    case PROP_FEATHER:
      options->feather = g_value_get_boolean (value);
      break;
    case PROP_FEATHER_RADIUS:
      options->feather_radius = g_value_get_double (value);
      break;

    case PROP_SELECT_TRANSPARENT:
      options->select_transparent = g_value_get_boolean (value);
      break;
    case PROP_SAMPLE_MERGED:
      options->sample_merged = g_value_get_boolean (value);
      break;
    case PROP_THRESHOLD:
      options->threshold = g_value_get_double (value);
      break;

    case PROP_AUTO_SHRINK:
      options->auto_shrink = g_value_get_boolean (value);
      break;
    case PROP_SHRINK_MERGED:
      options->shrink_merged = g_value_get_boolean (value);
      break;

    case PROP_FIXED_MODE:
      options->fixed_mode = g_value_get_enum (value);
      break;
    case PROP_FIXED_WIDTH:
      options->fixed_width = g_value_get_double (value);
      break;
    case PROP_FIXED_HEIGHT:
      options->fixed_height = g_value_get_double (value);
      break;
    case PROP_FIXED_UNIT:
      options->fixed_unit = g_value_get_int (value);
      break;

    case PROP_INTERACTIVE:
      options->interactive = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_selection_options_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GimpSelectionOptions *options = GIMP_SELECTION_OPTIONS (object);

  switch (property_id)
    {
    case PROP_OPERATION:
      g_value_set_enum (value, options->operation);
      break;
    case PROP_ANTIALIAS:
      g_value_set_boolean (value, options->antialias);
      break;
    case PROP_FEATHER:
      g_value_set_boolean (value, options->feather);
      break;
    case PROP_FEATHER_RADIUS:
      g_value_set_double (value, options->feather_radius);
      break;

    case PROP_SELECT_TRANSPARENT:
      g_value_set_boolean (value, options->select_transparent);
      break;
    case PROP_SAMPLE_MERGED:
      g_value_set_boolean (value, options->sample_merged);
      break;
    case PROP_THRESHOLD:
      g_value_set_double (value, options->threshold);
      break;

    case PROP_AUTO_SHRINK:
      g_value_set_boolean (value, options->auto_shrink);
      break;
    case PROP_SHRINK_MERGED:
      g_value_set_boolean (value, options->shrink_merged);
      break;

    case PROP_FIXED_MODE:
      g_value_set_enum (value, options->fixed_mode);
      break;
    case PROP_FIXED_WIDTH:
      g_value_set_double (value, options->fixed_width);
      break;
    case PROP_FIXED_HEIGHT:
      g_value_set_double (value, options->fixed_height);
      break;
    case PROP_FIXED_UNIT:
      g_value_set_int (value, options->fixed_unit);
      break;

    case PROP_INTERACTIVE:
      g_value_set_boolean (value, options->interactive);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_selection_options_reset (GimpToolOptions *tool_options)
{
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (tool_options),
                                        "antialias");

  if (pspec)
    G_PARAM_SPEC_BOOLEAN (pspec)->default_value =
      (tool_options->tool_info->tool_type != GIMP_TYPE_RECT_SELECT_TOOL);

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (tool_options),
                                        "threshold");

  if (pspec)
    G_PARAM_SPEC_DOUBLE (pspec)->default_value =
      GIMP_GUI_CONFIG (tool_options->tool_info->gimp->config)->default_threshold;

  GIMP_TOOL_OPTIONS_CLASS (parent_class)->reset (tool_options);
}

GtkWidget *
gimp_selection_options_gui (GimpToolOptions *tool_options)
{
  GObject              *config  = G_OBJECT (tool_options);
  GimpSelectionOptions *options = GIMP_SELECTION_OPTIONS (tool_options);
  GtkWidget            *vbox;
  GtkWidget            *button;

  vbox = gimp_tool_options_gui (tool_options);

  /*  the selection operation radio buttons  */
  {
    GtkWidget *hbox;
    GtkWidget *label;
    GList     *list;

    hbox = gimp_prop_enum_stock_box_new (config, "operation",
                                         "gimp-selection", 0, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    list = gtk_container_get_children (GTK_CONTAINER (hbox));
    gtk_box_reorder_child (GTK_BOX (hbox), GTK_WIDGET (list->next->next->data),
                           0);
    g_list_free (list);

    label = gtk_label_new (_("Mode:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (hbox), label, 0);
    gtk_widget_show (label);
  }

  /*  the antialias toggle button  */
  button = gimp_prop_check_button_new (config, "antialias",
                                       _("Antialiasing"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  if (tool_options->tool_info->tool_type == GIMP_TYPE_RECT_SELECT_TOOL)
    gtk_widget_set_sensitive (button, FALSE);

  /*  the feather frame  */
  {
    GtkWidget *frame;
    GtkWidget *table;

    frame = gimp_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    button = gimp_prop_check_button_new (config, "feather",
                                         _("Feather edges"));
    gtk_frame_set_label_widget (GTK_FRAME (frame), button);
    gtk_widget_show (button);

    table = gtk_table_new (1, 3, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 2);
    gtk_container_add (GTK_CONTAINER (frame), table);
    if (options->feather)
      gtk_widget_show (table);

    g_signal_connect_object (button, "toggled",
                             G_CALLBACK (gimp_toggle_button_set_visible),
                             table, 0);

    /*  the feather radius scale  */
    gimp_prop_scale_entry_new (config, "feather-radius",
                               GTK_TABLE (table), 0, 0,
                               _("Radius:"),
                               1.0, 10.0, 1,
                               FALSE, 0.0, 0.0);
  }

  /* selection tool with an interactive boundary that can be toggled */
  if (tool_options->tool_info->tool_type == GIMP_TYPE_ISCISSORS_TOOL)
    {
      button = gimp_prop_check_button_new (config, "interactive",
                                           _("Show interactive boundary"));
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);
    }

  /*  selection tools which operate on colors or contiguous regions  */
  if (tool_options->tool_info->tool_type == GIMP_TYPE_FUZZY_SELECT_TOOL ||
      tool_options->tool_info->tool_type == GIMP_TYPE_BY_COLOR_SELECT_TOOL)
    {
      GtkWidget *frame;
      GtkWidget *vbox2;
      GtkWidget *table;

      frame = gimp_frame_new (_("Finding Similar Colors"));
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);
      gtk_widget_show (vbox2);

      /*  the select transparent areas toggle  */
      button = gimp_prop_check_button_new (config, "select-transparent",
                                           _("Select transparent areas"));
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      /*  the sample merged toggle  */
      button = gimp_prop_check_button_new (config, "sample-merged",
                                           _("Sample merged"));
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      /*  the threshold scale  */
      table = gtk_table_new (1, 3, FALSE);
      gtk_table_set_col_spacings (GTK_TABLE (table), 2);
      gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
      gtk_widget_show (table);

      gimp_prop_scale_entry_new (config, "threshold",
                                 GTK_TABLE (table), 0, 0,
                                 _("Threshold:"),
                                 1.0, 16.0, 1,
                                 FALSE, 0.0, 0.0);
    }

  /*  widgets for fixed size select  */
  if (tool_options->tool_info->tool_type == GIMP_TYPE_RECT_SELECT_TOOL    ||
      tool_options->tool_info->tool_type == GIMP_TYPE_ELLIPSE_SELECT_TOOL)
    {
      GtkWidget *frame;
      GtkWidget *vbox2;
      GtkWidget *table;
      GtkWidget *combo;
      GtkWidget *menu;
      GtkWidget *width_spinbutton;
      GtkWidget *height_spinbutton;

      frame = gimp_frame_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      button = gimp_prop_check_button_new (config, "auto-shrink",
                                           _("Auto shrink selection"));
      gtk_frame_set_label_widget (GTK_FRAME (frame), button);
      gtk_widget_show (button);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);
      if (options->auto_shrink)
        gtk_widget_show (vbox2);

      g_signal_connect_object (button, "toggled",
                               G_CALLBACK (gimp_toggle_button_set_visible),
                               vbox2, 0);

      button = gimp_prop_check_button_new (config, "shrink-merged",
                                           _("Sample merged"));
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      frame = gimp_frame_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      combo = gimp_prop_enum_combo_box_new (config, "fixed-mode", 0, 0);
      gtk_frame_set_label_widget (GTK_FRAME (frame), combo);
      gtk_widget_show (combo);

      table = gtk_table_new (2, 3, FALSE);
      gtk_table_set_col_spacings (GTK_TABLE (table), 2);
      gtk_container_add (GTK_CONTAINER (frame), table);

      gtk_widget_set_sensitive (table,
                                options->fixed_mode != GIMP_RECT_SELECT_MODE_FREE);
      g_signal_connect (config, "notify::fixed-mode",
                        G_CALLBACK (selection_options_fixed_mode_notify),
                        table);

      width_spinbutton = gimp_prop_spin_button_new (config, "fixed-width",
                                                    1.0, 50.0, 0);
      gtk_entry_set_width_chars (GTK_ENTRY (width_spinbutton), 6);

      gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                                 _("Width:"), 0.0, 0.5,
                                 width_spinbutton, 1, FALSE);

      height_spinbutton = gimp_prop_spin_button_new (config, "fixed-height",
                                                     1.0, 50.0, 0);
      gtk_entry_set_width_chars (GTK_ENTRY (height_spinbutton), 6);

      gimp_table_attach_aligned (GTK_TABLE (table), 0, 1,
                                 _("Height:"), 0.0, 0.5,
                                 height_spinbutton, 1, FALSE);

      menu = gimp_prop_unit_menu_new (config, "fixed-unit", "%a");
      gtk_table_attach (GTK_TABLE (table), menu, 2, 3, 1, 2,
                        GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (menu);

      g_object_set_data (G_OBJECT (menu), "set_digits", width_spinbutton);
      g_object_set_data (G_OBJECT (width_spinbutton), "set_digits",
                         height_spinbutton);
      gimp_unit_menu_set_pixel_digits (GIMP_UNIT_MENU (menu), 0);

      gtk_widget_show (table);
    }

  return vbox;
}


/*  private functions  */

static void
selection_options_fixed_mode_notify (GimpSelectionOptions *options,
                                     GParamSpec           *pspec,
                                     GtkWidget            *fixed_box)
{
  gtk_widget_set_sensitive (fixed_box,
                            options->fixed_mode != GIMP_RECT_SELECT_MODE_FREE);
}