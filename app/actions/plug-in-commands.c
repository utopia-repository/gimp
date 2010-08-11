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

#include "actions-types.h"

#include "core/gimp.h"
#include "core/gimp-utils.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimpparamspecs.h"
#include "core/gimpprogress.h"

#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginmanager-data.h"

#include "pdb/gimpprocedure.h"

#include "widgets/gimpbufferview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpdatafactoryview.h"
#include "widgets/gimpfontview.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpimageeditor.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimpmessagebox.h"
#include "widgets/gimpmessagedialog.h"

#include "display/gimpdisplay.h"

#include "actions.h"
#include "plug-in-commands.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static gint  plug_in_collect_data_args     (GtkAction   *action,
                                            GimpObject  *object,
                                            GValueArray *args,
                                            gint         n_args);
static gint  plug_in_collect_image_args    (GtkAction   *action,
                                            GimpImage   *image,
                                            GValueArray *args,
                                            gint         n_args);
static gint  plug_in_collect_item_args     (GtkAction   *action,
                                            GimpImage   *image,
                                            GimpItem    *item,
                                            GValueArray *args,
                                            gint         n_args);
static gint  plug_in_collect_drawable_args (GtkAction   *action,
                                            GimpImage   *image,
                                            GValueArray *args,
                                            gint         n_args);
static void  plug_in_reset_all_response    (GtkWidget   *dialog,
                                            gint         response_id,
                                            Gimp        *gimp);


/*  public functions  */

void
plug_in_run_cmd_callback (GtkAction           *action,
                          GimpPlugInProcedure *proc,
                          gpointer             data)
{
  GimpProcedure *procedure = GIMP_PROCEDURE (proc);
  Gimp          *gimp;
  GValueArray   *args;
  gint           n_args    = 0;
  GimpDisplay   *display   = NULL;
  return_if_no_gimp (gimp, data);

  args = gimp_procedure_get_arguments (procedure);

  /* initialize the first argument  */
  g_value_set_int (&args->values[n_args], GIMP_RUN_INTERACTIVE);
  n_args++;

  switch (procedure->proc_type)
    {
    case GIMP_EXTENSION:
      break;

    case GIMP_PLUGIN:
    case GIMP_TEMPORARY:
      if (GIMP_IS_DATA_FACTORY_VIEW (data) ||
          GIMP_IS_FONT_VIEW (data)         ||
          GIMP_IS_BUFFER_VIEW (data))
        {
          GimpContainerEditor *editor = GIMP_CONTAINER_EDITOR (data);
          GimpContainer       *container;
          GimpContext         *context;
          GimpObject          *object;

          container = gimp_container_view_get_container (editor->view);
          context   = gimp_container_view_get_context (editor->view);

          object = gimp_context_get_by_type (context,
                                             container->children_type);

          n_args = plug_in_collect_data_args (action, object,
                                              args, n_args);
        }
      else if (GIMP_IS_IMAGE_EDITOR (data))
        {
          GimpImageEditor *editor = GIMP_IMAGE_EDITOR (data);
          GimpImage       *image;

          image = gimp_image_editor_get_image (editor);

          n_args = plug_in_collect_image_args (action, image,
                                               args, n_args);
        }
      else if (GIMP_IS_ITEM_TREE_VIEW (data))
        {
          GimpItemTreeView *view = GIMP_ITEM_TREE_VIEW (data);
          GimpImage        *image;
          GimpItem         *item;

          image = gimp_item_tree_view_get_image (view);

          if (image)
            item = GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_active_item (image);
          else
            item = NULL;

          n_args = plug_in_collect_item_args (action, image, item,
                                              args, n_args);
        }
      else
        {
          GimpImage *image;

          display = action_data_get_display (data);

          if (display)
            image = display->image;
          else
            image = NULL;

          n_args = plug_in_collect_drawable_args (action, image,
                                                  args, n_args);
        }
      break;

    default:
      g_error ("Unknown procedure type.");
      n_args = -1;
    }

  if (n_args >= 1)
    {
      gimp_value_array_truncate (args, n_args);

      /* run the plug-in procedure */
      gimp_procedure_execute_async (procedure,
                                    gimp, gimp_get_user_context (gimp),
                                    GIMP_PROGRESS (display), args,
                                    GIMP_OBJECT (display));

      /* remember only "standard" plug-ins */
      if (procedure->proc_type == GIMP_PLUGIN                 &&
          procedure->num_args  >= 3                           &&
          GIMP_IS_PARAM_SPEC_IMAGE_ID    (procedure->args[1]) &&
          GIMP_IS_PARAM_SPEC_DRAWABLE_ID (procedure->args[2]))
        {
          gimp_plug_in_manager_set_last_plug_in (gimp->plug_in_manager, proc);
        }
    }

  g_value_array_free (args);
}

