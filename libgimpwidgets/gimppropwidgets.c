/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimppropwidgets.c
 * Copyright (C) 2002-2004  Michael Natterer <mitch@gimp.org>
 *                          Sven Neumann <sven@gimp.org>
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "gimpwidgets.h"

#include "libgimp/libgimp-intl.h"


/*  utility function prototypes  */

static void         set_param_spec     (GObject     *object,
                                        GtkWidget   *widget,
                                        GParamSpec  *param_spec);
static void         set_radio_spec     (GObject     *object,
                                        GParamSpec  *param_spec);
static GParamSpec * get_param_spec     (GObject     *object);

static GParamSpec * find_param_spec    (GObject     *object,
                                        const gchar *property_name,
                                        const gchar *strloc);
static GParamSpec * check_param_spec   (GObject     *object,
                                        const gchar *property_name,
                                        GType         type,
                                        const gchar *strloc);

static gboolean     get_numeric_values (GObject     *object,
                                        GParamSpec  *param_spec,
                                        gdouble     *value,
                                        gdouble     *lower,
                                        gdouble     *upper,
                                        const gchar *strloc);

static void         connect_notify     (GObject     *config,
                                        const gchar *property_name,
                                        GCallback    callback,
                                        gpointer     callback_data);


/******************/
/*  check button  */
/******************/

static void   gimp_prop_check_button_callback (GtkWidget  *widget,
                                               GObject    *config);
static void   gimp_prop_check_button_notify   (GObject    *config,
                                               GParamSpec *param_spec,
                                               GtkWidget  *button);

/**
 * gimp_prop_check_button_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of boolean property controlled by checkbutton.
 * @label:         Label to give checkbutton (including mnemonic).
 *
 * Creates a #GtkCheckButton that displays and sets the specified
 * boolean property.
 *
 * Return value: The newly created #GtkCheckButton widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_check_button_new (GObject     *config,
                            const gchar *property_name,
                            const gchar *label)
{
  GParamSpec  *param_spec;
  GtkWidget   *button;
  gboolean     value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  button = gtk_check_button_new_with_mnemonic (label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);

  set_param_spec (G_OBJECT (button), button, param_spec);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_prop_check_button_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_check_button_notify),
                  button);

  return button;
}

static void
gimp_prop_check_button_callback (GtkWidget *widget,
                                 GObject   *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (widget));
  if (! param_spec)
    return;

  g_object_set (config,
                param_spec->name, GTK_TOGGLE_BUTTON (widget)->active,
                NULL);

  gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));
}

static void
gimp_prop_check_button_notify (GObject    *config,
                               GParamSpec *param_spec,
                               GtkWidget  *button)
{
  gboolean value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (GTK_TOGGLE_BUTTON (button)->active != value)
    {
      g_signal_handlers_block_by_func (button,
                                       gimp_prop_check_button_callback,
                                       config);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);
      gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (button));

      g_signal_handlers_unblock_by_func (button,
                                         gimp_prop_check_button_callback,
                                         config);
    }
}


static void   gimp_prop_enum_check_button_callback (GtkWidget  *widget,
                                                    GObject    *config);
static void   gimp_prop_enum_check_button_notify   (GObject    *config,
                                                    GParamSpec *param_spec,
                                                    GtkWidget  *button);

/**
 * gimp_prop_enum_check_button_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of enum property controlled by checkbutton.
 * @label:         Label to give checkbutton (including mnemonic).
 * @false_value:   Enum value corresponding to unchecked state.
 * @true_value:    Enum value corresonding to checked state.
 *
 * Creates a #GtkCheckButton that displays and sets the specified
 * property of type Enum.  Note that this widget only allows two values
 * for the enum, one corresponding to the "checked" state and the
 * other to the "unchecked" state.
 *
 * Return value: The newly created #GtkCheckButton widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_check_button_new (GObject     *config,
                                 const gchar *property_name,
                                 const gchar *label,
                                 gint         false_value,
                                 gint         true_value)
{
  GParamSpec  *param_spec;
  GtkWidget   *button;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  button = gtk_check_button_new_with_mnemonic (label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                value == true_value);

  if (value != false_value && value != true_value)
    gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);

  set_param_spec (G_OBJECT (button), button, param_spec);

  g_object_set_data (G_OBJECT (button), "false-value",
                     GINT_TO_POINTER (false_value));
  g_object_set_data (G_OBJECT (button), "true-value",
                     GINT_TO_POINTER (true_value));

  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_prop_enum_check_button_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_enum_check_button_notify),
                  button);

  return button;
}

static void
gimp_prop_enum_check_button_callback (GtkWidget *widget,
                                      GObject   *config)
{
  GParamSpec *param_spec;
  gint        false_value;
  gint        true_value;

  param_spec = get_param_spec (G_OBJECT (widget));
  if (! param_spec)
    return;

  false_value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                    "false-value"));
  true_value  = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                    "true-value"));

  g_object_set (config,
                param_spec->name,
                GTK_TOGGLE_BUTTON (widget)->active ? true_value : false_value,
                NULL);

  gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (widget), FALSE);

  gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));
}

static void
gimp_prop_enum_check_button_notify (GObject    *config,
                                    GParamSpec *param_spec,
                                    GtkWidget  *button)
{
  gint     value;
  gint     false_value;
  gint     true_value;
  gboolean active       = FALSE;
  gboolean inconsistent = FALSE;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  false_value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                    "false-value"));
  true_value  = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                    "true-value"));

  if (value == true_value)
    active = TRUE;
  else if (value != false_value)
    inconsistent = TRUE;

  gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button),
                                      inconsistent);

  if (GTK_TOGGLE_BUTTON (button)->active != active)
    {
      g_signal_handlers_block_by_func (button,
                                       gimp_prop_enum_check_button_callback,
                                       config);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
      gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (button));

      g_signal_handlers_unblock_by_func (button,
                                         gimp_prop_enum_check_button_callback,
                                         config);
    }
}


/*************************/
/*  int/enum combo box   */
/*************************/

static void   gimp_prop_int_combo_box_callback (GtkWidget   *widget,
                                                GObject     *config);
static void   gimp_prop_int_combo_box_notify   (GObject     *config,
                                                GParamSpec  *param_spec,
                                                GtkWidget   *widget);

/**
 * gimp_prop_int_combo_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of int property controlled by combo box.
 * @store:         #GimpIntStore holding list of labels, values, etc.
 *
 * Creates a #GimpIntComboBox widget to display and set the specified
 * property.  The contents of the widget are determined by @store,
 * which should be created using gimp_int_store_new().
 *
 * Return value: The newly created #GimpIntComboBox widget, optionally
 *               wrapped into a #GtkEventBox.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_int_combo_box_new (GObject      *config,
                             const gchar  *property_name,
                             GimpIntStore *store)
{
  GParamSpec *param_spec;
  GtkWidget  *combo_box;
  GtkWidget  *widget;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_INT, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  combo_box = g_object_new (GIMP_TYPE_INT_COMBO_BOX,
                            "model", store,
                            NULL);

  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo_box), value);

  g_signal_connect (combo_box, "changed",
                    G_CALLBACK (gimp_prop_int_combo_box_callback),
                    config);

  /*  can't set a tooltip on a combo_box  */
  if (g_param_spec_get_blurb (param_spec))
    {
      widget = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (widget), combo_box);
      gtk_widget_show (combo_box);
    }
  else
    {
      widget = combo_box;
    }

  set_param_spec (G_OBJECT (combo_box), widget, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_int_combo_box_notify),
                  combo_box);

  return widget;
}

/**
 * gimp_prop_enum_combo_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of enum property controlled by combo box.
 * @minimum:       Smallest allowed value of enum.
 * @maximum:       Largest allowed value of enum.
 *
 * Creates a #GimpIntComboBox widget to display and set the specified
 * enum property.  The @mimimum_value and @maximum_value give the
 * possibility of restricting the allowed range to a subset of the
 * enum.  If the two values are equal (e.g., 0, 0), then the full
 * range of the Enum is used.
 *
 * Return value: The newly created #GimpEnumComboBox widget, optionally
 *               wrapped into a #GtkEventBox.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_combo_box_new (GObject     *config,
                              const gchar *property_name,
                              gint         minimum,
                              gint         maximum)
{
  GParamSpec *param_spec;
  GtkWidget  *combo_box;
  GtkWidget  *widget;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  if (minimum != maximum)
    {
      GtkListStore *store;

      store = gimp_enum_store_new_with_range (param_spec->value_type,
                                              minimum, maximum);

      combo_box = g_object_new (GIMP_TYPE_ENUM_COMBO_BOX,
                                "model", store,
                                NULL);

      g_object_unref (store);
    }
  else
    {
      combo_box = gimp_enum_combo_box_new (param_spec->value_type);
    }

  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo_box), value);

  g_signal_connect (combo_box, "changed",
                    G_CALLBACK (gimp_prop_int_combo_box_callback),
                    config);

  /*  can't set a tooltip on a combo_box  */
  if (g_param_spec_get_blurb (param_spec))
    {
      widget = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (widget), combo_box);
      gtk_widget_show (combo_box);
    }
  else
    {
      widget = combo_box;
    }

  set_param_spec (G_OBJECT (combo_box), widget, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_int_combo_box_notify),
                  combo_box);

  return widget;
}

static void
gimp_prop_int_combo_box_callback (GtkWidget *widget,
                                  GObject   *config)
{
  GParamSpec  *param_spec;
  gint         value;

  param_spec = get_param_spec (G_OBJECT (widget));
  if (! param_spec)
    return;

  if (gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value))
    {
      g_object_set (config,
                    param_spec->name, value,
                    NULL);
    }
}

static void
gimp_prop_int_combo_box_notify (GObject    *config,
                                GParamSpec *param_spec,
                                GtkWidget  *combo_box)
{
  gint value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (combo_box,
                                   gimp_prop_int_combo_box_callback,
                                   config);

  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo_box), value);

  g_signal_handlers_unblock_by_func (combo_box,
                                     gimp_prop_int_combo_box_callback,
                                     config);
}


/************************/
/*  boolean combo box   */
/************************/

static void   gimp_prop_boolean_combo_box_callback (GtkWidget   *widget,
                                                    GObject     *config);
static void   gimp_prop_boolean_combo_box_notify   (GObject     *config,
                                                    GParamSpec  *param_spec,
                                                    GtkWidget   *widget);


