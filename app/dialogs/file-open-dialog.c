/* The GIMP -- an image manipulation program
 * Copyright (C) 1995, 1996, 1997 Spencer Kimball and Peter Mattis
 * Copyright (C) 1997 Josh MacDonald
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

#include "libgimpwidgets/gimpwidgets.h"

#include "dialogs-types.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimpprogress.h"

#include "file/file-open.h"
#include "file/file-utils.h"

#include "widgets/gimpfiledialog.h"
#include "widgets/gimphelp-ids.h"

#include "file-open-dialog.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void       file_open_dialog_response   (GtkWidget     *open_dialog,
                                               gint           response_id,
                                               Gimp          *gimp);
static gboolean   file_open_dialog_open_image (GtkWidget     *open_dialog,
                                               Gimp          *gimp,
                                               const gchar   *uri,
                                               const gchar   *entered_filename,
                                               PlugInProcDef *load_proc);
static gboolean   file_open_dialog_open_layer (GtkWidget     *open_dialog,
                                               GimpImage     *gimage,
                                               const gchar   *uri,
                                               const gchar   *entered_filename,
                                               PlugInProcDef *load_proc);


/*  public functions  */

GtkWidget *
file_open_dialog_new (Gimp *gimp)
{
  GtkWidget *dialog;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  dialog = gimp_file_dialog_new (gimp,
                                 GTK_FILE_CHOOSER_ACTION_OPEN,
                                 _("Open Image"), "gimp-file-open",
                                 GTK_STOCK_OPEN,
                                 GIMP_HELP_FILE_OPEN);

  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (file_open_dialog_response),
                    gimp);

  return dialog;
}


/*  private functions  */

static void
file_open_dialog_response (GtkWidget *open_dialog,
                           gint       response_id,
                           Gimp      *gimp)
{
  GimpFileDialog *dialog  = GIMP_FILE_DIALOG (open_dialog);
  GSList         *uris;
  GSList         *list;
  gboolean        success = FALSE;

  if (response_id != GTK_RESPONSE_OK)
    {
      if (! dialog->busy)
        gtk_widget_hide (open_dialog);

      return;
    }

  uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (open_dialog));

  gimp_file_dialog_set_sensitive (dialog, FALSE);

  /*  open layers in reverse order so they appear in the same
   *  order as in the file dialog
   */
  if (dialog->gimage)
    uris = g_slist_reverse (uris);

  for (list = uris; list; list = g_slist_next (list))
    {
      gchar *filename = file_utils_filename_from_uri (list->data);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        {
          if (dialog->gimage)
            {
              if (file_open_dialog_open_layer (open_dialog,
                                               dialog->gimage,
                                               list->data,
                                               list->data,
                                               dialog->file_proc))
                {
                  success = TRUE;
                }
            }
          else
            {
              if (file_open_dialog_open_image (open_dialog,
                                               gimp,
                                               list->data,
                                               list->data,
                                               dialog->file_proc))
                {
                  success = TRUE;

                  gdk_window_raise (open_dialog->window);
                }
            }
        }

      g_free (filename);

      if (dialog->canceled)
        break;
    }

  if (success)
    {
      gtk_widget_hide (open_dialog);

      if (dialog->gimage)
        gimp_image_flush (dialog->gimage);
    }

  gimp_file_dialog_set_sensitive (dialog, TRUE);

  g_slist_foreach (uris, (GFunc) g_free, NULL);
  g_slist_free (uris);
}

static gboolean
file_open_dialog_open_image (GtkWidget     *open_dialog,
                             Gimp          *gimp,
                             const gchar   *uri,
                             const gchar   *entered_filename,
                             PlugInProcDef *load_proc)
{
  GimpImage         *gimage;
  GimpPDBStatusType  status;
  GError            *error = NULL;

  gimage = file_open_with_proc_and_display (gimp,
                                            gimp_get_user_context (gimp),
                                            GIMP_PROGRESS (open_dialog),
                                            uri,
                                            entered_filename,
                                            load_proc,
                                            &status,
                                            &error);

  if (gimage)
    {
      return TRUE;
    }
  else if (status != GIMP_PDB_CANCEL)
    {
      gchar *filename = file_utils_uri_to_utf8_filename (uri);

      g_message (_("Opening '%s' failed:\n\n%s"),
                 filename, error->message);
      g_clear_error (&error);

      g_free (filename);
    }

  return FALSE;
}

static gboolean
file_open_dialog_open_layer (GtkWidget     *open_dialog,
                             GimpImage     *gimage,
                             const gchar   *uri,
                             const gchar   *entered_filename,
                             PlugInProcDef *load_proc)
{
  GimpLayer         *new_layer;
  GimpPDBStatusType  status;
  GError            *error = NULL;

  new_layer = file_open_layer (gimage->gimp,
                               gimp_get_user_context (gimage->gimp),
                               GIMP_PROGRESS (open_dialog),
                               gimage, uri,
                               &status, &error);

  if (new_layer)
    {
      GimpItem *new_item = GIMP_ITEM (new_layer);
      gint      width, height;
      gint      off_x, off_y;

      width  = gimp_image_get_width (gimage);
      height = gimp_image_get_height (gimage);

      gimp_item_offsets (new_item, &off_x, &off_y);

      off_x = (width  - gimp_item_width  (new_item)) / 2 - off_x;
      off_y = (height - gimp_item_height (new_item)) / 2 - off_y;

      gimp_item_translate (new_item, off_x, off_y, FALSE);

      gimp_image_add_layer (gimage, new_layer, -1);

      return TRUE;
    }
  else if (status != GIMP_PDB_CANCEL)
    {
      gchar *filename = file_utils_uri_to_utf8_filename (uri);

      g_message (_("Opening '%s' failed:\n\n%s"),
                 filename, error->message);
      g_clear_error (&error);

      g_free (filename);
    }

  return FALSE;
}