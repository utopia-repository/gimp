/* GIMP - The GNU Image Manipulation Program
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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "core/gimptooloptions.h"

#include "widgets/gimppropwidgets.h"

#include "gimprectangleoptions.h"
#include "gimptooloptions-gui.h"

#include "gimp-intl.h"


enum
{
  COLUMN_LEFT_NUMBER,
  COLUMN_RIGHT_NUMBER,
  COLUMN_TEXT,
  NUM_COLUMNS
};


static void     gimp_rectangle_options_iface_base_init             (GimpRectangleOptionsInterface *rectangle_options_iface);

static void     gimp_rectangle_options_unparent_fixed_rule_widgets (GimpRectangleOptionsPrivate   *private);
static void     gimp_rectangle_options_fixed_rule_changed          (GtkWidget                     *combo_box,
                                                                    GimpRectangleOptionsPrivate   *private);

static void     gimp_rectangle_options_setup_ratio_completion      (GimpRectangleOptions          *rectangle_options,
                                                                    GtkWidget                     *entry,
                                                                    GtkListStore                  *history);

static gboolean gimp_number_pair_entry_history_select              (GtkEntryCompletion            *completion,
                                                                    GtkTreeModel                  *model,
                                                                    GtkTreeIter                   *iter,
                                                                    GimpNumberPairEntry           *entry);

static void     gimp_number_pair_entry_history_add                 (GtkWidget                     *entry,
                                                                    GtkTreeModel                  *model);


/* TODO: Calculate this dynamically so that the GtkEntry:s are always
 * left-aligned with the right edge of the check buttons.
 */
#define FIXED_RULE_ENTRY_OFFSET 15


GType
gimp_rectangle_options_interface_get_type (void)
{
  static GType iface_type = 0;

  if (! iface_type)
    {
      const GTypeInfo iface_info =
      {
        sizeof (GimpRectangleOptionsInterface),
        (GBaseInitFunc)     gimp_rectangle_options_iface_base_init,
        (GBaseFinalizeFunc) NULL,
      };

      iface_type = g_type_register_static (G_TYPE_INTERFACE,
                                           "GimpRectangleOptionsInterface",
                                           &iface_info, 0);

      g_type_interface_add_prerequisite (iface_type, GIMP_TYPE_TOOL_OPTIONS);
    }

  return iface_type;
}