/**
 * gimp_prop_boolean_combo_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of boolean property controlled by combo box.
 * @true_text:     Label used for entry corresponding to %TRUE value.
 * @false_text:    Label used for entry corresponding to %FALSE value.
 *
 * Creates a #GtkComboBox widget to display and set the specified
 * boolean property.  The combo box will have two entries, one
 * displaying the @true_text label, the other displaying the
 * @false_text label.
 *
 * Return value: The newly created #GtkComboBox widget, optionally
 *               wrapped into a #GtkEventBox..
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_boolean_combo_box_new (GObject     *config,
                                 const gchar *property_name,
                                 const gchar *true_text,
                                 const gchar *false_text)
{
  GParamSpec *param_spec;
  GtkWidget  *combo_box;
  GtkWidget  *widget;
  gboolean    value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  combo_box = gtk_combo_box_new_text ();

  gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), true_text);
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), false_text);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), value ? 0 : 1);

  g_signal_connect (combo_box, "changed",
                    G_CALLBACK (gimp_prop_boolean_combo_box_callback),
                    config);

  /*  can't set a tooltip on a combo_box  */
  if (g_param_spec_get_blurb (param_spec))
    {
      widget = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (widget), combo_box);
      gtk_widget_show (combo_box);
    }
  else
    {
      widget = combo_box;
    }

  set_param_spec (G_OBJECT (combo_box), widget, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_boolean_combo_box_notify),
                  combo_box);

  return widget;
}

static void
gimp_prop_boolean_combo_box_callback (GtkWidget *widget,
                                      GObject   *config)
{
  GParamSpec  *param_spec;
  gint         value;

  param_spec = get_param_spec (G_OBJECT (widget));
  if (! param_spec)
    return;

  value = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  g_object_set (config,
                param_spec->name, value ? FALSE : TRUE,
                NULL);
}

static void
gimp_prop_boolean_combo_box_notify (GObject    *config,
                                    GParamSpec *param_spec,
                                    GtkWidget  *combo_box)
{
  gint value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (combo_box,
                                   gimp_prop_boolean_combo_box_callback,
                                   config);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), value ? 0 : 1);

  g_signal_handlers_unblock_by_func (combo_box,
                                     gimp_prop_boolean_combo_box_callback,
                                     config);
}


/*****************/
/*  radio boxes  */
/*****************/

static void  gimp_prop_radio_button_callback (GtkWidget   *widget,
                                              GObject     *config);
static void  gimp_prop_radio_button_notify   (GObject     *config,
                                              GParamSpec  *param_spec,
                                              GtkWidget   *button);


/**
 * gimp_prop_enum_radio_frame_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of enum property controlled by the radio buttons.
 * @title:         Label for the frame holding the buttons
 * @minimum:       Smallest value of enum to be included.
 * @maximum:       Largest value of enum to be included.
 *
 * Creates a group of radio buttons which function to set and display
 * the specified enum property.  The @minimum and @maximum arguments
 * allow only a subset of the enum to be used.  If the two arguments
 * are equal (e.g., 0, 0), then the full range of the enum will be used.
 *
 * Return value: A #GimpFrame containing the radio buttons.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_radio_frame_new (GObject     *config,
                                const gchar *property_name,
                                const gchar *title,
                                gint         minimum,
                                gint         maximum)
{
  GParamSpec *param_spec;
  GtkWidget  *frame;
  GtkWidget  *button;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  if (minimum != maximum)
    {
      frame = gimp_enum_radio_frame_new_with_range (param_spec->value_type,
                                                    minimum, maximum,
                                                    gtk_label_new (title),
                                                    G_CALLBACK (gimp_prop_radio_button_callback),
                                                    config,
                                                    &button);
    }
  else
    {
      frame = gimp_enum_radio_frame_new (param_spec->value_type,
                                         gtk_label_new (title),
                                         G_CALLBACK (gimp_prop_radio_button_callback),
                                         config,
                                         &button);
    }

  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (button), value);

  set_radio_spec (G_OBJECT (button), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_radio_button_notify),
                  button);

  g_object_set_data (G_OBJECT (frame), "radio-button", button);

  return frame;
}

/**
 * gimp_prop_enum_radio_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of enum property controlled by the radio buttons.
 * @minimum:       Smallest value of enum to be included.
 * @maximum:       Largest value of enum to be included.
 *
 * Creates a group of radio buttons which function to set and display
 * the specified enum property.  The @minimum and @maximum arguments
 * allow only a subset of the enum to be used.  If the two arguments
 * are equal (e.g., 0, 0), then the full range of the enum will be used.
 * If you want to assign a label to the group of radio buttons, use
 * gimp_prop_enum_radio_frame_new() instead of this function.
 *
 * Return value: A #GtkVBox containing the radio buttons.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_radio_box_new (GObject     *config,
                              const gchar *property_name,
                              gint         minimum,
                              gint         maximum)
{
  GParamSpec *param_spec;
  GtkWidget  *vbox;
  GtkWidget  *button;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  if (minimum != maximum)
    {
      vbox = gimp_enum_radio_box_new_with_range (param_spec->value_type,
                                                 minimum, maximum,
                                                 G_CALLBACK (gimp_prop_radio_button_callback),
                                                 config,
                                                 &button);
    }
  else
    {
      vbox = gimp_enum_radio_box_new (param_spec->value_type,
                                      G_CALLBACK (gimp_prop_radio_button_callback),
                                      config,
                                      &button);
    }

  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (button), value);

  set_radio_spec (G_OBJECT (button), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_radio_button_notify),
                  button);

  g_object_set_data (G_OBJECT (vbox), "radio-button", button);

  return vbox;
}


/***********/
/*  label  */
/***********/

static void  gimp_prop_enum_label_notify (GObject    *config,
                                          GParamSpec *param_spec,
                                          GtkWidget  *label);

/**
 * gimp_prop_enum_label_new:
 * @config:         Object to which property is attached.
 * @property_name:  Name of enum property to be displayed.
 *
 * Return value: The newly created #GimpEnumLabel widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_label_new (GObject     *config,
                          const gchar *property_name)
{
  GParamSpec *param_spec;
  GtkWidget  *label;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  label = gimp_enum_label_new (param_spec->value_type, value);

  set_param_spec (G_OBJECT (label), NULL, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_enum_label_notify),
                  label);

  return label;
}

static void
gimp_prop_enum_label_notify (GObject    *config,
                             GParamSpec *param_spec,
                             GtkWidget  *label)
{
  gint value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  gimp_enum_label_set_value (GIMP_ENUM_LABEL (label), value);
}


/**
 * gimp_prop_boolean_radio_frame_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of boolean property controlled by the radio buttons.
 * @title:         Label for the frame.
 * @true_text:     Label for the button corresponding to %TRUE.
 * @false_text:    Label for the button corresponding to %FALSE.
 *
 * Creates a pair of radio buttons which function to set and display
 * the specified boolean property.
 *
 * Return value: A #GimpFrame containing the radio buttons.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_boolean_radio_frame_new (GObject     *config,
                                   const gchar *property_name,
                                   const gchar *title,
                                   const gchar *true_text,
                                   const gchar *false_text)
{
  GParamSpec *param_spec;
  GtkWidget  *frame;
  GtkWidget  *button;
  gboolean    value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  frame =
    gimp_int_radio_group_new (TRUE, title,
                              G_CALLBACK (gimp_prop_radio_button_callback),
                              config, value,

                              false_text, FALSE, &button,
                              true_text,  TRUE,  NULL,

                              NULL);

  set_radio_spec (G_OBJECT (button), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_radio_button_notify),
                  button);

  g_object_set_data (G_OBJECT (frame), "radio-button", button);

  return frame;
}

/**
 * gimp_prop_enum_stock_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of enum property controlled by the radio buttons.
 * @stock_prefix:  The prefix of the group of stock ids to use.
 * @minimum:       Smallest value of enum to be included.
 * @maximum:       Largest value of enum to be included.
 *
 * Creates a horizontal box of radio buttons with stock icons, which
 * function to set and display the value of the specified Enum
 * property.  The stock_id for each icon is created by appending the
 * enum_value's nick to the given @stock_prefix.  See
 * gimp_enum_stock_box_new() for more information.
 *
 * Return value: A #GimpEnumStockBox containing the radio buttons.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_enum_stock_box_new (GObject     *config,
                              const gchar *property_name,
                              const gchar *stock_prefix,
                              gint         minimum,
                              gint         maximum)
{
  GParamSpec *param_spec;
  GtkWidget  *box;
  GtkWidget  *button;
  gint        value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_ENUM, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  if (minimum != maximum)
    {
      box = gimp_enum_stock_box_new_with_range (param_spec->value_type,
                                                minimum, maximum,
                                                stock_prefix,
                                                GTK_ICON_SIZE_MENU,
                                                G_CALLBACK (gimp_prop_radio_button_callback),
                                                config,
                                                &button);
    }
  else
    {
      box = gimp_enum_stock_box_new (param_spec->value_type,
                                     stock_prefix,
                                     GTK_ICON_SIZE_MENU,
                                     G_CALLBACK (gimp_prop_radio_button_callback),
                                     config,
                                     &button);
    }

  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (button), value);

  set_radio_spec (G_OBJECT (button), param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_radio_button_notify),
                  button);

  return box;
}

static void
gimp_prop_radio_button_callback (GtkWidget *widget,
                                 GObject   *config)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    {
      GParamSpec *param_spec;
      gint        value;

      param_spec = get_param_spec (G_OBJECT (widget));
      if (! param_spec)
        return;

      value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                  "gimp-item-data"));

      g_object_set (config,
                    param_spec->name, value,
                    NULL);
    }
}

static void
gimp_prop_radio_button_notify (GObject    *config,
                               GParamSpec *param_spec,
                               GtkWidget  *button)
{
  gint value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (button), value);
}


/*****************/
/*  adjustments  */
/*****************/

static void   gimp_prop_adjustment_callback (GtkAdjustment *adjustment,
                                             GObject       *config);
static void   gimp_prop_adjustment_notify   (GObject       *config,
                                             GParamSpec    *param_spec,
                                             GtkAdjustment *adjustment);

