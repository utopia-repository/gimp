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
#include <string.h>
#include "appenv.h"
#include "actionarea.h"
#include "colormaps.h"
#include "edit_selection.h"
#include "errors.h"
#include "gdisplay.h"
#include "general.h"
#include "global_edit.h"
#include "palette.h"
#include "selection.h"
#include "text_tool.h"
#include "tools.h"
#include "visual.h"
#include "widget.h"

#define TEXT_WIDTH 300
#define TEXT_HEIGHT 80

#define PIXELS 0
#define POINTS 1

#define FOUNDRY   0
#define FAMILY    1
#define WEIGHT    2
#define SLANT     3
#define SET_WIDTH 4
#define SPACING   10

typedef struct _TextTool TextTool;
struct _TextTool
{
  Widget shell;
  Widget dialog;
  Widget font_list;
  Widget size_menu;
  Widget size_text;
  Widget the_text;
  Widget menus[6];
  Widget antialias_items[10];
  Widget *foundry_items;
  Widget *weight_items;
  Widget *slant_items;
  Widget *set_width_items;
  Widget *spacing_items;
  int click_x;
  int click_y;
  int font_index;
  int size_type;
  int antialias;
  int foundry;
  int weight;
  int slant;
  int set_width;
  int spacing;
  void *gdisp_ptr;
};

typedef struct _FontInfo FontInfo;
struct _FontInfo
{
  char *family;         /* The font family this info struct describes. */
  int *foundries;       /* An array of valid foundries. */
  int *weights;         /* An array of valid weights. */
  int *slants;          /* An array of valid slants. */
  int *set_widths;      /* An array of valid set widths. */
  int *spacings;        /* An array of valid spacings */
  int **combos;         /* An array of valid combinations of the above 5 items */
  int ncombos;          /* The number of elements in the "combos" array */
  link_ptr fontnames;   /* An list of valid fontnames.
			 * This is used to make sure a family/foundry/weight/slant/set_width
			 *  combination is valid.
			 */
};

static void       text_button_press       (Tool *, XButtonEvent *, XtPointer);
static void       text_button_release     (Tool *, XButtonEvent *, XtPointer);
static void       text_motion             (Tool *, XMotionEvent *, XtPointer);
static void       text_control            (Tool *, int, XtPointer);

static void       text_create_dialog      (TextTool *);
static void       text_ok_callback        (Widget, XtPointer, XtPointer);
static void       text_cancel_callback    (Widget, XtPointer, XtPointer);
static void       text_destroy_callback   (Widget, XtPointer, XtPointer);
static void       text_font_callback      (Widget, XtPointer, XtPointer);
static void       text_size_callback      (Widget, XtPointer, XtPointer);
static void       text_antialias_callback (Widget, XtPointer, XtPointer);
static void       text_foundry_callback   (Widget, XtPointer, XtPointer);
static void       text_weight_callback    (Widget, XtPointer, XtPointer);
static void       text_slant_callback     (Widget, XtPointer, XtPointer);
static void       text_set_width_callback (Widget, XtPointer, XtPointer);
static void       text_spacing_callback   (Widget, XtPointer, XtPointer);
static void       text_validate_combo     (TextTool *, int);

static void       text_get_fonts          (void);
static void       text_insert_font        (FontInfo **, int *, char *);
static link_ptr   text_insert_field       (link_ptr, char *, int);
static char*      text_get_field          (char *, int);
static int        text_field_to_index     (char **, int, char *);
static int        text_is_xlfd_font_name  (char *);

static void       text_render             (TextTool *);
static GRegion*   text_ximage_to_region   (XImage *, int, int, int);

static ActionAreaItem action_items[] = 
{
  { "OK", text_ok_callback, NULL, NULL },
  { "Cancel", text_cancel_callback, NULL, NULL },
};

static TextTool *the_text_tool = NULL;
static FontInfo **font_info;
static int nfonts = -1;

static link_ptr foundries = NULL;
static link_ptr weights = NULL;
static link_ptr slants = NULL;
static link_ptr set_widths = NULL;
static link_ptr spacings = NULL;

static char **foundry_array = NULL;
static char **weight_array = NULL;
static char **slant_array = NULL;
static char **set_width_array = NULL;
static char **spacing_array = NULL;

static int nfoundries = 0;
static int nweights = 0;
static int nslants = 0;
static int nset_widths = 0;
static int nspacings = 0;

Tool*
tools_new_text ()
{
  Tool * tool;

  tool = xmalloc (sizeof (Tool));
  if (!the_text_tool)
    {
      the_text_tool = xmalloc (sizeof (TextTool));
      the_text_tool->shell = NULL;
      the_text_tool->dialog = NULL;
      the_text_tool->font_list = NULL;
      the_text_tool->size_menu = NULL;
      the_text_tool->size_text = NULL;
      the_text_tool->the_text = NULL;
      the_text_tool->font_index = 0;
      the_text_tool->size_type = PIXELS;
      the_text_tool->antialias = 1;
      the_text_tool->foundry = 0;
      the_text_tool->weight = 0;
      the_text_tool->slant = 0;
      the_text_tool->set_width = 0;
      the_text_tool->spacing = 0;
    }
  
  tool->type = TEXT;
  tool->state = INACTIVE;
  tool->scroll_lock = 1;  /* Do not allow scrolling */
  tool->private = (void *) the_text_tool;
  tool->button_press_func = text_button_press;
  tool->button_release_func = text_button_release;
  tool->motion_func = text_motion;
  tool->arrow_keys_func = edit_sel_arrow_keys_func;
  tool->control_func = text_control;

  return tool;
}

