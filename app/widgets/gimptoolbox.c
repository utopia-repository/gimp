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

#include <string.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimplist.h"
#include "core/gimptoolinfo.h"

#include "gimpdevices.h"
#include "gimpdialogfactory.h"
#include "gimphelp-ids.h"
#include "gimptoolbox.h"
#include "gimptoolbox-color-area.h"
#include "gimptoolbox-dnd.h"
#include "gimptoolbox-image-area.h"
#include "gimptoolbox-indicator-area.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"
#include "gtkhwrapbox.h"

#include "gimp-intl.h"


#define DEFAULT_TOOL_ICON_SIZE GTK_ICON_SIZE_BUTTON
#define DEFAULT_BUTTON_RELIEF  GTK_RELIEF_NONE

#define TOOL_BUTTON_DATA_KEY   "gimp-tool-button"
#define TOOL_INFO_DATA_KEY     "gimp-tool-info"


/*  local function prototypes  */

static void        gimp_toolbox_class_init       (GimpToolboxClass *klass);
static void        gimp_toolbox_init             (GimpToolbox      *toolbox);

static GObject   * gimp_toolbox_constructor      (GType           type,
                                                  guint           n_params,
                                                  GObjectConstructParam *params);

static gboolean    gimp_toolbox_delete_event     (GtkWidget      *widget,
                                                  GdkEventAny    *event);
static void        gimp_toolbox_size_allocate    (GtkWidget      *widget,
                                                  GtkAllocation  *allocation);
static void        gimp_toolbox_style_set        (GtkWidget      *widget,
                                                  GtkStyle       *previous_style);
static void        gimp_toolbox_book_added       (GimpDock       *dock,
                                                  GimpDockbook   *dockbook);
static void        gimp_toolbox_book_removed     (GimpDock       *dock,
                                                  GimpDockbook   *dockbook);
static void        gimp_toolbox_set_geometry     (GimpToolbox    *toolbox);

static void        toolbox_create_tools          (GimpToolbox    *toolbox,
                                                  GimpContext    *context);
static GtkWidget * toolbox_create_color_area     (GimpToolbox    *toolbox,
                                                  GimpContext    *context);
static GtkWidget * toolbox_create_foo_area       (GimpToolbox    *toolbox,
                                                  GimpContext    *context);
static GtkWidget * toolbox_create_image_area     (GimpToolbox    *toolbox,
                                                  GimpContext    *context);

static void        toolbox_area_notify           (GimpGuiConfig  *config,
                                                  GParamSpec     *pspec,
                                                  GtkWidget      *area);
static void        toolbox_tool_changed          (GimpContext    *context,
                                                  GimpToolInfo   *tool_info,
                                                  gpointer        data);

static void        toolbox_tool_reorder          (GimpContainer  *container,
                                                  GimpToolInfo   *tool_info,
                                                  gint            index,
                                                  GtkWidget      *wrap_box);
static void        toolbox_tool_visible_notify   (GimpToolInfo   *tool_info,
                                                  GParamSpec     *pspec,
                                                  GtkWidget      *button);

static void        toolbox_tool_button_toggled   (GtkWidget      *widget,
                                                  GimpToolInfo   *tool_info);
static gboolean    toolbox_tool_button_press     (GtkWidget      *widget,
                                                  GdkEventButton *bevent,
                                                  GimpToolbox    *toolbox);

static gboolean    toolbox_check_device          (GtkWidget      *widget,
                                                  GdkEvent       *event,
                                                  Gimp           *gimp);


/*  local variables  */

static GimpDockClass *parent_class = NULL;


GType
gimp_toolbox_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GimpToolboxClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_toolbox_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpToolbox),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_toolbox_init,
      };

      type = g_type_register_static (GIMP_TYPE_DOCK,
                                     "GimpToolbox",
                                     &type_info, 0);
    }

  return type;
}