/**
 * gimp_prop_spin_button_new:
 * @config:            Object to which property is attached.
 * @property_name:     Name of double property controlled by the spin button.
 * @step_increment:    Step size.
 * @page_increment:    Page size.
 * @digits:            Number of digits after decimal point to display.
 *
 * Creates a spin button to set and display the value of the
 * specified double property.
 *
 * Return value: A new #GimpSpinButton.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_spin_button_new (GObject     *config,
                           const gchar *property_name,
                           gdouble      step_increment,
                           gdouble      page_increment,
                           gint         digits)
{
  GParamSpec *param_spec;
  GtkWidget  *spinbutton;
  GtkObject  *adjustment;
  gdouble     value;
  gdouble     lower;
  gdouble     upper;

  param_spec = find_param_spec (config, property_name, G_STRFUNC);
  if (! param_spec)
    return NULL;

  if (! get_numeric_values (config,
                            param_spec, &value, &lower, &upper, G_STRFUNC))
    return NULL;

  if (! G_IS_PARAM_SPEC_DOUBLE (param_spec))
    digits = 0;

  spinbutton = gimp_spin_button_new (&adjustment,
                                     value, lower, upper,
                                     step_increment, page_increment,
                                     0.0, 1.0, digits);

  set_param_spec (G_OBJECT (adjustment), spinbutton, param_spec);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gimp_prop_adjustment_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_adjustment_notify),
                  adjustment);

  return spinbutton;
}

/**
 * gimp_prop_hscale_new:
 * @config:         Object to which property is attached.
 * @property_name:  Name of integer or double property controlled by the scale.
 * @step_increment: Step size.
 * @page_increment: Page size.
 * @digits:         Number of digits after decimal point to display.
 *
 * Creates a horizontal scale to control the value of the specified
 * integer or double property.
 *
 * Return value: A new #GtkScale.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_hscale_new (GObject     *config,
                      const gchar *property_name,
                      gdouble      step_increment,
                      gdouble      page_increment,
                      gint         digits)
{
  GParamSpec *param_spec;
  GtkWidget  *scale;
  GtkObject  *adjustment;
  gdouble     value;
  gdouble     lower;
  gdouble     upper;

  param_spec = find_param_spec (config, property_name, G_STRFUNC);
  if (! param_spec)
    return NULL;

  if (! get_numeric_values (config,
                            param_spec, &value, &lower, &upper, G_STRFUNC))
    return NULL;

  if (! G_IS_PARAM_SPEC_DOUBLE (param_spec))
    digits = 0;

  adjustment = gtk_adjustment_new (value, lower, upper,
                                   step_increment, page_increment, 0.0);

  scale = g_object_new (GTK_TYPE_HSCALE,
                        "adjustment", adjustment,
                        "digits",     digits,
                        NULL);

  set_param_spec (G_OBJECT (adjustment), scale, param_spec);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gimp_prop_adjustment_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_adjustment_notify),
                  adjustment);

  return scale;
}

/**
 * gimp_prop_scale_entry_new:
 * @config:            Object to which property is attached.
 * @property_name:     Name of double property controlled by the spin button.
 * @table:             The #GtkTable the widgets will be attached to.
 * @column:            The column to start with.
 * @row:               The row to attach the widgets.
 * @label:             The text for the #GtkLabel which will appear left of
 *                     the #GtkHScale.
 * @step_increment:    Step size.
 * @page_increment:    Page size.
 * @digits:            Number of digits after decimal point to display.
 * @restrict_scale:    %TRUE if the range of possible values of the
 *                     GtkSpinButton should be
 *                     the same as of the GtkHScale.
 * @restricted_lower:  The spinbutton's lower boundary if @restrict_scale
 *                     is %FALSE.
 * @restricted_upper:  The spinbutton's upper boundary if @restrict_scale
 *                     is %FALSE.
 *
 * Creates a #GimpScaleEntry (slider and spin button) to set and
 * display the value of the specified double property.  See
 * gimp_scale_entry_new() for more information.
 *
 * Return value: The #GtkSpinButton's #GtkAdjustment.
 *
 * Since GIMP 2.4
 */
GtkObject *
gimp_prop_scale_entry_new (GObject     *config,
                           const gchar *property_name,
                           GtkTable    *table,
                           gint         column,
                           gint         row,
                           const gchar *label,
                           gdouble      step_increment,
                           gdouble      page_increment,
                           gint         digits,
                           gboolean     restrict_scale,
                           gdouble      restricted_lower,
                           gdouble      restricted_upper)
{
  GParamSpec  *param_spec;
  GtkObject   *adjustment;
  const gchar *tooltip;
  gdouble      value;
  gdouble      lower;
  gdouble      upper;

  param_spec = find_param_spec (config, property_name, G_STRFUNC);
  if (! param_spec)
    return NULL;

  if (! get_numeric_values (config,
                            param_spec, &value, &lower, &upper, G_STRFUNC))
    return NULL;

  tooltip = dgettext (gimp_type_get_translation_domain (G_OBJECT_TYPE (config)),
                      g_param_spec_get_blurb (param_spec));

  if (! restrict_scale)
    {
      adjustment = gimp_scale_entry_new (table, column, row,
                                         label, -1, -1,
                                         value, lower, upper,
                                         step_increment, page_increment, digits,
                                         TRUE, 0.0, 0.0,
                                         tooltip,
                                         NULL);
    }
  else
    {
      adjustment = gimp_scale_entry_new (table, column, row,
                                         label, -1, -1,
                                         value,
                                         restricted_lower,
                                         restricted_upper,
                                         step_increment, page_increment, digits,
                                         FALSE, lower, upper,
                                         tooltip,
                                         NULL);
    }

  set_param_spec (G_OBJECT (adjustment), NULL,  param_spec);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gimp_prop_adjustment_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_adjustment_notify),
                  adjustment);

  return adjustment;
}

/**
 * gimp_prop_opacity_entry_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of double property controlled by the spin button.
 * @table:         The #GtkTable the widgets will be attached to.
 * @column:        The column to start with.
 * @row:           The row to attach the widgets.
 * @label:         The text for the #GtkLabel which will appear left of the
 *                 #GtkHScale.
 *
 * Creates a #GimpScaleEntry (slider and spin button) to set and
 * display the value of the specified double property, which should
 * represent an "opacity" variable with range 0 to 100.  See
 * gimp_scale_entry_new() for more information.
 *
 * Return value:  The #GtkSpinButton's #GtkAdjustment.
 *
 * Since GIMP 2.4
 */
GtkObject *
gimp_prop_opacity_entry_new (GObject     *config,
                             const gchar *property_name,
                             GtkTable    *table,
                             gint         column,
                             gint         row,
                             const gchar *label)
{
  GParamSpec  *param_spec;
  GtkObject   *adjustment;
  const gchar *tooltip;
  gdouble      value;
  gdouble      lower;
  gdouble      upper;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_DOUBLE, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config, property_name, &value, NULL);

  tooltip = dgettext (gimp_type_get_translation_domain (G_OBJECT_TYPE (config)),
                      g_param_spec_get_blurb (param_spec));

  value *= 100.0;
  lower = G_PARAM_SPEC_DOUBLE (param_spec)->minimum * 100.0;
  upper = G_PARAM_SPEC_DOUBLE (param_spec)->maximum * 100.0;

  adjustment = gimp_scale_entry_new (table, column, row,
                                     label, -1, -1,
                                     value, lower, upper,
                                     1.0, 10.0, 1,
                                     TRUE, 0.0, 0.0,
                                     tooltip,
                                     NULL);

  set_param_spec (G_OBJECT (adjustment), NULL,  param_spec);
  g_object_set_data (G_OBJECT (adjustment),
                     "opacity-scale", GINT_TO_POINTER (TRUE));

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gimp_prop_adjustment_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_adjustment_notify),
                  adjustment);

  return adjustment;
}


static void
gimp_prop_adjustment_callback (GtkAdjustment *adjustment,
                               GObject       *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (adjustment));
  if (! param_spec)
    return;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (gint) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (guint) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_LONG (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (glong) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_ULONG (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (gulong) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_INT64 (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (gint64) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_UINT64 (param_spec))
    {
      g_object_set (config,
                    param_spec->name, (guint64) adjustment->value,
                    NULL);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      gdouble value;

      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (adjustment),
                                              "opacity-scale")))
        value = adjustment->value / 100.0;
      else
        value = adjustment->value;

      g_object_set (config, param_spec->name, value, NULL);
    }
}

static void
gimp_prop_adjustment_notify (GObject       *config,
                             GParamSpec    *param_spec,
                             GtkAdjustment *adjustment)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config, param_spec->name, &int_value, NULL);

      value = int_value;
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      guint uint_value;

      g_object_get (config, param_spec->name, &uint_value, NULL);

      value = uint_value;
    }
  else if (G_IS_PARAM_SPEC_LONG (param_spec))
    {
      glong long_value;

      g_object_get (config, param_spec->name, &long_value, NULL);

      value = long_value;
    }
  else if (G_IS_PARAM_SPEC_ULONG (param_spec))
    {
      gulong ulong_value;

      g_object_get (config, param_spec->name, &ulong_value, NULL);

      value = ulong_value;
    }
  else if (G_IS_PARAM_SPEC_INT64 (param_spec))
    {
      gint64 int64_value;

      g_object_get (config, param_spec->name, &int64_value, NULL);

      value = int64_value;
    }
  else if (G_IS_PARAM_SPEC_UINT64 (param_spec))
    {
      guint64 uint64_value;

      g_object_get (config, param_spec->name, &uint64_value, NULL);

#if defined _MSC_VER && (_MSC_VER < 1300)
      value = (gint64) uint64_value;
#else
      value = uint64_value;
#endif
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      g_object_get (config, param_spec->name, &value, NULL);

      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (adjustment),
                                              "opacity-scale")))
        value *= 100.0;
    }
  else
    {
      g_warning ("%s: unhandled param spec of type %s",
                 G_STRFUNC, G_PARAM_SPEC_TYPE_NAME (param_spec));
      return;
    }

  if (adjustment->value != value)
    {
      g_signal_handlers_block_by_func (adjustment,
                                       gimp_prop_adjustment_callback,
                                       config);

      gtk_adjustment_set_value (adjustment, value);

      g_signal_handlers_unblock_by_func (adjustment,
                                         gimp_prop_adjustment_callback,
                                         config);
    }
}


/*************/
/*  memsize  */
/*************/

static void   gimp_prop_memsize_callback (GimpMemsizeEntry *entry,
                                          GObject          *config);
static void   gimp_prop_memsize_notify   (GObject          *config,
                                          GParamSpec       *param_spec,
                                          GimpMemsizeEntry *entry);

