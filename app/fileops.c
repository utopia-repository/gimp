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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/wait.h>

#include "appenv.h"
#include "errors.h"
#include "gdisplay.h"
#include "general.h"
#include "fileops.h"
#include "widget.h"
#include "workprocs.h"
#include "linked.h"
#include "plug_in.h"
#include "buildmenu.h"
#include "general.h"


/*  for file error messages  */
extern int errno;

typedef struct _FileFilter _FileFilter, *FileFilter;
typedef struct _FileData _FileData, *FileData;
typedef struct _FileOptions _FileOptions, FileOptions;

struct _FileFilter {
  link_ptr extensions;
  char *prog;
  char *title;
};

struct _FileData {
  char *filename;
  char *prog;
};

struct _FileOptions {
  char *str1;
  long ID;
};


/*  Forward function declarations  */

static void file_open_ok_callback       (Widget, XtPointer, XtPointer);
static void file_save_ok_callback       (Widget, XtPointer, XtPointer);
static void file_cancel_callback        (Widget, XtPointer, XtPointer);
static void file_destroy_callback       (Widget, XtPointer, XtPointer);
static void file_filter_callback        (Widget, XtPointer, XtPointer);
static void file_image_callback         (Widget, XtPointer, XtPointer);

static char* find_file_filter           (char *);
static void call_file_filter            (char *, char *, long, void *);

static char* get_extension              (char *);

static void file_overwrite              (Widget, char *);
static void file_overwrite_yes_callback (Widget, XtPointer, XtPointer);
static void file_overwrite_no_callback  (Widget, XtPointer, XtPointer);

static void file_add                    (char *, char *);
static void file_plug_in_callback       (void *, void *);


/*  Some variables  */

static link_ptr file_filters = NULL;
static MenuItem *file_items;

static link_ptr file_list = NULL;
static int file_is_opening = 0;


void
file_io_init ()
{
  FileFilter ff;
  link_ptr tmp;
  long n;

  n = 3;
  tmp = file_filters;
  do {
    n++;
    tmp = next_item (tmp);
  } while (tmp);

  file_items = (MenuItem *) xmalloc (sizeof (MenuItem) * n);

  file_items[0].label = "by extension";
  file_items[0].class = &xmPushButtonGadgetClass;
  file_items[0].mnemonic = 0;
  file_items[0].accelerator = NULL;
  file_items[0].accel_text = NULL;
  file_items[0].callback = file_filter_callback;
  file_items[0].callback_data = NULL;
  file_items[0].subitems = NULL;

  file_items[1].label = "";
  file_items[1].class = &xmSeparatorGadgetClass;
  file_items[1].mnemonic = 0;
  file_items[1].accelerator = NULL;
  file_items[1].accel_text = NULL;
  file_items[1].callback = NULL;
  file_items[1].callback_data = NULL;
  file_items[1].subitems = NULL;

  n = 2;
  tmp = file_filters;
  while (tmp)
    {
      ff = tmp->data;
      file_items[n].label = ff->title;
      file_items[n].class = &xmPushButtonGadgetClass;
      file_items[n].mnemonic = 0;
      file_items[n].accelerator = NULL;
      file_items[n].accel_text = NULL;
      file_items[n].callback = file_filter_callback;
      file_items[n].callback_data = ff->prog;
      file_items[n].subitems = NULL;

      tmp = next_item (tmp);
      n++;
    }

  file_items[n].label = NULL;
}

void
add_file_filter (ext, prog, types, title)
     char *ext, *prog, *types, *title;
{
  FileFilter ff;
  FileFilter ff2;
  link_ptr last, tmp, new_link;
  char *t, *p;

  ff = xmalloc (sizeof (_FileFilter));
  ff->extensions = NULL;
  ff->prog = xstrdup (prog);
  ff->title = xstrdup (title);

  t = strtok (ext, ",");
  while (t)
    {
      p = xstrdup (t);
      t = p;
      while (*t)
	{
	  *t = tolower (*t);
	  t++;
	}
      ff->extensions = add_to_list (ff->extensions, p);
      t = strtok (NULL, ",");
    }

  /* create a sorted list of file filters */
  last = NULL;

  if (file_filters)
    {
      tmp = file_filters;
      while (tmp)
	{
	  ff2 = tmp->data;
	  if (strcmp (ff->title, ff2->title) < 0)
	    {
	      new_link = alloc_list ();
	      if (!new_link)
		fatal_error ("Unable to allocate memory");

	      new_link->data = ff;
	      new_link->next = tmp;
	      if (!last)
		file_filters = new_link;
	      else
		last->next = new_link;

	      if (tmp == file_filters)
		file_filters = new_link;
	      break;
	    }
	  last = tmp;
	  tmp = next_item (tmp);
	}

      if (!tmp)
	file_filters = append_to_list (file_filters, ff);
    }
  else
    file_filters = add_to_list (file_filters, ff);
}


