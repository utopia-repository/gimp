/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimppatternfactoryview.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpviewable.h"

#include "gimpcontainerview.h"
#include "gimppatternfactoryview.h"
#include "gimpviewrenderer.h"


GType
gimp_pattern_factory_view_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpPatternFactoryViewClass),
        NULL,           /* base_init      */
        NULL,           /* base_finalize  */
        NULL,           /* class_init     */
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpPatternFactoryView),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      view_type = g_type_register_static (GIMP_TYPE_DATA_FACTORY_VIEW,
                                          "GimpPatternFactoryView",
                                          &view_info, 0);
    }

  return view_type;
}

GtkWidget *
gimp_pattern_factory_view_new (GimpViewType      view_type,
                               GimpDataFactory  *factory,
                               GimpContext      *context,
                               gint              preview_size,
                               gint              preview_border_width,
                               GimpMenuFactory  *menu_factory)
{
  GimpPatternFactoryView *factory_view;

  g_return_val_if_fail (GIMP_IS_DATA_FACTORY (factory), NULL);
  g_return_val_if_fail (preview_size > 0 &&
                        preview_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE, NULL);
  g_return_val_if_fail (preview_border_width >= 0 &&
                        preview_border_width <= GIMP_VIEW_MAX_BORDER_WIDTH,
                        NULL);

  factory_view = g_object_new (GIMP_TYPE_PATTERN_FACTORY_VIEW, NULL);

  if (! gimp_data_factory_view_construct (GIMP_DATA_FACTORY_VIEW (factory_view),
                                          view_type,
                                          factory,
                                          context,
                                          preview_size, preview_border_width,
                                          menu_factory, "<Patterns>",
                                          "/patterns-popup",
                                          "patterns"))
    {
      g_object_unref (factory_view);
      return NULL;
    }

  gtk_widget_hide (GIMP_DATA_FACTORY_VIEW (factory_view)->edit_button);
  gtk_widget_hide (GIMP_DATA_FACTORY_VIEW (factory_view)->duplicate_button);

  return GTK_WIDGET (factory_view);
}