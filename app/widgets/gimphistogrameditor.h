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

#ifndef __GIMP_HISTOGRAM_EDITOR_H__
#define __GIMP_HISTOGRAM_EDITOR_H__


#include "gimpimageeditor.h"


#define GIMP_TYPE_HISTOGRAM_EDITOR            (gimp_histogram_editor_get_type ())
#define GIMP_HISTOGRAM_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_HISTOGRAM_EDITOR, GimpHistogramEditor))
#define GIMP_HISTOGRAM_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_HISTOGRAM_EDITOR, GimpHistogramEditorClass))
#define GIMP_IS_HISTOGRAM_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_HISTOGRAM_EDITOR))
#define GIMP_IS_HISTOGRAM_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_HISTOGRAM_EDITOR))
#define GIMP_HISTOGRAM_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_HISTOGRAM_EDITOR, GimpHistogramEditorClass))


typedef struct _GimpHistogramEditorClass GimpHistogramEditorClass;

struct _GimpHistogramEditor
{
  GimpImageEditor       parent_instance;

  GimpDrawable         *drawable;
  GimpHistogram        *histogram;

  guint                 idle_id;

  GtkWidget            *name;
  GtkWidget            *menu;
  GtkWidget            *box;
  GtkWidget            *labels[6];
};

struct _GimpHistogramEditorClass
{
  GimpImageEditorClass  parent_class;
};


GType       gimp_histogram_editor_get_type (void) G_GNUC_CONST;

GtkWidget * gimp_histogram_editor_new      (void);


#endif /* __GIMP_HISTOGRAM_EDITOR_H__ */