void
tools_free_text (tool)
     Tool * tool;
{
}

static void  
text_button_press (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  GDisplay * gdisp;
  TextTool * text_tool;
  int i;

  gdisp = gdisp_ptr;
  text_tool = tool->private;
  text_tool->gdisp_ptr = gdisp_ptr;

  tool->state = ACTIVE;
  tool->gdisp_ptr = gdisp_ptr;

  /*  Check if the pointer was clicked inside a selection...If so, start
   *  an edit selection, which allows the selection to be dragged.
   */
  if (selection_point_inside (gdisp->select, gdisp_ptr, bevent->x, bevent->y))
    {
      XGrabPointer (DISPLAY, XtWindow (gdisp->disp_image->canvas), False,
		    Button1MotionMask | ButtonReleaseMask, GrabModeAsync,
		    GrabModeAsync, None, None, bevent->time);

      init_edit_selection (tool, gdisp->select, gdisp_ptr, bevent->x, bevent->y);
      return;
    }

  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, 
			       &text_tool->click_x, &text_tool->click_y,
			       True);

  if (!text_tool->shell)
    text_create_dialog (text_tool);
  
  switch (gdisp->gimage->type)
    {
    case RGB_GIMAGE:
    case GREY_GIMAGE:
      for (i = 1; i < 10; i++)
	XtSetSensitive (text_tool->antialias_items[i], True);
      break;
    case INDEXED_GIMAGE:
      for (i = 1; i < 10; i++)
	XtSetSensitive (text_tool->antialias_items[i], False);
      XtVaSetValues (text_tool->menus[0], XmNmenuHistory, text_tool->antialias_items[0], NULL);
      break;
    }
  
  XtPopup (text_tool->shell, XtGrabNone);
}

static void  
text_button_release (tool, bevent, gdisp_ptr)
     Tool * tool;
     XButtonEvent * bevent;
     XtPointer gdisp_ptr;
{
  tool->state = INACTIVE;
}

static void  
text_motion (tool, mevent, gdisp_ptr)
     Tool * tool;
     XMotionEvent * mevent;
     XtPointer gdisp_ptr;
{
}

static void  
text_control (tool, action, gdisp_ptr)
     Tool * tool;
     int action;
     XtPointer gdisp_ptr;
{
}

