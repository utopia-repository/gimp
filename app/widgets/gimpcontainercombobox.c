/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainercombobox.c
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#include "widgets-types.h"

#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpviewable.h"

#include "gimpcellrendererviewable.h"
#include "gimpcontainercombobox.h"
#include "gimpcontainerview.h"
#include "gimpviewrenderer.h"


enum
{
  COLUMN_RENDERER,
  COLUMN_NAME,
  NUM_COLUMNS
};


static void     gimp_container_combo_box_class_init   (GimpContainerComboBoxClass *klass);
static void     gimp_container_combo_box_init         (GimpContainerComboBox  *view);

static void     gimp_container_combo_box_view_iface_init (GimpContainerViewInterface *view_iface);

static gpointer gimp_container_combo_box_insert_item  (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gint                    index);
static void     gimp_container_combo_box_remove_item  (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gpointer                insert_data);
static void     gimp_container_combo_box_reorder_item (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gint                    new_index,
                                                       gpointer                insert_data);
static void     gimp_container_combo_box_rename_item  (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gpointer                insert_data);
static gboolean  gimp_container_combo_box_select_item (GimpContainerView      *view,
                                                       GimpViewable           *viewable,
                                                       gpointer                insert_data);
static void     gimp_container_combo_box_clear_items  (GimpContainerView      *view);
static void gimp_container_combo_box_set_preview_size (GimpContainerView      *view);

static void     gimp_container_combo_box_changed      (GtkComboBox            *combo_box,
                                                       GimpContainerView      *view);
static void gimp_container_combo_box_renderer_update  (GimpViewRenderer       *renderer,
                                                       GimpContainerView      *view);


static GtkComboBoxClass           *parent_class      = NULL;
static GimpContainerViewInterface *parent_view_iface = NULL;


GType
gimp_container_combo_box_get_type (void)
{
  static GType view_type = 0;

  if (! view_type)
    {
      static const GTypeInfo view_info =
      {
        sizeof (GimpContainerComboBoxClass),
        NULL,           /* base_init      */
        NULL,           /* base_finalize  */
        (GClassInitFunc) gimp_container_combo_box_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpContainerComboBox),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_container_combo_box_init,
      };

      static const GInterfaceInfo view_iface_info =
      {
        (GInterfaceInitFunc) gimp_container_combo_box_view_iface_init,
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      view_type = g_type_register_static (GTK_TYPE_COMBO_BOX,
                                          "GimpContainerComboBox",
                                          &view_info, 0);

      g_type_add_interface_static (view_type, GIMP_TYPE_CONTAINER_VIEW,
                                   &view_iface_info);
    }

  return view_type;
}

static void
gimp_container_combo_box_class_init (GimpContainerComboBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_container_view_set_property;
  object_class->get_property = gimp_container_view_get_property;

  g_object_class_override_property (object_class,
                                    GIMP_CONTAINER_VIEW_PROP_CONTAINER,
                                    "container");
  g_object_class_override_property (object_class,
                                    GIMP_CONTAINER_VIEW_PROP_CONTEXT,
                                    "context");
  g_object_class_override_property (object_class,
                                    GIMP_CONTAINER_VIEW_PROP_REORDERABLE,
                                    "reorderable");
  g_object_class_override_property (object_class,
                                    GIMP_CONTAINER_VIEW_PROP_PREVIEW_SIZE,
                                    "preview-size");
  g_object_class_override_property (object_class,
                                    GIMP_CONTAINER_VIEW_PROP_PREVIEW_BORDER_WIDTH,
                                    "preview-border-width");
}

static void
gimp_container_combo_box_init (GimpContainerComboBox *combo_box)
{
  GtkListStore    *store;
  GtkCellLayout   *layout;
  GtkCellRenderer *cell;

  store = gtk_list_store_new (NUM_COLUMNS,
                              GIMP_TYPE_VIEW_RENDERER,
                              G_TYPE_STRING);

  gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));

  g_object_unref (store);

  layout = GTK_CELL_LAYOUT (combo_box);

  cell = gimp_cell_renderer_viewable_new ();
  gtk_cell_layout_pack_start (layout, cell, FALSE);
  gtk_cell_layout_set_attributes (layout, cell,
                                  "renderer", COLUMN_RENDERER,
                                  NULL);

  combo_box->viewable_renderer = cell;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (layout, cell, TRUE);
  gtk_cell_layout_set_attributes (layout, cell,
                                  "text", COLUMN_NAME,
                                  NULL);

  g_signal_connect (combo_box, "changed",
                    G_CALLBACK (gimp_container_combo_box_changed),
                    combo_box);

}

