/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpview.h
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#ifndef __GIMP_VIEW_H__
#define __GIMP_VIEW_H__


#define GIMP_TYPE_VIEW            (gimp_view_get_type ())
#define GIMP_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_VIEW, GimpView))
#define GIMP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_VIEW, GimpViewClass))
#define GIMP_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GIMP_TYPE_VIEW))
#define GIMP_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_VIEW))
#define GIMP_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_VIEW, GimpViewClass))


typedef struct _GimpViewClass  GimpViewClass;

struct _GimpView
{
  GtkWidget         parent_instance;

  GdkWindow        *event_window;

  GimpViewable     *viewable;
  GimpViewRenderer *renderer;

  gboolean          clickable;
  gboolean          eat_button_events;
  gboolean          show_popup;
  gboolean          expand;

  /*< private >*/
  gboolean          in_button;
  gboolean          has_grab;
  GdkModifierType   press_state;
};

struct _GimpViewClass
{
  GtkWidgetClass       parent_class;

  /*  signals  */
  void        (* clicked)          (GimpView        *view,
                                    GdkModifierType  modifier_state);
  void        (* double_clicked)   (GimpView        *view);
  void        (* context)          (GimpView        *view);
};


GType       gimp_view_get_type          (void) G_GNUC_CONST;

GtkWidget * gimp_view_new               (GimpViewable  *viewable,
                                         gint           size,
                                         gint           border_width,
                                         gboolean       is_popup);
GtkWidget * gimp_view_new_full          (GimpViewable  *viewable,
                                         gint           width,
                                         gint           height,
                                         gint           border_width,
                                         gboolean       is_popup,
                                         gboolean       clickable,
                                         gboolean       show_popup);
GtkWidget * gimp_view_new_by_types      (GType          view_type,
                                         GType          viewable_type,
                                         gint           size,
                                         gint           border_width,
                                         gboolean       is_popup);
GtkWidget * gimp_view_new_full_by_types (GType          view_type,
                                         GType          viewable_type,
                                         gint           width,
                                         gint           height,
                                         gint           border_width,
                                         gboolean       is_popup,
                                         gboolean       clickable,
                                         gboolean       show_popup);

void        gimp_view_set_viewable      (GimpView      *view,
                                         GimpViewable  *viewable);
void        gimp_view_set_expand        (GimpView      *view,
                                         gboolean       expand);


#endif /* __GIMP_VIEW_H__ */