static void
gimp_toolbox_class_init (GimpToolboxClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GimpDockClass  *dock_class   = GIMP_DOCK_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructor   = gimp_toolbox_constructor;

  widget_class->delete_event  = gimp_toolbox_delete_event;
  widget_class->size_allocate = gimp_toolbox_size_allocate;
  widget_class->style_set     = gimp_toolbox_style_set;

  dock_class->book_added      = gimp_toolbox_book_added;
  dock_class->book_removed    = gimp_toolbox_book_removed;

  gtk_widget_class_install_style_property
    (widget_class, g_param_spec_enum ("tool_icon_size",
                                      NULL, NULL,
                                      GTK_TYPE_ICON_SIZE,
                                      DEFAULT_TOOL_ICON_SIZE,
                                      G_PARAM_READABLE));

  gtk_widget_class_install_style_property
    (widget_class, g_param_spec_enum ("button_relief",
                                      NULL, NULL,
                                      GTK_TYPE_RELIEF_STYLE,
                                      DEFAULT_BUTTON_RELIEF,
                                      G_PARAM_READABLE));
}

static void
gimp_toolbox_init (GimpToolbox *toolbox)
{
  gtk_window_set_role (GTK_WINDOW (toolbox), "gimp-toolbox");

  gimp_help_connect (GTK_WIDGET (toolbox), gimp_standard_help_func,
                     GIMP_HELP_TOOLBOX, NULL);
}

