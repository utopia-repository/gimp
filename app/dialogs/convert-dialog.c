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

#include "core/gimp.h"
#include "core/gimpcontainer-filter.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpimage.h"
#include "core/gimpimage-convert.h"
#include "core/gimplist.h"
#include "core/gimppalette.h"
#include "core/gimpprogress.h"

#include "widgets/gimpenumwidgets.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpenumcombobox.h"
#include "widgets/gimpviewablebox.h"
#include "widgets/gimpviewabledialog.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimp-intl.h"


typedef struct
{
  GtkWidget              *dialog;

  GimpImage              *gimage;
  GimpProgress           *progress;
  GimpContext            *context;
  GimpContainer          *container;
  GimpPalette            *custom_palette;

  GimpConvertDitherType   dither_type;
  gboolean                alpha_dither;
  gboolean                remove_dups;
  gint                    num_colors;
  GimpConvertPaletteType  palette_type;
} IndexedDialog;


static void        convert_dialog_response        (GtkWidget        *widget,
                                                   gint              response_id,
                                                   IndexedDialog    *dialog);
static GtkWidget * convert_dialog_palette_box     (IndexedDialog    *dialog);
static gboolean    convert_dialog_palette_filter  (const GimpObject *object,
                                                   gpointer          user_data);
static void        convert_dialog_palette_changed (GimpContext      *context,
                                                   GimpPalette      *palette,
                                                   IndexedDialog    *dialog);


/*  defaults  */

static GimpConvertDitherType   saved_dither_type  = GIMP_FS_DITHER;
static gboolean                saved_alpha_dither = FALSE;
static gboolean                saved_remove_dups  = TRUE;
static gint                    saved_num_colors   = 256;
static GimpConvertPaletteType  saved_palette_type = GIMP_MAKE_PALETTE;
static GimpPalette            *saved_palette      = NULL;


/*  public functions  */

