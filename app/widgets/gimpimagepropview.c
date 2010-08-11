/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpImagePropView
 * Copyright (C) 2005  Michael Natterer <mitch@gimp.org>
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

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpcontainer.h"
#include "core/gimpimage.h"
#include "core/gimpundostack.h"
#include "core/gimpunit.h"
#include "core/gimp-utils.h"

#include "gimpimagepropview.h"
#include "gimppropwidgets.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_IMAGE
};


static GObject * gimp_image_prop_view_constructor  (GType              type,
                                                    guint              n_params,
                                                    GObjectConstructParam *params);
static void      gimp_image_prop_view_set_property (GObject           *object,
                                                    guint              property_id,
                                                    const GValue      *value,
                                                    GParamSpec        *pspec);
static void      gimp_image_prop_view_get_property (GObject           *object,
                                                    guint              property_id,
                                                    GValue            *value,
                                                    GParamSpec        *pspec);

static GtkWidget * gimp_image_prop_view_add_label  (GtkTable          *table,
                                                    gint               row,
                                                    const gchar       *text);
static void        gimp_image_prop_view_undo_event (GimpImage         *gimage,
                                                    GimpUndoEvent      event,
                                                    GimpUndo          *undo,
                                                    GimpImagePropView *view);
static void        gimp_image_prop_view_update     (GimpImagePropView *view);


G_DEFINE_TYPE (GimpImagePropView, gimp_image_prop_view, GTK_TYPE_TABLE);

#define parent_class gimp_image_prop_view_parent_class