static void 
text_create_dialog (text_tool)
     TextTool * text_tool;
{
  Widget action_widget;
  Widget menu;
  Widget menu_item;
  Widget *menu_items[6];
  int nmenu_items[6];
  char *menu_strs[6];
  char **menu_item_strs[6];
  XtCallbackProc menu_callbacks[6];
  XmStringTable strings;
  XmString str;
  Arg xt_args[3];
  char buffer[16];
  int i, j;

  /* Create the shell and dialog */
  
  text_tool->shell = XtVaCreatePopupShell ("textToolDialog",
					   xmDialogShellWidgetClass, toplevel,
					   XmNtitle, "Text Tool",
					   XmNvisual, color_visual,
					   XmNcolormap, get_colormap (RGB_GIMAGE),
					   XmNdepth, color_depth,
					   XmNdeleteResponse, XmDESTROY,
					   NULL);
  XtAddCallback (text_tool->shell, XtNdestroyCallback, text_destroy_callback, text_tool);
  
  text_tool->dialog = XtVaCreateWidget ("dialog",
					xmFormWidgetClass, text_tool->shell,
					XmNfractionBase, 100,
					XmNdefaultPosition, False,
					NULL);
  XtAddCallback (text_tool->dialog, XmNmapCallback, map_dialog, NULL);
  
  /* Create the action area */
  
  action_items[0].data = text_tool;
  action_items[1].data = text_tool;
  action_widget = build_action_area (text_tool->dialog, action_items, 2);
  XtVaSetValues (action_widget,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNleftOffset, 10,
		 XmNrightOffset, 10,
		 XmNbottomOffset, 10,
		 NULL);
  XtManageChild (action_widget);

  /* Create the size text */

  text_tool->size_text = XtVaCreateManagedWidget ("sizeText",
						  xmTextFieldWidgetClass, text_tool->dialog,
						  XmNwidth, 80,
						  XmNleftAttachment, XmATTACH_POSITION,
						  XmNleftPosition, 50,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNleftOffset, 5,
						  XmNtopOffset, 10,
						  NULL);
  XmTextSetString (text_tool->size_text, "50");

  /* Create the size menu */

  i = 0;
  XtSetArg (xt_args[i], XmNvisual, color_visual); i++;
  XtSetArg (xt_args[i], XmNdepth, color_depth); i++;
  XtSetArg (xt_args[i], XmNcolormap, get_colormap (RGB_GIMAGE)); i++;
  menu = XmCreatePulldownMenu (text_tool->dialog, "_pulldown", xt_args, i);
  
  i = 0;
  XtSetArg (xt_args[i], XmNsubMenuId, menu); i++;
  text_tool->size_menu = XmCreateOptionMenu (text_tool->dialog, "Size", xt_args, i);
  XtVaSetValues (text_tool->size_menu,
		 XmNtopAttachment, XmATTACH_FORM,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, text_tool->size_text,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, text_tool->size_text,
		 XmNleftOffset, 0,
		 XmNrightOffset, 10,
		 NULL);

  menu_item = XtVaCreateManagedWidget ("Pixels", 
				       xmPushButtonGadgetClass, menu, 
				       XmNuserData, PIXELS,
				       NULL);
  XtAddCallback (menu_item, XmNactivateCallback, text_size_callback, text_tool);

  menu_item = XtVaCreateManagedWidget ("Points", 
				       xmPushButtonGadgetClass, menu, 
				       XmNuserData, POINTS,
				       NULL);
  XtAddCallback (menu_item, XmNactivateCallback, text_size_callback, text_tool);

  XtManageChild (text_tool->size_menu);

  /* Allocate the arrays for the foundry, weight, slant, set_width and spacing menu items */

  if (nfonts == -1)
    text_get_fonts ();

  text_tool->foundry_items = xmalloc (sizeof (Widget) * nfoundries);
  text_tool->weight_items = xmalloc (sizeof (Widget) * nweights);
  text_tool->slant_items = xmalloc (sizeof (Widget) * nslants);
  text_tool->set_width_items = xmalloc (sizeof (Widget) * nset_widths);
  text_tool->spacing_items = xmalloc (sizeof (Widget) * nspacings);

  menu_items[0] = text_tool->antialias_items;
  menu_items[1] = text_tool->foundry_items;
  menu_items[2] = text_tool->weight_items;
  menu_items[3] = text_tool->slant_items;
  menu_items[4] = text_tool->set_width_items;
  menu_items[5] = text_tool->spacing_items;

  nmenu_items[0] = 10;
  nmenu_items[1] = nfoundries;
  nmenu_items[2] = nweights;
  nmenu_items[3] = nslants;
  nmenu_items[4] = nset_widths;
  nmenu_items[5] = nspacings;

  menu_strs[0] = "Antialias";
  menu_strs[1] = "Foundry";
  menu_strs[2] = "Weight";
  menu_strs[3] = "Slant";
  menu_strs[4] = "Set width";
  menu_strs[5] = "Spacing";

  menu_item_strs[0] = NULL;
  menu_item_strs[1] = foundry_array;
  menu_item_strs[2] = weight_array;
  menu_item_strs[3] = slant_array;
  menu_item_strs[4] = set_width_array;
  menu_item_strs[5] = spacing_array;

  menu_callbacks[0] = text_antialias_callback;
  menu_callbacks[1] = text_foundry_callback;
  menu_callbacks[2] = text_weight_callback;
  menu_callbacks[3] = text_slant_callback;
  menu_callbacks[4] = text_set_width_callback;
  menu_callbacks[5] = text_spacing_callback;

  /* Create the other menus */

  for (j = 0; j < 6; j++)
    {
      i = 0;
      XtSetArg (xt_args[i], XmNvisual, color_visual); i++;
      XtSetArg (xt_args[i], XmNdepth, color_depth); i++;
      XtSetArg (xt_args[i], XmNcolormap, get_colormap (RGB_GIMAGE)); i++;
      menu = XmCreatePulldownMenu (text_tool->dialog, "_pulldown", xt_args, i);
      XtVaSetValues (menu,
		     XmNvisual, color_visual,
		     XmNdepth, color_depth,
		     XmNcolormap, get_colormap (RGB_GIMAGE),
		     NULL);
      
      i = 0;
      str = XmStringCreateLocalized (menu_strs[j]);
      XtSetArg (xt_args[i], XmNsubMenuId, menu); i++;
      XtSetArg (xt_args[i], XmNlabelString, str); i++;
      text_tool->menus[j] = XmCreateOptionMenu (text_tool->dialog, menu_strs[j], xt_args, i);
      XtVaSetValues (text_tool->menus[j],
		     XmNtopAttachment, XmATTACH_WIDGET,
		     XmNtopWidget, (j == 0) ? (text_tool->size_menu) : (text_tool->menus[j-1]),
		     XmNbottomAttachment, (j == 5) ? (XmATTACH_POSITION) : (XmATTACH_NONE),
		     XmNbottomPosition, 70,
		     XmNleftAttachment, XmATTACH_POSITION,
		     XmNleftPosition, 50,
		     XmNrightAttachment, XmATTACH_FORM,
		     XmNtopOffset, 10,
		     XmNbottomOffset, 5,
		     XmNleftOffset, 5,
		     XmNrightOffset, 10,
		     NULL);
      
      if (menu_item_strs[j])
	{
	  for (i = 0; i < nmenu_items[j]; i++)
	    {
	      menu_items[j][i] = XtVaCreateManagedWidget (menu_item_strs[j][i],
							  xmPushButtonGadgetClass, menu,
							  XmNuserData, i,
							  NULL);
	      XtAddCallback (menu_items[j][i], XmNactivateCallback, menu_callbacks[j], text_tool);
	    }
	}
      else
	{
	  for (i = 0; i < nmenu_items[j]; i++)
	    {
	      sprintf (buffer, "%d", i+1);
	      menu_items[j][i] = XtVaCreateManagedWidget (buffer, 
							  xmPushButtonGadgetClass, menu, 
							  XmNuserData, i+1,
							  NULL);
	      XtAddCallback (menu_items[j][i], XmNactivateCallback, menu_callbacks[j], text_tool);
	    }
	}
      
      XtManageChild (text_tool->menus[j]);
    }

  /* Create the font listbox */
  
  strings = xmalloc (sizeof (XmString*) * nfonts);
  for (i = 0; i < nfonts; i++)
    strings[i] = XmStringCreateLocalized (font_info[i]->family);

  text_tool->font_list = XmCreateScrolledList (text_tool->dialog, "fontListbox", NULL, 0);
  XtVaSetValues (text_tool->font_list,
		 XmNselectionPolicy, XmBROWSE_SELECT,
		 XmNscrollBarDisplayPolicy, XmSTATIC,
		 XmNitemCount, nfonts,
		 XmNitems, strings,
		 NULL);
  XtVaSetValues (XtParent (text_tool->font_list),
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNtopAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_POSITION,
		 XmNrightPosition, 50,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, text_tool->menus[5],
		 XmNleftOffset, 10,
		 XmNrightOffset, 5,
		 XmNtopOffset, 10,
		 XmNbottomOffset, 0,
		 NULL);

  XtAddCallback (text_tool->font_list, XmNbrowseSelectionCallback, text_font_callback, text_tool);
  XtManageChild (text_tool->font_list);

  XmListSelectItem (text_tool->font_list, strings[0], True);
  for (i = 0; i < nfonts; i++)
    XmStringFree (strings[i]);
  xfree (strings);

  /* Create the text area */

  i = 0;
  XtSetArg (xt_args[i], XmNeditMode, XmMULTI_LINE_EDIT); i++;
  text_tool->the_text = XmCreateScrolledText (text_tool->dialog, "scrolledText", xt_args, i);
  XtVaSetValues (XtParent (text_tool->the_text),
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNtopAttachment, XmATTACH_POSITION,
		 XmNtopPosition, 70,
		 XmNbottomAttachment, XmATTACH_WIDGET,
		 XmNbottomWidget, action_widget,
		 XmNleftOffset, 10,
		 XmNrightOffset, 10,
		 XmNtopOffset, 5,
		 XmNbottomOffset, 10,
		 NULL);
  
  XmTextSetString (text_tool->the_text, "");
  XtManageChild (text_tool->the_text);

  XtManageChild (text_tool->dialog);
}

