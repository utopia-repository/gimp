/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpviewabledialog.c
 * Copyright (C) 2000 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimpimage.h"
#include "core/gimpitem.h"

#include "file/file-utils.h"

#include "gimpview.h"
#include "gimpviewabledialog.h"


enum
{
  PROP_0,
  PROP_STOCK_ID,
  PROP_DESC,
  PROP_PARENT
};


static void   gimp_viewable_dialog_class_init (GimpViewableDialogClass *klass);
static void   gimp_viewable_dialog_init       (GimpViewableDialog      *dialog);

static void   gimp_viewable_dialog_set_property (GObject            *object,
                                                 guint               property_id,
                                                 const GValue       *value,
                                                 GParamSpec         *pspec);

static void   gimp_viewable_dialog_destroy      (GtkObject          *object);

static void   gimp_viewable_dialog_name_changed (GimpObject         *object,
                                                 GimpViewableDialog *dialog);
static void   gimp_viewable_dialog_close        (GimpViewableDialog *dialog);


static GimpDialogClass *parent_class = NULL;


GType
gimp_viewable_dialog_get_type (void)
{
  static GType dialog_type = 0;

  if (! dialog_type)
    {
      static const GTypeInfo dialog_info =
      {
        sizeof (GimpViewableDialogClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_viewable_dialog_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpViewableDialog),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_viewable_dialog_init,
      };

      dialog_type = g_type_register_static (GIMP_TYPE_DIALOG,
					    "GimpViewableDialog",
					    &dialog_info, 0);
    }

  return dialog_type;
}

static void
gimp_viewable_dialog_class_init (GimpViewableDialogClass *klass)
{
  GtkObjectClass *gtk_object_class;
  GObjectClass   *object_class;

  gtk_object_class = GTK_OBJECT_CLASS (klass);
  object_class     = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gtk_object_class->destroy = gimp_viewable_dialog_destroy;

  object_class->set_property = gimp_viewable_dialog_set_property;

  g_object_class_install_property (object_class, PROP_STOCK_ID,
                                   g_param_spec_string ("stock-id", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DESC,
                                   g_param_spec_string ("description", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_PARENT,
                                   g_param_spec_object ("parent", NULL, NULL,
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_viewable_dialog_init (GimpViewableDialog *dialog)
{
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *vbox;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame,
                      FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  dialog->icon = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (dialog->icon), 0.5, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->icon, FALSE, FALSE, 0);
  gtk_widget_show (dialog->icon);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);
  gtk_widget_show (vbox);

  dialog->desc_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (dialog->desc_label), 0.0, 0.5);
  gimp_label_set_attributes (GTK_LABEL (dialog->desc_label),
                             PANGO_ATTR_SCALE,  PANGO_SCALE_LARGE,
                             PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD,
                             -1);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->desc_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->desc_label);

  dialog->viewable_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (dialog->viewable_label), 0.0, 0.5);
  gimp_label_set_attributes (GTK_LABEL (dialog->viewable_label),
                             PANGO_ATTR_SCALE,  PANGO_SCALE_SMALL,
                             -1);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->viewable_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->viewable_label);
}

