/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpcellrenderertoggle.c
 * Copyright (C) 2003-2004  Sven Neumann <sven@gimp.org>
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

#include <config.h>

#include <gtk/gtk.h>

#include "gimpwidgetstypes.h"

#include "gimpwidgetsmarshal.h"
#include "gimpcellrenderertoggle.h"


#define DEFAULT_ICON_SIZE  GTK_ICON_SIZE_BUTTON


enum
{
  CLICKED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_STOCK_ID,
  PROP_STOCK_SIZE
};


static void gimp_cell_renderer_toggle_class_init (GimpCellRendererToggleClass *klass);

static void gimp_cell_renderer_toggle_finalize     (GObject         *object);
static void gimp_cell_renderer_toggle_get_property (GObject         *object,
                                                    guint            param_id,
                                                    GValue          *value,
                                                    GParamSpec      *pspec);
static void gimp_cell_renderer_toggle_set_property (GObject         *object,
                                                    guint            param_id,
                                                    const GValue    *value,
                                                    GParamSpec      *pspec);
static void gimp_cell_renderer_toggle_get_size     (GtkCellRenderer *cell,
                                                    GtkWidget       *widget,
                                                    GdkRectangle    *rectangle,
                                                    gint            *x_offset,
                                                    gint            *y_offset,
                                                    gint            *width,
                                                    gint            *height);
static void gimp_cell_renderer_toggle_render       (GtkCellRenderer *cell,
                                                    GdkWindow       *window,
                                                    GtkWidget       *widget,
                                                    GdkRectangle    *background_area,
                                                    GdkRectangle    *cell_area,
                                                    GdkRectangle    *expose_area,
                                                    GtkCellRendererState flags);
static gboolean gimp_cell_renderer_toggle_activate (GtkCellRenderer *cell,
                                                    GdkEvent        *event,
                                                    GtkWidget       *widget,
                                                    const gchar     *path,
                                                    GdkRectangle    *background_area,
                                                    GdkRectangle    *cell_area,
                                                    GtkCellRendererState  flags);
static void gimp_cell_renderer_toggle_create_pixbuf (GimpCellRendererToggle *toggle,
                                                     GtkWidget              *widget);


static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };

static GtkCellRendererToggleClass *parent_class = NULL;


GType
gimp_cell_renderer_toggle_get_type (void)
{
  static GType cell_type = 0;

  if (! cell_type)
    {
      static const GTypeInfo cell_info =
      {
        sizeof (GimpCellRendererToggleClass),
        NULL,		/* base_init      */
        NULL,		/* base_finalize  */
        (GClassInitFunc) gimp_cell_renderer_toggle_class_init,
        NULL,		/* class_finalize */
        NULL,		/* class_data     */
        sizeof (GimpCellRendererToggle),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      cell_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TOGGLE,
                                          "GimpCellRendererToggle",
                                          &cell_info, 0);
    }

  return cell_type;
}

static void
gimp_cell_renderer_toggle_class_init (GimpCellRendererToggleClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class   = GTK_CELL_RENDERER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  toggle_cell_signals[CLICKED] =
    g_signal_new ("clicked",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GimpCellRendererToggleClass, clicked),
		  NULL, NULL,
		  _gimp_widgets_marshal_VOID__STRING_FLAGS,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
                  GDK_TYPE_MODIFIER_TYPE);

  object_class->finalize     = gimp_cell_renderer_toggle_finalize;
  object_class->get_property = gimp_cell_renderer_toggle_get_property;
  object_class->set_property = gimp_cell_renderer_toggle_set_property;

  cell_class->get_size       = gimp_cell_renderer_toggle_get_size;
  cell_class->render         = gimp_cell_renderer_toggle_render;
  cell_class->activate       = gimp_cell_renderer_toggle_activate;

  g_object_class_install_property (object_class,
                                   PROP_STOCK_ID,
                                   g_param_spec_string ("stock_id",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_STOCK_SIZE,
                                   g_param_spec_int ("stock_size",
                                                     NULL, NULL,
                                                     0, G_MAXINT,
                                                     DEFAULT_ICON_SIZE,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT));
}