static void*
file_add_open_options (w)
     Widget w;
{
  Widget option_menu, menu;
  FileOptions *value;

  option_menu = BuildMenu (w, XmMENU_OPTION, "File plug-in",
                           0, False, file_items,
			   DefaultVisualOfScreen (XtScreen (toplevel)), 
			   DefaultColormapOfScreen (XtScreen (toplevel)),
			   DefaultDepthOfScreen (XtScreen (toplevel)));

  XtManageChild (option_menu);

  value = xmalloc (sizeof (FileOptions));
  value->str1 = NULL;
  value->ID = 0;
  XtVaGetValues (option_menu,
                 XmNsubMenuId, &menu,
                 NULL);
  XtVaSetValues (menu,
                 XmNuserData, value,
                 NULL);

  return value;
}

static void*
file_add_save_options (w, ID)
     Widget w;
     int ID;
{
  extern link_ptr image_list;

  Widget rowcol, option, menu, item;
  XmString str;
  Arg args[2];
  char *label;
  FileOptions *value;
  GImage *gimage;
  char *temp_str;
  link_ptr tmp;
  int n;

  rowcol = XtVaCreateWidget ("dialogPartition", xmRowColumnWidgetClass, w,
                             XmNpacking, XmPACK_TIGHT,
                             XmNorientation, XmVERTICAL,
                             XmNnumColumns, 1,
                             NULL);

  menu = XmCreatePulldownMenu (rowcol, "_pulldown", NULL, 0);

  n = 0;
  str = XmStringCreateLocalized ("Select an image");
  XtSetArg (args[n], XmNsubMenuId, menu); n++;
  XtSetArg (args[n], XmNlabelString, str); n++;
  option = XmCreateOptionMenu (rowcol, "Select an image", args, n);
  XmStringFree (str);
  
  tmp = image_list;
  while (tmp)
    {
      gimage = tmp->data;
      tmp = next_item (tmp);

      if (!ID)
	ID = gimage->ID;

      temp_str = prune_filename (gimage->filename);
      label = xmalloc (strlen (temp_str) + 5);
      sprintf (label, "%s-%d\n", temp_str, gimage->ID);

      item = XtVaCreateManagedWidget (label, xmPushButtonGadgetClass, menu, NULL);
      XtAddCallback (item, XmNactivateCallback, file_image_callback, (void*) gimage->ID);

      if (gimage->ID == ID)
	XtVaSetValues (menu, XmNmenuHistory, item, NULL);

      xfree (label);
    }
  
  XtManageChild (rowcol);
  XtManageChild (option);

  value = file_add_open_options (rowcol);
  value->ID = ID;
  XtVaSetValues (menu,
                 XmNuserData, value,
                 NULL);

  return value;
}

void
file_open_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  static Widget filesel = NULL;
  static Widget frame = NULL;
  Widget help_widget;
  void *value;
 
  if (!filesel)
    {
      filesel = XmCreateFileSelectionDialog (toplevel, "openFileDialog", NULL, 0);
      XtVaSetValues (filesel, 
		     XmNdefaultPosition, False,
		     XmNdeleteResponse, XmDESTROY,
		     NULL);

      help_widget = XmFileSelectionBoxGetChild (filesel, XmDIALOG_HELP_BUTTON);
      XtSetSensitive (help_widget, False);

      frame = XtVaCreateManagedWidget ("frame", xmFrameWidgetClass, filesel,
				       XmNshadowType, XmSHADOW_ETCHED_IN,
				       NULL);

      value = file_add_open_options (frame);
      XtVaSetValues (filesel, XmNuserData, value, NULL);      
      XtManageChild (frame);

      XtAddCallback (filesel, XmNmapCallback, map_dialog, NULL);
      XtAddCallback (filesel, XmNokCallback, file_open_ok_callback, filesel);
      XtAddCallback (filesel, XmNcancelCallback, file_cancel_callback, filesel);
      XtAddCallback (filesel, XmNdestroyCallback, file_destroy_callback, &filesel);
    }
  
  XtManageChild (filesel);
}

