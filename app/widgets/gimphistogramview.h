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

#ifndef __GIMP_HISTOGRAM_VIEW_H__
#define __GIMP_HISTOGRAM_VIEW_H__


#include <gtk/gtkdrawingarea.h>


#define GIMP_TYPE_HISTOGRAM_VIEW            (gimp_histogram_view_get_type ())
#define GIMP_HISTOGRAM_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_HISTOGRAM_VIEW, GimpHistogramView))
#define GIMP_HISTOGRAM_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_HISTOGRAM_VIEW, GimpHistogramViewClass))
#define GIMP_IS_HISTOGRAM_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_HISTOGRAM_VIEW))
#define GIMP_IS_HISTOGRAM_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_HISTOGRAM_VIEW))
#define GIMP_HISTOGRAM_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_HISTOGRAM_VIEW, GimpHistogramView))


typedef struct _GimpHistogramViewClass  GimpHistogramViewClass;

struct _GimpHistogramView
{
  GtkDrawingArea         parent_instance;

  GimpHistogram         *histogram;
  GimpHistogramChannel   channel;
  GimpHistogramScale     scale;
  gint                   start;
  gint                   end;

  gint                   border_width;
  gint                   subdivisions;

  /* hack */
  gboolean               light_histogram;
};

struct _GimpHistogramViewClass
{
  GtkDrawingAreaClass  parent_class;

  void (* range_changed) (GimpHistogramView *view,
			  gint               start,
			  gint               end);
};


GType           gimp_histogram_view_get_type      (void) G_GNUC_CONST;

GtkWidget     * gimp_histogram_view_new           (gboolean              range);

void            gimp_histogram_view_set_histogram (GimpHistogramView    *view,
                                                   GimpHistogram        *histogram);
GimpHistogram * gimp_histogram_view_get_histogram (GimpHistogramView    *view);

void            gimp_histogram_view_set_channel   (GimpHistogramView    *view,
                                                   GimpHistogramChannel  channel);
GimpHistogramChannel gimp_histogram_view_get_channel (GimpHistogramView *view);

void            gimp_histogram_view_set_scale     (GimpHistogramView    *view,
                                                   GimpHistogramScale    scale);
GimpHistogramScale gimp_histogram_view_get_scale  (GimpHistogramView    *view);

void            gimp_histogram_view_set_range     (GimpHistogramView    *view,
                                                   gint                  start,
                                                   gint                  end);
void            gimp_histogram_view_get_range     (GimpHistogramView    *view,
                                                   gint                 *start,
                                                   gint                 *end);


#endif /* __GIMP_HISTOGRAM_VIEW_H__ */