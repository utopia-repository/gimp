/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpcontrollerkeyboard.h
 * Copyright (C) 2004 Michael Natterer <mitch@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_CONTROLLER_KEYBOARD_H__
#define __GIMP_CONTROLLER_KEYBOARD_H__


#define GIMP_ENABLE_CONTROLLER_UNDER_CONSTRUCTION
#include "libgimpwidgets/gimpcontroller.h"


#define GIMP_TYPE_CONTROLLER_KEYBOARD            (gimp_controller_keyboard_get_type ())
#define GIMP_CONTROLLER_KEYBOARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CONTROLLER_KEYBOARD, GimpControllerKeyboard))
#define GIMP_CONTROLLER_KEYBOARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_CONTROLLER_KEYBOARD, GimpControllerKeyboardClass))
#define GIMP_IS_CONTROLLER_KEYBOARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CONTROLLER_KEYBOARD))
#define GIMP_IS_CONTROLLER_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_CONTROLLER_KEYBOARD))
#define GIMP_CONTROLLER_KEYBOARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_CONTROLLER_KEYBOARD, GimpControllerKeyboardClass))


typedef struct _GimpControllerKeyboardClass GimpControllerKeyboardClass;

struct _GimpControllerKeyboard
{
  GimpController parent_instance;
};

struct _GimpControllerKeyboardClass
{
  GimpControllerClass parent_class;
};


GType      gimp_controller_keyboard_get_type  (void) G_GNUC_CONST;

gboolean   gimp_controller_keyboard_key_press (GimpControllerKeyboard *keyboard,
                                               const GdkEventKey      *kevent);


#endif /* __GIMP_CONTROLLER_KEYBOARD_H__ */