GtkWidget *
convert_dialog_new (GimpImage    *gimage,
                    GtkWidget    *parent,
                    GimpProgress *progress)
{
  IndexedDialog *dialog;
  GtkWidget     *main_vbox;
  GtkWidget     *vbox;
  GtkWidget     *hbox;
  GtkWidget     *label;
  GtkObject     *adjustment;
  GtkWidget     *spinbutton;
  GtkWidget     *frame;
  GtkWidget     *toggle;
  GtkWidget     *palette_box;
  GtkWidget     *combo;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);

  dialog = g_new0 (IndexedDialog, 1);

  dialog->gimage       = gimage;
  dialog->progress     = progress;
  dialog->dither_type  = saved_dither_type;
  dialog->alpha_dither = saved_alpha_dither;
  dialog->remove_dups  = saved_remove_dups;
  dialog->num_colors   = saved_num_colors;
  dialog->palette_type = saved_palette_type;

  dialog->dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (gimage),
                              _("Indexed Color Conversion"),
                              "gimp-image-convert-indexed",
                              GIMP_STOCK_CONVERT_INDEXED,
                              _("Convert Image to Indexed Colors"),
                              parent,
                              gimp_standard_help_func,
                              GIMP_HELP_IMAGE_CONVERT_INDEXED,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_OK,     GTK_RESPONSE_OK,

                              NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog->dialog), FALSE);

  g_object_weak_ref (G_OBJECT (dialog->dialog),
                     (GWeakNotify) g_free, dialog);

  g_signal_connect (dialog->dialog, "response",
                    G_CALLBACK (convert_dialog_response),
                    dialog);

  palette_box = convert_dialog_palette_box (dialog);

  if (dialog->context)
    g_object_weak_ref (G_OBJECT (dialog->dialog),
                       (GWeakNotify) g_object_unref,
                       dialog->context);

  if (dialog->container)
    g_object_weak_ref (G_OBJECT (dialog->dialog),
                       (GWeakNotify) g_object_unref,
                       dialog->container);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog->dialog)->vbox),
		     main_vbox);
  gtk_widget_show (main_vbox);


  /*  palette  */

  frame = gimp_enum_radio_frame_new (GIMP_TYPE_CONVERT_PALETTE_TYPE,
                                     gtk_label_new (_("Colormap")),
                                     G_CALLBACK (gimp_radio_button_update),
                                     &dialog->palette_type,
                                     &toggle);
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (toggle),
                                   dialog->palette_type);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /*  max n_colors  */
  hbox = gtk_hbox_new (FALSE, 6);
  gimp_enum_radio_frame_add (GTK_FRAME (frame), hbox, GIMP_MAKE_PALETTE);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Maximum number of colors:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  if (dialog->num_colors == 256 && gimp_image_has_alpha (gimage))
    dialog->num_colors = 255;

  spinbutton = gimp_spin_button_new (&adjustment, dialog->num_colors,
				     2, 256, 1, 8, 0, 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  g_signal_connect (adjustment, "value_changed",
		    G_CALLBACK (gimp_int_adjustment_update),
		    &dialog->num_colors);

  /*  custom palette  */
  if (palette_box)
    {
      vbox = gtk_vbox_new (FALSE, 2);
      gimp_enum_radio_frame_add (GTK_FRAME (frame), vbox, GIMP_CUSTOM_PALETTE);
      gtk_widget_show (vbox);

      gtk_box_pack_start (GTK_BOX (vbox), palette_box, FALSE, FALSE, 0);
      gtk_widget_show (palette_box);

      toggle = gtk_check_button_new_with_mnemonic (_("_Remove unused colors "
                                                     "from final palette"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                    dialog->remove_dups);
      gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
      gtk_widget_show (toggle);

      g_signal_connect (toggle, "toggled",
			G_CALLBACK (gimp_toggle_button_update),
			&dialog->remove_dups);
    }


  /*  dithering  */

  frame = gimp_frame_new (_("Dithering"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Color _dithering:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo = gimp_enum_combo_box_new (GIMP_TYPE_CONVERT_DITHER_TYPE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show (combo);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              dialog->dither_type,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &dialog->dither_type);

  toggle =
    gtk_check_button_new_with_mnemonic (_("Enable dithering of _transparency"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                dialog->alpha_dither);
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
		    G_CALLBACK (gimp_toggle_button_update),
		    &dialog->alpha_dither);

  return dialog->dialog;
}


/*  private functions  */

static void
convert_dialog_response (GtkWidget     *widget,
                         gint           response_id,
                         IndexedDialog *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      GimpProgress *progress;

      progress = gimp_progress_start (dialog->progress,
                                      _("Converting to indexed..."), FALSE);

      /*  Convert the image to indexed color  */
      gimp_image_convert (dialog->gimage,
                          GIMP_INDEXED,
                          dialog->num_colors,
                          dialog->dither_type,
                          dialog->alpha_dither,
                          dialog->remove_dups,
                          dialog->palette_type,
                          dialog->custom_palette,
                          progress);

      if (progress)
        gimp_progress_end (progress);

      gimp_image_flush (dialog->gimage);

      /* Save defaults for next time */
      saved_dither_type  = dialog->dither_type;
      saved_alpha_dither = dialog->alpha_dither;
      saved_remove_dups  = dialog->remove_dups;
      saved_num_colors   = dialog->num_colors;
      saved_palette_type = dialog->palette_type;
      saved_palette      = dialog->custom_palette;
    }

  gtk_widget_destroy (dialog->dialog);
}

static GtkWidget *
convert_dialog_palette_box (IndexedDialog *dialog)
{
  Gimp        *gimp;
  GList       *list;
  GimpPalette *palette;
  GimpPalette *web_palette   = NULL;
  gboolean     default_found = FALSE;

  gimp = dialog->gimage->gimp;

  /* We can't dither to > 256 colors */
  dialog->container = gimp_container_filter (gimp->palette_factory->container,
                                             convert_dialog_palette_filter,
                                             NULL);

  if (! gimp_container_num_children (dialog->container))
    {
      g_object_unref (dialog->container);
      return NULL;
    }

  dialog->context = gimp_context_new (gimp, "convert-dialog", NULL);

  for (list = GIMP_LIST (dialog->container)->list;
       list;
       list = g_list_next (list))
    {
      palette = (GimpPalette *) list->data;

      /* Preferentially, the initial default is 'Web' if available */
      if (web_palette == NULL &&
	  g_ascii_strcasecmp (GIMP_OBJECT (palette)->name, "Web") == 0)
	{
	  web_palette = palette;
	}

      if (saved_palette == palette)
        {
          dialog->custom_palette = saved_palette;
          default_found = TRUE;
        }
    }

  if (! default_found)
    {
      if (web_palette)
        dialog->custom_palette = web_palette;
      else
        dialog->custom_palette = GIMP_LIST (dialog->container)->list->data;
    }

  gimp_context_set_palette (dialog->context, dialog->custom_palette);

  g_signal_connect (dialog->context, "palette-changed",
                    G_CALLBACK (convert_dialog_palette_changed),
                    dialog);

  return gimp_palette_box_new (dialog->container, dialog->context, 4);
}

static gboolean
convert_dialog_palette_filter (const GimpObject *object,
                               gpointer          user_data)
{
  GimpPalette *palette = GIMP_PALETTE (object);

  return palette->n_colors > 0 && palette->n_colors <= 256;
}

static void
convert_dialog_palette_changed (GimpContext   *context,
				GimpPalette   *palette,
                                IndexedDialog *dialog)
{
  if (! palette)
    return;

  if (palette->n_colors > 256)
    {
      g_message (_("Cannot convert to a palette with more than 256 colors."));
    }
  else
    {
      dialog->custom_palette = palette;
    }
}