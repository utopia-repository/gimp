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

#include "libgimpwidgets/gimpwidgets.h"

#include "dialogs-types.h"

#include "core/gimpimage.h"

#include "vectors/gimpvectors.h"

#include "widgets/gimpviewabledialog.h"

#include "vectors-options-dialog.h"

#include "gimp-intl.h"


/*  public functions  */

VectorsOptionsDialog *
vectors_options_dialog_new (GimpImage   *gimage,
                            GimpVectors *vectors,
                            GtkWidget   *parent,
                            const gchar *vectors_name,
                            const gchar *title,
                            const gchar *role,
                            const gchar *stock_id,
                            const gchar *desc,
                            const gchar *help_id)
{
  VectorsOptionsDialog *options;
  GimpViewable         *viewable;
  GtkWidget            *hbox;
  GtkWidget            *vbox;
  GtkWidget            *table;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (vectors == NULL || GIMP_IS_VECTORS (vectors), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (role != NULL, NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (desc != NULL, NULL);
  g_return_val_if_fail (help_id != NULL, NULL);

  options = g_new0 (VectorsOptionsDialog, 1);

  options->gimage  = gimage;
  options->vectors = vectors;

  if (vectors)
    viewable = GIMP_VIEWABLE (vectors);
  else
    viewable = GIMP_VIEWABLE (gimage);

  options->dialog =
    gimp_viewable_dialog_new (viewable,
                              title, role, stock_id, desc,
                              parent,
                              gimp_standard_help_func,
                              help_id,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_OK,     GTK_RESPONSE_OK,

                              NULL);

  g_object_weak_ref (G_OBJECT (options->dialog),
		     (GWeakNotify) g_free,
		     options);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (options->dialog)->vbox), hbox);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  options->name_entry = gtk_entry_new ();
  gtk_widget_set_size_request (options->name_entry, 150, -1);
  gtk_entry_set_activates_default (GTK_ENTRY (options->name_entry), TRUE);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 0,
                             _("Path Name:"), 0.0, 0.5,
                             options->name_entry, 1, FALSE);

  gtk_entry_set_text (GTK_ENTRY (options->name_entry), vectors_name);

  return options;
}