static GObject *
gimp_toolbox_constructor (GType                  type,
                          guint                  n_params,
                          GObjectConstructParam *params)
{
  GObject       *object;
  GimpToolbox   *toolbox;
  GimpContext   *context;
  GimpGuiConfig *config;
  GimpUIManager *manager;
  GtkWidget     *main_vbox;
  GtkWidget     *vbox;
  GdkDisplay    *display;
  GList         *list;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  toolbox = GIMP_TOOLBOX (object);

  context = GIMP_DOCK (toolbox)->context;
  config  = GIMP_GUI_CONFIG (context->gimp->config);

  gimp_window_set_hint (GTK_WINDOW (toolbox), config->toolbox_window_hint);

  main_vbox = GIMP_DOCK (toolbox)->main_vbox;

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (main_vbox), vbox, 0);
  gtk_widget_show (vbox);

  manager = gimp_ui_managers_from_name ("<Image>")->data;

  toolbox->menu_bar = gimp_ui_manager_ui_get (manager, "/toolbox-menubar");

  if (toolbox->menu_bar)
    {
      gtk_box_pack_start (GTK_BOX (vbox), toolbox->menu_bar, FALSE, FALSE, 0);
      gtk_widget_show (toolbox->menu_bar);
    }

  gtk_window_add_accel_group (GTK_WINDOW (toolbox),
                              gtk_ui_manager_get_accel_group (GTK_UI_MANAGER (manager)));

  toolbox->tool_wbox = gtk_hwrap_box_new (FALSE);
  gtk_wrap_box_set_justify (GTK_WRAP_BOX (toolbox->tool_wbox), GTK_JUSTIFY_TOP);
  gtk_wrap_box_set_line_justify (GTK_WRAP_BOX (toolbox->tool_wbox),
                                 GTK_JUSTIFY_LEFT);
  gtk_wrap_box_set_aspect_ratio (GTK_WRAP_BOX (toolbox->tool_wbox), 5.0 / 6.0);

  gtk_box_pack_start (GTK_BOX (vbox), toolbox->tool_wbox, FALSE, FALSE, 0);
  gtk_widget_show (toolbox->tool_wbox);

  toolbox->area_wbox = gtk_hwrap_box_new (FALSE);
  gtk_wrap_box_set_justify (GTK_WRAP_BOX (toolbox->area_wbox), GTK_JUSTIFY_TOP);
  gtk_wrap_box_set_line_justify (GTK_WRAP_BOX (toolbox->area_wbox),
                                 GTK_JUSTIFY_LEFT);
  gtk_wrap_box_set_aspect_ratio (GTK_WRAP_BOX (toolbox->area_wbox), 5.0 / 6.0);

  gtk_box_pack_start (GTK_BOX (vbox), toolbox->area_wbox, FALSE, FALSE, 0);
  gtk_widget_show (toolbox->area_wbox);

  /* We need to know when the current device changes, so we can update
   * the correct tool - to do this we connect to motion events.
   * We can't just use EXTENSION_EVENTS_CURSOR though, since that
   * would get us extension events for the mouse pointer, and our
   * device would change to that and not change back. So we check
   * manually that all devices have a cursor, before establishing the check.
   */
  display = gtk_widget_get_display (GTK_WIDGET (toolbox));
  for (list = gdk_display_list_devices (display); list; list = list->next)
    {
      if (! ((GdkDevice *) (list->data))->has_cursor)
	break;
    }

  if (! list)  /* all devices have cursor */
    {
      g_signal_connect (toolbox, "motion_notify_event",
			G_CALLBACK (toolbox_check_device),
			context->gimp);

      gtk_widget_add_events (GTK_WIDGET (toolbox), GDK_POINTER_MOTION_MASK);
      gtk_widget_set_extension_events (GTK_WIDGET (toolbox),
                                       GDK_EXTENSION_EVENTS_CURSOR);
    }

  toolbox_create_tools (toolbox, context);

  toolbox->color_area = toolbox_create_color_area (toolbox, context);
  gtk_wrap_box_pack_wrapped (GTK_WRAP_BOX (toolbox->area_wbox),
                             toolbox->color_area,
                             TRUE, TRUE, FALSE, TRUE, TRUE);
  if (config->toolbox_color_area)
    gtk_widget_show (toolbox->color_area);

  g_signal_connect_object (config, "notify::toolbox-color-area",
                           G_CALLBACK (toolbox_area_notify),
                           toolbox->color_area, 0);

  toolbox->foo_area = toolbox_create_foo_area (toolbox, context);
  gtk_wrap_box_pack (GTK_WRAP_BOX (toolbox->area_wbox), toolbox->foo_area,
                     TRUE, TRUE, FALSE, TRUE);
  if (config->toolbox_foo_area)
    gtk_widget_show (toolbox->foo_area);

  g_signal_connect_object (config, "notify::toolbox-foo-area",
                           G_CALLBACK (toolbox_area_notify),
                           toolbox->foo_area, 0);

  toolbox->image_area = toolbox_create_image_area (toolbox, context);
  gtk_wrap_box_pack (GTK_WRAP_BOX (toolbox->area_wbox), toolbox->image_area,
                     TRUE, TRUE, FALSE, TRUE);
  if (config->toolbox_image_area)
    gtk_widget_show (toolbox->image_area);

  g_signal_connect_object (config, "notify::toolbox-image-area",
                           G_CALLBACK (toolbox_area_notify),
                           toolbox->image_area, 0);

  g_signal_connect_object (context, "tool-changed",
			   G_CALLBACK (toolbox_tool_changed),
			   toolbox->tool_wbox,
			   0);

  gimp_toolbox_dnd_init (GIMP_TOOLBOX (toolbox));

  gimp_toolbox_style_set (GTK_WIDGET (toolbox), GTK_WIDGET (toolbox)->style);

  return object;
}

static gboolean
gimp_toolbox_delete_event (GtkWidget   *widget,
                           GdkEventAny *event)
{
  gimp_exit (GIMP_DOCK (widget)->context->gimp, FALSE);

  return TRUE;
}