static void
gimp_container_combo_box_view_iface_init (GimpContainerViewInterface *view_iface)
{
  parent_view_iface = g_type_interface_peek_parent (view_iface);

  if (! parent_view_iface)
    parent_view_iface = g_type_default_interface_peek (GIMP_TYPE_CONTAINER_VIEW);

  view_iface->insert_item      = gimp_container_combo_box_insert_item;
  view_iface->remove_item      = gimp_container_combo_box_remove_item;
  view_iface->reorder_item     = gimp_container_combo_box_reorder_item;
  view_iface->rename_item      = gimp_container_combo_box_rename_item;
  view_iface->select_item      = gimp_container_combo_box_select_item;
  view_iface->clear_items      = gimp_container_combo_box_clear_items;
  view_iface->set_preview_size = gimp_container_combo_box_set_preview_size;

  view_iface->insert_data_free = (GDestroyNotify) g_free;

}

GtkWidget *
gimp_container_combo_box_new (GimpContainer *container,
                              GimpContext   *context,
                              gint           preview_size,
                              gint           preview_border_width)
{
  GtkWidget         *combo_box;
  GimpContainerView *view;

  g_return_val_if_fail (container == NULL || GIMP_IS_CONTAINER (container),
                        NULL);
  g_return_val_if_fail (context == NULL || GIMP_IS_CONTEXT (context), NULL);

  combo_box = g_object_new (GIMP_TYPE_CONTAINER_COMBO_BOX, NULL);

  view = GIMP_CONTAINER_VIEW (combo_box);

  gimp_container_view_set_preview_size (view,
                                        preview_size, preview_border_width);

  if (container)
    gimp_container_view_set_container (view, container);

  if (context)
    gimp_container_view_set_context (view, context);

  return combo_box;
}

static void
gimp_container_combo_box_set (GimpContainerComboBox *combo_box,
                              GtkTreeIter           *iter,
                              GimpViewable          *viewable)
{
  GimpContainerView *view = GIMP_CONTAINER_VIEW (combo_box);
  GtkTreeModel      *model;
  GimpViewRenderer  *renderer;
  gchar             *name;
  gint               preview_size;
  gint               border_width;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));

  preview_size = gimp_container_view_get_preview_size (view, &border_width);

  name = gimp_viewable_get_description (viewable, NULL);

  renderer = gimp_view_renderer_new (G_TYPE_FROM_INSTANCE (viewable),
                                     preview_size, border_width,
                                     FALSE);
  gimp_view_renderer_set_viewable (renderer, viewable);
  gimp_view_renderer_remove_idle (renderer);

  g_signal_connect (renderer, "update",
                    G_CALLBACK (gimp_container_combo_box_renderer_update),
                    view);

  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      COLUMN_RENDERER, renderer,
                      COLUMN_NAME,     name,
                      -1);

  g_object_unref (renderer);
  g_free (name);
}

/*  GimpContainerView methods  */

static gpointer
gimp_container_combo_box_insert_item (GimpContainerView *view,
                                      GimpViewable      *viewable,
                                      gint               index)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
  GtkTreeIter  *iter;

  iter = g_new0 (GtkTreeIter, 1);

  if (index == -1)
    gtk_list_store_append (GTK_LIST_STORE (model), iter);
  else
    gtk_list_store_insert (GTK_LIST_STORE (model), iter, index);

  gimp_container_combo_box_set (GIMP_CONTAINER_COMBO_BOX (view),
                                iter, viewable);

  return (gpointer) iter;
}

static void
gimp_container_combo_box_remove_item (GimpContainerView *view,
                                      GimpViewable      *viewable,
                                      gpointer           insert_data)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
  GtkTreeIter  *iter  = insert_data;

  if (iter)
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), iter);

#ifdef __GNUC__
#warning FIXME: remove this hack as soon as bug #149906 is fixed
#endif
      /*  if the store is empty after this remove, clear out renderers
       *  from all cells so they don't keep refing the viewables
       */
      if (! gtk_tree_model_iter_n_children (model, NULL))
        g_object_set (GIMP_CONTAINER_COMBO_BOX (view)->viewable_renderer,
                      "renderer", NULL,
                      NULL);
    }
}

