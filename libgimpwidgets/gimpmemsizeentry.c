/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpmemsizeentry.c
 * Copyright (C) 2000-2003  Sven Neumann <sven@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gimpwidgetstypes.h"

#include "gimpmemsizeentry.h"
#include "gimpwidgets.h"

#include "libgimp/libgimp-intl.h"


enum
{
  VALUE_CHANGED,
  LAST_SIGNAL
};

static void  gimp_memsize_entry_class_init    (GimpMemsizeEntryClass *klass);
static void  gimp_memsize_entry_init          (GimpMemsizeEntry      *entry);
static void  gimp_memsize_entry_finalize      (GObject               *object);

static void  gimp_memsize_entry_adj_callback  (GtkAdjustment         *adj,
					       GimpMemsizeEntry      *entry);
static void  gimp_memsize_entry_unit_callback (GtkWidget             *widget,
					       GimpMemsizeEntry      *entry);


static guint         gimp_memsize_entry_signals[LAST_SIGNAL] = { 0 };
static GtkHBoxClass *parent_class                            = NULL;


GType
gimp_memsize_entry_get_type (void)
{
  static GType entry_type = 0;

  if (! entry_type)
    {
      static const GTypeInfo entry_info =
      {
        sizeof (GimpMemsizeEntryClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_memsize_entry_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpMemsizeEntry),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_memsize_entry_init,
      };

      entry_type = g_type_register_static (GTK_TYPE_HBOX, "GimpMemsizeEntry",
					   &entry_info, 0);
    }

  return entry_type;
}

static void
gimp_memsize_entry_class_init (GimpMemsizeEntryClass *klass)
{
  GObjectClass *object_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gimp_memsize_entry_finalize;

  gimp_memsize_entry_signals[VALUE_CHANGED] =
    g_signal_new ("value_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpMemsizeEntryClass, value_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  klass->value_changed = NULL;
}

static void
gimp_memsize_entry_init (GimpMemsizeEntry *entry)
{
  gtk_box_set_spacing (GTK_BOX (entry), 4);

  entry->value      = 0;
  entry->lower      = 0;
  entry->upper      = 0;
  entry->shift      = 0;
  entry->adjustment = NULL;
  entry->menu       = NULL;
}

static void
gimp_memsize_entry_finalize (GObject *object)
{
  GimpMemsizeEntry *entry = (GimpMemsizeEntry *) object;

  if (entry->adjustment)
    {
      g_object_unref (entry->adjustment);
      entry->adjustment = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_memsize_entry_adj_callback (GtkAdjustment    *adj,
				 GimpMemsizeEntry *entry)
{
  guint64 size = gtk_adjustment_get_value (adj);

  entry->value = size << entry->shift;

  g_signal_emit (entry, gimp_memsize_entry_signals[VALUE_CHANGED], 0);
}

static void
gimp_memsize_entry_unit_callback (GtkWidget        *widget,
				  GimpMemsizeEntry *entry)
{
  guint  shift;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), (gint *) &shift);

#if _MSC_VER < 1300
#  define CAST (gint64)
#else
#  define CAST
#endif

  if (shift != entry->shift)
    {
      entry->shift = shift;

      entry->adjustment->value = CAST entry->value >> shift;
      entry->adjustment->lower = CAST entry->lower >> shift;
      entry->adjustment->upper = CAST entry->upper >> shift;

      gtk_adjustment_value_changed (entry->adjustment);
      gtk_adjustment_changed (entry->adjustment);
    }
#undef CAST
}


/**
 * gimp_memsize_entry_new:
 * @value: the initial value (in Bytes)
 * @lower: the lower limit for the value (in Bytes)
 * @upper: the upper limit for the value (in Bytes)
 *
 * Creates a new #GimpMemsizeEntry which is a #GtkHBox with a #GtkSpinButton
 * and a #GtkOptionMenu all setup to allow the user to enter memory sizes.
 *
 * Returns: Pointer to the new #GimpMemsizeEntry.
 **/
GtkWidget *
gimp_memsize_entry_new (guint64  value,
			guint64  lower,
			guint64  upper)
{
  GimpMemsizeEntry *entry;
  guint             shift;

#if _MSC_VER < 1300
#  define CAST (gint64)
#else
#  define CAST
#endif

  g_return_val_if_fail (value >= lower && value <= upper, NULL);

  entry = g_object_new (GIMP_TYPE_MEMSIZE_ENTRY, NULL);

  for (shift = 30; shift > 10; shift -= 10)
    {
      if (value > ((guint64) 1 << shift) &&
          value % ((guint64) 1 << shift) == 0)
        break;
    }

  entry->value = value;
  entry->lower = lower;
  entry->upper = upper;
  entry->shift = shift;

  entry->spinbutton = gimp_spin_button_new ((GtkObject **) &entry->adjustment,
                                            CAST (value >> shift),
                                            CAST (lower >> shift),
                                            CAST (upper >> shift),
                                            1, 8, 0, 1, 0);

#undef CAST

  g_object_ref (entry->adjustment);
  gtk_object_sink (GTK_OBJECT (entry->adjustment));

  gtk_entry_set_width_chars (GTK_ENTRY (entry->spinbutton), 10);
  gtk_box_pack_start (GTK_BOX (entry), entry->spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (entry->spinbutton);

  g_signal_connect (entry->adjustment, "value_changed",
                    G_CALLBACK (gimp_memsize_entry_adj_callback),
                    entry);

  entry->menu = gimp_int_combo_box_new (_("Kilobytes"), 10,
                                        _("Megabytes"), 20,
                                        _("Gigabytes"), 30,
                                        NULL);

  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (entry->menu), shift);

  g_signal_connect (entry->menu, "changed",
                    G_CALLBACK (gimp_memsize_entry_unit_callback),
                    entry);

  gtk_box_pack_start (GTK_BOX (entry), entry->menu, FALSE, FALSE, 0);
  gtk_widget_show (entry->menu);

  return GTK_WIDGET (entry);
}

/**
 * gimp_memsize_entry_set_value:
 * @entry: a #GimpMemsizeEntry
 * @value: the new value (in Bytes)
 *
 * Sets the @entry's value. Please note that the #GimpMemsizeEntry rounds
 * the value to full Kilobytes.
 **/
void
gimp_memsize_entry_set_value (GimpMemsizeEntry *entry,
			      guint64           value)
{
  guint shift;

  g_return_if_fail (GIMP_IS_MEMSIZE_ENTRY (entry));
  g_return_if_fail (value >= entry->lower && value <= entry->upper);

  for (shift = 30; shift > 10; shift -= 10)
    {
      if (value > ((guint64) 1 << shift) &&
          value % ((guint64) 1 << shift) == 0)
        break;
    }

  if (shift != entry->shift)
    {
      entry->shift = shift;
      entry->value = value;

      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (entry->menu), shift);
    }

#if _MSC_VER < 1300
#  define CAST (gint64)
#else
#  define CAST
#endif

  gtk_adjustment_set_value (entry->adjustment, CAST (value >> shift));

#undef CASE
}

/**
 * gimp_memsize_entry_get_value:
 * @entry: a #GimpMemsizeEntry
 *
 * Retrieves the current value from a #GimpMemsizeEntry.
 *
 * Returns: the current value of @entry (in Bytes).
 **/
guint64
gimp_memsize_entry_get_value (GimpMemsizeEntry *entry)
{
  g_return_val_if_fail (GIMP_IS_MEMSIZE_ENTRY (entry), 0);

  return entry->value;
}