void
file_save_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  extern link_ptr image_list;
  static Widget filesel = NULL;
  static Widget frame = NULL;
  static void *value = NULL;
  Widget help_widget;
  GDisplay *gdisplay;

  if (!image_list)
    return;

  gdisplay = gdisplay_active (w);
 
  if (!filesel)
    {
      filesel = XmCreateFileSelectionDialog (toplevel, "saveFileDialog", NULL, 0);
      XtVaSetValues (filesel, 
		     XmNdefaultPosition, False,
		     XmNdeleteResponse, XmDESTROY,
		     NULL);
      help_widget = XmFileSelectionBoxGetChild (filesel, XmDIALOG_HELP_BUTTON);
      XtSetSensitive (help_widget, False);

      XtAddCallback (filesel, XmNmapCallback, map_dialog, NULL);
      XtAddCallback (filesel, XmNokCallback, file_save_ok_callback, filesel);
      XtAddCallback (filesel, XmNcancelCallback, file_cancel_callback, filesel);
      XtAddCallback (filesel, XmNdestroyCallback, file_destroy_callback, &filesel);

      frame = XtVaCreateManagedWidget ("frame", xmFrameWidgetClass, filesel,
				       XmNshadowType, XmSHADOW_ETCHED_IN,
				       NULL);
    }

  if (frame)
    XtUnmanageChild (frame);
  if (value)
    xfree (value);

  value = file_add_save_options (frame, (gdisplay) ? gdisplay->gimage->ID : 0);

  XtVaSetValues (filesel, XmNuserData, value, NULL);  
  XtManageChild (frame);
  XtManageChild (filesel);
}


int
file_open (filename, prog)
     char *filename, *prog;
{
  struct stat buf;
  int err;
  
  err = stat (filename, &buf);
  if (!err)
    {
      if (buf.st_mode & S_IFDIR)
	{
	  warning ("\"%s\" is a directory", filename);
	  return 0;
	}
    }
  else
    {
      if (errno == ENOENT)
	warning ("\"%s\" does not exist", filename);
      else
	warning ("unknown error");
      return 0;
    }

  file_add (filename, prog);

  return 1;
}

int
file_save (ID, filename, ext)
     long ID;
     char *filename, *ext;
{
  GImage *gimage;
  char *prog, *base_name;
  struct stat buf;
  int err;

  err = stat (filename, &buf);
  if (!err)
    {
      if (buf.st_mode & S_IFDIR)
	{
	  warning ("\"%s\" is a directory (cannot overwrite)", filename);
	  return 0;
	}
    }

  base_name = prune_filename (filename);
  if (base_name && base_name[0])
    {
      if (!ext)
	ext = get_extension (filename);
      prog = find_file_filter (ext);
      if (!prog)
	{
	  warning ("Unable to find file filter for: \"%s\"", filename);
	  return 0;
	}
      else
	{
	  gimage = gimage_get_ID (ID);
	  gimage->has_filename = 1;
	  if (gimage->filename)
	    xfree (gimage->filename);
	  gimage->filename = xstrdup (filename);
	  gdisplay_update_title (ID);
	  
	  call_file_filter (prog, filename, SAVE, gimage);
	  return 1;
	}
    }
  else
    return 1;
}

static void
file_open_ok_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XmFileSelectionBoxCallbackStruct *cbs;
  char *filename;
  FileOptions *value;

  cbs = (XmFileSelectionBoxCallbackStruct *) call_data;

  if (!XmStringGetLtoR (cbs->value, XmFONTLIST_DEFAULT_TAG, &filename))
    return;

  XtVaGetValues (w, XmNuserData, &value, NULL);

  if (file_open (filename, value->str1))
    file_cancel_callback (w, client_data, call_data);	  /*  close the dialog  */

  XtFree (filename);
}