static void
text_ok_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool * text_tool;
  
  text_tool = client_data;
  XtPopdown (text_tool->shell);
  
  text_render (text_tool);
}

static void
text_cancel_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool * text_tool;

  text_tool = client_data;
  XtPopdown (text_tool->shell);
}

static void  
text_destroy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  if (the_text_tool)
    {
      the_text_tool->shell = NULL;
      the_text_tool->dialog = NULL;
      the_text_tool->font_list = NULL;
      the_text_tool->size_menu = NULL;
      the_text_tool->size_text = NULL;
      the_text_tool->the_text = NULL;
      the_text_tool->font_index = 0;
      the_text_tool->size_type = PIXELS;
      the_text_tool->antialias = 1;
      the_text_tool->foundry = 0;
      the_text_tool->weight = 0;
      the_text_tool->slant = 0;
      the_text_tool->set_width = 0;
      the_text_tool->spacing = 0;

      free (the_text_tool->foundry_items);
      free (the_text_tool->weight_items);
      free (the_text_tool->slant_items);
      free (the_text_tool->set_width_items);
      free (the_text_tool->spacing_items);

      the_text_tool->foundry_items = NULL;
      the_text_tool->weight_items = NULL;
      the_text_tool->slant_items = NULL;
      the_text_tool->set_width_items = NULL;
      the_text_tool->spacing_items = NULL;
    }
}

static void
text_font_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmListCallbackStruct *cd;
  TextTool *text_tool;
  FontInfo *font;
  int i;
  
  cd = call_data;
  text_tool = client_data;
  text_tool->font_index = cd->item_position - 1;

  font = font_info[text_tool->font_index];

  if (text_tool->foundry && !font->foundries[text_tool->foundry])
    {
      text_tool->foundry = 0;
      XtVaSetValues (text_tool->menus[1], XmNmenuHistory, text_tool->foundry_items[0], NULL);
    }
  if (text_tool->weight && !font->weights[text_tool->weight])
    {
      text_tool->weight = 0;
      XtVaSetValues (text_tool->menus[2], XmNmenuHistory, text_tool->weight_items[0], NULL);
    }
  if (text_tool->slant && !font->slants[text_tool->slant])
    {
      text_tool->slant = 0;
      XtVaSetValues (text_tool->menus[3], XmNmenuHistory, text_tool->slant_items[0], NULL);
    }
  if (text_tool->set_width && !font->set_widths[text_tool->set_width])
    {
      text_tool->set_width = 0;
      XtVaSetValues (text_tool->menus[4], XmNmenuHistory, text_tool->set_width_items[0], NULL);
    }
  if (text_tool->spacing && !font->spacings[text_tool->spacing])
    {
      text_tool->spacing = 0;
      XtVaSetValues (text_tool->menus[5], XmNmenuHistory, text_tool->spacing_items[0], NULL);
    }
  
  for (i = 0; i < nfoundries; i++)
    XtSetSensitive (text_tool->foundry_items[i], font->foundries[i] ? True : False);
  for (i = 0; i < nweights; i++)
    XtSetSensitive (text_tool->weight_items[i], font->weights[i] ? True : False);
  for (i = 0; i < nslants; i++)
    XtSetSensitive (text_tool->slant_items[i], font->slants[i] ? True : False);
  for (i = 0; i < nset_widths; i++)
    XtSetSensitive (text_tool->set_width_items[i], font->set_widths[i] ? True : False);
  for (i = 0; i < nspacings; i++)
    XtSetSensitive (text_tool->spacing_items[i], font->spacings[i] ? True : False);
}

static void
text_size_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->size_type = user_data;
}

static void
text_antialias_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->antialias = user_data;
}

static void
text_foundry_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->foundry = user_data;
  text_validate_combo (text_tool, 0);
}

static void
text_weight_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->weight = user_data;
  text_validate_combo (text_tool, 1);
}

static void
text_slant_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->slant = user_data;
  text_validate_combo (text_tool, 2);
}

static void
text_set_width_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->set_width = user_data;
  text_validate_combo (text_tool, 3);
}

static void
text_spacing_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  TextTool *text_tool;
  long user_data;
  
  text_tool = client_data;
  XtVaGetValues (w, XmNuserData, &user_data, NULL);
  text_tool->spacing = user_data;
  text_validate_combo (text_tool, 4);
}

