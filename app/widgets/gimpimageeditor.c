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

#include "widgets-types.h"

#include "core/gimpcontext.h"
#include "core/gimpimage.h"

#include "gimpdocked.h"
#include "gimpimageeditor.h"
#include "gimpuimanager.h"


static void   gimp_image_editor_class_init (GimpImageEditorClass *klass);
static void   gimp_image_editor_init       (GimpImageEditor      *editor);
static void   gimp_image_editor_docked_iface_init (GimpDockedInterface *docked_iface);

static void   gimp_image_editor_set_context    (GimpDocked       *docked,
                                                GimpContext      *context);
static void   gimp_image_editor_destroy        (GtkObject        *object);
static void   gimp_image_editor_real_set_image (GimpImageEditor  *editor,
                                                GimpImage        *gimage);
static void   gimp_image_editor_image_flush    (GimpImage        *gimage,
                                                GimpImageEditor  *editor);


static GimpEditorClass *parent_class = NULL;


GType
gimp_image_editor_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo editor_info =
      {
        sizeof (GimpImageEditorClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_image_editor_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpImageEditor),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_image_editor_init,
      };
      static const GInterfaceInfo docked_iface_info =
      {
        (GInterfaceInitFunc) gimp_image_editor_docked_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      type = g_type_register_static (GIMP_TYPE_EDITOR,
                                     "GimpImageEditor",
                                     &editor_info, 0);

      g_type_add_interface_static (type, GIMP_TYPE_DOCKED,
                                   &docked_iface_info);
    }

  return type;
}

static void
gimp_image_editor_class_init (GimpImageEditorClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->destroy = gimp_image_editor_destroy;

  klass->set_image      = gimp_image_editor_real_set_image;
}

static void
gimp_image_editor_init (GimpImageEditor *editor)
{
  editor->gimage = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
}

static void
gimp_image_editor_docked_iface_init (GimpDockedInterface *docked_iface)
{
  docked_iface->set_context = gimp_image_editor_set_context;
}

static void
gimp_image_editor_set_context (GimpDocked  *docked,
                               GimpContext *context)
{
  GimpImageEditor *editor = GIMP_IMAGE_EDITOR (docked);
  GimpImage       *image  = NULL;

  if (editor->context)
    g_signal_handlers_disconnect_by_func (editor->context,
                                          gimp_image_editor_set_image,
                                          editor);

  editor->context = context;

  if (context)
    {
      g_signal_connect_swapped (context, "image_changed",
                                G_CALLBACK (gimp_image_editor_set_image),
                                editor);

      image = gimp_context_get_image (context);
    }

  gimp_image_editor_set_image (editor, image);
}

static void
gimp_image_editor_destroy (GtkObject *object)
{
  GimpImageEditor *editor = GIMP_IMAGE_EDITOR (object);

  if (editor->gimage)
    gimp_image_editor_set_image (editor, NULL);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gimp_image_editor_real_set_image (GimpImageEditor *editor,
                                  GimpImage       *gimage)
{
  if (editor->gimage)
    g_signal_handlers_disconnect_by_func (editor->gimage,
                                          gimp_image_editor_image_flush,
                                          editor);

  editor->gimage = gimage;

  if (editor->gimage)
    g_signal_connect (editor->gimage, "flush",
                      G_CALLBACK (gimp_image_editor_image_flush),
                      editor);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), gimage != NULL);
}


/*  public functions  */

void
gimp_image_editor_set_image (GimpImageEditor *editor,
                             GimpImage       *gimage)
{
  g_return_if_fail (GIMP_IS_IMAGE_EDITOR (editor));
  g_return_if_fail (gimage == NULL || GIMP_IS_IMAGE (gimage));

  if (gimage != editor->gimage)
    {
      GIMP_IMAGE_EDITOR_GET_CLASS (editor)->set_image (editor, gimage);

      if (GIMP_EDITOR (editor)->ui_manager)
        gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                                GIMP_EDITOR (editor)->popup_data);
    }
}


/*  private functions  */

static void
gimp_image_editor_image_flush (GimpImage       *gimage,
                               GimpImageEditor *editor)
{
  if (GIMP_EDITOR (editor)->ui_manager)
    gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                            GIMP_EDITOR (editor)->popup_data);
}