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

#include "dialogs-types.h"

#include "core/gimplayer.h"

#include "widgets/gimpenumwidgets.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewabledialog.h"

#include "layer-add-mask-dialog.h"

#include "gimp-intl.h"


/*  public functions  */

LayerAddMaskDialog *
layer_add_mask_dialog_new (GimpLayer       *layer,
                           GtkWidget       *parent,
                           GimpAddMaskType  add_mask_type,
                           gboolean         invert)
{
  LayerAddMaskDialog *dialog;
  GtkWidget          *vbox;
  GtkWidget          *frame;
  GtkWidget          *button;

  g_return_val_if_fail (GIMP_IS_LAYER (layer), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);

  dialog = g_new0 (LayerAddMaskDialog, 1);

  dialog->layer         = layer;
  dialog->add_mask_type = add_mask_type;
  dialog->invert        = invert;

  dialog->dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (layer),
                              _("Add Layer Mask"), "gimp-layer-add-mask",
                              GIMP_STOCK_LAYER_MASK,
                              _("Add a Mask to the Layer"),
                              parent,
                              gimp_standard_help_func,
                              GIMP_HELP_LAYER_MASK_ADD,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_OK,     GTK_RESPONSE_OK,

                              NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog->dialog), FALSE);

  g_object_weak_ref (G_OBJECT (dialog->dialog),
		     (GWeakNotify) g_free, dialog);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog->dialog)->vbox), vbox);
  gtk_widget_show (vbox);

  frame =
    gimp_enum_radio_frame_new (GIMP_TYPE_ADD_MASK_TYPE,
                               gtk_label_new (_("Initialize Layer Mask to:")),
                               G_CALLBACK (gimp_radio_button_update),
                               &dialog->add_mask_type,
                               &button);
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (button),
                                   dialog->add_mask_type);

  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  button = gtk_check_button_new_with_mnemonic (_("In_vert Mask"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), dialog->invert);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &dialog->invert);

  return dialog;
}