void
plug_in_repeat_cmd_callback (GtkAction *action,
                             gint       value,
                             gpointer   data)
{
  GimpProcedure *procedure;
  Gimp          *gimp;
  GimpDisplay   *display;
  GimpDrawable  *drawable;
  gboolean       interactive = TRUE;
  return_if_no_gimp (gimp, data);
  return_if_no_display (display, data);

  drawable = gimp_image_active_drawable (display->image);
  if (! drawable)
    return;

  if (strcmp (gtk_action_get_name (action), "plug-in-repeat") == 0)
    interactive = FALSE;

  procedure = g_slist_nth_data (gimp->plug_in_manager->last_plug_ins, value);

  if (procedure)
    {
      GValueArray *args = gimp_procedure_get_arguments (procedure);

      g_value_set_int         (&args->values[0],
                               interactive ?
                               GIMP_RUN_INTERACTIVE : GIMP_RUN_WITH_LAST_VALS);
      gimp_value_set_image    (&args->values[1], display->image);
      gimp_value_set_drawable (&args->values[2], drawable);

      gimp_value_array_truncate (args, 3);

      /* run the plug-in procedure */
      gimp_procedure_execute_async (procedure, gimp,
                                    gimp_get_user_context (gimp),
                                    GIMP_PROGRESS (display), args,
                                    GIMP_OBJECT (display));

      g_value_array_free (args);
    }
}

void
plug_in_reset_all_cmd_callback (GtkAction *action,
                                gpointer   data)
{
  Gimp      *gimp;
  GtkWidget *dialog;
  return_if_no_gimp (gimp, data);

  dialog = gimp_message_dialog_new (_("Reset all Filters"), GIMP_STOCK_QUESTION,
                                    NULL, 0,
                                    gimp_standard_help_func, NULL,

                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                    GIMP_STOCK_RESET, GTK_RESPONSE_OK,

                                    NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (plug_in_reset_all_response),
                    gimp);

  gimp_message_box_set_primary_text (GIMP_MESSAGE_DIALOG (dialog)->box,
                                     _("Do you really want to reset all "
                                       "filters to default values?"));

  gtk_widget_show (dialog);
}


/*  private functions  */

static gint
plug_in_collect_data_args (GtkAction   *action,
                           GimpObject  *object,
                           GValueArray *args,
                           gint         n_args)
{
  if (args->n_values > n_args &&
      G_VALUE_HOLDS_STRING (&args->values[n_args]))
    {
      if (object)
        {
          g_value_set_string (&args->values[n_args],
                              gimp_object_get_name (object));
          n_args++;
        }
      else
        {
          g_warning ("Uh-oh, no active data object for the plug-in!");
          return -1;
        }
    }

  return n_args;
}

static gint
plug_in_collect_image_args (GtkAction   *action,
                            GimpImage   *image,
                            GValueArray *args,
                            gint         n_args)
{
  if (args->n_values > n_args &&
      GIMP_VALUE_HOLDS_IMAGE_ID (&args->values[n_args]))
    {
      if (image)
        {
          gimp_value_set_image (&args->values[n_args], image);
          n_args++;
        }
      else
        {
          g_warning ("Uh-oh, no active image for the plug-in!");
          return -1;
        }
    }

  return n_args;
}

static gint
plug_in_collect_item_args (GtkAction   *action,
                           GimpImage   *image,
                           GimpItem    *item,
                           GValueArray *args,
                           gint         n_args)
{
  if (args->n_values > n_args &&
      GIMP_VALUE_HOLDS_IMAGE_ID (&args->values[n_args]))
    {
      if (image)
        {
          gimp_value_set_image (&args->values[n_args], image);
          n_args++;

          if (args->n_values > n_args &&
              GIMP_VALUE_HOLDS_ITEM_ID (&args->values[n_args]))
            {
              if (item)
                {
                  gimp_value_set_item (&args->values[n_args], item);
                  n_args++;
                }
              else
                {
                  g_warning ("Uh-oh, no active item for the plug-in!");
                  return -1;
                }
            }
        }
    }

  return n_args;
}

static gint
plug_in_collect_drawable_args (GtkAction   *action,
                               GimpImage   *image,
                               GValueArray *args,
                               gint         n_args)
{
  if (args->n_values > n_args &&
      GIMP_VALUE_HOLDS_IMAGE_ID (&args->values[n_args]))
    {
      if (image)
        {
          gimp_value_set_image (&args->values[n_args], image);
          n_args++;

          if (args->n_values > n_args &&
              GIMP_VALUE_HOLDS_DRAWABLE_ID (&args->values[n_args]))
            {
              GimpDrawable *drawable = gimp_image_active_drawable (image);

              if (drawable)
                {
                  gimp_value_set_drawable (&args->values[n_args], drawable);
                  n_args++;
                }
              else
                {
                  g_warning ("Uh-oh, no active drawable for the plug-in!");
                  return -1;
                }
            }
        }
    }

  return n_args;
}

static void
plug_in_reset_all_response (GtkWidget *dialog,
                            gint       response_id,
                            Gimp      *gimp)
{
  gtk_widget_destroy (dialog);

  if (response_id == GTK_RESPONSE_OK)
    gimp_plug_in_manager_data_free (gimp->plug_in_manager);
}