static void
file_save_ok_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  static Widget last_widget;
  static Widget last_client_data;
  static Widget last_call_data;
  static char *filename;

  XmFileSelectionBoxCallbackStruct *cbs;
  FileOptions *value;
  char str[64];
  struct stat buf;
  int err;

  if (w)
    {
      last_widget = w;
      last_client_data = client_data;
      last_call_data = call_data;

      cbs = (XmFileSelectionBoxCallbackStruct *) call_data;
      
      if (!XmStringGetLtoR (cbs->value, XmFONTLIST_DEFAULT_TAG, &filename))
	return;
      
      err = stat (filename, &buf);
      if (!err)
	{
	  if (buf.st_mode & S_IFDIR)
	    {
	      warning ("\"%s\" is a directory (cannot overwrite)", filename);
	      return;
	    }
	  
	  sprintf (str, "Overwrite file \"%s\"", filename);
	  file_overwrite (client_data, str);
	}
      else if (errno == ENOENT)
	{
	  XtVaGetValues (w, XmNuserData, &value, NULL);
	  if (file_save (value->ID, filename, value->str1))
	    file_cancel_callback (w, client_data, call_data);

	  XtFree (filename);
	}
    }
  else
    {
      if ((long) client_data)
	{
	  XtVaGetValues (last_widget, XmNuserData, &value, NULL);
	  if (file_save (value->ID, filename, value->str1))
	    file_cancel_callback (last_widget, last_client_data, last_call_data);
	}

      XtFree (filename);
    }
}


static void
file_destroy_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  Widget * shell;

  shell = (Widget *) client_data;

  /*  Set the dialog shell to NULL, so that it is re-created.  */
  *shell = NULL;
}


static void
file_cancel_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XtUnmanageChild ((Widget) client_data);
}

static void
file_filter_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  FileOptions *value;

  XtVaGetValues (XtParent (w), XmNuserData, &value, NULL);
  value->str1 = client_data;
}

static void
file_image_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  FileOptions *value;

  XtVaGetValues (XtParent (w), XmNuserData, &value, NULL);
  value->ID = (long) client_data;
}

static char*
find_file_filter (ext)
     char *ext;
{
  FileFilter ff;
  link_ptr tmp, tmp2;
  char buf[16], *t;

  strcpy (buf, ext);
  t = buf;
  while (*t)
    {
      *t = tolower (*t);
      t++;
    }

  tmp = file_filters;
  while (tmp)
    {
      ff = tmp->data;
      tmp = next_item (tmp);

      tmp2 = ff->extensions;
      while (tmp2)
	{
	  if (strcmp (tmp2->data, buf) == 0)
	    return ff->prog;
	  tmp2 = tmp2->next;
	}
    }
  
  return NULL;
}

static void
call_file_filter (prog, filename, msg, data)
     char *prog, *filename;
     long msg;
     void *data;
{
  PlugInP plug_in;

  plug_in = make_plug_in (prog, data, NULL);
  if (plug_in_open (plug_in))
    {
      if (msg == LOAD)
	plug_in_set_callback (plug_in, file_plug_in_callback, NULL);
      plug_in_send_message (plug_in, msg, strlen (filename) + 1, filename);
    }
  else
    free_plug_in (plug_in);
}

static char*
get_extension (file)
     char *file;
{
  char *period = file;

  while (*file)
    if (*file++ == '.')
      period = file;

  return period;
}