static void
gimp_toolbox_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GimpToolbox   *toolbox = GIMP_TOOLBOX (widget);
  Gimp          *gimp;
  GimpGuiConfig *config;
  GimpToolInfo  *tool_info;
  GtkWidget     *tool_button;

  if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  if (! GIMP_DOCK (widget)->context)
    return;

  gimp = GIMP_DOCK (widget)->context->gimp;

  config = GIMP_GUI_CONFIG (gimp->config);

  tool_info = (GimpToolInfo *)
    gimp_container_get_child_by_name (gimp->tool_info_list,
                                      "gimp-rect-select-tool");
  tool_button = g_object_get_data (G_OBJECT (tool_info), TOOL_BUTTON_DATA_KEY);

  if (tool_button)
    {
      GtkRequisition  button_requisition;
      GList          *list;
      gint            n_tools;
      gint            tool_rows;
      gint            tool_columns;

      gtk_widget_size_request (tool_button, &button_requisition);

      for (list = GIMP_LIST (gimp->tool_info_list)->list, n_tools = 0;
           list;
           list = list->next)
        {
          tool_info = (GimpToolInfo *) list->data;

          if (tool_info->visible)
            n_tools++;
        }

      tool_columns = MAX (1, (allocation->width / button_requisition.width));
      tool_rows    = n_tools / tool_columns;

      if (n_tools % tool_columns)
        tool_rows++;

      if (toolbox->tool_rows    != tool_rows  ||
          toolbox->tool_columns != tool_columns)
        {
          toolbox->tool_rows    = tool_rows;
          toolbox->tool_columns = tool_columns;

          gtk_widget_set_size_request (toolbox->tool_wbox, -1,
                                       tool_rows * button_requisition.height);
        }
    }

  {
    GtkRequisition  color_requisition;
    GtkRequisition  foo_requisition;
    GtkRequisition  image_requisition;
    gint            width;
    gint            height;
    gint            n_areas;
    gint            area_rows;
    gint            area_columns;

    gtk_widget_size_request (toolbox->color_area, &color_requisition);
    gtk_widget_size_request (toolbox->foo_area,   &foo_requisition);
    gtk_widget_size_request (toolbox->image_area, &image_requisition);

    width  = MAX (color_requisition.width,
                  MAX (foo_requisition.width,
                       image_requisition.width));
    height = MAX (color_requisition.height,
                  MAX (foo_requisition.height,
                       image_requisition.height));

    n_areas = (config->toolbox_color_area +
               config->toolbox_foo_area   +
               config->toolbox_image_area);

    area_columns = MAX (1, (allocation->width / width));
    area_rows    = n_areas / area_columns;

    if (n_areas % area_columns)
      area_rows++;

    if (toolbox->area_rows    != area_rows  ||
        toolbox->area_columns != area_columns)
      {
        toolbox->area_rows    = area_rows;
        toolbox->area_columns = area_columns;

        gtk_widget_set_size_request (toolbox->area_wbox, -1,
                                     area_rows * height);
      }
  }
}

static void
gimp_toolbox_style_set (GtkWidget *widget,
                        GtkStyle  *previous_style)
{
  Gimp           *gimp;
  GtkIconSize     tool_icon_size;
  GtkReliefStyle  relief;
  GList          *list;

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);

  if (! GIMP_DOCK (widget)->context)
    return;

  gimp = GIMP_DOCK (widget)->context->gimp;

  gtk_widget_style_get (widget,
                        "tool_icon_size", &tool_icon_size,
                        "button_relief",  &relief,
                        NULL);

  for (list = GIMP_LIST (gimp->tool_info_list)->list;
       list;
       list = g_list_next (list))
    {
      GimpToolInfo *tool_info = list->data;
      GtkWidget    *tool_button;

      tool_button = g_object_get_data (G_OBJECT (tool_info),
                                       TOOL_BUTTON_DATA_KEY);

      if (tool_button)
        {
          GtkImage *image;
          gchar    *stock_id;

          image = GTK_IMAGE (GTK_BIN (tool_button)->child);

          gtk_image_get_stock (image, &stock_id, NULL);
          gtk_image_set_from_stock (image, stock_id, tool_icon_size);

          gtk_button_set_relief (GTK_BUTTON (tool_button), relief);
        }
    }

  gimp_toolbox_set_geometry (GIMP_TOOLBOX (widget));
}