/**
 * gimp_prop_memsize_entry_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of memsize property.
 *
 * Creates a #GimpMemsizeEntry (spin button and option menu) to set
 * and display the value of the specified memsize property.  See
 * gimp_memsize_entry_new() for more information.
 *
 * Return value:  A new #GimpMemsizeEntry.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_memsize_entry_new (GObject     *config,
                             const gchar *property_name)
{
  GParamSpec       *param_spec;
  GParamSpecUInt64 *uint64_spec;
  GtkWidget        *entry;
  guint64           value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_MEMSIZE, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  uint64_spec = G_PARAM_SPEC_UINT64 (param_spec);

  g_return_val_if_fail (uint64_spec->minimum <= GIMP_MAX_MEMSIZE, NULL);
  g_return_val_if_fail (uint64_spec->maximum <= GIMP_MAX_MEMSIZE, NULL);

  entry = gimp_memsize_entry_new (value,
                                  uint64_spec->minimum,
                                  uint64_spec->maximum);

  set_param_spec (G_OBJECT (entry),
                  GIMP_MEMSIZE_ENTRY (entry)->spinbutton,
                  param_spec);

  g_signal_connect (entry, "value-changed",
                    G_CALLBACK (gimp_prop_memsize_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_memsize_notify),
                  entry);

  return entry;
}


static void
gimp_prop_memsize_callback (GimpMemsizeEntry *entry,
                            GObject          *config)
{
  GParamSpec *param_spec;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  g_return_if_fail (G_IS_PARAM_SPEC_UINT64 (param_spec));

  g_object_set (config,
                param_spec->name, gimp_memsize_entry_get_value (entry),
                NULL);
}

static void
gimp_prop_memsize_notify (GObject          *config,
                          GParamSpec       *param_spec,
                          GimpMemsizeEntry *entry)
{
  guint64  value;

  g_return_if_fail (G_IS_PARAM_SPEC_UINT64 (param_spec));

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (entry->value != value)
    {
      g_signal_handlers_block_by_func (entry,
                                       gimp_prop_memsize_callback,
                                       config);

      gimp_memsize_entry_set_value (entry, value);

      g_signal_handlers_unblock_by_func (entry,
                                         gimp_prop_memsize_callback,
                                         config);
    }
}


/***********/
/*  label  */
/***********/

static void   gimp_prop_label_notify (GObject    *config,
                                      GParamSpec *param_spec,
                                      GtkWidget  *label);

/**
 * gimp_prop_label_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of string property.
 *
 * Creates a #GtkLabel to display the value of the specified String
 * property.  If the user should be able to edit the string, use
 * gimp_prop_entry_new() instead.
 *
 * Return value:  A new #GtkLabel widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_label_new (GObject     *config,
                     const gchar *property_name)
{
  GParamSpec *param_spec;
  GtkWidget  *label;
  gchar      *value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  label = gtk_label_new (value ? value : "");
  g_free (value);

  set_param_spec (G_OBJECT (label), label, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_label_notify),
                  label);

  return label;
}

static void
gimp_prop_label_notify (GObject    *config,
                        GParamSpec *param_spec,
                        GtkWidget  *label)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  gtk_label_set_text (GTK_LABEL (label), value ? value : "");

  g_free (value);
}


/***********/
/*  entry  */
/***********/

static void   gimp_prop_entry_callback (GtkWidget  *entry,
                                        GObject    *config);
static void   gimp_prop_entry_notify   (GObject    *config,
                                        GParamSpec *param_spec,
                                        GtkWidget  *entry);

/**
 * gimp_prop_entry_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of string property.
 * @max_len:       Maximum allowed length of string.
 *
 * Creates a #GtkEntry to set and display the value of the specified
 * string property.
 *
 * Return value:  A new #GtkEntry widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_entry_new (GObject     *config,
                     const gchar *property_name,
                     gint         max_len)
{
  GParamSpec *param_spec;
  GtkWidget  *entry;
  gchar      *value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), value ? value : "");

  g_free (value);

  if (max_len > 0)
    gtk_entry_set_max_length (GTK_ENTRY (entry), max_len);

  set_param_spec (G_OBJECT (entry), entry, param_spec);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (gimp_prop_entry_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_entry_notify),
                  entry);

  return entry;
}

static void
gimp_prop_entry_callback (GtkWidget *entry,
                          GObject   *config)
{
  GParamSpec  *param_spec;
  const gchar *text;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_entry_notify,
                                   entry);

  g_object_set (config,
                param_spec->name, text,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_entry_notify,
                                     entry);
}

static void
gimp_prop_entry_notify (GObject    *config,
                        GParamSpec *param_spec,
                        GtkWidget  *entry)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (entry,
                                   gimp_prop_entry_callback,
                                   config);

  gtk_entry_set_text (GTK_ENTRY (entry), value ? value : "");

  g_signal_handlers_unblock_by_func (entry,
                                     gimp_prop_entry_callback,
                                     config);

  g_free (value);
}


/*****************/
/*  text buffer  */
/*****************/

static void   gimp_prop_text_buffer_callback (GtkTextBuffer *text_buffer,
                                              GObject       *config);
static void   gimp_prop_text_buffer_notify   (GObject       *config,
                                              GParamSpec    *param_spec,
                                              GtkTextBuffer *text_buffer);

/**
 * gimp_prop_text_buffer_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of string property.
 * @max_len:       Maximum allowed length of text (in characters).
 *
 * Creates a #GtkTextBuffer to set and display the value of the
 * specified string property.  Unless the string is expected to
 * contain multiple lines or a large amount of text, use
 * gimp_prop_entry_new() instead.  See #GtkTextView for information on
 * how to insert a text buffer into a visible widget.
 *
 * If @max_len is 0 or negative, the text buffer allows an unlimited
 * number of characters to be entered.
 *
 * Return value:  A new #GtkTextBuffer.
 *
 * Since GIMP 2.4
 */
GtkTextBuffer *
gimp_prop_text_buffer_new (GObject     *config,
                           const gchar *property_name,
                           gint         max_len)
{
  GParamSpec    *param_spec;
  GtkTextBuffer *text_buffer;
  gchar         *value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  text_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (text_buffer, value ? value : "", -1);

  g_free (value);

  if (max_len > 0)
    g_object_set_data (G_OBJECT (text_buffer), "max-len",
                       GINT_TO_POINTER (max_len));

  set_param_spec (G_OBJECT (text_buffer), NULL, param_spec);

  g_signal_connect (text_buffer, "changed",
                    G_CALLBACK (gimp_prop_text_buffer_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_text_buffer_notify),
                  text_buffer);

  return text_buffer;
}

static void
gimp_prop_text_buffer_callback (GtkTextBuffer *text_buffer,
                                GObject       *config)
{
  GParamSpec  *param_spec;
  GtkTextIter  start_iter;
  GtkTextIter  end_iter;
  gchar       *text;
  gint         max_len;

  param_spec = get_param_spec (G_OBJECT (text_buffer));
  if (! param_spec)
    return;

  gtk_text_buffer_get_bounds (text_buffer, &start_iter, &end_iter);
  text = gtk_text_buffer_get_text (text_buffer, &start_iter, &end_iter, FALSE);

  max_len = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (text_buffer),
                                                "max-length"));

  if (max_len > 0 && g_utf8_strlen (text, -1) > max_len)
    {
      g_message (dngettext (GETTEXT_PACKAGE "-libgimp",
                            "This text input field is limited to %d character.",
                            "This text input field is limited to %d characters.",
                            max_len), max_len);

      gtk_text_buffer_get_iter_at_offset (text_buffer,
                                          &start_iter, max_len - 1);
      gtk_text_buffer_get_end_iter (text_buffer, &end_iter);

      /*  this calls us recursively, but in the else branch  */
      gtk_text_buffer_delete (text_buffer, &start_iter, &end_iter);
    }
  else
    {
      g_signal_handlers_block_by_func (config,
                                       gimp_prop_text_buffer_notify,
                                       text_buffer);

      g_object_set (config,
                    param_spec->name, text,
                    NULL);

      g_signal_handlers_unblock_by_func (config,
                                         gimp_prop_text_buffer_notify,
                                         text_buffer);
    }

  g_free (text);
}

static void
gimp_prop_text_buffer_notify (GObject       *config,
                              GParamSpec    *param_spec,
                              GtkTextBuffer *text_buffer)
{
  gchar *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (text_buffer,
                                   gimp_prop_text_buffer_callback,
                                   config);

  gtk_text_buffer_set_text (text_buffer, value ? value : "", -1);

  g_signal_handlers_unblock_by_func (text_buffer,
                                     gimp_prop_text_buffer_callback,
                                     config);

  g_free (value);
}


/****************/
/*  file entry  */
/****************/


static void   gimp_prop_file_entry_callback (GimpFileEntry *entry,
                                             GObject       *config);
static void   gimp_prop_file_entry_notify   (GObject       *config,
                                             GParamSpec    *param_spec,
                                             GimpFileEntry *entry);

/**
 * gimp_prop_file_entry_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of Path property.
 * @filesel_title: Label for the file selector.
 * @dir_only:      %TRUE if the file entry should accept directories only.
 * @check_valid:   %TRUE if the widget should check if the entered file
 *                 really exists.
 *
 * Creates a #GimpFileEntry to set and display the specified Path property.
 *
 * Return value:  A new #GimpFileEntry widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_file_entry_new (GObject     *config,
                          const gchar *property_name,
                          const gchar *filesel_title,
                          gboolean     dir_only,
                          gboolean     check_valid)
{
  GParamSpec *param_spec;
  GtkWidget  *entry;
  gchar      *filename;
  gchar      *value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_CONFIG_PATH, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  entry = gimp_file_entry_new (filesel_title, filename, dir_only, check_valid);
  g_free (filename);

  set_param_spec (G_OBJECT (entry),
                  GIMP_FILE_ENTRY (entry)->entry,
                  param_spec);

  g_signal_connect (entry, "filename-changed",
                    G_CALLBACK (gimp_prop_file_entry_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_file_entry_notify),
                  entry);

  return entry;
}

static void
gimp_prop_file_entry_callback (GimpFileEntry *entry,
                               GObject       *config)
{
  GParamSpec *param_spec;
  gchar      *value;
  gchar      *utf8;

  param_spec = get_param_spec (G_OBJECT (entry));
  if (! param_spec)
    return;

  value = gimp_file_entry_get_filename (entry);
  utf8 = g_filename_to_utf8 (value, -1, NULL, NULL, NULL);
  g_free (value);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_file_entry_notify,
                                   entry);

  g_object_set (config,
                param_spec->name, utf8,
                NULL);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_file_entry_notify,
                                   entry);

  g_free (utf8);
}

static void
gimp_prop_file_entry_notify (GObject       *config,
                             GParamSpec    *param_spec,
                             GimpFileEntry *entry)
{
  gchar *value;
  gchar *filename;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  g_signal_handlers_block_by_func (entry,
                                   gimp_prop_file_entry_callback,
                                   config);

  gimp_file_entry_set_filename (entry, filename);

  g_signal_handlers_unblock_by_func (entry,
                                     gimp_prop_file_entry_callback,
                                     config);

  g_free (filename);
}


/*****************/
/*  file button  */
/*****************/


