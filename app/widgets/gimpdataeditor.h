/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdataeditor.h
 * Copyright (C) 2002-2004 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_DATA_EDITOR_H__
#define __GIMP_DATA_EDITOR_H__


#include "gimpeditor.h"


#define GIMP_TYPE_DATA_EDITOR            (gimp_data_editor_get_type ())
#define GIMP_DATA_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DATA_EDITOR, GimpDataEditor))
#define GIMP_DATA_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DATA_EDITOR, GimpDataEditorClass))
#define GIMP_IS_DATA_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DATA_EDITOR))
#define GIMP_IS_DATA_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DATA_EDITOR))
#define GIMP_DATA_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DATA_EDITOR, GimpDataEditorClass))


typedef struct _GimpDataEditorClass GimpDataEditorClass;

struct _GimpDataEditor
{
  GimpEditor       parent_instance;

  GimpDataFactory *data_factory;
  GimpData        *data;
  gboolean         data_editable;

  GtkWidget       *name_entry;

  GtkWidget       *save_button;
  GtkWidget       *revert_button;
};

struct _GimpDataEditorClass
{
  GimpEditorClass  parent_class;

  /*  virtual functions  */
  void (* set_data) (GimpDataEditor *editor,
                     GimpData       *data);
};


GType       gimp_data_editor_get_type (void) G_GNUC_CONST;

void        gimp_data_editor_set_data (GimpDataEditor *editor,
                                       GimpData       *data);
GimpData  * gimp_data_editor_get_data (GimpDataEditor *editor);


#endif  /*  __GIMP_DATA_EDITOR_H__  */