static void
gimp_viewable_dialog_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpViewableDialog *dialog = GIMP_VIEWABLE_DIALOG (object);

  switch (property_id)
    {
    case PROP_STOCK_ID:
      gtk_image_set_from_stock (GTK_IMAGE (dialog->icon),
                                g_value_get_string (value),
                                GTK_ICON_SIZE_LARGE_TOOLBAR);
      break;
    case PROP_DESC:
      gtk_label_set_text (GTK_LABEL (dialog->desc_label),
                          g_value_get_string (value));
      break;
    case PROP_PARENT:
      {
        GtkWidget *parent = g_value_get_object (value);

        if (parent)
          {
            if (GTK_IS_WINDOW (parent))
              gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                            GTK_WINDOW (parent));
            else
              gtk_window_set_screen (GTK_WINDOW (dialog),
                                     gtk_widget_get_screen (parent));
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_viewable_dialog_destroy (GtkObject *object)
{
  GimpViewableDialog *dialog = GIMP_VIEWABLE_DIALOG (object);

  if (dialog->preview)
    gimp_viewable_dialog_set_viewable (dialog, NULL);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
gimp_viewable_dialog_new (GimpViewable *viewable,
                          const gchar  *title,
                          const gchar  *role,
                          const gchar  *stock_id,
                          const gchar  *desc,
                          GtkWidget    *parent,
                          GimpHelpFunc  help_func,
                          const gchar  *help_id,
                          ...)
{
  GimpViewableDialog *dialog;
  va_list             args;

  g_return_val_if_fail (! viewable || GIMP_IS_VIEWABLE (viewable), NULL);
  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (role != NULL, NULL);
  g_return_val_if_fail (parent == NULL || GTK_IS_WIDGET (parent), NULL);

  dialog = g_object_new (GIMP_TYPE_VIEWABLE_DIALOG,
                         "title",       title,
                         "role",        role,
                         "help-func",   help_func,
                         "help-id",     help_id,
                         "stock-id",    stock_id,
                         "description", desc,
                         "parent",      parent,
                         NULL);

  va_start (args, help_id);
  gimp_dialog_add_buttons_valist (GIMP_DIALOG (dialog), args);
  va_end (args);

  if (viewable)
    gimp_viewable_dialog_set_viewable (dialog, viewable);

  return GTK_WIDGET (dialog);
}

void
gimp_viewable_dialog_set_viewable (GimpViewableDialog *dialog,
                                   GimpViewable       *viewable)
{
  g_return_if_fail (GIMP_IS_VIEWABLE_DIALOG (dialog));
  g_return_if_fail (! viewable || GIMP_IS_VIEWABLE (viewable));

  if (dialog->preview)
    {
      GimpViewable *old_viewable;

      old_viewable = GIMP_VIEW (dialog->preview)->viewable;

      if (viewable == old_viewable)
        return;

      gtk_widget_destroy (dialog->preview);

      if (old_viewable)
        {
          g_signal_handlers_disconnect_by_func (old_viewable,
                                                gimp_viewable_dialog_name_changed,
                                                dialog);

          g_signal_handlers_disconnect_by_func (old_viewable,
                                                gimp_viewable_dialog_close,
                                                dialog);
        }
    }

  if (viewable)
    {
      g_signal_connect_object (viewable,
                               GIMP_VIEWABLE_GET_CLASS (viewable)->name_changed_signal,
                               G_CALLBACK (gimp_viewable_dialog_name_changed),
                               dialog,
                               0);

      dialog->preview = gimp_view_new (viewable, 32, 1, TRUE);
      gtk_box_pack_end (GTK_BOX (dialog->icon->parent), dialog->preview,
                        FALSE, FALSE, 2);
      gtk_widget_show (dialog->preview);

      g_object_add_weak_pointer (G_OBJECT (dialog->preview),
                                 (gpointer *) &dialog->preview);

      gimp_viewable_dialog_name_changed (GIMP_OBJECT (viewable), dialog);

      if (GIMP_IS_ITEM (viewable))
        {
          g_signal_connect_object (viewable, "removed",
                                   G_CALLBACK (gimp_viewable_dialog_close),
                                   dialog,
                                   G_CONNECT_SWAPPED);
        }
      else
        {
          g_signal_connect_object (viewable, "disconnect",
                                   G_CALLBACK (gimp_viewable_dialog_close),
                                   dialog,
                                   G_CONNECT_SWAPPED);
        }
    }
}


/*  private functions  */

static void
gimp_viewable_dialog_name_changed (GimpObject         *object,
                                   GimpViewableDialog *dialog)
{
  gchar *name;

  name = gimp_viewable_get_description (GIMP_VIEWABLE (object), NULL);

  if (GIMP_IS_ITEM (object))
    {
      const gchar *uri;
      gchar       *basename;
      gchar       *tmp;

      uri = gimp_image_get_uri (gimp_item_get_image (GIMP_ITEM (object)));
      tmp = name;

      basename = file_utils_uri_to_utf8_basename (uri);
      name = g_strdup_printf ("%s-%d (%s)",
                              tmp,
                              gimp_item_get_ID (GIMP_ITEM (object)),
                              basename);

      g_free (basename);
      g_free (tmp);
    }

  gtk_label_set_text (GTK_LABEL (dialog->viewable_label), name);
  g_free (name);
}

static void
gimp_viewable_dialog_close (GimpViewableDialog *dialog)
{
  g_signal_emit_by_name (dialog, "close");
}