static void  gimp_prop_file_chooser_button_callback (GtkFileChooser *button,
                                                     GObject        *config);
static void  gimp_prop_file_chooser_button_notify   (GObject        *config,
                                                     GParamSpec     *param_spec,
                                                     GtkFileChooser *button);

/**
 * gimp_prop_file_chooser_button_new:
 * @config:        object to which property is attached.
 * @property_name: name of path property.
 * @title:         the title of the browse dialog.
 * @action:        the open mode for the widget.
 *
 * Creates a #GtkFileChooserButton to edit the specified path property.
 *
 * Note that #GtkFileChooserButton implements the #GtkFileChooser
 * interface; you can use the #GtkFileChooser API with it.
 *
 * Return value:  A new #GtkFileChooserButton.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_file_chooser_button_new (GObject              *config,
                                   const gchar          *property_name,
                                   const gchar          *title,
                                   GtkFileChooserAction  action)
{
  GParamSpec *param_spec;
  GtkWidget  *button;
  GtkWidget  *widget;
  gchar      *filename;
  gchar      *value;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_CONFIG_PATH, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  button = gtk_file_chooser_button_new (title, action);

  if (filename)
    {
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (button), filename);
      g_free (filename);
    }

  /*  can't set a tooltip on a file-chooser button  */
  if (g_param_spec_get_blurb (param_spec))
    {
      widget = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (widget), button);
      gtk_widget_show (button);
    }
  else
    {
      widget = button;
    }

  set_param_spec (G_OBJECT (button), widget, param_spec);

  g_signal_connect (button, "selection-changed",
                    G_CALLBACK (gimp_prop_file_chooser_button_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_file_chooser_button_notify),
                  button);

  return widget;
}

static void
gimp_prop_file_chooser_button_callback (GtkFileChooser *button,
                                        GObject        *config)
{
  GParamSpec *param_spec;
  gchar      *value;
  gchar      *utf8;

  param_spec = get_param_spec (G_OBJECT (button));
  if (! param_spec)
    return;

  value = gtk_file_chooser_get_filename (button);
  utf8 = value ? g_filename_to_utf8 (value, -1, NULL, NULL, NULL) : NULL;
  g_free (value);

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (! (value && utf8 && strcmp (value, utf8) == 0))
    {
      g_signal_handlers_block_by_func (config,
                                       gimp_prop_file_chooser_button_notify,
                                       button);

      g_object_set (config,
                    param_spec->name, utf8,
                    NULL);

      g_signal_handlers_block_by_func (config,
                                       gimp_prop_file_chooser_button_notify,
                                       button);
    }

  g_free (value);
  g_free (utf8);
}

static void
gimp_prop_file_chooser_button_notify (GObject        *config,
                                      GParamSpec     *param_spec,
                                      GtkFileChooser *button)
{
  gchar *value;
  gchar *filename;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  g_signal_handlers_block_by_func (button,
                                   gimp_prop_file_chooser_button_callback,
                                   config);

  gtk_file_chooser_set_filename (button, filename);

  g_signal_handlers_unblock_by_func (button,
                                     gimp_prop_file_chooser_button_callback,
                                     config);

  g_free (filename);
}


/*****************/
/*  path editor  */
/*****************/

static void   gimp_prop_path_editor_path_callback     (GimpPathEditor *editor,
                                                       GObject        *config);
static void   gimp_prop_path_editor_writable_callback (GimpPathEditor *editor,
                                                       GObject        *config);
static void   gimp_prop_path_editor_path_notify       (GObject        *config,
                                                       GParamSpec     *param_spec,
                                                       GimpPathEditor *editor);
static void   gimp_prop_path_editor_writable_notify   (GObject        *config,
                                                       GParamSpec     *param_spec,
                                                       GimpPathEditor *editor);

GtkWidget *
gimp_prop_path_editor_new (GObject     *config,
                           const gchar *path_property_name,
                           const gchar *writable_property_name,
                           const gchar *filesel_title)
{
  GParamSpec *path_param_spec;
  GParamSpec *writable_param_spec = NULL;
  GtkWidget  *editor;
  gchar      *value;
  gchar      *filename;

  g_return_val_if_fail (G_IS_OBJECT (config), NULL);
  g_return_val_if_fail (path_property_name != NULL, NULL);

  path_param_spec = check_param_spec (config, path_property_name,
                                      GIMP_TYPE_PARAM_CONFIG_PATH, G_STRFUNC);
  if (! path_param_spec)
    return NULL;

  if (writable_property_name)
    {
      writable_param_spec = check_param_spec (config, writable_property_name,
                                              GIMP_TYPE_PARAM_CONFIG_PATH,
                                              G_STRFUNC);
      if (! writable_param_spec)
        return NULL;
    }

  g_object_get (config,
                path_property_name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  editor = gimp_path_editor_new (filesel_title, filename);
  g_free (filename);

  if (writable_property_name)
    {
      g_object_get (config,
                    writable_property_name, &value,
                    NULL);

      filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
      g_free (value);

      gimp_path_editor_set_writable_path (GIMP_PATH_EDITOR (editor), filename);
      g_free (filename);
    }

  g_object_set_data (G_OBJECT (editor), "gimp-config-param-spec-path",
                     path_param_spec);

  g_signal_connect (editor, "path-changed",
                    G_CALLBACK (gimp_prop_path_editor_path_callback),
                    config);

  connect_notify (config, path_property_name,
                  G_CALLBACK (gimp_prop_path_editor_path_notify),
                  editor);

  if (writable_property_name)
    {
      g_object_set_data (G_OBJECT (editor), "gimp-config-param-spec-writable",
                         writable_param_spec);

      g_signal_connect (editor, "writable-changed",
                        G_CALLBACK (gimp_prop_path_editor_writable_callback),
                        config);

      connect_notify (config, writable_property_name,
                      G_CALLBACK (gimp_prop_path_editor_writable_notify),
                      editor);
    }

  return editor;
}

static void
gimp_prop_path_editor_path_callback (GimpPathEditor *editor,
                                     GObject        *config)
{
  GParamSpec *path_param_spec;
  GParamSpec *writable_param_spec;
  gchar      *value;
  gchar      *utf8;

  path_param_spec     = g_object_get_data (G_OBJECT (editor),
                                           "gimp-config-param-spec-path");
  writable_param_spec = g_object_get_data (G_OBJECT (editor),
                                           "gimp-config-param-spec-writable");
  if (! path_param_spec)
    return;

  value = gimp_path_editor_get_path (editor);
  utf8 = g_filename_to_utf8 (value, -1, NULL, NULL, NULL);
  g_free (value);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_path_editor_path_notify,
                                   editor);

  g_object_set (config,
                path_param_spec->name, utf8,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_path_editor_path_notify,
                                     editor);

  g_free (utf8);

  if (writable_param_spec)
    {
      value = gimp_path_editor_get_writable_path (editor);
      utf8 = g_filename_to_utf8 (value, -1, NULL, NULL, NULL);
      g_free (value);

      g_signal_handlers_block_by_func (config,
                                       gimp_prop_path_editor_writable_notify,
                                       editor);

      g_object_set (config,
                    writable_param_spec->name, utf8,
                    NULL);

      g_signal_handlers_unblock_by_func (config,
                                         gimp_prop_path_editor_writable_notify,
                                         editor);

      g_free (utf8);
    }
}

static void
gimp_prop_path_editor_writable_callback (GimpPathEditor *editor,
                                         GObject        *config)
{
  GParamSpec *param_spec;
  gchar      *value;
  gchar      *utf8;

  param_spec = g_object_get_data (G_OBJECT (editor),
                                  "gimp-config-param-spec-writable");
  if (! param_spec)
    return;

  value = gimp_path_editor_get_writable_path (editor);
  utf8 = g_filename_to_utf8 (value, -1, NULL, NULL, NULL);
  g_free (value);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_path_editor_writable_notify,
                                   editor);

  g_object_set (config,
                param_spec->name, utf8,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_path_editor_writable_notify,
                                     editor);

  g_free (utf8);
}

static void
gimp_prop_path_editor_path_notify (GObject        *config,
                                   GParamSpec     *param_spec,
                                   GimpPathEditor *editor)
{
  gchar *value;
  gchar *filename;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  g_signal_handlers_block_by_func (editor,
                                   gimp_prop_path_editor_path_callback,
                                   config);

  gimp_path_editor_set_path (editor, filename);

  g_signal_handlers_unblock_by_func (editor,
                                     gimp_prop_path_editor_path_callback,
                                     config);

  g_free (filename);
}

static void
gimp_prop_path_editor_writable_notify (GObject        *config,
                                       GParamSpec     *param_spec,
                                       GimpPathEditor *editor)
{
  gchar *value;
  gchar *filename;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  filename = value ? gimp_config_path_expand (value, TRUE, NULL) : NULL;
  g_free (value);

  g_signal_handlers_block_by_func (editor,
                                   gimp_prop_path_editor_writable_callback,
                                   config);

  gimp_path_editor_set_writable_path (editor, filename);

  g_signal_handlers_unblock_by_func (editor,
                                     gimp_prop_path_editor_writable_callback,
                                     config);

  g_free (filename);
}


/***************/
/*  sizeentry  */
/***************/

static void   gimp_prop_size_entry_callback    (GimpSizeEntry *sizeentry,
                                                GObject       *config);
static void   gimp_prop_size_entry_notify      (GObject       *config,
                                                GParamSpec    *param_spec,
                                                GimpSizeEntry *sizeentry);
static void   gimp_prop_size_entry_notify_unit (GObject       *config,
                                                GParamSpec    *param_spec,
                                                GimpSizeEntry *sizeentry);


