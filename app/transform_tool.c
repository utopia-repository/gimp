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
#include <stdlib.h>
#include "appenv.h"
#include "autodialog.h"
#include "gdisplay.h"
#include "tools.h"
#include "rotate_tool.h"
#include "scale_tool.h"
#include "shear_tool.h"
#include "transform_tool.h"

#define ROTATION    0
#define SCALING     1
#define SHEARING    2

/*  local functions  */
static void         transform_dialog_callback (int, int, void *, void *);
static void         transform_change_type     (int);

/*  Static variables  */
static int          transform_type = ROTATION;
int                 smoothing = 1;   /*  this variable is not static...  */

static AutoDialog   transform_dlg = NULL;
static long         type_value;
static long         smoothing_value;
static int          rotate_ID;
static int          scale_ID;
static int          shear_ID;
static int          smoothing_ID;


int
transform_tool_smoothing ()
{
  return smoothing;
}


Tool *
tools_new_transform_tool ()
{
  switch (transform_type)
    {
    case ROTATION :
      return tools_new_rotate_tool ();
      break;
    case SCALING :
      return tools_new_scale_tool ();
      break;
    case SHEARING :
      return tools_new_shear_tool ();
      break;
    default :
      return NULL;
      break;
    }

}


void
tools_free_transform_tool (tool)
     Tool * tool;
{
  switch (transform_type)
    {
    case ROTATION :
      tools_free_rotate_tool (tool);
      break;
    case SCALING :
      tools_free_scale_tool (tool);
      break;
    case SHEARING :
      tools_free_shear_tool (tool);
      break;
    }
  
}


void
transform_tool_dialog ()
{
  int radio_ID;
  int group_ID;
  int frame_ID;
  long button_set [3];

  if (!transform_dlg)
    {
      transform_dlg = dialog_new ("Transformation Options", transform_dialog_callback, NULL);
      
      group_ID = dialog_new_item (transform_dlg, 0, GROUP_ROWS, NULL, NULL);

      frame_ID = dialog_new_item (transform_dlg, group_ID, ITEM_FRAME, "Transform Type", NULL);

      radio_ID = dialog_new_item (transform_dlg, frame_ID, (GROUP_ROWS | GROUP_RADIO), NULL, NULL);

      type_value = transform_type;
      button_set [0] = (type_value == ROTATION);
      button_set [1] = (type_value == SCALING);
      button_set [2] = (type_value == SHEARING);

      rotate_ID = dialog_new_item (transform_dlg, radio_ID, ITEM_RADIO_BUTTON, "Rotation", NULL);
      dialog_change_item (transform_dlg, rotate_ID, button_set);
      scale_ID = dialog_new_item (transform_dlg, radio_ID, ITEM_RADIO_BUTTON, "Scaling", NULL);
      dialog_change_item (transform_dlg, scale_ID, button_set + 1);
      shear_ID = dialog_new_item (transform_dlg, radio_ID, ITEM_RADIO_BUTTON, "Shearing", NULL);
      dialog_change_item (transform_dlg, shear_ID, button_set + 2);

      smoothing_ID = dialog_new_item (transform_dlg, group_ID, 
				      ITEM_CHECK_BUTTON, "Smoothing", NULL);
      smoothing_value = smoothing;
      dialog_change_item (transform_dlg, smoothing_ID, &smoothing_value);

      dialog_show (transform_dlg);
    }
}


static void
transform_change_type (new_type)
     int new_type;
{
  if (transform_type != new_type)
    {
      /*  change the type, free the old tool, create the new tool  */
      transform_type = new_type;
      
      tools_select (TRANSFORM_TOOL);
    }
}


static void
transform_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  switch (item_ID)
    {
    case OK_ID:
      dialog_close (transform_dlg);
      transform_dlg = NULL;
      break;
    case CANCEL_ID:
      transform_type = type_value;
      smoothing = smoothing_value;
      dialog_close (transform_dlg);
      transform_dlg = NULL;
      break;
    default:
      if (item_ID == smoothing_ID)
	smoothing = *((long*) call_data);
      else if (item_ID == rotate_ID && *((long*) call_data))
	transform_change_type (ROTATION);
      else if (item_ID == scale_ID && *((long*) call_data))
	transform_change_type (SCALING);
      else if (item_ID == shear_ID && *((long*) call_data))
	transform_change_type (SHEARING);

      break;
    }
}