static void 
text_validate_combo (text_tool, which)
     TextTool *text_tool;
     int which;
{
  FontInfo *font;
  int which_val;
  int new_combo[5];
  int best_combo[5];
  int best_matches;
  int matches;
  int i;

  font = font_info[text_tool->font_index];

  switch (which)
    {
    case 0:
      which_val = text_tool->foundry;
      break;
    case 1:
      which_val = text_tool->weight;
      break;
    case 2:
      which_val = text_tool->slant;
      break;
    case 3:
      which_val = text_tool->set_width;
      break;
    case 4:
      which_val = text_tool->spacing;
      break;
    }
  
  best_matches = -1;
  best_combo[0] = 0;
  best_combo[1] = 0;
  best_combo[2] = 0;
  best_combo[3] = 0;
  best_combo[4] = 0;
  
  for (i = 0; i < font->ncombos; i++)
    {
      /* we must match the which field */
      if (font->combos[i][which] == which_val)
	{
	  matches = 0;
	  new_combo[0] = 0;
	  new_combo[1] = 0;
	  new_combo[2] = 0;
	  new_combo[3] = 0;
	  new_combo[4] = 0;
	  
	  if ((text_tool->foundry == 0) || (text_tool->foundry == font->combos[i][0]))
	    {
	      matches++;
	      if (text_tool->foundry)
		new_combo[0] = font->combos[i][0];
	    }
	  if ((text_tool->weight == 0) || (text_tool->weight == font->combos[i][1]))
	    {
	      matches++;
	      if (text_tool->weight)
		new_combo[1] = font->combos[i][1];
	    }
	  if ((text_tool->slant == 0) || (text_tool->slant == font->combos[i][2]))
	    {
	      matches++;
	      if (text_tool->slant)
		new_combo[2] = font->combos[i][2];
	    }
	  if ((text_tool->set_width == 0) || (text_tool->set_width == font->combos[i][3]))
	    {
	      matches++;
	      if (text_tool->set_width)
		new_combo[3] = font->combos[i][3];
	    }
	  if ((text_tool->spacing == 0) || (text_tool->spacing == font->combos[i][4]))
	    {
	      matches++;
	      if (text_tool->spacing)
		new_combo[4] = font->combos[i][4];
	    }

	  /* if we get all 5 matches simply return */
	  if (matches == 5)
	    return;

	  if (matches > best_matches)
	    {
	      best_matches = matches;
	      best_combo[0] = new_combo[0];
	      best_combo[1] = new_combo[1];
	      best_combo[2] = new_combo[2];
	      best_combo[3] = new_combo[3];
	      best_combo[4] = new_combo[4];
	    }
	}
    }

  text_tool->foundry = best_combo[0];
  text_tool->weight = best_combo[1];
  text_tool->slant = best_combo[2];
  text_tool->set_width = best_combo[3];
  text_tool->spacing = best_combo[4];

  XtVaSetValues (text_tool->menus[1], 
		 XmNmenuHistory, text_tool->foundry_items[text_tool->foundry], 
		 NULL);
  XtVaSetValues (text_tool->menus[2], 
		 XmNmenuHistory, text_tool->weight_items[text_tool->weight], 
		 NULL);
  XtVaSetValues (text_tool->menus[3], 
		 XmNmenuHistory, text_tool->slant_items[text_tool->slant], 
		 NULL);
  XtVaSetValues (text_tool->menus[4], 
		 XmNmenuHistory, text_tool->set_width_items[text_tool->set_width], 
		 NULL);
  XtVaSetValues (text_tool->menus[5], 
		 XmNmenuHistory, text_tool->spacing_items[text_tool->spacing], 
		 NULL);
}