static void
gimp_cell_renderer_toggle_finalize (GObject *object)
{
  GimpCellRendererToggle *toggle;

  toggle = GIMP_CELL_RENDERER_TOGGLE (object);

  if (toggle->stock_id)
    {
      g_free (toggle->stock_id);
      toggle->stock_id = NULL;
    }
  if (toggle->pixbuf)
    {
      g_object_unref (toggle->pixbuf);
      toggle->pixbuf = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_cell_renderer_toggle_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GimpCellRendererToggle *toggle = GIMP_CELL_RENDERER_TOGGLE (object);

  switch (param_id)
    {
    case PROP_STOCK_ID:
      g_value_set_string (value, toggle->stock_id);
      break;
    case PROP_STOCK_SIZE:
      g_value_set_int (value, toggle->stock_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gimp_cell_renderer_toggle_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GimpCellRendererToggle *toggle = GIMP_CELL_RENDERER_TOGGLE (object);

  switch (param_id)
    {
    case PROP_STOCK_ID:
      if (toggle->stock_id)
	g_free (toggle->stock_id);
      toggle->stock_id = g_value_dup_string (value);
      break;
    case PROP_STOCK_SIZE:
      toggle->stock_size = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

  if (toggle->pixbuf)
    {
      g_object_unref (toggle->pixbuf);
      toggle->pixbuf = NULL;
    }
}

static void
gimp_cell_renderer_toggle_get_size (GtkCellRenderer *cell,
                                    GtkWidget       *widget,
                                    GdkRectangle    *cell_area,
                                    gint            *x_offset,
                                    gint            *y_offset,
                                    gint            *width,
                                    gint            *height)
{
  GimpCellRendererToggle *toggle = GIMP_CELL_RENDERER_TOGGLE (cell);
  gint                    calc_width;
  gint                    calc_height;
  gint                    pixbuf_width;
  gint                    pixbuf_height;

  if (!toggle->stock_id)
    {
      GTK_CELL_RENDERER_CLASS (parent_class)->get_size (cell,
                                                        widget,
                                                        cell_area,
                                                        x_offset, y_offset,
                                                        width, height);
      return;
    }

  if (!toggle->pixbuf)
    gimp_cell_renderer_toggle_create_pixbuf (toggle, widget);

  pixbuf_width  = gdk_pixbuf_get_width  (toggle->pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (toggle->pixbuf);

  calc_width  = (pixbuf_width +
                 (gint) cell->xpad * 2 + widget->style->xthickness * 2);
  calc_height = (pixbuf_height +
                 (gint) cell->ypad * 2 + widget->style->ythickness * 2);

  if (x_offset) *x_offset = 0;
  if (y_offset) *y_offset = 0;

  if (cell_area)
    {
      if (x_offset)
	{
	  *x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                        (1.0 - cell->xalign) : cell->xalign) *
                       (cell_area->width - calc_width));
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = cell->yalign * (cell_area->height - calc_height);
	  *y_offset = MAX (*y_offset, 0);
	}
    }

  if (width)  *width  = calc_width;
  if (height) *height = calc_height;
}

static void
gimp_cell_renderer_toggle_render (GtkCellRenderer      *cell,
                                  GdkWindow            *window,
                                  GtkWidget            *widget,
                                  GdkRectangle         *background_area,
                                  GdkRectangle         *cell_area,
                                  GdkRectangle         *expose_area,
                                  GtkCellRendererState  flags)
{
  GimpCellRendererToggle *toggle = GIMP_CELL_RENDERER_TOGGLE (cell);
  GdkRectangle            toggle_rect;
  GdkRectangle            draw_rect;
  GtkStateType            state;
  gboolean                active;

  if (!toggle->stock_id)
    {
      GTK_CELL_RENDERER_CLASS (parent_class)->render (cell, window, widget,
                                                      background_area,
                                                      cell_area, expose_area,
                                                      flags);
      return;
    }

  gimp_cell_renderer_toggle_get_size (cell, widget, cell_area,
                                      &toggle_rect.x,
                                      &toggle_rect.y,
                                      &toggle_rect.width,
                                      &toggle_rect.height);

  toggle_rect.x      += cell_area->x + cell->xpad;
  toggle_rect.y      += cell_area->y + cell->ypad;
  toggle_rect.width  -= cell->xpad * 2;
  toggle_rect.height -= cell->ypad * 2;

  if (toggle_rect.width <= 0 || toggle_rect.height <= 0)
    return;

  active =
    gtk_cell_renderer_toggle_get_active (GTK_CELL_RENDERER_TOGGLE (cell));

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
	state = GTK_STATE_SELECTED;
      else
	state = GTK_STATE_ACTIVE;
    }
  else
    {
      if (GTK_CELL_RENDERER_TOGGLE (cell)->activatable)
        state = GTK_STATE_NORMAL;
      else
        state = GTK_STATE_INSENSITIVE;
    }

  if (gdk_rectangle_intersect (expose_area, cell_area, &draw_rect) &&
      (flags & GTK_CELL_RENDERER_PRELIT))
    gtk_paint_shadow (widget->style,
                      window,
                      state,
                      active ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                      &draw_rect,
                      widget, NULL,
                      toggle_rect.x,     toggle_rect.y,
                      toggle_rect.width, toggle_rect.height);

  if (active)
    {
      toggle_rect.x      += widget->style->xthickness;
      toggle_rect.y      += widget->style->ythickness;
      toggle_rect.width  -= widget->style->xthickness * 2;
      toggle_rect.height -= widget->style->ythickness * 2;

      if (gdk_rectangle_intersect (&draw_rect, &toggle_rect, &draw_rect))
        gdk_draw_pixbuf (window,
                         widget->style->black_gc,
                         toggle->pixbuf,
                         /* pixbuf 0, 0 is at toggle_rect.x, toggle_rect.y */
                         draw_rect.x - toggle_rect.x,
                         draw_rect.y - toggle_rect.y,
                         draw_rect.x,
                         draw_rect.y,
                         draw_rect.width,
                         draw_rect.height,
                         GDK_RGB_DITHER_NORMAL,
                         0, 0);
    }
}

static gboolean
gimp_cell_renderer_toggle_activate (GtkCellRenderer      *cell,
                                    GdkEvent             *event,
                                    GtkWidget            *widget,
                                    const gchar          *path,
                                    GdkRectangle         *background_area,
                                    GdkRectangle         *cell_area,
                                    GtkCellRendererState  flags)
{
  GtkCellRendererToggle *toggle = GTK_CELL_RENDERER_TOGGLE (cell);

  if (toggle->activatable)
    {
      GdkModifierType state = 0;

      if (event && ((GdkEventAny *) event)->type == GDK_BUTTON_PRESS)
        state = ((GdkEventButton *) event)->state;

      gimp_cell_renderer_toggle_clicked (GIMP_CELL_RENDERER_TOGGLE (cell),
                                         path, state);

      return TRUE;
    }

  return FALSE;
}

static void
gimp_cell_renderer_toggle_create_pixbuf (GimpCellRendererToggle *toggle,
                                         GtkWidget              *widget)
{
  if (toggle->pixbuf)
    g_object_unref (toggle->pixbuf);

  toggle->pixbuf = gtk_widget_render_icon (widget,
                                           toggle->stock_id,
                                           toggle->stock_size, NULL);
}


/**
 * gimp_cell_renderer_toggle_new:
 * @stock_id: the stock_id of the icon to use for the active state
 *
 * Creates a custom version of the #GtkCellRendererToggle. Instead of
 * showing the standard toggle button, it shows a stock icon if the
 * cell is active and no icon otherwise. This cell renderer is for
 * example used in the Layers treeview to indicate and control the
 * layer's visibility by showing %GIMP_STOCK_VISIBLE.
 *
 * Return value: a new #GimpCellRendererToggle
 *
 * Since: GIMP 2.2
 **/
GtkCellRenderer *
gimp_cell_renderer_toggle_new (const gchar *stock_id)
{
  return g_object_new (GIMP_TYPE_CELL_RENDERER_TOGGLE,
                       "stock_id", stock_id,
                       NULL);
}

/**
 * gimp_cell_renderer_toggle_clicked:
 * @cell: a #GimpCellRendererToggle
 * @path:
 * @state:
 *
 * Emits the "clicked" signal from a #GimpCellRendererToggle.
 *
 * Since: GIMP 2.2
 **/
void
gimp_cell_renderer_toggle_clicked (GimpCellRendererToggle *cell,
                                   const gchar            *path,
                                   GdkModifierType         state)
{
  g_return_if_fail (GIMP_IS_CELL_RENDERER_TOGGLE (cell));
  g_return_if_fail (path != NULL);

  g_signal_emit (cell, toggle_cell_signals[CLICKED], 0, path, state);
}