static void
gimp_rectangle_options_iface_base_init (GimpRectangleOptionsInterface *iface)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("auto-shrink",
                                                                 NULL, NULL,
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("shrink-merged",
                                                                 NULL,
                                                                 N_("Use all visible layers when shrinking "
                                                                    "the selection"),
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("highlight",
                                                                 NULL, NULL,
                                                                 TRUE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_enum ("guide",
                                                              NULL, NULL,
                                                              GIMP_TYPE_RECTANGLE_GUIDE,
                                                              GIMP_RECTANGLE_GUIDE_NONE,
                                                              GIMP_CONFIG_PARAM_FLAGS |
                                                              GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("x0",
                                                                NULL, NULL,
                                                                -GIMP_MAX_IMAGE_SIZE,
                                                                GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("y0",
                                                                NULL, NULL,
                                                                -GIMP_MAX_IMAGE_SIZE,
                                                                GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("width",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("height",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("aspect-numerator",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                1.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("aspect-denominator",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                1.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("default-aspect-numerator",
                                                                NULL, NULL,
                                                                0.001, GIMP_MAX_IMAGE_SIZE,
                                                                1.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("default-aspect-denominator",
                                                                NULL, NULL,
                                                                0.001, GIMP_MAX_IMAGE_SIZE,
                                                                1.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("overridden-fixed-aspect",
                                                                 NULL, NULL,
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("fixed-center",
                                                                 NULL, NULL,
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("fixed-rule-active",
                                                                 NULL, NULL,
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_enum ("fixed-rule",
                                                              NULL, NULL,
                                                              GIMP_TYPE_RECTANGLE_TOOL_FIXED_RULE,
                                                              GIMP_RECTANGLE_TOOL_FIXED_ASPECT,
                                                              GIMP_CONFIG_PARAM_FLAGS |
                                                              GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("desired-fixed-width",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("desired-fixed-height",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("desired-fixed-size-width",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("desired-fixed-size-height",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("default-fixed-size-width",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_PARAM_READWRITE |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("default-fixed-size-height",
                                                                NULL, NULL,
                                                                0.0, GIMP_MAX_IMAGE_SIZE,
                                                                100.0,
                                                                GIMP_PARAM_READWRITE |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_boolean ("overridden-fixed-size",
                                                                 NULL, NULL,
                                                                 FALSE,
                                                                 GIMP_CONFIG_PARAM_FLAGS |
                                                                 GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("center-x",
                                                                NULL, NULL,
                                                                -GIMP_MAX_IMAGE_SIZE,
                                                                GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_CONFIG_PARAM_FLAGS |
                                                                GIMP_PARAM_STATIC_STRINGS));

      g_object_interface_install_property (iface,
                                           g_param_spec_double ("center-y",
                                                                NULL, NULL,
                                                                -GIMP_MAX_IMAGE_SIZE,
                                                                GIMP_MAX_IMAGE_SIZE,
                                                                0.0,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

     g_object_interface_install_property (iface,
                                          gimp_param_spec_unit ("unit",
                                                                NULL, NULL,
                                                                TRUE, TRUE,
                                                                GIMP_UNIT_PIXEL,
                                                                GIMP_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT));

      initialized = TRUE;
    }
}

static void
gimp_rectangle_options_private_finalize (GimpRectangleOptionsPrivate *private)
{
  g_object_unref (private->fixed_width_entry);
  g_object_unref (private->fixed_height_entry);
  g_object_unref (private->fixed_aspect_hbox);
  g_object_unref (private->fixed_size_hbox);
  g_object_unref (private->size_button_box);
  g_object_unref (private->aspect_button_box);

  g_object_unref (private->aspect_history);
  g_object_unref (private->size_history);

  g_slice_free (GimpRectangleOptionsPrivate, private);
}

GimpRectangleOptionsPrivate *
gimp_rectangle_options_get_private (GimpRectangleOptions *options)
{
  GimpRectangleOptionsPrivate *private;

  static GQuark private_key = 0;

  g_return_val_if_fail (GIMP_IS_RECTANGLE_OPTIONS (options), NULL);

  if (! private_key)
    private_key = g_quark_from_static_string ("gimp-rectangle-options-private");

  private = g_object_get_qdata (G_OBJECT (options), private_key);

  if (! private)
    {
      private = g_slice_new0 (GimpRectangleOptionsPrivate);

      private->aspect_history = gtk_list_store_new (NUM_COLUMNS,
                                                    G_TYPE_DOUBLE,
                                                    G_TYPE_DOUBLE,
                                                    G_TYPE_STRING);

      private->size_history = gtk_list_store_new (NUM_COLUMNS,
                                                  G_TYPE_DOUBLE,
                                                  G_TYPE_DOUBLE,
                                                  G_TYPE_STRING);

      g_object_set_qdata_full (G_OBJECT (options), private_key, private,
                               (GDestroyNotify) gimp_rectangle_options_private_finalize);
    }

  return private;
}

/**
 * gimp_rectangle_options_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #GimpRectangleOptions. A #GimpRectangleOptionsProp property is installed
 * for each property, using the values from the #GimpRectangleOptionsProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don't collide with some other property values they
 * are using (that's what %GIMP_RECTANGLE_OPTIONS_PROP_LAST is good for).
 **/
void
gimp_rectangle_options_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK,
                                    "auto-shrink");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED,
                                    "shrink-merged");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT,
                                    "highlight");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_GUIDE,
                                    "guide");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_X0,
                                    "x0");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_Y0,
                                    "y0");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_WIDTH,
                                    "width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT,
                                    "height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR,
                                    "aspect-numerator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR,
                                    "aspect-denominator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR,
                                    "default-aspect-numerator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR,
                                    "default-aspect-denominator");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT,
                                    "overridden-fixed-aspect");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE,
                                    "fixed-rule-active");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE,
                                    "fixed-rule");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH,
                                    "desired-fixed-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT,
                                    "desired-fixed-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH,
                                    "desired-fixed-size-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT,
                                    "desired-fixed-size-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH,
                                    "default-fixed-size-width");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT,
                                    "default-fixed-size-height");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE,
                                    "overridden-fixed-size");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER,
                                    "fixed-center");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_CENTER_X,
                                    "center-x");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_CENTER_Y,
                                    "center-y");
  g_object_class_override_property (klass,
                                    GIMP_RECTANGLE_OPTIONS_PROP_UNIT,
                                    "unit");
}

void
gimp_rectangle_options_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GimpRectangleOptions        *options  = GIMP_RECTANGLE_OPTIONS (object);
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  switch (property_id)
    {
    case GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK:
      private->auto_shrink = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED:
      private->shrink_merged = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT:
      private->highlight = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_GUIDE:
      private->guide = g_value_get_enum (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_X0:
      private->x0 = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_Y0:
      private->y0 = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_WIDTH:
      private->width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT:
      private->height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR:
      private->aspect_numerator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR:
      private->aspect_denominator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR:
      private->default_aspect_numerator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR:
      private->default_aspect_denominator = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT:
      private->overridden_fixed_aspect = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER:
      private->fixed_center = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE:
      private->fixed_rule_active = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE:
      private->fixed_rule = g_value_get_enum (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH:
      private->desired_fixed_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT:
      private->desired_fixed_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH:
      private->desired_fixed_size_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT:
      private->desired_fixed_size_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH:
      private->default_fixed_size_width = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT:
      private->default_fixed_size_height = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE:
      private->overridden_fixed_size = g_value_get_boolean (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_CENTER_X:
      private->center_x = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_CENTER_Y:
      private->center_y = g_value_get_double (value);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_UNIT:
      private->unit = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
gimp_rectangle_options_get_property (GObject      *object,
                                     guint         property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GimpRectangleOptions        *options  = GIMP_RECTANGLE_OPTIONS (object);
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (options);

  switch (property_id)
    {
    case GIMP_RECTANGLE_OPTIONS_PROP_AUTO_SHRINK:
      g_value_set_boolean (value, private->auto_shrink);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_SHRINK_MERGED:
      g_value_set_boolean (value, private->shrink_merged);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HIGHLIGHT:
      g_value_set_boolean (value, private->highlight);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_GUIDE:
      g_value_set_enum (value, private->guide);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_X0:
      g_value_set_double (value, private->x0);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_Y0:
      g_value_set_double (value, private->y0);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_WIDTH:
      g_value_set_double (value, private->width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_HEIGHT:
      g_value_set_double (value, private->height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_NUMERATOR:
      g_value_set_double (value, private->aspect_numerator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_ASPECT_DENOMINATOR:
      g_value_set_double (value, private->aspect_denominator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_NUMERATOR:
      g_value_set_double (value, private->default_aspect_numerator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_ASPECT_DENOMINATOR:
      g_value_set_double (value, private->default_aspect_denominator);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_ASPECT:
      g_value_set_boolean (value, private->overridden_fixed_aspect);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_CENTER:
      g_value_set_boolean (value, private->fixed_center);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE_ACTIVE:
      g_value_set_boolean (value, private->fixed_rule_active);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_FIXED_RULE:
      g_value_set_enum (value, private->fixed_rule);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_WIDTH:
      g_value_set_double (value, private->desired_fixed_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_HEIGHT:
      g_value_set_double (value, private->desired_fixed_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_WIDTH:
      g_value_set_double (value, private->desired_fixed_size_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DESIRED_FIXED_SIZE_HEIGHT:
      g_value_set_double (value, private->desired_fixed_size_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_WIDTH:
      g_value_set_double (value, private->default_fixed_size_width);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_DEFAULT_FIXED_SIZE_HEIGHT:
      g_value_set_double (value, private->default_fixed_size_height);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_OVERRIDDEN_FIXED_SIZE:
      g_value_set_boolean (value, private->overridden_fixed_size);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_CENTER_X:
      g_value_set_double (value, private->center_x);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_CENTER_Y:
      g_value_set_double (value, private->center_y);
      break;
    case GIMP_RECTANGLE_OPTIONS_PROP_UNIT:
      g_value_set_int (value, private->unit);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/**
 * gimp_rectangle_options_unparent_fixed_rule_widgets:
 * @table:
 *
 * Removes any fixed rule widgets from the tool options that are in the tool
 * options already. Meant to be used before inserting a new widget in the fixed
 * rule tool options area.
 */
static void
gimp_rectangle_options_unparent_fixed_rule_widgets (GimpRectangleOptionsPrivate *private)
{
  if (gtk_widget_get_parent (private->fixed_width_entry) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->fixed_width_entry);

  if (gtk_widget_get_parent (private->fixed_height_entry) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->fixed_height_entry);

  if (gtk_widget_get_parent (private->fixed_aspect_hbox) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->fixed_aspect_hbox);

  if (gtk_widget_get_parent (private->fixed_size_hbox) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->fixed_size_hbox);

  if (gtk_widget_get_parent (private->size_button_box) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->size_button_box);

  if (gtk_widget_get_parent (private->aspect_button_box) != NULL)
    gtk_container_remove (GTK_CONTAINER (private->second_row_hbox),
                          private->aspect_button_box);
}

/**
 * gimp_rectangle_options_fixed_rule_changed:
 * @combo_box:
 * @private:
 *
 * Updates tool options widgets depending on current fixed rule state.
 */
static void
gimp_rectangle_options_fixed_rule_changed (GtkWidget                   *combo_box,
                                           GimpRectangleOptionsPrivate *private)
{
  gboolean width_entry_sensitive;
  gboolean height_entry_sensitive;

  /* Setup sensitivity for Width and Height entries */

  width_entry_sensitive = !(private->fixed_rule_active &&
                            (private->fixed_rule ==
                             GIMP_RECTANGLE_TOOL_FIXED_WIDTH ||
                             private->fixed_rule ==
                             GIMP_RECTANGLE_TOOL_FIXED_SIZE));

  height_entry_sensitive = !(private->fixed_rule_active &&
                             (private->fixed_rule ==
                              GIMP_RECTANGLE_TOOL_FIXED_HEIGHT ||
                              private->fixed_rule ==
                              GIMP_RECTANGLE_TOOL_FIXED_SIZE));

  if (GTK_WIDGET_IS_SENSITIVE (private->width_entry) !=
      width_entry_sensitive)
    gtk_widget_set_sensitive (private->width_entry, width_entry_sensitive);

  if (GTK_WIDGET_IS_SENSITIVE (private->height_entry) !=
      height_entry_sensitive)
    gtk_widget_set_sensitive (private->height_entry, height_entry_sensitive);


  /* Setup current fixed rule entries */

  switch (private->fixed_rule)
    {
    case GIMP_RECTANGLE_TOOL_FIXED_ASPECT:
      if (gtk_widget_get_parent (private->fixed_aspect_hbox) == NULL)
        {
          gimp_rectangle_options_unparent_fixed_rule_widgets (private);

          gtk_box_pack_start_defaults (GTK_BOX (private->second_row_hbox),
                                       private->fixed_aspect_hbox);

          gtk_box_pack_start (GTK_BOX (private->second_row_hbox),
                              private->aspect_button_box,
                              FALSE, TRUE, 0);
        }
      break;

    case GIMP_RECTANGLE_TOOL_FIXED_WIDTH:
      if (gtk_widget_get_parent (private->fixed_width_entry) == NULL)
        {
          gimp_rectangle_options_unparent_fixed_rule_widgets (private);

          gtk_box_pack_start_defaults (GTK_BOX (private->second_row_hbox),
                                       private->fixed_width_entry);
        }
      break;

    case GIMP_RECTANGLE_TOOL_FIXED_HEIGHT:
      if (gtk_widget_get_parent (private->fixed_height_entry) == NULL)
        {
          gimp_rectangle_options_unparent_fixed_rule_widgets (private);

          gtk_box_pack_start_defaults (GTK_BOX (private->second_row_hbox),
                                       private->fixed_height_entry);
        }
      break;

    case GIMP_RECTANGLE_TOOL_FIXED_SIZE:
      if (gtk_widget_get_parent (private->fixed_size_hbox) == NULL)
        {
          gimp_rectangle_options_unparent_fixed_rule_widgets (private);

          gtk_box_pack_start_defaults (GTK_BOX (private->second_row_hbox),
                                       private->fixed_size_hbox);

          gtk_box_pack_start (GTK_BOX (private->second_row_hbox),
                              private->size_button_box,
                              FALSE, TRUE, 0);
        }
      break;
    }
}

GtkWidget *
gimp_rectangle_options_gui (GimpToolOptions *tool_options)
{
  GimpRectangleOptionsPrivate *private;

  GObject   *config = G_OBJECT (tool_options);
  GtkWidget *vbox   = gimp_tool_options_gui (tool_options);
  GtkWidget *button;
  GtkWidget *combo;
  GtkWidget *table;
  GtkWidget *entry;
  GtkWidget *hbox;
  GList     *children;
  gint       row = 0;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (tool_options);

  /* Fixed Center */
  button = gimp_prop_check_button_new (config, "fixed-center",
                                       _("Expand from center"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* Rectangle fixed-rules (e.g. aspect or width). */
  {
    table = gtk_table_new (2, 1, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 0);
    gtk_table_set_row_spacings (GTK_TABLE (table), 0);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
    gtk_widget_show (table);

    /* Setup first row */
    {
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_table_attach_defaults (GTK_TABLE (table), hbox, 0, 1, 0, 1);
      gtk_widget_show (hbox);

      button = gimp_prop_check_button_new (config, "fixed-rule-active",
                                           _("Fixed:"));
      g_signal_connect (button, "toggled",
                        G_CALLBACK (gimp_rectangle_options_fixed_rule_changed),
                        private);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      combo = gimp_prop_enum_combo_box_new (config, "fixed-rule", 0, 0);
      g_signal_connect (combo, "changed",
                        G_CALLBACK (gimp_rectangle_options_fixed_rule_changed),
                        private);
      gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
      gtk_widget_show (combo);
    }

    /* Setup second row */
    {
      private->entry_alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
      gtk_alignment_set_padding (GTK_ALIGNMENT (private->entry_alignment),
                                 0, 0, FIXED_RULE_ENTRY_OFFSET, 0);
      gtk_table_attach (GTK_TABLE (table), private->entry_alignment,
                        0, 1, 1, 2,
                        GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (private->entry_alignment);


      private->second_row_hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (private->entry_alignment),
                         private->second_row_hbox);
      gtk_widget_show (private->second_row_hbox);
    }

    /* Create and prepare the widgets that are on the second row
     * dependant of current fixed-rule
     */
    {
      /* Aspect ratio entry */
      entry =
        gimp_prop_number_pair_entry_new (config,
                                         "aspect-numerator",
                                         "aspect-denominator",
                                         "default-aspect-numerator",
                                         "default-aspect-denominator",
                                         "overridden-fixed-aspect",
                                         FALSE, TRUE,
                                         ":/",
                                         TRUE,
                                         0.001, GIMP_MAX_IMAGE_SIZE);

      gimp_rectangle_options_setup_ratio_completion (GIMP_RECTANGLE_OPTIONS (tool_options),
                                                     entry,
                                                     private->aspect_history);
      gtk_widget_show (entry);

      private->aspect_button_box =
        gimp_prop_enum_stock_box_new (G_OBJECT (entry),
                                      "aspect", "gimp", -1, -1);
      g_object_ref_sink (private->aspect_button_box);
      gtk_widget_show (private->aspect_button_box);

      /* hide "square" */
      children =
        gtk_container_get_children (GTK_CONTAINER (private->aspect_button_box));
      gtk_widget_hide (children->data);
      g_list_free (children);

      private->fixed_aspect_hbox = gtk_hbox_new (FALSE, 0);
      g_object_ref_sink (private->fixed_aspect_hbox);
      gtk_box_pack_start_defaults (GTK_BOX (private->fixed_aspect_hbox), entry);
      gtk_widget_show (private->fixed_aspect_hbox);

      /* Fixed width entry */
      private->fixed_width_entry =
        gimp_prop_size_entry_new (config, "desired-fixed-width", "unit", "%a",
                                  GIMP_SIZE_ENTRY_UPDATE_SIZE, 300);
      g_object_ref_sink (private->fixed_width_entry);
      gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (private->fixed_width_entry),
                                      FALSE);
      gtk_table_set_col_spacing (GTK_TABLE (private->fixed_width_entry), 1, 0);
      gtk_widget_show (private->fixed_width_entry);


      /* Fixed height entry */
      private->fixed_height_entry =
        gimp_prop_size_entry_new (config, "desired-fixed-height", "unit", "%a",
                                  GIMP_SIZE_ENTRY_UPDATE_SIZE, 300);
      g_object_ref_sink (private->fixed_height_entry);
      gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (private->fixed_height_entry),
                                      FALSE);
      gtk_table_set_col_spacing (GTK_TABLE (private->fixed_height_entry), 1, 0);
      gtk_widget_show (private->fixed_height_entry);

      /* Size entry */
      entry =
        gimp_prop_number_pair_entry_new (config,
                                         "desired-fixed-size-width",
                                         "desired-fixed-size-height",
                                         "default-fixed-size-width",
                                         "default-fixed-size-height",
                                         "overridden-fixed-size",
                                         TRUE, FALSE,
                                         "xX*",
                                         FALSE,
                                         1, GIMP_MAX_IMAGE_SIZE);

      gimp_rectangle_options_setup_ratio_completion (GIMP_RECTANGLE_OPTIONS (tool_options),
                                                     entry,
                                                     private->size_history);
      gtk_widget_show (entry);


      private->fixed_size_hbox = gtk_hbox_new (FALSE, 0);
      g_object_ref_sink (private->fixed_size_hbox);
      gtk_box_pack_start_defaults (GTK_BOX (private->fixed_size_hbox),
                                   entry);
      gtk_widget_show (private->fixed_size_hbox);

      private->size_button_box =
        gimp_prop_enum_stock_box_new (G_OBJECT (entry),
                                      "aspect", "gimp", -1, -1);
      g_object_ref_sink (private->size_button_box);
      gtk_widget_show (private->size_button_box);

      /* hide "square" */
      children =
        gtk_container_get_children (GTK_CONTAINER (private->size_button_box));
      gtk_widget_hide (children->data);
      g_list_free (children);
    }
  }

  /*  Highlight  */
  button = gimp_prop_check_button_new (config, "highlight",
                                       _("Highlight"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* X, Y, Width, Height table */

  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 3);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  /* X */
  entry = gimp_prop_size_entry_new (config, "x0", "unit", "%a",
                                    GIMP_SIZE_ENTRY_UPDATE_SIZE, 300);
  gtk_table_set_col_spacing (GTK_TABLE (entry), 1, 0);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (entry), FALSE);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("X:"), 0.0, 0.5,
                             entry, 1, FALSE);

  /* Y */
  entry = gimp_prop_size_entry_new (config, "y0", "unit", "%a",
                                    GIMP_SIZE_ENTRY_UPDATE_SIZE, 300);
  gtk_table_set_col_spacing (GTK_TABLE (entry), 1, 0);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (entry), FALSE);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Y:"), 0.0, 0.5,
                             entry, 1, FALSE);

  /* Width */
  private->width_entry = gimp_prop_size_entry_new (config,
                                                   "width", "unit", "%a",
                                                   GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                                   300);
  gtk_table_set_col_spacing (GTK_TABLE (private->width_entry), 1, 0);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (private->width_entry),
                                  FALSE);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row,
                             _("Width:"), 0.0, 0.5,
                             private->width_entry, 1, FALSE);
  row++;

  /* Height */
  private->height_entry = gimp_prop_size_entry_new (config,
                                                    "height", "unit", "%a",
                                                    GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                                    300);
  gtk_table_set_col_spacing (GTK_TABLE (private->height_entry), 1, 0);
  gimp_size_entry_show_unit_menu (GIMP_SIZE_ENTRY (private->height_entry),
                                  FALSE);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row,
                             _("Height:"), 0.0, 0.5,
                             private->height_entry, 1, FALSE);
  row++;

  /*  Guide  */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  combo = gimp_prop_enum_combo_box_new (config, "guide", 0, 0);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), combo);
  gtk_widget_show (combo);

  /*  Auto Shrink  */
  button = gtk_button_new_with_label (_("Auto Shrink Selection"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_show (button);

  private->auto_shrink_button = button;

  button = gimp_prop_check_button_new (config, "shrink-merged",
                                       _("Shrink merged"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);


  /* Setup initial fixed rule widgets */
  gimp_rectangle_options_fixed_rule_changed (NULL, private);

  return vbox;
}

/**
 * gimp_rectangle_options_fixed_rule_active:
 * @rectangle_options:
 * @fixed_rule:
 *
 * Return value: %TRUE if @fixed_rule is active, %FALSE otherwise.
 */
gboolean
gimp_rectangle_options_fixed_rule_active (GimpRectangleOptions       *rectangle_options,
                                          GimpRectangleToolFixedRule  fixed_rule)
{
  GimpRectangleOptionsPrivate *private;

  g_return_val_if_fail (GIMP_IS_RECTANGLE_OPTIONS (rectangle_options), FALSE);

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (rectangle_options);

  return private->fixed_rule_active &&
         private->fixed_rule == fixed_rule;
}

static void
gimp_rectangle_options_setup_ratio_completion (GimpRectangleOptions *rectangle_options,
                                               GtkWidget            *entry,
                                               GtkListStore         *history)
{
  GtkEntryCompletion *completion;
  GimpRectangleOptionsPrivate *private;

  private = GIMP_RECTANGLE_OPTIONS_GET_PRIVATE (rectangle_options);

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION,
                             "model", history,
                             "inline-completion", TRUE,
                             NULL);

  gtk_entry_completion_set_text_column (completion, COLUMN_TEXT);
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  g_signal_connect (entry, "ratio-changed",
                    G_CALLBACK (gimp_number_pair_entry_history_add),
                    history);

  g_signal_connect (completion, "match-selected",
                    G_CALLBACK (gimp_number_pair_entry_history_select),
                    entry);
}

static gboolean
gimp_number_pair_entry_history_select (GtkEntryCompletion  *completion,
                                       GtkTreeModel        *model,
                                       GtkTreeIter         *iter,
                                       GimpNumberPairEntry *entry)
{
  gdouble left_number;
  gdouble right_number;

  gtk_tree_model_get (model, iter,
                      COLUMN_LEFT_NUMBER,  &left_number,
                      COLUMN_RIGHT_NUMBER, &right_number,
                      -1);

  gimp_number_pair_entry_set_values (entry, left_number, right_number);

  return TRUE;
}

static void
gimp_number_pair_entry_history_add (GtkWidget    *entry,
                                    GtkTreeModel *model)
{
  GValue               value = { 0, };
  GtkTreeIter          iter;
  gboolean             iter_valid;
  gdouble              left_number;
  gdouble              right_number;
  const gchar         *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  gimp_number_pair_entry_get_values (GIMP_NUMBER_PAIR_ENTRY (entry),
                                     &left_number,
                                     &right_number);

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
       iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get_value (model, &iter, COLUMN_TEXT, &value);

      if (strcmp (text, g_value_get_string (&value)) == 0)
        {
          g_value_unset (&value);
          break;
        }

      g_value_unset (&value);
    }

  if (iter_valid)
    {
      gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, NULL);
    }
  else
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_LEFT_NUMBER,  left_number,
                          COLUMN_RIGHT_NUMBER, right_number,
                          COLUMN_TEXT,        text,
                          -1);

      /* FIXME: limit the size of the history */
    }
}
