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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "appenv.h"
#include "buildmenu.h"
#include "widget.h"

Widget
BuildMenu (parent, menu_type, menu_title, menu_mnemonic, tear_off, items, visual, cmap, depth)
     Widget parent;
     int menu_type;
     char *menu_title;
     int menu_mnemonic;
     int tear_off;
     MenuItem *items;
     Visual *visual;
     Colormap cmap;
     int depth;
{
  Widget menu, cascade, widget;
  int i;
  XmString str;
  Arg args[5];
  int n;

  n = 0;
  XtSetArg (args[n], XmNvisual, visual); n++;
  XtSetArg (args[n], XmNdepth, depth); n++;
  XtSetArg (args[n], XmNcolormap, cmap); n++;

  if (menu_type == XmMENU_PULLDOWN || menu_type == XmMENU_OPTION)
    menu = XmCreatePulldownMenu (parent, "_pulldown", args, n);
  else
    menu = XmCreatePopupMenu (parent, "_popup", args, n);

  if (tear_off)
    XtVaSetValues (menu, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);

  if (menu_type == XmMENU_PULLDOWN)
    {
      str = XmStringCreateLocalized (menu_title);
      cascade = XtVaCreateManagedWidget (menu_title,
					 xmCascadeButtonGadgetClass, parent,
					 XmNsubMenuId, menu,
					 XmNmnemonic, menu_mnemonic,
					 NULL);
      XmStringFree (str);
    }
  else if (menu_type == XmMENU_OPTION)
    {
      Arg args[5];
      int n = 0;

      str = XmStringCreateLocalized (menu_title);
      XtSetArg (args[n], XmNsubMenuId, menu); n++;
      XtSetArg (args[n], XmNlabelString, str); n++;
      cascade = XmCreateOptionMenu (parent, menu_title, args, n);
      XmStringFree (str);
    }
  
  for (i = 0; items[i].label != NULL; i++)
    {
      n = 0;

      if (items[i].mnemonic)
	{
	  XtSetArg (args[n], XmNmnemonic, items[i].mnemonic);  n++;
	}

      if (items[i].accelerator)
	{
	  str = XmStringCreateLocalized (items[i].accel_text);
	  XtSetArg (args[n], XmNaccelerator, items[i].accelerator);  n++;
	  XtSetArg (args[n], XmNacceleratorText, str);  n++;
	}
      else
	str = NULL;

      if (items[i].subitems)
	if (menu_type == XmMENU_OPTION) 
	  {
	    XtWarning ("You can't have submenus from option menu items.");
	    continue;
	  }
	else
	  widget = BuildMenu (menu, XmMENU_PULLDOWN, items[i].label,
			      items[i].mnemonic, tear_off, items[i].subitems,
			      visual, cmap, depth);
      else
	widget = XtCreateManagedWidget (items[i].label, *items[i].class, menu, 
					args, n);

      if (str) 
	XmStringFree (str);

      if (items[i].callback)
	XtAddCallback (widget,
		       (items[i].class == &xmToggleButtonWidgetClass ||
			items[i].class == &xmToggleButtonGadgetClass) ?
		       XmNvalueChangedCallback : XmNactivateCallback,
		       items[i].callback, items[i].callback_data);

      items[i].w = widget;
    }

  items[i].w = (menu_type == XmMENU_POPUP) ? menu : cascade;

  return (menu_type == XmMENU_POPUP) ? menu : cascade;
}