static void
gimp_toolbox_book_added (GimpDock     *dock,
                         GimpDockbook *dockbook)
{
  if (g_list_length (dock->dockbooks) == 1)
    gimp_toolbox_set_geometry (GIMP_TOOLBOX (dock));
}

static void
gimp_toolbox_book_removed (GimpDock     *dock,
                           GimpDockbook *dockbook)
{
  if (dock->dockbooks == NULL &&
      ! (GTK_OBJECT_FLAGS (dock) & GTK_IN_DESTRUCTION))
    gimp_toolbox_set_geometry (GIMP_TOOLBOX (dock));
}

static void
gimp_toolbox_set_geometry (GimpToolbox *toolbox)
{
  Gimp         *gimp;
  GimpToolInfo *tool_info;
  GtkWidget    *tool_button;

  gimp = GIMP_DOCK (toolbox)->context->gimp;

  tool_info = (GimpToolInfo *)
    gimp_container_get_child_by_name (gimp->tool_info_list,
                                      "gimp-rect-select-tool");
  tool_button = g_object_get_data (G_OBJECT (tool_info), TOOL_BUTTON_DATA_KEY);

  if (tool_button)
    {
      GtkWidget      *main_vbox;
      GtkRequisition  menubar_requisition = { 0, 0 };
      GtkRequisition  button_requisition;
      gint            border_width;
      GdkGeometry     geometry;

      main_vbox = GIMP_DOCK (toolbox)->main_vbox;

      if (toolbox->menu_bar)
        gtk_widget_size_request (toolbox->menu_bar, &menubar_requisition);

      gtk_widget_size_request (tool_button, &button_requisition);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (main_vbox));

      geometry.min_width  = (2 * border_width +
                             2 * button_requisition.width);
      geometry.min_height = -1;
      geometry.width_inc  = button_requisition.width;
      geometry.height_inc = (GIMP_DOCK (toolbox)->dockbooks ?
                             1 : button_requisition.height);

      gtk_window_set_geometry_hints (GTK_WINDOW (toolbox),
                                     NULL,
                                     &geometry,
                                     GDK_HINT_MIN_SIZE   |
                                     GDK_HINT_RESIZE_INC |
                                     GDK_HINT_USER_POS);
    }
}

GtkWidget *
gimp_toolbox_new (GimpDialogFactory *dialog_factory,
                  Gimp              *gimp)
{
  GimpToolbox *toolbox;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (dialog_factory), NULL);
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  toolbox = g_object_new (GIMP_TYPE_TOOLBOX,
                          "title",          _("The GIMP"),
                          "context",        gimp_get_user_context (gimp),
                          "dialog-factory", dialog_factory,
                          NULL);

  return GTK_WIDGET (toolbox);
}


/*  private functions  */

static gboolean
gimp_toolbox_button_accel_find_func (GtkAccelKey *key,
                                     GClosure    *closure,
                                     gpointer     data)
{
  return (GClosure *) data == closure;
}

static void
gimp_toolbox_substitute_underscores (gchar *str)
{
  gchar *p;

  for (p = str; *p; p++)
    if (*p == '_')
      *p = ' ';
}