static void
text_get_fonts ()
{
  char **fontnames;
  char *fontname;
  char *field;
  link_ptr temp_list;
  int num_fonts;
  int index;
  int i, j;

  /* construct a valid font pattern */

  fontnames = XListFonts (DISPLAY, "-*-*-*-*-*-*-0-0-75-75-*-0-*-*", 32767, &num_fonts);

  /* the maximum size of the table is the number of font names returned */
  font_info = xmalloc (sizeof (FontInfo**) * num_fonts);
  
  /* insert the fontnames into a table */
  nfonts = 0;
  for (i = 0; i < num_fonts; i++)
    if (text_is_xlfd_font_name (fontnames[i]))
      {
	text_insert_font (font_info, &nfonts, fontnames[i]);
	
	foundries = text_insert_field (foundries, fontnames[i], FOUNDRY);
	weights = text_insert_field (weights, fontnames[i], WEIGHT);
	slants = text_insert_field (slants, fontnames[i], SLANT);
	set_widths = text_insert_field (set_widths, fontnames[i], SET_WIDTH);
	spacings = text_insert_field (spacings, fontnames[i], SPACING);
      }

  XFreeFontNames (fontnames);

  nfoundries = list_length (foundries) + 1;
  nweights = list_length (weights) + 1;
  nslants = list_length (slants) + 1;
  nset_widths = list_length (set_widths) + 1;
  nspacings = list_length (spacings) + 1;

  foundry_array = xmalloc (sizeof (char*) * nfoundries);
  weight_array = xmalloc (sizeof (char*) * nweights);
  slant_array = xmalloc (sizeof (char*) * nslants);
  set_width_array = xmalloc (sizeof (char*) * nset_widths);
  spacing_array = xmalloc (sizeof (char*) * nspacings);

  i = 1;
  temp_list = foundries;
  while (temp_list)
    {
      foundry_array[i++] = temp_list->data;
      temp_list = temp_list->next;
    }

  i = 1;
  temp_list = weights;
  while (temp_list)
    {
      weight_array[i++] = temp_list->data;
      temp_list = temp_list->next;
    }

  i = 1;
  temp_list = slants;
  while (temp_list)
    {
      slant_array[i++] = temp_list->data;
      temp_list = temp_list->next;
    }

  i = 1;
  temp_list = set_widths;
  while (temp_list)
    {
      set_width_array[i++] = temp_list->data;
      temp_list = temp_list->next;
    }

  i = 1;
  temp_list = spacings;
  while (temp_list)
    {
      spacing_array[i++] = temp_list->data;
      temp_list = temp_list->next;
    }

  foundry_array[0] = "*";
  weight_array[0] = "*";
  slant_array[0] = "*";
  set_width_array[0] = "*";
  spacing_array[0] = "*";

  for (i = 0; i < nfonts; i++)
    {
      font_info[i]->foundries = xmalloc (sizeof (int) * nfoundries);
      font_info[i]->weights = xmalloc (sizeof (int) * nweights);
      font_info[i]->slants = xmalloc (sizeof (int) * nslants);
      font_info[i]->set_widths = xmalloc (sizeof (int) * nset_widths);
      font_info[i]->spacings = xmalloc (sizeof (int) * nspacings);
      font_info[i]->ncombos = list_length (font_info[i]->fontnames);
      font_info[i]->combos = xmalloc (sizeof (int*) * font_info[i]->ncombos);

      for (j = 0; j < nfoundries; j++)
	font_info[i]->foundries[j] = 0;
      for (j = 0; j < nweights; j++)
	font_info[i]->weights[j] = 0;
      for (j = 0; j < nslants; j++)
	font_info[i]->slants[j] = 0;
      for (j = 0; j < nset_widths; j++)
	font_info[i]->set_widths[j] = 0;
      for (j = 0; j < nspacings; j++)
	font_info[i]->spacings[j] = 0;
      
      font_info[i]->foundries[0] = 1;
      font_info[i]->weights[0] = 1;
      font_info[i]->slants[0] = 1;
      font_info[i]->set_widths[0] = 1;
      font_info[i]->spacings[0] = 1;

      j = 0;
      temp_list = font_info[i]->fontnames;
      while (temp_list)
	{
	  fontname = temp_list->data;
	  temp_list = temp_list->next;
	  
	  font_info[i]->combos[j] = xmalloc (sizeof (int) * 5);
	  
	  field = text_get_field (fontname, FOUNDRY);
	  index = text_field_to_index (foundry_array, nfoundries, field);
	  font_info[i]->foundries[index] = 1;
	  font_info[i]->combos[j][0] = index;
	  free (field);

	  field = text_get_field (fontname, WEIGHT);
	  index = text_field_to_index (weight_array, nweights, field);
	  font_info[i]->weights[index] = 1;
	  font_info[i]->combos[j][1] = index;
	  free (field);

	  field = text_get_field (fontname, SLANT);
	  index = text_field_to_index (slant_array, nslants, field);
	  font_info[i]->slants[index] = 1;
	  font_info[i]->combos[j][2] = index;
	  free (field);

	  field = text_get_field (fontname, SET_WIDTH);
	  index = text_field_to_index (set_width_array, nset_widths, field);
	  font_info[i]->set_widths[index] = 1;
	  font_info[i]->combos[j][3] = index;
	  free (field);

	  field = text_get_field (fontname, SPACING);
	  index = text_field_to_index (spacing_array, nspacings, field);
	  font_info[i]->spacings[index] = 1;
	  font_info[i]->combos[j][4] = index;
	  free (field);

	  j += 1;
	}
    }
}

static void 
text_insert_font (table, ntable, fontname)
     FontInfo **table;
     int *ntable;
     char *fontname;
{
  FontInfo *temp_info;
  char *family;
  int lower, upper;
  int middle, cmp;
  
  /* insert a fontname into a table */
  family = text_get_field (fontname, FAMILY);
  if (!family)
    return;

  lower = 0;
  if (*ntable > 0)
    {
      /* Do a binary search to determine if we have already encountered
       *  a font from this family.
       */
      upper = *ntable;
      while (lower < upper)
	{
	  middle = (lower + upper) >> 1;
	  
	  cmp = strcmp (family, table[middle]->family);
	  if (cmp == 0)
	    {
	      table[middle]->fontnames = add_to_list (table[middle]->fontnames, xstrdup (fontname));
	      return;
	    }
	  else if (cmp < 0)
	    upper = middle;
	  else if (cmp > 0)
	    lower = middle+1;
	}
    }

  /* Add another entry to the table for this new font family */
  table[*ntable] = xmalloc (sizeof (FontInfo));
  table[*ntable]->family = family;
  table[*ntable]->foundries = NULL;
  table[*ntable]->weights = NULL;
  table[*ntable]->slants = NULL;
  table[*ntable]->set_widths = NULL;
  table[*ntable]->fontnames = NULL;
  table[*ntable]->fontnames = add_to_list (table[*ntable]->fontnames, xstrdup (fontname));
  (*ntable)++;

  /* Quickly insert the entry into the table in sorted order
   *  using a modification of insertion sort and the knowledge
   *  that the entries proper position in the table was determined
   *  above in the binary search and is contained in the "lower"
   *  variable.
   */
  if (*ntable > 1)
    {
      temp_info = table[*ntable - 1];
      
      upper = *ntable - 1;
      while (lower != upper)
	{
	  table[upper] = table[upper-1];
	  upper -= 1;
	}
      
      table[lower] = temp_info;
    }
}

