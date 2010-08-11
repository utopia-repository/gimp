/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpProfileChooserDialog
 * Copyright (C) 2006 Sven Neumann <sven@gimp.org>
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

#include "widgets-types.h"

#include "core/gimp.h"

#include "plug-in/plug-in-icc-profile.h"

#include "gimpprofilechooserdialog.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_GIMP
};


static void   gimp_profile_chooser_dialog_class_init     (GimpProfileChooserDialogClass *klass);
static void   gimp_profile_chooser_dialog_init           (GimpProfileChooserDialog *dialog);
static GObject * gimp_profile_chooser_dialog_constructor (GType                     type,
                                                          guint                     n_params,
                                                          GObjectConstructParam    *params);

static void   gimp_profile_chooser_dialog_dispose        (GObject                  *object);
static void   gimp_profile_chooser_dialog_set_property   (GObject                  *object,
                                                          guint                     prop_id,
                                                          const GValue             *value,
                                                          GParamSpec               *pspec);
static void   gimp_profile_chooser_dialog_get_property   (GObject                  *object,
                                                          guint                     prop_id,
                                                          GValue                   *value,
                                                          GParamSpec               *pspec);

static void   gimp_profile_chooser_dialog_style_set      (GtkWidget                *widget,
                                                          GtkStyle                 *prev_style);

static void   gimp_profile_chooser_dialog_update_preview (GimpProfileChooserDialog *dialog);

static GtkWidget * gimp_profile_view_new                 (GimpProfileChooserDialog *dialog);
static gboolean    gimp_profile_view_query               (GimpProfileChooserDialog *dialog);


G_DEFINE_TYPE (GimpProfileChooserDialog, gimp_profile_chooser_dialog,
               GTK_TYPE_FILE_CHOOSER_DIALOG);

#define parent_class gimp_profile_chooser_dialog_parent_class


static void
gimp_profile_chooser_dialog_class_init (GimpProfileChooserDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructor  = gimp_profile_chooser_dialog_constructor;
  object_class->dispose      = gimp_profile_chooser_dialog_dispose;
  object_class->get_property = gimp_profile_chooser_dialog_get_property;
  object_class->set_property = gimp_profile_chooser_dialog_set_property;

  widget_class->style_set    = gimp_profile_chooser_dialog_style_set;

  g_object_class_install_property (object_class, PROP_GIMP,
                                   g_param_spec_object ("gimp",
                                                        NULL, NULL,
                                                        GIMP_TYPE_GIMP,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_profile_chooser_dialog_init (GimpProfileChooserDialog *dialog)
{
  dialog->idle_id = 0;

  dialog->info_label = g_object_new (GTK_TYPE_LABEL,
                                     "xalign", 0.0,
                                     "yalign", 0.0,
                                     NULL);

  gtk_widget_set_size_request (dialog->info_label, 200, -1);

  dialog->preview = gimp_profile_view_new (dialog);
}

static GObject *
gimp_profile_chooser_dialog_constructor (GType                  type,
                                         guint                  n_params,
                                         GObjectConstructParam *params)
{
  GObject                  *object;
  GimpProfileChooserDialog *dialog;
  GtkFileFilter            *filter;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  dialog = GIMP_PROFILE_CHOOSER_DIALOG (object);

  gtk_window_set_role (GTK_WINDOW (dialog), "gimp-profile-chooser-dialog");

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

#ifndef G_OS_WIN32
  {
    const gchar folder[] = "/usr/share/color/icc";

    if (g_file_test (folder, G_FILE_TEST_IS_DIR))
      gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog),
                                            folder, NULL);
  }
#endif

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All files (*.*)"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("ICC color profile (*.icc, *.icm)"));
  gtk_file_filter_add_pattern (filter, "*.[Ii][Cc][Cc]");
  gtk_file_filter_add_pattern (filter, "*.[Ii][Cc][Mm]");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog),
                                       dialog->preview);

  g_signal_connect (dialog, "update-preview",
                    G_CALLBACK (gimp_profile_chooser_dialog_update_preview),
                    NULL);

  return object;
}

static void
gimp_profile_chooser_dialog_dispose (GObject *object)
{
  GimpProfileChooserDialog *dialog = GIMP_PROFILE_CHOOSER_DIALOG (object);

  if (dialog->idle_id)
    {
      g_source_remove (dialog->idle_id);
      dialog->idle_id = 0;
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_profile_chooser_dialog_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GimpProfileChooserDialog *dialog = GIMP_PROFILE_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_GIMP:
      dialog->gimp = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gimp_profile_chooser_dialog_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GimpProfileChooserDialog *dialog = GIMP_PROFILE_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_GIMP:
      g_value_set_object (value, dialog->gimp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gimp_profile_chooser_dialog_style_set (GtkWidget *widget,
                                       GtkStyle  *prev_style)
{
  GimpProfileChooserDialog *dialog = GIMP_PROFILE_CHOOSER_DIALOG (widget);
  GtkWidget                *ebox;

  GTK_WIDGET_CLASS (parent_class)->style_set (widget, prev_style);

  gtk_widget_modify_bg (dialog->preview, GTK_STATE_NORMAL,
                        &widget->style->base[GTK_STATE_NORMAL]);
  gtk_widget_modify_bg (dialog->preview, GTK_STATE_INSENSITIVE,
                        &widget->style->base[GTK_STATE_NORMAL]);

  ebox = gtk_bin_get_child (GTK_BIN (widget));

  gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL,
                        &widget->style->base[GTK_STATE_NORMAL]);
  gtk_widget_modify_bg (ebox, GTK_STATE_INSENSITIVE,
                        &widget->style->base[GTK_STATE_NORMAL]);
}

GtkWidget *
gimp_profile_chooser_dialog_new (Gimp        *gimp,
                                 const gchar *title)
{
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  return g_object_new (GIMP_TYPE_PROFILE_CHOOSER_DIALOG,
                       "gimp",  gimp,
                       "title", title,
                       NULL);
}

static void
gimp_profile_chooser_dialog_update_preview (GimpProfileChooserDialog *dialog)
{
  gtk_label_set_text (GTK_LABEL (dialog->info_label), "");

  if (dialog->idle_id)
    g_source_remove (dialog->idle_id);

  dialog->idle_id = g_idle_add ((GSourceFunc) gimp_profile_view_query,
                                dialog);
}

static GtkWidget *
gimp_profile_view_new (GimpProfileChooserDialog *dialog)
{
  GtkWidget *frame;
  GtkWidget *ebox;
  GtkWidget *vbox;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

  ebox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (frame), ebox);
  gtk_widget_show (ebox);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (ebox), vbox);
  gtk_widget_show (vbox);

  gtk_box_pack_start (GTK_BOX (vbox), dialog->info_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->info_label);

  return frame;
}

static gboolean
gimp_profile_view_query (GimpProfileChooserDialog *dialog)
{
  gchar  *uri;
  gchar  *filename;

  uri = gtk_file_chooser_get_preview_uri (GTK_FILE_CHOOSER (dialog));
  if (! uri)
    return FALSE;

  filename = g_filename_from_uri (uri, NULL, NULL);

  if (filename)
    {
      gchar *info = NULL;

      if (plug_in_icc_profile_file_info (dialog->gimp,
                                         gimp_get_user_context (dialog->gimp),
                                         NULL,
                                         filename,
                                         NULL, NULL, &info,
                                         NULL))
        {
          gtk_label_set_text (GTK_LABEL (dialog->info_label), info);
          g_free (info);
        }

      g_free (filename);
    }

  g_free (uri);

  return FALSE;
}