static void
gimp_toolbox_button_accel_changed (GtkAccelGroup   *accel_group,
                                   guint            unused1,
                                   GdkModifierType  unused2,
                                   GClosure        *accel_closure,
                                   GtkWidget       *tool_button)
{
  GClosure *button_closure;

  button_closure = g_object_get_data (G_OBJECT (tool_button),
                                      "toolbox-accel-closure");

  if (accel_closure == button_closure)
    {
      GimpToolInfo *tool_info;
      GtkAccelKey  *accel_key;
      gchar        *tooltip;

      tool_info = g_object_get_data (G_OBJECT (tool_button), TOOL_INFO_DATA_KEY);

      accel_key = gtk_accel_group_find (accel_group,
                                        gimp_toolbox_button_accel_find_func,
                                        accel_closure);

      if (accel_key            &&
          accel_key->accel_key &&
          accel_key->accel_flags & GTK_ACCEL_VISIBLE)
        {
          /*  mostly taken from gtk-2-0/gtk/gtkaccellabel.c:1.46
           */
          GtkAccelLabelClass *accel_label_class;
          GString            *gstring;
          gboolean            seen_mod = FALSE;
          gunichar            ch;

          accel_label_class = g_type_class_peek (GTK_TYPE_ACCEL_LABEL);

          gstring = g_string_new (tool_info->help);
          g_string_append (gstring, "     ");

          if (accel_key->accel_mods & GDK_SHIFT_MASK)
            {
              g_string_append (gstring, accel_label_class->mod_name_shift);
              seen_mod = TRUE;
            }
          if (accel_key->accel_mods & GDK_CONTROL_MASK)
            {
              if (seen_mod)
                g_string_append (gstring, accel_label_class->mod_separator);
              g_string_append (gstring, accel_label_class->mod_name_control);
              seen_mod = TRUE;
            }
          if (accel_key->accel_mods & GDK_MOD1_MASK)
            {
              if (seen_mod)
                g_string_append (gstring, accel_label_class->mod_separator);
              g_string_append (gstring, accel_label_class->mod_name_alt);
              seen_mod = TRUE;
            }
          if (seen_mod)
            g_string_append (gstring, accel_label_class->mod_separator);

          ch = gdk_keyval_to_unicode (accel_key->accel_key);
          if (ch && (g_unichar_isgraph (ch) || ch == ' ') &&
              (ch < 0x80 || accel_label_class->latin1_to_char))
            {
              switch (ch)
                {
                case ' ':
                  g_string_append (gstring, "Space");
                  break;
                case '\\':
                  g_string_append (gstring, "Backslash");
                  break;
                default:
                  g_string_append_unichar (gstring, g_unichar_toupper (ch));
                  break;
                }
            }
          else
            {
              gchar *tmp;

              tmp = gtk_accelerator_name (accel_key->accel_key, 0);
              if (tmp[0] != 0 && tmp[1] == 0)
                tmp[0] = g_ascii_toupper (tmp[0]);
              gimp_toolbox_substitute_underscores (tmp);
              g_string_append (gstring, tmp);
              g_free (tmp);
            }

          tooltip = g_string_free (gstring, FALSE);
        }
      else
        {
          tooltip = g_strdup (tool_info->help);
        }

      gimp_help_set_help_data (tool_button, tooltip, tool_info->help_id);

      g_free (tooltip);
    }
}