static void
gimp_container_combo_box_reorder_item (GimpContainerView *view,
                                       GimpViewable      *viewable,
                                       gint               new_index,
                                       gpointer           insert_data)
{
  GtkTreeModel  *model     = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
  GimpContainer *container = gimp_container_view_get_container (view);
  GtkTreeIter   *iter      = insert_data;

  if (!iter)
    return;

  if (new_index == -1 || new_index == container->num_children - 1)
    {
      gtk_list_store_move_before (GTK_LIST_STORE (model), iter, NULL);
    }
  else if (new_index == 0)
    {
      gtk_list_store_move_after (GTK_LIST_STORE (model), iter, NULL);
    }
  else
    {
      GtkTreePath *path;
      gint         old_index;

      path = gtk_tree_model_get_path (model, iter);
      old_index = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);

      if (new_index != old_index)
        {
          GtkTreeIter  place;

          path = gtk_tree_path_new_from_indices (new_index, -1);
          gtk_tree_model_get_iter (model, &place, path);
          gtk_tree_path_free (path);

          if (new_index > old_index)
            gtk_list_store_move_after (GTK_LIST_STORE (model), iter, &place);
          else
            gtk_list_store_move_before (GTK_LIST_STORE (model), iter, &place);
        }
    }
}

static void
gimp_container_combo_box_rename_item (GimpContainerView *view,
                                      GimpViewable      *viewable,
                                      gpointer           insert_data)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
  GtkTreeIter  *iter  = insert_data;

  if (iter)
    {
      gchar *name = gimp_viewable_get_description (viewable, NULL);

      gtk_list_store_set (GTK_LIST_STORE (model), iter,
                          COLUMN_NAME, name,
                          -1);

      g_free (name);
    }
}

static gboolean
gimp_container_combo_box_select_item (GimpContainerView *view,
                                      GimpViewable      *viewable,
                                      gpointer           insert_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (view);
  GtkTreeIter *iter      = insert_data;

  g_signal_handlers_block_by_func (combo_box,
                                   gimp_container_combo_box_changed,
                                   view);

  if (iter)
    {
      gtk_combo_box_set_active_iter (combo_box, iter);
    }
  else
    {
      gtk_combo_box_set_active (combo_box, -1);
    }

  g_signal_handlers_unblock_by_func (combo_box,
                                     gimp_container_combo_box_changed,
                                     view);

  return TRUE;
}

static void
gimp_container_combo_box_clear_items (GimpContainerView *view)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));

  gtk_list_store_clear (GTK_LIST_STORE (model));

  parent_view_iface->clear_items (view);
}

static void
gimp_container_combo_box_set_preview_size (GimpContainerView *view)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
  GtkTreeIter   iter;
  gboolean      iter_valid;
  gint          preview_size;
  gint          border_width;

  preview_size = gimp_container_view_get_preview_size (view, &border_width);

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter);
       iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      GimpViewRenderer *renderer;

      gtk_tree_model_get (model, &iter,
                          COLUMN_RENDERER, &renderer,
                          -1);

      gimp_view_renderer_set_size (renderer, preview_size, border_width);
      g_object_unref (renderer);
    }
}

static void
gimp_container_combo_box_changed (GtkComboBox       *combo_box,
                                  GimpContainerView *view)
{
  GtkTreeIter iter;

  if (parent_class->changed)
    parent_class->changed (combo_box);

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GimpViewRenderer *renderer;

      gtk_tree_model_get (gtk_combo_box_get_model (combo_box), &iter,
                          COLUMN_RENDERER, &renderer,
                          -1);

      gimp_container_view_item_selected (view, renderer->viewable);
      g_object_unref (renderer);
    }
}

static void
gimp_container_combo_box_renderer_update (GimpViewRenderer  *renderer,
                                          GimpContainerView *view)
{
  GtkTreeIter *iter = gimp_container_view_lookup (view, renderer->viewable);

  if (iter)
    {
      GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));
      GtkTreePath  *path;

      path = gtk_tree_model_get_path (model, iter);
      gtk_tree_model_row_changed (model, path, iter);
      gtk_tree_path_free (path);
    }
}