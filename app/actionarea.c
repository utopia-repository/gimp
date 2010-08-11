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
#include "actionarea.h"

#define TIGHTNESS 20

Widget
build_action_area (parent, actions, num_actions)
     Widget parent;
     ActionAreaItem *actions;
     int num_actions;
{
  Widget action_area;
  Widget widget;
  int i;

  action_area = XtVaCreateWidget ("action_area", xmFormWidgetClass, parent,
				  XmNfractionBase, TIGHTNESS * num_actions - 1,
				  XmNleftOffset, 10,
				  XmNrightOffset, 10,
				  NULL);

  for (i = 0; i < num_actions; i++)
    {
      widget = XtVaCreateManagedWidget (actions[i].label,
					xmPushButtonWidgetClass, action_area,
					XmNleftAttachment, i ? XmATTACH_POSITION : XmATTACH_FORM,
					XmNleftPosition, TIGHTNESS * i,
					XmNtopAttachment, XmATTACH_FORM,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNrightAttachment,
					 (i != num_actions - 1) ? XmATTACH_POSITION : XmATTACH_FORM,
					XmNrightPosition, TIGHTNESS * i + (TIGHTNESS - 1),
					XmNshowAsDefault, i == 0,
					XmNdefaultButtonShadowThickness, 1,
					NULL);
      
      if (actions[i].callback)
	XtAddCallback (widget, XmNactivateCallback,
		       actions[i].callback, actions[i].data);

      if (i == 0)
	{
	  Dimension height, h;

	  XtVaGetValues (action_area, XmNmarginHeight, &h, NULL);
	  XtVaGetValues (widget, XmNheight, &height, NULL);
	  height += 2 * h;
	  XtVaSetValues (action_area,
			 XmNdefaultButton, widget,
			 XmNpaneMaximum, height,
			 XmNpaneMinimum, height,
			 NULL);
	}

      actions[i].widget = widget;
    }

  return action_area;
}