static void
toolbox_create_tools (GimpToolbox *toolbox,
                      GimpContext *context)
{
  GimpUIManager *ui_manager;
  GimpToolInfo  *active_tool;
  GList         *list;
  GSList        *group = NULL;

  ui_manager = gimp_ui_managers_from_name ("<Image>")->data;

  if (ui_manager)
    gimp_ui_manager_ui_get (ui_manager, "/dummy-menubar");

  active_tool = gimp_context_get_tool (context);

  for (list = GIMP_LIST (context->gimp->tool_info_list)->list;
       list;
       list = g_list_next (list))
    {
      GimpToolInfo *tool_info;
      GtkWidget    *button;
      GtkWidget    *image;
      const gchar  *stock_id;

      tool_info = (GimpToolInfo *) list->data;

      button = gtk_radio_button_new (group);
      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

      gtk_wrap_box_pack (GTK_WRAP_BOX (toolbox->tool_wbox), button,
                         FALSE, FALSE, FALSE, FALSE);

      if (tool_info->visible)
        gtk_widget_show (button);

      g_signal_connect_object (tool_info, "notify::visible",
                               G_CALLBACK (toolbox_tool_visible_notify),
                               button, 0);

      stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_info));
      image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_widget_show (image);

      g_object_set_data (G_OBJECT (tool_info), TOOL_BUTTON_DATA_KEY, button);
      g_object_set_data (G_OBJECT (button),    TOOL_INFO_DATA_KEY,   tool_info);

      if (tool_info == active_tool)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      g_signal_connect (button, "toggled",
			G_CALLBACK (toolbox_tool_button_toggled),
			tool_info);

      g_signal_connect (button, "button_press_event",
			G_CALLBACK (toolbox_tool_button_press),
                        toolbox);

      if (ui_manager)
        {
          GtkAction   *action;
          const gchar *identifier;
          gchar       *tmp;
          gchar       *name;
          GClosure    *accel_closure = NULL;

          identifier = gimp_object_get_name (GIMP_OBJECT (tool_info));

          tmp = g_strndup (identifier + strlen ("gimp-"),
                           strlen (identifier) - strlen ("gimp--tool"));
          name = g_strdup_printf ("tools-%s", tmp);
          g_free (tmp);

          action = gimp_ui_manager_find_action (ui_manager, "tools", name);

          g_free (name);

#if HAVE_GTK_ACTION_GET_ACCEL_CLOSURE
          accel_closure = gtk_action_get_accel_closure (action);
#else
          {
            GSList *list;

            for (list = gtk_action_get_proxies (action);
                 list;
                 list = g_slist_next (list))
              {
                if (GTK_IS_MENU_ITEM (list->data))
                  {
                    GtkWidget *label = GTK_BIN (list->data)->child;

                    if (GTK_IS_ACCEL_LABEL (label))
                      {
                        g_object_get (label,
                                      "accel-closure", &accel_closure,
                                      NULL);

                        if (accel_closure)
                          break;
                      }
                  }
              }
          }
#endif

          if (accel_closure)
            {
              GtkAccelGroup *accel_group;

              g_object_set_data (G_OBJECT (button), "toolbox-accel-closure",
                                 accel_closure);

              accel_group =
                gtk_accel_group_from_accel_closure (accel_closure);

              g_signal_connect_object (accel_group, "accel_changed",
                                       G_CALLBACK (gimp_toolbox_button_accel_changed),
                                       button, 0);

              gimp_toolbox_button_accel_changed (accel_group,
                                                 0, 0,
                                                 accel_closure,
                                                 button);
            }
          else
            {
              gimp_help_set_help_data (button,
                                       tool_info->help, tool_info->help_id);
            }
        }
    }

  g_signal_connect_object (context->gimp->tool_info_list, "reorder",
                           G_CALLBACK (toolbox_tool_reorder),
                           toolbox->tool_wbox, 0);
}

static GtkWidget *
toolbox_create_color_area (GimpToolbox *toolbox,
                           GimpContext *context)
{
  GtkWidget *frame;
  GtkWidget *alignment;
  GtkWidget *col_area;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_set_border_width (GTK_CONTAINER (alignment), 3);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  gimp_help_set_help_data (alignment, NULL, GIMP_HELP_TOOLBOX_COLOR_AREA);

  col_area = gimp_toolbox_color_area_create (toolbox, 54, 42);
  gtk_container_add (GTK_CONTAINER (alignment), col_area);
  gtk_widget_show (col_area);

  gimp_help_set_help_data
    (col_area, _("Foreground & background colors.  The black and white squares "
                 "reset colors.  The arrows swap colors. Double click to open "
                 "the color selection dialog."), NULL);

  return frame;
}

static GtkWidget *
toolbox_create_foo_area (GimpToolbox *toolbox,
                         GimpContext *context)
{
  GtkWidget *frame;
  GtkWidget *alignment;
  GtkWidget *foo_area;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_set_border_width (GTK_CONTAINER (alignment), 3);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  gimp_help_set_help_data (alignment, NULL, GIMP_HELP_TOOLBOX_INDICATOR_AREA);

  foo_area = gimp_toolbox_indicator_area_create (toolbox);
  gtk_container_add (GTK_CONTAINER (alignment), foo_area);
  gtk_widget_show (foo_area);

  return frame;
}