/**
 * gimp_prop_size_entry_new:
 * @config:             Object to which property is attached.
 * @property_name:      Name of int or double property.
 * @unit_property_name: Name of unit property.
 * @unit_format:        A printf-like unit-format string as is used with
 *                      gimp_unit_menu_new().
 * @update_policy:      How the automatic pixel <-> real-world-unit
 *                      calculations should be done.
 * @resolution:         The resolution (in dpi) for the field.
 *
 * Creates a #GimpSizeEntry to set and display the specified double or
 * int property, and its associated unit property.  Note that this
 * function is only suitable for creating a size entry holding a
 * single value.  Use gimp_prop_coordinates_new() to create a size
 * entry holding two values.
 *
 * Return value:  A new #GimpSizeEntry widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_size_entry_new (GObject                   *config,
                          const gchar               *property_name,
                          const gchar               *unit_property_name,
                          const gchar               *unit_format,
                          GimpSizeEntryUpdatePolicy  update_policy,
                          gdouble                    resolution)
{
  GtkWidget  *sizeentry;
  GParamSpec *param_spec;
  GParamSpec *unit_param_spec;
  gboolean    show_pixels;
  gdouble     value;
  gdouble     lower;
  gdouble     upper;
  GimpUnit    unit_value;

  param_spec = find_param_spec (config, property_name, G_STRFUNC);
  if (! param_spec)
    return NULL;

  if (! get_numeric_values (config,
                            param_spec, &value, &lower, &upper, G_STRFUNC))
    return NULL;

  if (unit_property_name)
    {
      GValue value = { 0 };

      unit_param_spec = check_param_spec (config, unit_property_name,
                                          GIMP_TYPE_PARAM_UNIT, G_STRFUNC);
      if (! unit_param_spec)
        return NULL;

      g_value_init (&value, unit_param_spec->value_type);
      g_value_set_int (&value, GIMP_UNIT_PIXEL);
      show_pixels =
        (g_param_value_validate (unit_param_spec, &value) == FALSE);
      g_value_unset (&value);

      g_object_get (config,
                    unit_property_name, &unit_value,
                    NULL);
    }
  else
    {
      unit_param_spec = NULL;
      unit_value      = GIMP_UNIT_INCH;
      show_pixels     = FALSE;
    }

  sizeentry = gimp_size_entry_new (1, unit_value, unit_format,
                                   TRUE, FALSE, FALSE,
                                   ceil (log (upper) / log (10) + 2),
                                   update_policy);
  gtk_table_set_col_spacing (GTK_TABLE (sizeentry), 1, 4);

  set_param_spec (NULL,
                  gimp_size_entry_get_help_widget (GIMP_SIZE_ENTRY (sizeentry),
                                                   0),
                  param_spec);

  if (unit_param_spec)
    set_param_spec (NULL,
                    GIMP_SIZE_ENTRY (sizeentry)->unitmenu, unit_param_spec);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sizeentry), unit_value);

  if (update_policy == GIMP_SIZE_ENTRY_UPDATE_SIZE)
    gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 0,
                                    resolution, FALSE);

  gimp_size_entry_set_value_boundaries (GIMP_SIZE_ENTRY (sizeentry), 0,
                                        lower, upper);

  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (sizeentry), 0, value);

  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec",
                     param_spec);

  g_signal_connect (sizeentry, "value-changed",
                    G_CALLBACK (gimp_prop_size_entry_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_size_entry_notify),
                  sizeentry);

  if (unit_property_name)
    {
      g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-unit",
                         unit_param_spec);

      g_signal_connect (sizeentry, "unit-changed",
                        G_CALLBACK (gimp_prop_size_entry_callback),
                        config);

      connect_notify (config, unit_property_name,
                      G_CALLBACK (gimp_prop_size_entry_notify_unit),
                      sizeentry);
    }

  return sizeentry;
}

static void
gimp_prop_size_entry_callback (GimpSizeEntry *sizeentry,
                               GObject       *config)
{
  GParamSpec *param_spec;
  GParamSpec *unit_param_spec;
  gdouble     value;
  GimpUnit    unit_value;

  param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                  "gimp-config-param-spec");
  if (! param_spec)
    return;

  unit_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                       "gimp-config-param-spec-unit");

  value      = gimp_size_entry_get_value (sizeentry, 0);
  unit_value = gimp_size_entry_get_unit (sizeentry);

  if (unit_param_spec)
    {
      GimpUnit  old_unit;

      g_object_get (config,
                    unit_param_spec->name, &old_unit,
                    NULL);

      if (unit_value == old_unit)
        unit_param_spec = NULL;
    }

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      g_object_set (config,
                    param_spec->name, ROUND (value),

                    unit_param_spec ?
                    unit_param_spec->name : NULL, unit_value,

                    NULL);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      g_object_set (config,
                    param_spec->name, value,

                    unit_param_spec ?
                    unit_param_spec->name : NULL, unit_value,

                    NULL);
    }
}

static void
gimp_prop_size_entry_notify (GObject       *config,
                             GParamSpec    *param_spec,
                             GimpSizeEntry *sizeentry)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }

  if (value != gimp_size_entry_get_value (sizeentry, 0))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_size_entry_callback,
                                       config);

      gimp_size_entry_set_value (sizeentry, 0, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_size_entry_callback,
                                         config);
    }
}

static void
gimp_prop_size_entry_notify_unit (GObject       *config,
                                  GParamSpec    *param_spec,
                                  GimpSizeEntry *sizeentry)
{
  GimpUnit value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (value != gimp_size_entry_get_unit (sizeentry))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_size_entry_callback,
                                       config);

      gimp_size_entry_set_unit (sizeentry, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_size_entry_callback,
                                         config);
    }
}


/*****************/
/*  coordinates  */
/*****************/

static void   gimp_prop_coordinates_callback    (GimpSizeEntry *sizeentry,
                                                 GObject       *config);
static void   gimp_prop_coordinates_notify_x    (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);
static void   gimp_prop_coordinates_notify_y    (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);
static void   gimp_prop_coordinates_notify_unit (GObject       *config,
                                                 GParamSpec    *param_spec,
                                                 GimpSizeEntry *sizeentry);


