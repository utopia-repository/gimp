/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpgradientselect.c
 * Copyright (C) 2004 Michael Natterer <mitch@gimp.org>
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

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpgradient.h"

#include "pdb/procedural_db.h"

#include "gimpcontainerbox.h"
#include "gimpdatafactoryview.h"
#include "gimpgradientselect.h"


enum
{
  PROP_0,
  PROP_SAMPLE_SIZE
};


static void   gimp_gradient_select_class_init (GimpGradientSelectClass *klass);

static GObject  * gimp_gradient_select_constructor  (GType          type,
                                                     guint          n_params,
                                                     GObjectConstructParam *params);
static void       gimp_gradient_select_set_property (GObject       *object,
                                                     guint          property_id,
                                                     const GValue  *value,
                                                     GParamSpec    *pspec);

static Argument * gimp_gradient_select_run_callback (GimpPdbDialog *dialog,
                                                     GimpObject    *object,
                                                     gboolean       closing,
                                                     gint          *n_return_vals);


static GimpPdbDialogClass *parent_class = NULL;


GType
gimp_gradient_select_get_type (void)
{
  static GType dialog_type = 0;

  if (! dialog_type)
    {
      static const GTypeInfo dialog_info =
      {
        sizeof (GimpGradientSelectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_gradient_select_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpGradientSelect),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      dialog_type = g_type_register_static (GIMP_TYPE_PDB_DIALOG,
                                            "GimpGradientSelect",
                                            &dialog_info, 0);
    }

  return dialog_type;
}

static void
gimp_gradient_select_class_init (GimpGradientSelectClass *klass)
{
  GObjectClass       *object_class = G_OBJECT_CLASS (klass);
  GimpPdbDialogClass *pdb_class    = GIMP_PDB_DIALOG_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructor  = gimp_gradient_select_constructor;
  object_class->set_property = gimp_gradient_select_set_property;

  pdb_class->run_callback    = gimp_gradient_select_run_callback;

  g_object_class_install_property (object_class, PROP_SAMPLE_SIZE,
                                   g_param_spec_int ("sample-size", NULL, NULL,
                                                     0, 10000, 84,
                                                     G_PARAM_WRITABLE |
                                                     G_PARAM_CONSTRUCT_ONLY));
}

static GObject *
gimp_gradient_select_constructor (GType                  type,
                                  guint                  n_params,
                                  GObjectConstructParam *params)
{
  GObject       *object;
  GimpPdbDialog *dialog;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  dialog = GIMP_PDB_DIALOG (object);

  dialog->view =
    gimp_data_factory_view_new (GIMP_VIEW_TYPE_LIST,
                                dialog->context->gimp->gradient_factory,
                                dialog->context,
                                GIMP_VIEW_SIZE_MEDIUM, 1,
                                dialog->menu_factory, "<Gradients>",
                                "/gradients-popup",
                                "gradients");

  gimp_container_box_set_size_request (GIMP_CONTAINER_BOX (GIMP_CONTAINER_EDITOR (dialog->view)->view),
                                       6 * (GIMP_VIEW_SIZE_MEDIUM + 2),
                                       6 * (GIMP_VIEW_SIZE_MEDIUM + 2));

  gtk_container_set_border_width (GTK_CONTAINER (dialog->view), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog->view);
  gtk_widget_show (dialog->view);

  return object;
}

static Argument *
gimp_gradient_select_run_callback (GimpPdbDialog *dialog,
                                   GimpObject    *object,
                                   gboolean       closing,
                                   gint          *n_return_vals)
{
  GimpGradient *gradient = GIMP_GRADIENT (object);
  gdouble      *values, *pv;
  gdouble       pos, delta;
  GimpRGB       color;
  gint          i;

  i      = GIMP_GRADIENT_SELECT (dialog)->sample_size;
  pos    = 0.0;
  delta  = 1.0 / (i - 1);

  values = g_new (gdouble, 4 * i);
  pv     = values;

  while (i--)
    {
      gimp_gradient_get_color_at (gradient, pos, FALSE, &color);

      *pv++ = color.r;
      *pv++ = color.g;
      *pv++ = color.b;
      *pv++ = color.a;

      pos += delta;
    }

  return procedural_db_run_proc (dialog->caller_context->gimp,
                                 dialog->caller_context,
                                 NULL,
                                 dialog->callback_name,
                                 n_return_vals,
                                 GIMP_PDB_STRING,     GIMP_OBJECT (gradient)->name,
                                 GIMP_PDB_INT32,      GIMP_GRADIENT_SELECT (dialog)->sample_size * 4,
                                 GIMP_PDB_FLOATARRAY, values,
                                 GIMP_PDB_INT32,      closing,
                                 GIMP_PDB_END);
}

static void
gimp_gradient_select_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpGradientSelect *select = GIMP_GRADIENT_SELECT (object);

  switch (property_id)
    {
    case PROP_SAMPLE_SIZE:
      select->sample_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}