static void
gimp_image_prop_view_class_init (GimpImagePropViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor  = gimp_image_prop_view_constructor;
  object_class->set_property = gimp_image_prop_view_set_property;
  object_class->get_property = gimp_image_prop_view_get_property;

  g_object_class_install_property (object_class, PROP_IMAGE,
                                   g_param_spec_object ("image", NULL, NULL,
                                                        GIMP_TYPE_IMAGE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_image_prop_view_init (GimpImagePropView *view)
{
  gtk_table_set_col_spacings (GTK_TABLE (view), 6);
  gtk_table_set_row_spacings (GTK_TABLE (view), 3);
}

static void
gimp_image_prop_view_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpImagePropView *view = GIMP_IMAGE_PROP_VIEW (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      view->image = GIMP_IMAGE (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_image_prop_view_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GimpImagePropView *view = GIMP_IMAGE_PROP_VIEW (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, view->image);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static GObject *
gimp_image_prop_view_constructor (GType                  type,
                                  guint                  n_params,
                                  GObjectConstructParam *params)
{
  GimpImagePropView *view;
  GtkTable          *table;
  GObject           *object;
  gint               row = 0;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  view = GIMP_IMAGE_PROP_VIEW (object);
  table = GTK_TABLE (view);

  g_assert (view->image != NULL);

  view->pixel_size_label =
    gimp_image_prop_view_add_label (table, row++, _("Pixel dimensions:"));

  view->print_size_label =
    gimp_image_prop_view_add_label (table, row++, _("Print size:"));

  view->resolution_label =
    gimp_image_prop_view_add_label (table, row++, _("Resolution:"));

  view->colorspace_label =
    gimp_image_prop_view_add_label (table, row, _("Color space:"));

  gtk_table_set_row_spacing (GTK_TABLE (view), row++, 12);

  view->memsize_label =
    gimp_image_prop_view_add_label (table, row++, _("Size in memory:"));

  view->undo_label =
    gimp_image_prop_view_add_label (table, row++, _("Undo steps:"));

  view->redo_label =
    gimp_image_prop_view_add_label (table, row, _("Redo steps:"));

  gtk_table_set_row_spacing (GTK_TABLE (view), row++, 12);

  view->pixels_label =
    gimp_image_prop_view_add_label (table, row++, _("Number of pixels:"));

  view->layers_label =
    gimp_image_prop_view_add_label (table, row++, _("Number of layers:"));

  view->channels_label =
    gimp_image_prop_view_add_label (table, row++, _("Number of channels:"));

  view->vectors_label =
    gimp_image_prop_view_add_label (table, row++, _("Number of paths:"));

  g_signal_connect_object (view->image, "size-changed",
                           G_CALLBACK (gimp_image_prop_view_update),
                           G_OBJECT (view),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view->image, "resolution-changed",
                           G_CALLBACK (gimp_image_prop_view_update),
                           G_OBJECT (view),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view->image, "unit-changed",
                           G_CALLBACK (gimp_image_prop_view_update),
                           G_OBJECT (view),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view->image, "mode-changed",
                           G_CALLBACK (gimp_image_prop_view_update),
                           G_OBJECT (view),
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view->image, "undo-event",
                           G_CALLBACK (gimp_image_prop_view_undo_event),
                           G_OBJECT (view),
                           0);

  gimp_image_prop_view_update (view);

  return object;
}


/*  public functions  */

GtkWidget *
gimp_image_prop_view_new (GimpImage *image)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);

  return g_object_new (GIMP_TYPE_IMAGE_PROP_VIEW,
                       "image", image,
                       NULL);
}


/*  private functions  */

static GtkWidget *
gimp_image_prop_view_add_label (GtkTable    *table,
                                gint         row,
                                const gchar *text)
{
  GtkWidget *label;
  GtkWidget *desc;

  desc = g_object_new (GTK_TYPE_LABEL,
                       "label",  text,
                       "xalign", 1.0,
                       "yalign", 0.5,
                       NULL);
  gimp_label_set_attributes (GTK_LABEL (desc),
                             PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD,
                             -1);
  gtk_table_attach (table, desc,
                    0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (desc);

  label = g_object_new (GTK_TYPE_LABEL,
                        "xalign",     0.0,
                        "yalign",     0.5,
                        "selectable", TRUE,
                        NULL);
  gtk_table_attach (table, label,
                    1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  return label;
}

static void
gimp_image_prop_view_label_set_memsize (GtkWidget  *label,
                                        GimpObject *object)
{
  gchar *str = gimp_memsize_to_string (gimp_object_get_memsize (object, NULL));

  gtk_label_set_text (GTK_LABEL (label), str);
  g_free (str);
}

static void
gimp_image_prop_view_label_set_undo (GtkWidget     *label,
                                     GimpUndoStack *stack)
{
  gint steps = gimp_undo_stack_get_depth (stack);

  if (steps > 0)
    {
      GimpObject *object = GIMP_OBJECT (stack);
      gchar      *str;
      gchar       buf[256];

      str = gimp_memsize_to_string (gimp_object_get_memsize (object, NULL));
      g_snprintf (buf, sizeof (buf), "%d (%s)", steps, str);
      g_free (str);

      gtk_label_set_text (GTK_LABEL (label), buf);
    }
  else
    {
      /*  no undo (or redo) steps available  */
      gtk_label_set_text (GTK_LABEL (label), _("None"));
    }
}

static void
gimp_image_prop_view_undo_event (GimpImage         *gimage,
                                 GimpUndoEvent      event,
                                 GimpUndo          *undo,
                                 GimpImagePropView *view)
{
  gimp_image_prop_view_update (view);
}

static void
gimp_image_prop_view_update (GimpImagePropView *view)
{
  GimpImage         *image = view->image;
  GimpImageBaseType  type;
  GimpUnit           unit;
  gdouble            unit_factor;
  gint               unit_digits;
  const gchar       *desc;
  gchar              format_buf[32];
  gchar              buf[256];

  /*  pixel size  */
  g_snprintf (buf, sizeof (buf), ngettext ("%d x %d pixel",
                                           "%d x %d pixels", image->height),
              image->width, image->height);
  gtk_label_set_text (GTK_LABEL (view->pixel_size_label), buf);

  /*  print size  */
  unit = gimp_get_default_unit ();

  unit_factor = _gimp_unit_get_factor (image->gimp, unit);
  unit_digits = _gimp_unit_get_digits (image->gimp, unit);

  g_snprintf (format_buf, sizeof (format_buf), "%%.%df x %%.%df %s",
              unit_digits + 1, unit_digits + 1,
              _gimp_unit_get_plural (image->gimp, unit));
  g_snprintf (buf, sizeof (buf), format_buf,
              image->width  * unit_factor / image->xresolution,
              image->height * unit_factor / image->yresolution);
  gtk_label_set_text (GTK_LABEL (view->print_size_label), buf);

  /*  resolution  */
  unit = gimp_image_get_unit (image);
  unit_factor = _gimp_unit_get_factor (image->gimp, unit);

  g_snprintf (format_buf, sizeof (format_buf), _("pixels/%s"),
              _gimp_unit_get_abbreviation (image->gimp, unit));
  g_snprintf (buf, sizeof (buf), _("%g x %g %s"),
              image->xresolution / unit_factor,
              image->yresolution / unit_factor,
              unit == GIMP_UNIT_INCH ? _("dpi") : format_buf);
  gtk_label_set_text (GTK_LABEL (view->resolution_label), buf);

  /*  color type  */
  type = gimp_image_base_type (image);

  gimp_enum_get_value (GIMP_TYPE_IMAGE_BASE_TYPE, type,
                       NULL, NULL, &desc, NULL);

  switch (type)
    {
    case GIMP_RGB:
    case GIMP_GRAY:
      g_snprintf (buf, sizeof (buf), "%s", desc);
      break;
    case GIMP_INDEXED:
      g_snprintf (buf, sizeof (buf),
                  "%s (%d %s)", desc, image->num_cols, _("colors"));
      break;
    }

  gtk_label_set_text (GTK_LABEL (view->colorspace_label), buf);

  /*  size in memory  */
  gimp_image_prop_view_label_set_memsize (view->memsize_label,
                                          GIMP_OBJECT (image));

  /*  undo / redo  */
  gimp_image_prop_view_label_set_undo (view->undo_label, image->undo_stack);
  gimp_image_prop_view_label_set_undo (view->redo_label, image->redo_stack);

  /*  number of layers  */
  g_snprintf (buf, sizeof (buf), "%d", image->width * image->height);
  gtk_label_set_text (GTK_LABEL (view->pixels_label), buf);

  /*  number of layers  */
  g_snprintf (buf, sizeof (buf), "%d",
              gimp_container_num_children (image->layers));
  gtk_label_set_text (GTK_LABEL (view->layers_label), buf);

  /*  number of channels  */
  g_snprintf (buf, sizeof (buf), "%d",
              gimp_container_num_children (image->channels));
  gtk_label_set_text (GTK_LABEL (view->channels_label), buf);

  /*  number of vectors  */
  g_snprintf (buf, sizeof (buf), "%d",
              gimp_container_num_children (image->vectors));
  gtk_label_set_text (GTK_LABEL (view->vectors_label), buf);
}