/**
 * gimp_prop_coordinates_new:
 * @config:             Object to which property is attached.
 * @x_property_name:    Name of int or double property for X coordinate.
 * @y_property_name:    Name of int or double property for Y coordinate.
 * @unit_property_name: Name of unit property.
 * @unit_format:        A printf-like unit-format string as is used with
 *                      gimp_unit_menu_new().
 * @update_policy:      How the automatic pixel <-> real-world-unit
 *                      calculations should be done.
 * @xresolution:        The resolution (in dpi) for the X coordinate.
 * @yresolution:        The resolution (in dpi) for the Y coordinate.
 * @has_chainbutton:    Whether to add a chainbutton to the size entry.
 *
 * Creates a #GimpSizeEntry to set and display two double or int
 * properties, which will usually represent X and Y coordinates, and
 * their associated unit property.
 *
 * Return value:  A new #GimpSizeEntry widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_coordinates_new (GObject                   *config,
                           const gchar               *x_property_name,
                           const gchar               *y_property_name,
                           const gchar               *unit_property_name,
                           const gchar               *unit_format,
                           GimpSizeEntryUpdatePolicy  update_policy,
                           gdouble                    xresolution,
                           gdouble                    yresolution,
                           gboolean                   has_chainbutton)
{
  GtkWidget *sizeentry;
  GtkWidget *chainbutton = NULL;

  sizeentry = gimp_size_entry_new (2, GIMP_UNIT_INCH, unit_format,
                                   FALSE, FALSE, TRUE, 10,
                                   update_policy);

  if (has_chainbutton)
    {
      chainbutton = gimp_chain_button_new (GIMP_CHAIN_BOTTOM);
      gtk_table_attach_defaults (GTK_TABLE (sizeentry), chainbutton,
                                 1, 3, 3, 4);
      gtk_widget_show (chainbutton);
    }

  if (! gimp_prop_coordinates_connect (config,
                                       x_property_name,
                                       y_property_name,
                                       unit_property_name,
                                       sizeentry,
                                       chainbutton,
                                       xresolution,
                                       yresolution))
    {
      gtk_widget_destroy (sizeentry);
      return NULL;
    }

  return sizeentry;
}

gboolean
gimp_prop_coordinates_connect (GObject     *config,
                               const gchar *x_property_name,
                               const gchar *y_property_name,
                               const gchar *unit_property_name,
                               GtkWidget   *sizeentry,
                               GtkWidget   *chainbutton,
                               gdouble      xresolution,
                               gdouble      yresolution)
{
  GParamSpec *x_param_spec;
  GParamSpec *y_param_spec;
  GParamSpec *unit_param_spec;
  gdouble     x_value, x_lower, x_upper;
  gdouble     y_value, y_lower, y_upper;
  GimpUnit    unit_value;
  gdouble    *old_x_value;
  gdouble    *old_y_value;
  GimpUnit   *old_unit_value;
  gboolean    chain_checked;

  g_return_val_if_fail (GIMP_IS_SIZE_ENTRY (sizeentry), FALSE);
  g_return_val_if_fail (GIMP_SIZE_ENTRY (sizeentry)->number_of_fields == 2,
                        FALSE);
  g_return_val_if_fail (chainbutton == NULL ||
                        GIMP_IS_CHAIN_BUTTON (chainbutton), FALSE);

  x_param_spec = find_param_spec (config, x_property_name, G_STRFUNC);
  if (! x_param_spec)
    return FALSE;

  y_param_spec = find_param_spec (config, y_property_name, G_STRFUNC);
  if (! y_param_spec)
    return FALSE;

  if (! get_numeric_values (config, x_param_spec,
                            &x_value, &x_lower, &x_upper, G_STRFUNC) ||
      ! get_numeric_values (config, y_param_spec,
                            &y_value, &y_lower, &y_upper, G_STRFUNC))
    return FALSE;

  if (unit_property_name)
    {
      unit_param_spec = check_param_spec (config, unit_property_name,
                                          GIMP_TYPE_PARAM_UNIT, G_STRFUNC);
      if (! unit_param_spec)
        return FALSE;

      g_object_get (config,
                    unit_property_name, &unit_value,
                    NULL);
    }
  else
    {
      unit_param_spec = NULL;
      unit_value      = GIMP_UNIT_INCH;
    }

  set_param_spec (NULL,
                  gimp_size_entry_get_help_widget (GIMP_SIZE_ENTRY (sizeentry),
                                                   0),
                  x_param_spec);
  set_param_spec (NULL,
                  gimp_size_entry_get_help_widget (GIMP_SIZE_ENTRY (sizeentry),
                                                   1),
                  y_param_spec);

  if (unit_param_spec)
    set_param_spec (NULL,
                    GIMP_SIZE_ENTRY (sizeentry)->unitmenu, unit_param_spec);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (sizeentry), unit_value);

  switch (GIMP_SIZE_ENTRY (sizeentry)->update_policy)
    {
    case GIMP_SIZE_ENTRY_UPDATE_SIZE:
      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 0,
                                      xresolution, FALSE);
      gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (sizeentry), 1,
                                      yresolution, FALSE);
      chain_checked = (ABS (x_value - y_value) < 1);
      break;

    case GIMP_SIZE_ENTRY_UPDATE_RESOLUTION:
      chain_checked = (ABS (x_value - y_value) < GIMP_MIN_RESOLUTION);
      break;

    default:
      chain_checked = (x_value == y_value);
      break;
    }

  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 0,
                                         x_lower, x_upper);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (sizeentry), 1,
                                         y_lower, y_upper);

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 0, x_value);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (sizeentry), 1, y_value);

  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-x",
                     x_param_spec);
  g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-y",
                     y_param_spec);

  old_x_value  = g_new0 (gdouble, 1);
  *old_x_value = x_value;
  g_object_set_data_full (G_OBJECT (sizeentry), "old-x-value",
                          old_x_value,
                          (GDestroyNotify) g_free);

  old_y_value  = g_new0 (gdouble, 1);
  *old_y_value = y_value;
  g_object_set_data_full (G_OBJECT (sizeentry), "old-y-value",
                          old_y_value,
                          (GDestroyNotify) g_free);

  if (chainbutton)
    {
      if (chain_checked)
        gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (chainbutton), TRUE);

      g_object_set_data (G_OBJECT (sizeentry), "chainbutton", chainbutton);
    }

  g_signal_connect (sizeentry, "value-changed",
                    G_CALLBACK (gimp_prop_coordinates_callback),
                    config);
  g_signal_connect (sizeentry, "refval-changed",
                    G_CALLBACK (gimp_prop_coordinates_callback),
                    config);

  connect_notify (config, x_property_name,
                  G_CALLBACK (gimp_prop_coordinates_notify_x),
                  sizeentry);
  connect_notify (config, y_property_name,
                  G_CALLBACK (gimp_prop_coordinates_notify_y),
                  sizeentry);

  if (unit_property_name)
    {
      g_object_set_data (G_OBJECT (sizeentry), "gimp-config-param-spec-unit",
                         unit_param_spec);

      old_unit_value  = g_new0 (GimpUnit, 1);
      *old_unit_value = unit_value;
      g_object_set_data_full (G_OBJECT (sizeentry), "old-unit-value",
                              old_unit_value,
                              (GDestroyNotify) g_free);

      g_signal_connect (sizeentry, "unit-changed",
                        G_CALLBACK (gimp_prop_coordinates_callback),
                        config);

      connect_notify (config, unit_property_name,
                      G_CALLBACK (gimp_prop_coordinates_notify_unit),
                      sizeentry);
    }

  return TRUE;
}

static void
gimp_prop_coordinates_callback (GimpSizeEntry *sizeentry,
                                GObject       *config)
{
  GParamSpec *x_param_spec;
  GParamSpec *y_param_spec;
  GParamSpec *unit_param_spec;
  gdouble     x_value;
  gdouble     y_value;
  GimpUnit    unit_value;
  gdouble    *old_x_value;
  gdouble    *old_y_value;
  GimpUnit   *old_unit_value;
  gboolean    backwards;

  x_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                    "gimp-config-param-spec-x");
  y_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                    "gimp-config-param-spec-y");

  if (! x_param_spec || ! y_param_spec)
    return;

  unit_param_spec = g_object_get_data (G_OBJECT (sizeentry),
                                       "gimp-config-param-spec-unit");

  x_value    = gimp_size_entry_get_refval (sizeentry, 0);
  y_value    = gimp_size_entry_get_refval (sizeentry, 1);
  unit_value = gimp_size_entry_get_unit (sizeentry);

  old_x_value    = g_object_get_data (G_OBJECT (sizeentry), "old-x-value");
  old_y_value    = g_object_get_data (G_OBJECT (sizeentry), "old-y-value");
  old_unit_value = g_object_get_data (G_OBJECT (sizeentry), "old-unit-value");

  if (! old_x_value || ! old_y_value || (unit_param_spec && ! old_unit_value))
    return;

  /*
   * FIXME: if the entry was created using gimp_coordinates_new, then
   * the chain button is handled automatically and the following block
   * of code is unnecessary (and, in fact, redundant).
   */
  if (x_value != y_value)
    {
      GtkWidget *chainbutton;

      chainbutton = g_object_get_data (G_OBJECT (sizeentry), "chainbutton");

      if (chainbutton &&
          gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (chainbutton)) &&
          ! g_object_get_data (G_OBJECT (chainbutton), "constrains-ratio"))
        {
          if (x_value != *old_x_value)
            y_value = x_value;
          else if (y_value != *old_y_value)
            x_value = y_value;
        }
    }

  backwards = (*old_x_value == x_value);

  if (*old_x_value == x_value &&
      *old_y_value == y_value &&
      (old_unit_value == NULL || *old_unit_value == unit_value))
    return;

  *old_x_value = x_value;
  *old_y_value = y_value;

  if (old_unit_value)
    *old_unit_value = unit_value;

  if (unit_param_spec)
    g_object_set (config,
                  unit_param_spec->name, unit_value,
                  NULL);

  if (G_IS_PARAM_SPEC_INT (x_param_spec) &&
      G_IS_PARAM_SPEC_INT (y_param_spec))
    {
      if (backwards)
        g_object_set (config,
                      y_param_spec->name, ROUND (y_value),
                      x_param_spec->name, ROUND (x_value),
                      NULL);
      else
        g_object_set (config,
                      x_param_spec->name, ROUND (x_value),
                      y_param_spec->name, ROUND (y_value),
                      NULL);

    }
  else if (G_IS_PARAM_SPEC_DOUBLE (x_param_spec) &&
           G_IS_PARAM_SPEC_DOUBLE (y_param_spec))
    {
      if (backwards)
        g_object_set (config,
                      y_param_spec->name, y_value,
                      x_param_spec->name, x_value,
                      NULL);
      else
        g_object_set (config,
                      x_param_spec->name, x_value,
                      y_param_spec->name, y_value,
                      NULL);
    }
}

static void
gimp_prop_coordinates_notify_x (GObject       *config,
                                GParamSpec    *param_spec,
                                GimpSizeEntry *sizeentry)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }

  if (value != gimp_size_entry_get_refval (sizeentry, 0))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_refval (sizeentry, 0, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}

static void
gimp_prop_coordinates_notify_y (GObject       *config,
                                GParamSpec    *param_spec,
                                GimpSizeEntry *sizeentry)
{
  gdouble value;

  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      gint int_value;

      g_object_get (config,
                    param_spec->name, &int_value,
                    NULL);

      value = int_value;
    }
  else
    {
      g_object_get (config,
                    param_spec->name, &value,
                    NULL);
    }

  if (value != gimp_size_entry_get_refval (sizeentry, 1))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_refval (sizeentry, 1, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}

static void
gimp_prop_coordinates_notify_unit (GObject       *config,
                                   GParamSpec    *param_spec,
                                   GimpSizeEntry *sizeentry)
{
  GimpUnit value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (value != gimp_size_entry_get_unit (sizeentry))
    {
      g_signal_handlers_block_by_func (sizeentry,
                                       gimp_prop_coordinates_callback,
                                       config);

      gimp_size_entry_set_unit (sizeentry, value);

      g_signal_handlers_unblock_by_func (sizeentry,
                                         gimp_prop_coordinates_callback,
                                         config);
    }
}


/****************/
/*  color area  */
/****************/

static void   gimp_prop_color_area_callback (GtkWidget  *widget,
                                             GObject    *config);
static void   gimp_prop_color_area_notify   (GObject    *config,
                                             GParamSpec *param_spec,
                                             GtkWidget  *area);

/**
 * gimp_prop_color_area_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of RGB property.
 * @width:         Width of color area.
 * @height:        Height of color area.
 * @type:          How transparency is represented.
 *
 * Creates a #GimpColorArea to set and display the value of an RGB
 * property.
 *
 * Return value:  A new #GimpColorArea widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_color_area_new (GObject           *config,
                          const gchar       *property_name,
                          gint               width,
                          gint               height,
                          GimpColorAreaType  type)
{
  GParamSpec *param_spec;
  GtkWidget  *area;
  GimpRGB    *value;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_RGB, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  area = gimp_color_area_new (value, type,
                              GDK_BUTTON1_MASK | GDK_BUTTON2_MASK);
  gtk_widget_set_size_request (area, width, height);

  g_free (value);

  set_param_spec (G_OBJECT (area), area, param_spec);

  g_signal_connect (area, "color-changed",
                    G_CALLBACK (gimp_prop_color_area_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_color_area_notify),
                  area);

  return area;
}

static void
gimp_prop_color_area_callback (GtkWidget *area,
                               GObject   *config)
{
  GParamSpec *param_spec;
  GimpRGB     value;

  param_spec = get_param_spec (G_OBJECT (area));
  if (! param_spec)
    return;

  gimp_color_area_get_color (GIMP_COLOR_AREA (area), &value);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_color_area_notify,
                                   area);

  g_object_set (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_color_area_notify,
                                     area);
}

static void
gimp_prop_color_area_notify (GObject    *config,
                             GParamSpec *param_spec,
                             GtkWidget  *area)
{
  GimpRGB *value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  g_signal_handlers_block_by_func (area,
                                   gimp_prop_color_area_callback,
                                   config);

  gimp_color_area_set_color (GIMP_COLOR_AREA (area), value);

  g_free (value);

  g_signal_handlers_unblock_by_func (area,
                                     gimp_prop_color_area_callback,
                                     config);
}


/***************/
/*  unit menu  */
/***************/

static void   gimp_prop_unit_menu_callback (GtkWidget  *menu,
                                            GObject    *config);
static void   gimp_prop_unit_menu_notify   (GObject    *config,
                                            GParamSpec *param_spec,
                                            GtkWidget  *menu);