static link_ptr
text_insert_field (list, fontname, field_num)
     link_ptr list;
     char *fontname;
     int field_num;
{
  link_ptr temp_list;
  link_ptr prev_list;
  link_ptr new_list;
  char *field;
  int cmp;

  field = text_get_field (fontname, field_num);
  if (!field)
    return list;

  temp_list = list;
  prev_list = NULL;

  while (temp_list)
    {
      cmp = strcmp (field, temp_list->data);
      if (cmp == 0)
	{
	  free (field);
	  return list;
	}
      else if (cmp < 0)
	{
	  new_list = alloc_list ();
	  new_list->data = field;
	  new_list->next = temp_list;
	  if (prev_list)
	    {
	      prev_list->next = new_list;
	      return list;
	    }
	  else
	    return new_list;
	}
      else
	{
	  prev_list = temp_list;
	  temp_list = temp_list->next;
	}
    }

  new_list = alloc_list ();
  new_list->data = field;
  new_list->next = NULL;
  if (prev_list)
    {
      prev_list->next = new_list;
      return list;
    }
  else
    return new_list;
}

static char*
text_get_field (fontname, field_num)
     char *fontname;
     int field_num;
{
  char *t1, *t2;
  char *field;

  /* we assume this is a valid fontname...that is, it has 14 fields */
  
  t1 = fontname;
  while (*t1 && (field_num >= 0))
    if (*t1++ == '-')
      field_num--;

  t2 = t1;
  while (*t2 && (*t2 != '-'))
    t2++;

  if (t1 != t2)
    {
      field = xmalloc (1 + (long) t2 - (long) t1);
      strncpy (field, t1, (long) t2 - (long) t1);
      field[(long) t2 - (long) t1] = 0;
      return field;
    }

  return xstrdup ("(nil)");
}

static int
text_field_to_index (table, ntable, field)
     char **table;
     int ntable;
     char *field;
{
  int i;

  for (i = 0; i < ntable; i++)
    if (strcmp (field, table[i]) == 0)
      return i;

  return -1;
}

static int
text_is_xlfd_font_name (fontname)
     char *fontname;
{
  int i;

  i = 0;
  while (*fontname)
    if (*fontname++ == '-')
      i++;

  return (i == 14);
}