static GtkWidget *
toolbox_create_image_area (GimpToolbox *toolbox,
                           GimpContext *context)
{
  GtkWidget *frame;
  GtkWidget *alignment;
  GtkWidget *image_area;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_set_border_width (GTK_CONTAINER (alignment), 3);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_widget_show (alignment);

  gimp_help_set_help_data (alignment, NULL, GIMP_HELP_TOOLBOX_IMAGE_AREA);

  image_area = gimp_toolbox_image_area_create (toolbox, 52, 42);
  gtk_container_add (GTK_CONTAINER (alignment), image_area);
  gtk_widget_show (image_area);

  return frame;
}

static void
toolbox_area_notify (GimpGuiConfig *config,
                     GParamSpec    *pspec,
                     GtkWidget     *area)
{
  gboolean visible;

  if (config->toolbox_color_area ||
      config->toolbox_foo_area   ||
      config->toolbox_image_area)
    {
      GtkRequisition req;

      gtk_widget_show (area->parent);

      /* HACK HACK HACK */
      gtk_widget_size_request (area, &req);
      gtk_widget_set_size_request (area->parent, req.width, req.height);
    }
  else
    {
      gtk_widget_hide (area->parent);
      gtk_widget_set_size_request (area->parent, -1, -1);
    }

  g_object_get (config, pspec->name, &visible, NULL);
  g_object_set (area, "visible", visible, NULL);
}

static void
toolbox_tool_changed (GimpContext  *context,
		      GimpToolInfo *tool_info,
		      gpointer      data)
{
  if (tool_info)
    {
      GtkWidget *toolbox_button = g_object_get_data (G_OBJECT (tool_info),
                                                     TOOL_BUTTON_DATA_KEY);

      if (toolbox_button && ! GTK_TOGGLE_BUTTON (toolbox_button)->active)
	{
	  g_signal_handlers_block_by_func (toolbox_button,
					   toolbox_tool_button_toggled,
					   tool_info);

	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbox_button),
                                        TRUE);

	  g_signal_handlers_unblock_by_func (toolbox_button,
					     toolbox_tool_button_toggled,
					     tool_info);
	}
    }
}

static void
toolbox_tool_reorder (GimpContainer *container,
                      GimpToolInfo  *tool_info,
                      gint           index,
                      GtkWidget     *wrap_box)
{
  if (tool_info)
    {
      GtkWidget *button = g_object_get_data (G_OBJECT (tool_info),
                                             TOOL_BUTTON_DATA_KEY);

      gtk_wrap_box_reorder_child (GTK_WRAP_BOX (wrap_box), button, index);
    }
}

static void
toolbox_tool_visible_notify (GimpToolInfo *tool_info,
                             GParamSpec   *pspec,
                             GtkWidget    *button)
{
  if (tool_info->visible)
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

static void
toolbox_tool_button_toggled (GtkWidget    *widget,
			     GimpToolInfo *tool_info)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    gimp_context_set_tool (gimp_get_user_context (tool_info->gimp), tool_info);
}

static gboolean
toolbox_tool_button_press (GtkWidget      *widget,
			   GdkEventButton *event,
			   GimpToolbox    *toolbox)
{
  if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1))
    {
      gimp_dialog_factory_dialog_raise (GIMP_DOCK (toolbox)->dialog_factory,
                                        gtk_widget_get_screen (widget),
                                        "gimp-tool-options",
                                        -1);
    }

  return FALSE;
}

static gboolean
toolbox_check_device (GtkWidget *widget,
		      GdkEvent  *event,
		      Gimp      *gimp)
{
  gimp_devices_check_change (gimp, event);

  return FALSE;
}