/**
 * gimp_prop_unit_menu_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of Unit property.
 * @unit_format:   A printf-like format string which is used to create
 *                 the unit strings.
 *
 * Creates a #GimpUnitMenu to set and display the value of a Unit
 * property.  See gimp_unit_menu_new() for more information.
 *
 * Return value:  A new #GimpUnitMenu widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_unit_menu_new (GObject     *config,
                         const gchar *property_name,
                         const gchar *unit_format)
{
  GParamSpec *param_spec;
  GtkWidget  *menu;
  GimpUnit    unit;
  GValue      value = { 0, };
  gboolean    show_pixels;
  gboolean    show_percent;

  param_spec = check_param_spec (config, property_name,
                                 GIMP_TYPE_PARAM_UNIT, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_value_init (&value, param_spec->value_type);

  g_value_set_int (&value, GIMP_UNIT_PIXEL);
  show_pixels = (g_param_value_validate (param_spec, &value) == FALSE);

  g_value_set_int (&value, GIMP_UNIT_PERCENT);
  show_percent = (g_param_value_validate (param_spec, &value) == FALSE);

  g_value_unset (&value);

  g_object_get (config,
                property_name, &unit,
                NULL);

  menu = gimp_unit_menu_new (unit_format,
                             unit, show_pixels, show_percent, TRUE);

  set_param_spec (G_OBJECT (menu), menu, param_spec);

  g_signal_connect (menu, "unit-changed",
                    G_CALLBACK (gimp_prop_unit_menu_callback),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_unit_menu_notify),
                  menu);

  return menu;
}

static void
gimp_prop_unit_menu_callback (GtkWidget *menu,
                              GObject   *config)
{
  GParamSpec *param_spec;
  GimpUnit    unit;

  param_spec = get_param_spec (G_OBJECT (menu));
  if (! param_spec)
    return;

  gimp_unit_menu_update (menu, &unit);

  g_signal_handlers_block_by_func (config,
                                   gimp_prop_unit_menu_notify,
                                   menu);

  g_object_set (config,
                param_spec->name, unit,
                NULL);

  g_signal_handlers_unblock_by_func (config,
                                     gimp_prop_unit_menu_notify,
                                     menu);
}

static void
gimp_prop_unit_menu_notify (GObject    *config,
                            GParamSpec *param_spec,
                            GtkWidget  *menu)
{
  GimpUnit  unit;

  g_object_get (config,
                param_spec->name, &unit,
                NULL);

  g_signal_handlers_block_by_func (menu,
                                   gimp_prop_unit_menu_callback,
                                   config);

  gimp_unit_menu_set_unit (GIMP_UNIT_MENU (menu), unit);
  gimp_unit_menu_update (menu, &unit);

  g_signal_handlers_unblock_by_func (menu,
                                     gimp_prop_unit_menu_callback,
                                     config);
}


/*****************/
/*  stock image  */
/*****************/

static void   gimp_prop_stock_image_notify (GObject    *config,
                                            GParamSpec *param_spec,
                                            GtkWidget  *image);

/**
 * gimp_prop_stock_image_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of string property.
 * @icon_size:     Size of desired stock image.
 *
 * Creates a widget to display a stock image representing the value of the
 * specified string property, which should encode a Stock ID.
 * See gtk_image_new_from_stock() for more information.
 *
 * Return value:  A new #GtkImage widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_stock_image_new (GObject     *config,
                           const gchar *property_name,
                           GtkIconSize  icon_size)
{
  GParamSpec *param_spec;
  GtkWidget  *image;
  gchar      *stock_id;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_STRING, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &stock_id,
                NULL);

  image = gtk_image_new_from_stock (stock_id, icon_size);

  if (stock_id)
    g_free (stock_id);

  set_param_spec (G_OBJECT (image), image, param_spec);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_stock_image_notify),
                  image);

  return image;
}

static void
gimp_prop_stock_image_notify (GObject    *config,
                              GParamSpec *param_spec,
                              GtkWidget  *image)
{
  gchar *stock_id;

  g_object_get (config,
                param_spec->name, &stock_id,
                NULL);

  gtk_image_set_from_stock (GTK_IMAGE (image), stock_id,
                            GTK_IMAGE (image)->icon_size);

  if (stock_id)
    g_free (stock_id);
}


/**************/
/*  expander  */
/**************/

static void   gimp_prop_expanded_notify (GtkExpander *expander,
                                         GParamSpec  *param_spec,
                                         GObject     *config);
static void   gimp_prop_expander_notify (GObject     *config,
                                         GParamSpec  *param_spec,
                                         GtkExpander *expander);


/**
 * gimp_prop_expander_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of boolean property.
 * @label:         Label for expander.
 *
 * Creates a #GtkExpander controlled by the specified boolean property.
 * A value of %TRUE for the property corresponds to the expanded state
 * for the widget.
 *
 * Return value:  A new #GtkExpander widget.
 *
 * Since GIMP 2.4
 */
GtkWidget *
gimp_prop_expander_new (GObject     *config,
                        const gchar *property_name,
                        const gchar *label)
{
  GParamSpec *param_spec;
  GtkWidget  *expander;
  gboolean    value;

  param_spec = check_param_spec (config, property_name,
                                 G_TYPE_PARAM_BOOLEAN, G_STRFUNC);
  if (! param_spec)
    return NULL;

  g_object_get (config,
                property_name, &value,
                NULL);

  expander = g_object_new (GTK_TYPE_EXPANDER,
                           "label",    label,
                           "expanded", value,
                           NULL);

  set_param_spec (G_OBJECT (expander), expander, param_spec);

  g_signal_connect (expander, "notify::expanded",
                    G_CALLBACK (gimp_prop_expanded_notify),
                    config);

  connect_notify (config, property_name,
                  G_CALLBACK (gimp_prop_expander_notify),
                  expander);

  return expander;
}

static void
gimp_prop_expanded_notify (GtkExpander *expander,
                           GParamSpec  *param_spec,
                           GObject     *config)
{
  param_spec = get_param_spec (G_OBJECT (expander));
  if (! param_spec)
    return;

  g_object_set (config,
                param_spec->name, gtk_expander_get_expanded (expander),
                NULL);
}

static void
gimp_prop_expander_notify (GObject     *config,
                           GParamSpec  *param_spec,
                           GtkExpander *expander)
{
  gboolean value;

  g_object_get (config,
                param_spec->name, &value,
                NULL);

  if (gtk_expander_get_expanded (expander) != value)
    {
      g_signal_handlers_block_by_func (expander,
                                       gimp_prop_expanded_notify,
                                       config);

      gtk_expander_set_expanded (expander, value);

      g_signal_handlers_unblock_by_func (expander,
                                         gimp_prop_expanded_notify,
                                         config);
    }
}


/*******************************/
/*  private utility functions  */
/*******************************/

static GQuark param_spec_quark = 0;

static void
set_param_spec (GObject     *object,
                GtkWidget   *widget,
                GParamSpec  *param_spec)
{
  if (object)
    {
      if (! param_spec_quark)
        param_spec_quark = g_quark_from_static_string ("gimp-config-param-spec");

      g_object_set_qdata (object, param_spec_quark, param_spec);
    }

  if (widget)
    {
      const gchar *blurb = g_param_spec_get_blurb (param_spec);

      if (blurb)
        {
          const gchar *domain;

          domain = gimp_type_get_translation_domain (param_spec->owner_type);
          gimp_help_set_help_data (widget, dgettext (domain, blurb), NULL);
        }
    }
}

static void
set_radio_spec (GObject    *object,
                GParamSpec *param_spec)
{
  GSList *list;

  for (list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (object));
       list;
       list = g_slist_next (list))
    {
      set_param_spec (list->data, NULL, param_spec);
    }
}

static GParamSpec *
get_param_spec (GObject *object)
{
  if (! param_spec_quark)
    param_spec_quark = g_quark_from_static_string ("gimp-config-param-spec");

  return g_object_get_qdata (object, param_spec_quark);
}

static GParamSpec *
find_param_spec (GObject     *object,
                 const gchar *property_name,
                 const gchar *strloc)
{
  GParamSpec *param_spec;

  param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                             property_name);

  if (! param_spec)
    g_warning ("%s: %s has no property named '%s'",
               strloc,
               g_type_name (G_TYPE_FROM_INSTANCE (object)),
               property_name);

  return param_spec;
}

static GParamSpec *
check_param_spec (GObject     *object,
                  const gchar *property_name,
                  GType        type,
                  const gchar *strloc)
{
  GParamSpec *param_spec;

  param_spec = find_param_spec (object, property_name, strloc);

  if (param_spec && ! g_type_is_a (G_TYPE_FROM_INSTANCE (param_spec), type))
    {
      g_warning ("%s: property '%s' of %s is not a %s",
                 strloc,
                 param_spec->name,
                 g_type_name (param_spec->owner_type),
                 g_type_name (type));
      return NULL;
    }

  return param_spec;
}

static gboolean
get_numeric_values (GObject     *object,
                    GParamSpec  *param_spec,
                    gdouble     *value,
                    gdouble     *lower,
                    gdouble     *upper,
                    const gchar *strloc)
{
  if (G_IS_PARAM_SPEC_INT (param_spec))
    {
      GParamSpecInt *int_spec = G_PARAM_SPEC_INT (param_spec);
      gint           int_value;

      g_object_get (object, param_spec->name, &int_value, NULL);

      *value = int_value;
      *lower = int_spec->minimum;
      *upper = int_spec->maximum;
    }
  else if (G_IS_PARAM_SPEC_UINT (param_spec))
    {
      GParamSpecUInt *uint_spec = G_PARAM_SPEC_UINT (param_spec);
      guint           uint_value;

      g_object_get (object, param_spec->name, &uint_value, NULL);

      *value = uint_value;
      *lower = uint_spec->minimum;
      *upper = uint_spec->maximum;
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (param_spec))
    {
      GParamSpecDouble *double_spec = G_PARAM_SPEC_DOUBLE (param_spec);

      g_object_get (object, param_spec->name, value, NULL);

      *lower = double_spec->minimum;
      *upper = double_spec->maximum;
    }
  else
    {
      g_warning ("%s: property '%s' of %s is not numeric",
                 strloc,
                 param_spec->name,
                 g_type_name (G_TYPE_FROM_INSTANCE (object)));
      return FALSE;
    }

  return TRUE;
}

static void
connect_notify (GObject     *config,
                const gchar *property_name,
                GCallback    callback,
                gpointer     callback_data)
{
  gchar *notify_name;

  notify_name = g_strconcat ("notify::", property_name, NULL);

  g_signal_connect_object (config, notify_name, callback, callback_data, 0);

  g_free (notify_name);
}