static void
text_render (text_tool)
     TextTool * text_tool;
{
  static int pixmap_sizes[10] = { 200, 200, 201, 200, 200, 204, 203, 200, 207, 200 };

  XFontStruct *font;
  Pixmap pixmap;
  XImage *ximage;
  GC gc;
  GDisplay *gdisp;
  GRegion *gregion;
  GRegion *temp_region;
  Selection *new_select;
  unsigned char color[3];
  char fontname[256];
  char *text, *str;
  char *size_text;
  int nstrs;
  int line_width, line_height;
  int pixmap_width, pixmap_height;
  int text_width, text_height;
  int width, height;
  int antialias;
  char pixel_size[12], point_size[12];
  float size;
  char *foundry_str;
  char *weight_str;
  char *slant_str;
  char *set_width_str;
  char *spacing_str;
  int x1, y1, x2, y2;
  int x, y, i, j, k;
  
  /* get the text */
  text = XmTextGetString (text_tool->the_text);
  size_text = XmTextGetString (text_tool->size_text);
  size = atof (size_text);
  
  if ((size > 0) && text && (strlen (text) > 0))
    {
      gdisp = text_tool->gdisp_ptr;
      
      /* determine the amount of antialiasing */
      antialias = text_tool->antialias;

      /* scale the text based on the antialiasing amount */
      size *= antialias;
      
      switch (text_tool->size_type)
	{
	case PIXELS:
	  sprintf (pixel_size, "%d", (int) size);
	  sprintf (point_size, "*");
	  break;
	case POINTS:
	  sprintf (pixel_size, "*");
	  sprintf (point_size, "%d", (int) (size * 10));
	  break;
	}

      foundry_str = foundry_array[text_tool->foundry];
      if (strcmp (foundry_str, "(nil)") == 0)
	foundry_str = "";
      weight_str = weight_array[text_tool->weight];
      if (strcmp (weight_str, "(nil)") == 0)
	weight_str = "";
      slant_str = slant_array[text_tool->slant];
      if (strcmp (slant_str, "(nil)") == 0)
	slant_str = "";
      set_width_str = set_width_array[text_tool->set_width];
      if (strcmp (set_width_str, "(nil)") == 0)
	set_width_str = "";
      spacing_str = spacing_array[text_tool->spacing];
      if (strcmp (spacing_str, "(nil)") == 0)
	spacing_str = "";

      /* create the fontname */
      sprintf (fontname, "-%s-%s-%s-%s-%s-*-%s-%s-75-75-%s-*-*-*",
	       foundry_str,
	       font_info[text_tool->font_index]->family, 
	       weight_str,
	       slant_str,
	       set_width_str,
	       pixel_size, point_size,
	       spacing_str);

      /* load the font in */
      font = XLoadQueryFont (DISPLAY, fontname);
      if (!font)
	fatal_error ("unable to load font");

      /* determine the bounding box of the text */
      width = -1;
      height = 0;
      line_height = font->ascent + font->descent;

      nstrs = 0;
      str = strtok (text, "\n");
      while (str)
	{
	  nstrs += 1;

	  line_width = XTextWidth (font, str, strlen (str));
	  if (line_width > width)
	    width = line_width;
	  height += line_height;
	  
	  str = strtok (NULL, "\n");
	}

      /* We limit the largest pixmap we create to approximately 200x200.
       * This is approximate since it depends on the amount of antialiasing.
       * Basically, we want the width and height to be divisible by the antialiasing
       *  amount. (Which lies in the range 1-10).
       * This avoids problems on some X-servers (Xinside) which have problems
       *  with large pixmaps. (Specifically pixmaps which are larger - wich
       *  or height - than the screen).
       */
      pixmap_width = (width < pixmap_sizes[antialias-1]) ? width : pixmap_sizes[antialias-1];
      pixmap_height = (height < pixmap_sizes[antialias-1]) ? height : pixmap_sizes[antialias-1];

      /* determine the actual text size based on the amount of antialiasing */
      text_width = width / antialias;
      text_height = height / antialias;

      /* create the pixmap of depth 1...sometimes called a bitmap */
      pixmap = XCreatePixmap (DISPLAY, XtWindow (gdisp->disp_image->canvas), 
			      pixmap_width, pixmap_height, 1);

      /* create the gc */
      gc = XCreateGC (DISPLAY, pixmap, 0, NULL);
      XSetFont (DISPLAY, gc, font->fid);

      /* Render the text into the pixmap.
       * Since the pixmap may not fully bound the text (because we limit its size)
       *  we must tile it around the texts actual bounding box.
       */
      gregion = gregion_new (height, width);
      for (i = 0; i < height; i += pixmap_height)
	{
	  for (j = 0; j < width; j += pixmap_width)
	    {
	      /* erase the pixmap */
	      XSetForeground (DISPLAY, gc, WhitePixel (DISPLAY, DefaultScreen (DISPLAY)));
	      XFillRectangle (DISPLAY, pixmap, gc, 0, 0, width, height);
	      XSetForeground (DISPLAY, gc, BlackPixel (DISPLAY, DefaultScreen (DISPLAY)));

	      /* adjust the x and y values */
	      x = -j;
	      y = font->ascent - i;
	      str = text;
	      
	      for (k = 0; k < nstrs; k++)
		{
		  XDrawString (DISPLAY, pixmap, gc, x, y, str, strlen (str));
		  str += strlen (str) + 1;
		  y += line_height;
		}
	      
	      /* create the XImage */
	      ximage = XGetImage (DISPLAY, pixmap, 0, 0, 
				  pixmap_width, pixmap_height, 
				  AllPlanes, ZPixmap);
	      
	      if (!ximage)
		fatal_error ("sanity check failed: could not get ximage");

	      if (ximage->bits_per_pixel != 1)
		fatal_error ("sanity check failed: image should have 1 bit per pixel");

	      /* convert the XImage bitmap to a region */
	      temp_region = text_ximage_to_region (ximage, pixmap_width, pixmap_height, antialias);

	      /* Add this portion of the text region into the main region.
	       * The offsets "j / antialias" and "i / antialias" take into account
	       *  the tiling of the pixmap. (We must divide by antialias because "i"
	       *  and "j" are specified in the actual text size...not the user
	       *  specified text size).
	       */
	      gregion_combine_offset_region (gregion, ADD, temp_region,
					     j / antialias, i / antialias);
	      
	      /* free the image */
	      XDestroyImage (ximage);
	    }
	}

      if (gregion)
	{
	  /* make the new selection */
	  new_select = (Selection *) xmalloc (sizeof (Selection));
	  
	  /* get the current foreground color */
	  palette_get_foreground (&color[0], &color[1], &color[2]);
	  
	  /*  Set the selection information  */
	  new_select->region = gregion;
	  new_select->offset_x = text_tool->click_x;
	  new_select->offset_y = text_tool->click_y;
	  gregion_find_bounds (gregion, &x1, &y1, &x2, &y2);
	  new_select->float_buf = temp_buf_new ((x2 - x1), (y2 - y1), 
						gdisp->gimage->bpp, 0, 0, color);
	  new_select->orig_buf = NULL;
	  
	  /* replace the current selection */
	  selection_replace (gdisp->select, gdisp, new_select);
	}

      /* free the pixmap */
      XFreePixmap (DISPLAY, pixmap);

      /* free the gc */
      XFreeGC (DISPLAY, gc);

      /* free the font */
      XFreeFont (DISPLAY, font);
    }

  /* free the text */
  if (text)
    XtFree (text);
}

static GRegion*
text_ximage_to_region (ximage, width, height, scale)
     XImage *ximage;
     int width, height, scale;
{
  GRegion *gregion;
  int black_pixel;
  int pixel;
  int value, last_value;
  int scalex, scaley;
  int scale2;
  int x, y, w;
  int i, j;

  width /= scale;
  height /= scale;
  scale2 = scale * scale;
  black_pixel = BlackPixel (DISPLAY, DefaultScreen (DISPLAY));
  
  gregion = gregion_new (height, width);
  
  for (y = 0, scaley = 0; y < height; y++, scaley += scale)
    {
      w = 0;
      last_value = 0;
      
      for (x = 0, scalex = 0; x < width; x++, scalex += scale)
	{
	  /* calculate the pixel value */
	  
	  value = 0;
	  for (i = scaley; i < scaley + scale; i++)
	    for (j = scalex; j < scalex + scale; j++)
	      {
		pixel = XGetPixel (ximage, j, i);
		if (pixel == black_pixel)
		  value += 255;
	      }
	  value = value / scale2;
	  
	  /* calculate the segment */
	  
	  if (w && (value != last_value))
	    {
	      gregion_add_segment (gregion, x - w, y, w, last_value);
	      w = 0;
	    }

	  if (value)
	    w++;

	  last_value = value;
	}
      
      if (w)
	gregion_add_segment (gregion, x - w, y, w, last_value);
    }

  return gregion;
}