static void
file_overwrite (parent, str)
     Widget parent;
     char *str;
{
  static Widget shell = NULL;
  static Widget dialog = NULL;
  static Widget rowcol = NULL;
  static Widget label = NULL;
  static Widget action_area = NULL;
  static Widget yes_button = NULL;
  static Widget no_button = NULL;
 
  if (!shell)
    {
      shell = XtVaCreatePopupShell ("overwriteDialog", 
				    xmDialogShellWidgetClass, toplevel, 
				    NULL);

      dialog = XtVaCreateWidget ("bulletinBoard", xmBulletinBoardWidgetClass, shell, NULL);
      XtVaSetValues (dialog, XmNdefaultPosition, False, NULL);
      XtAddCallback (dialog, XmNmapCallback, map_dialog, NULL);

      rowcol = XtVaCreateManagedWidget ("dialogPartition", xmRowColumnWidgetClass, dialog,
					XmNpacking, XmPACK_TIGHT,
					XmNorientation, XmVERTICAL,
					NULL);
      
      label = XtVaCreateManagedWidget ("dialogQuery", xmLabelGadgetClass, rowcol, NULL);
      action_area = XtVaCreateManagedWidget ("actionArea", xmFormWidgetClass, rowcol, NULL);
      yes_button = XtVaCreateManagedWidget ("Yes", xmPushButtonGadgetClass, action_area,
					    XmNrightAttachment, XmATTACH_POSITION,
					    XmNleftAttachment, XmATTACH_FORM,
					    XmNtopAttachment, XmATTACH_FORM,
					    XmNbottomAttachment, XmATTACH_FORM,
					    XmNrightPosition, 45,
					    NULL);
      no_button = XtVaCreateManagedWidget ("No", xmPushButtonGadgetClass, action_area,
					   XmNrightAttachment, XmATTACH_FORM,
                                           XmNleftAttachment, XmATTACH_POSITION,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNbottomAttachment, XmATTACH_FORM,
                                           XmNleftPosition, 55,
                                           NULL);

      XtAddCallback (yes_button, XmNactivateCallback, file_overwrite_yes_callback, shell);
      XtAddCallback (no_button, XmNactivateCallback, file_overwrite_no_callback, shell);
    }

  if (XtIsManaged (dialog))
    XtUnmanageChild (dialog);
  
  {
    XmString tempstr = XmStringCreateLocalized (str);
    XtVaSetValues (label, XmNlabelString, tempstr, NULL);
    XmStringFree (tempstr);
  }

  XtManageChild (dialog);
  XtManageChild (shell);
  XtPopup (shell, XtGrabNone);
  XtAddGrab (shell, True, False);
}

static void
file_overwrite_yes_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XtRemoveGrab ((Widget) client_data);
  XtPopdown ((Widget) client_data);
  file_save_ok_callback (NULL, (XtPointer) 1, NULL);
}

static void
file_overwrite_no_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  XtRemoveGrab ((Widget) client_data);
  XtPopdown ((Widget) client_data);
  file_save_ok_callback (NULL, (XtPointer) 0, NULL);
}

static void
file_add (filename, prog)
     char *filename, *prog;
{
  FileData data;

  data = xmalloc (sizeof (_FileData));

  data->filename = xstrdup (filename);
  if (prog)
    data->prog = xstrdup (prog);
  else
    data->prog = NULL;

  file_list = append_to_list (file_list, data);

  if (!file_is_opening)
    file_plug_in_callback (NULL, 0);
}

static void
file_plug_in_callback (client_data, call_data)
     void *client_data;
     void *call_data;
{
  link_ptr tmp_link;
  FileData filedata;
  char *prog, *base_name;
  char *ext;
  
  if ((int) call_data == 2)
    {
      tmp_link = file_list;
      while (tmp_link)
	{
	  filedata = tmp_link->data;
	  tmp_link = tmp_link->next;

	  if (filedata->filename)
	    xfree (filedata->filename);
	  if (filedata->prog)
	    xfree (filedata->prog);
	  xfree (filedata);
	}

      file_list = free_list (file_list);
    }
  
  if (file_list)
    {
      tmp_link = file_list;
      file_list = file_list->next;
      
      filedata = tmp_link->data;
      tmp_link->next = NULL;
      free_list (tmp_link);

      base_name = prune_filename (filedata->filename);
      if (base_name && base_name[0])
	{
	  if (filedata->prog)
	    {
	      prog = filedata->prog;
	    }
	  else
	    {
	      ext = get_extension (base_name);
	      prog = find_file_filter (ext);
	    }
	  
	  if (!prog)
	    {
	      warning ("Unable to find file filter for: \"%s\"", filedata->filename);
	      if (filedata->filename)
		xfree (filedata->filename);
	      if (filedata->prog)
		xfree (filedata->prog);
	      xfree (filedata);
	      
	      file_plug_in_callback (client_data, call_data);
	      return;
	    }
	  else
	    {
	      call_file_filter (prog, filedata->filename, LOAD, NULL);

	      if (filedata->filename)
		xfree (filedata->filename);
	      if (filedata->prog)
		xfree (filedata->prog);
	      xfree (filedata);
	      
	      file_is_opening = 1;
	    }
	}
    }
  else
    file_is_opening = 0;
}
