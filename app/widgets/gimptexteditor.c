/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpTextEditor
 * Copyright (C) 2002-2003  Sven Neumann <sven@gimp.org>
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

#include "core/gimpmarshal.h"

#include "gimphelp-ids.h"
#include "gimpmenufactory.h"
#include "gimptexteditor.h"
#include "gimpuimanager.h"

#include "gimp-intl.h"


enum
{
  TEXT_CHANGED,
  DIR_CHANGED,
  LAST_SIGNAL
};


static void      gimp_text_editor_class_init    (GimpTextEditorClass *klass);
static void      gimp_text_editor_init          (GimpTextEditor      *editor);

static void      gimp_text_editor_text_changed  (GtkTextBuffer       *buffer,
                                                 GimpTextEditor      *editor);


static GimpDialogClass *parent_class = NULL;
static guint            text_editor_signals[LAST_SIGNAL] = { 0 };


GType
gimp_text_editor_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpTextEditorClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_text_editor_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpTextEditor),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_text_editor_init,
      };

      type = g_type_register_static (GIMP_TYPE_DIALOG,
                                     "GimpTextEditor",
                                     &info, 0);
    }

  return type;
}

static void
gimp_text_editor_class_init (GimpTextEditorClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);

  text_editor_signals[TEXT_CHANGED] =
    g_signal_new ("text_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpTextEditorClass, text_changed),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  text_editor_signals[DIR_CHANGED] =
    g_signal_new ("dir_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpTextEditorClass, dir_changed),
		  NULL, NULL,
		  gimp_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  klass->text_changed = NULL;
  klass->dir_changed  = NULL;
}

static void
gimp_text_editor_init (GimpTextEditor  *editor)
{
  editor->view        = NULL;
  editor->file_dialog = NULL;
  editor->ui_manager  = NULL;

  switch (gtk_widget_get_default_direction ())
    {
    case GTK_TEXT_DIR_NONE:
    case GTK_TEXT_DIR_LTR:
      editor->base_dir = GIMP_TEXT_DIRECTION_LTR;
      break;
    case GTK_TEXT_DIR_RTL:
      editor->base_dir = GIMP_TEXT_DIRECTION_RTL;
      break;
    }
}


/*  public functions  */

GtkWidget *
gimp_text_editor_new (const gchar     *title,
                      GimpMenuFactory *menu_factory)
{
  GimpTextEditor *editor;
  GtkTextBuffer  *buffer;
  GtkWidget      *toolbar;
  GtkWidget      *scrolled_window;

  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_MENU_FACTORY (menu_factory), NULL);

  editor = g_object_new (GIMP_TYPE_TEXT_EDITOR,
                         "title",     title,
                         "role",      "gimp-text-editor",
                         "help-func", gimp_standard_help_func,
                         "help-id",   GIMP_HELP_TEXT_EDITOR_DIALOG,
                         NULL);

  gtk_dialog_add_button (GTK_DIALOG (editor),
                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  g_signal_connect (editor, "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  editor->ui_manager = gimp_menu_factory_manager_new (menu_factory,
                                                      "<TextEditor>",
                                                      editor, FALSE);

  toolbar = gimp_ui_manager_ui_get (editor->ui_manager,
                                    "/text-editor-toolbar");

  if (toolbar)
    {
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox), toolbar,
                          FALSE, FALSE, 0);
      gtk_widget_show (toolbar);
    }

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox),
                      scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  editor->view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled_window), editor->view);
  gtk_widget_show (editor->view);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->view));

  g_signal_connect (buffer, "changed",
		    G_CALLBACK (gimp_text_editor_text_changed),
		    editor);

  switch (editor->base_dir)
    {
    case GIMP_TEXT_DIRECTION_LTR:
      gtk_widget_set_direction (editor->view, GTK_TEXT_DIR_LTR);
      break;
    case GIMP_TEXT_DIRECTION_RTL:
      gtk_widget_set_direction (editor->view, GTK_TEXT_DIR_RTL);
      break;
    }

  gtk_widget_set_size_request (editor->view, 128, 64);

  gtk_widget_grab_focus (editor->view);

  gimp_ui_manager_update (editor->ui_manager, editor);

  return GTK_WIDGET (editor);
}

void
gimp_text_editor_set_text (GimpTextEditor *editor,
                           const gchar    *text,
                           gint            len)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GIMP_IS_TEXT_EDITOR (editor));
  g_return_if_fail (text != NULL || len == 0);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->view));

  if (text)
    gtk_text_buffer_set_text (buffer, text, len);
  else
    gtk_text_buffer_set_text (buffer, "", 0);
}

gchar *
gimp_text_editor_get_text (GimpTextEditor *editor)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter;
  GtkTextIter    end_iter;

  g_return_val_if_fail (GIMP_IS_TEXT_EDITOR (editor), NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->view));

  gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);

  return gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
}

void
gimp_text_editor_set_direction (GimpTextEditor    *editor,
                                GimpTextDirection  base_dir)
{
  g_return_if_fail (GIMP_IS_TEXT_EDITOR (editor));

  if (editor->base_dir == base_dir)
    return;

  editor->base_dir = base_dir;

  if (editor->view)
    {
      switch (editor->base_dir)
        {
        case GIMP_TEXT_DIRECTION_LTR:
          gtk_widget_set_direction (editor->view, GTK_TEXT_DIR_LTR);
          break;
        case GIMP_TEXT_DIRECTION_RTL:
          gtk_widget_set_direction (editor->view, GTK_TEXT_DIR_RTL);
          break;
        }
    }

  gimp_ui_manager_update (editor->ui_manager, editor);

  g_signal_emit (editor, text_editor_signals[DIR_CHANGED], 0);
}

GimpTextDirection
gimp_text_editor_get_direction (GimpTextEditor *editor)
{
  g_return_val_if_fail (GIMP_IS_TEXT_EDITOR (editor), GIMP_TEXT_DIRECTION_LTR);

  return editor->base_dir;
}


/*  private functions  */

static void
gimp_text_editor_text_changed (GtkTextBuffer  *buffer,
                               GimpTextEditor *editor)
{
  g_signal_emit (editor, text_editor_signals[TEXT_CHANGED], 0);
}
