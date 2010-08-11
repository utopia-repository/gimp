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
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include "appenv.h"
#include "autodialog.h"
#include "errors.h"
#include "fileops.h"
#include "general.h"
#include "gdisplay.h"
#include "gimage.h"
#include "gimprc.h"
#include "linked.h"
#include "menus.h"
#include "palette.h"
#include "plug_in.h"
#include "shadow_ops.h"
#include "widget.h"

#define MESSAGE_DELAY  5000

typedef void (*MsgProc) (PlugInP, MsgP);

/*
 * This is a private structure for storing information about a plug-in
 *  as it is being parsed in from the "gimprc" file. The idea is that
 *  a list of the structures below is build up as the file is parsed.
 *  Then the structure is traversed a menu is created which is then 
 *  placed in the appropriate position in the menubar and popup menus.
 * Note: An item doesn't need 'accelerator' or 'accel_text' fields, but
 *       it does need a 'prog' and 'title' field.
 */

typedef struct _PlugInItem _PlugInItem, *PlugInItem;
struct _PlugInItem {
  char     * prog;             /* the name of the program */
  char     * title;            /* the title of the program as it appears in this item */
  char     * accelerator;      /* the accelerator */
  char     * accel_text;       /* the accelerator text */
  link_ptr   subitems;         /* subitems (if any) for this item */
};

/*
 * This is another private structure for storing information about a plug-in.
 *  It stores information about parameters that plug-ins use for their
 *  operation.
 */

typedef struct _PlugInParams _PlugInParams, *PlugInParams;
struct _PlugInParams {
  char * prog;
  long   size;
  void * data;
};

typedef struct _MessageDialog _MessageDialog, *MessageDialog;
struct _MessageDialog {
  Widget dlg;
  XtIntervalId timer;
  PlugInP plug_in;
};

/*
 * Static declarations for the parsing routines.
 */

static MenuItem* plug_in_make_menu (link_ptr);
static char* plug_in_strip_title (char *);
static void plug_in_callback (Widget, XtPointer, XtPointer);
static void* plug_in_read (PlugInP, long);
static int plug_in_write (PlugInP, long, void*);
static AutoDialog plug_in_find_dialog (PlugInP, int);
static PlugInParams plug_in_find_params (char *);
static void plug_in_set_params (char *, long, void *);
static Boolean plug_in_workproc (XtPointer);

/*
 * Static declarations for the message handlers.
 */

static void handle_quit (PlugInP, MsgP);

static void handle_image_new (PlugInP, MsgP);
static void handle_image_display (PlugInP, MsgP);
static void handle_image_input_output (PlugInP, MsgP);
static void handle_image_color (PlugInP, MsgP);
static void handle_image_update (PlugInP, MsgP);

static void handle_load (PlugInP, MsgP);
static void handle_save (PlugInP, MsgP);

static void handle_params (PlugInP, MsgP);
static void handle_progress (PlugInP, MsgP);
static void handle_message (PlugInP, MsgP);

static void handle_get_params (PlugInP, MsgP);
static void handle_set_params (PlugInP, MsgP);

static void handle_dialog (PlugInP, MsgP);
static void handle_dialog_new (PlugInP, MsgP);
static void handle_dialog_show (PlugInP, MsgP);
static void handle_dialog_update (PlugInP, MsgP);
static void handle_dialog_close (PlugInP, MsgP);
static void handle_dialog_new_item (PlugInP, MsgP);
static void handle_dialog_show_item (PlugInP, MsgP);
static void handle_dialog_hide_item (PlugInP, MsgP);
static void handle_dialog_change_item (PlugInP, MsgP);
static void handle_dialog_delete_item (PlugInP, MsgP);

static void handle_cancel_callback (void *, void *);
static void handle_dialog_callback (int, int, void *, void *);

static void handle_message_dialog_callback (Widget, XtPointer, XtPointer);
static void handle_message_dialog_timeout (XtPointer, XtIntervalId *);

/*
 * Static declarations of local variables
 */

static link_ptr active_plug_ins = NULL;
static link_ptr plug_in_params = NULL;

static MsgProc msg_procs[] =
{
  handle_quit,
  handle_image_new,
  handle_image_display,
  handle_image_input_output,
  handle_image_input_output,
  handle_image_color,
  handle_image_update,
  handle_load,
  handle_save,
  handle_params,
  handle_progress,
  handle_message,
  handle_dialog,
};

static MsgProc param_procs[] =
{
  handle_get_params,
  handle_set_params,
};

static MsgProc dialog_procs[] =
{
  handle_dialog_new,
  handle_dialog_show,
  handle_dialog_update,
  handle_dialog_close,
  handle_dialog_new_item,
  handle_dialog_show_item,
  handle_dialog_hide_item,
  handle_dialog_change_item,
  handle_dialog_delete_item,
  NULL,
};

static int n_msg_procs = sizeof (msg_procs) / sizeof (MsgProc);
static int n_param_procs = sizeof (param_procs) / sizeof (MsgProc);
static int n_dialog_procs = sizeof (dialog_procs) / sizeof (MsgProc);

/*
 * This function should be called once before initialization of the
 *  interface. 
 */

void
plug_in_init ()
{
  MenuItem *plug_in_items;

  /* Take the list of items and create a menu */
  plug_in_items = plug_in_make_menu (plug_ins);

  /* Set the new menu as the filter menu */
  set_filter_menu (plug_in_items);
}

void
plug_in_kill ()
{
  PlugInP plug_in;

  while (active_plug_ins)
    {
      plug_in = active_plug_ins->data;
      message ("killing: %d: %s", plug_in->pid, plug_in->args[0]);
      plug_in_close (plug_in, 1);
    }
}

/*
 * Add an item to the list 'items'. 
 * There are several things that are done in the process.
 * The 'title' string is parsed (if you can call it that) and
 *  the item is filtered down to the appropriate sub menu (or
 *  one is created).
 * An insertion sort mechanism is used so that the menu will
 *  be sorted.
 * Memory for strings is allocated in several places. Take note.
 */

link_ptr
plug_in_add_item (items, prog, title, accelerator, accel_text)
     link_ptr items;
     char *prog, *title, *accelerator, *accel_text;
{
  PlugInItem plug_in;
  char *item_title;
  link_ptr tmp, new_link;
  link_ptr prev;
  int val, has_subitems;

  /* strip the first item (before a slash) off the title */
  item_title = plug_in_strip_title (title);

  /* advance the title string forward to the next item */
  title += strlen (item_title);

  /* Is this the last item in the title string?
     If not, advance one more time so we are actually at the next item */
  has_subitems = (title && *title);
  if (has_subitems)
    title++;

  /* Insert the item in the list */
  if (items)
    {
      prev = NULL;
      tmp = items;
      do {
	  if (tmp)
	    {
	      plug_in = tmp->data;
	      
	      /* do the comparison needed for the insertion sort */
	      val = strcmp (item_title, plug_in->title);
	    }
	  else
	    val = -1;

          if (val < 0)
            {
	      /* this is the place the item goes */

	      /* make a new plug-in */
	      plug_in = xmalloc (sizeof (_PlugInItem));

	      /* Only copy the program name if this is the actual item.
	       * Ditto for the accelerator and the accelerator text.
	       */
	      plug_in->prog = has_subitems ? NULL : xstrdup (prog);
	      plug_in->title = item_title;
	      plug_in->accelerator = (has_subitems || !accelerator) ? NULL : xstrdup (accelerator);
	      plug_in->accel_text = (has_subitems || !accel_text) ? NULL : xstrdup (accel_text);
	      plug_in->subitems = NULL;

	      /* Insert the item into the list. We'll have to create
	       *  a new link and then do a little insertion. 
	       */
              new_link = alloc_list ();
	      if (!new_link)
		fatal_error ("Unable to allocate memory");

              new_link->data = plug_in;
              new_link->next = tmp;

              if (prev)
                prev->next = new_link;
              if (tmp == items)
                items = new_link;

	      /* Set 'val' equal to 0 so that we'll fall through the next
	       *  comparison. (Since this IS the correct location for the item).
	       */
	      val = 0;
            }
	  
	  if (val == 0)
	    {
	      /* We get here if there is already an item with our name on it
	       *  in the current menu. That item MUST be a submenu, so we'll just
	       *  add ourselves into its list.
	       */

	      if (has_subitems)
		plug_in->subitems = plug_in_add_item (plug_in->subitems, prog, title, 
						      accelerator, accel_text);
	      break;
	    }

	  /* Advance to the next item in the list.
	   */
          prev = tmp;
          tmp = next_item (tmp);
        } while (prev);
    }
  else
    {
      /* There are no items in the 'items' list, so we'll just start
       *  one right now.
       */
      plug_in = xmalloc (sizeof (_PlugInItem));

      plug_in->prog = has_subitems ? NULL : xstrdup (prog);
      plug_in->title = item_title;
      plug_in->accelerator = (has_subitems || !accelerator) ? NULL : xstrdup (accelerator);
      plug_in->accel_text = (has_subitems || !accel_text) ? NULL : xstrdup (accel_text);
      plug_in->subitems = NULL;

      items = add_to_list (items, plug_in);
      
      if (has_subitems)
	plug_in->subitems = plug_in_add_item (plug_in->subitems, prog, title, 
					      accelerator, accel_text);
    }

  return items;
}

/*
 * This function makes the actual menu be recursing through the
 *  previously created 'items' structure.
 * Note: This function destroys the 'items' structures as it runs.
 */

static MenuItem*
plug_in_make_menu (items)
     link_ptr items;
{
  MenuItem *menu_items;
  PlugInItem plug_in;
  link_ptr tmp;
  long n;

  menu_items = NULL;

  if (items)
    {
      /* Count the number of items that will be in this menu.
       */
      n = 1;
      tmp = items;
      while (tmp)
	{
	  tmp = next_item (tmp);
	  n++;
	}
      
      /* Allocate space for this menu.
       */
      menu_items = (MenuItem *) xmalloc (sizeof (MenuItem) * n);
      
      /* Create the items in the menu.
       */
      n = 0;
      tmp = items;
      while (tmp)
	{
	  plug_in = tmp->data;
	  
	  /* We'll just grab the pointers to the character strings, since
	   *  they won't be destroyed when the 'items' list is. (ie They'll
	   *  hang around).
	   * Submenus are created by the recursive call.
	   */
	  menu_items[n].label = plug_in->title;
	  menu_items[n].class = &xmPushButtonGadgetClass;
	  menu_items[n].mnemonic = 0;
	  menu_items[n].accelerator = plug_in->accelerator;
	  menu_items[n].accel_text = plug_in->accel_text;
	  menu_items[n].callback = plug_in_callback;
	  menu_items[n].callback_data = plug_in->prog;
	  menu_items[n].subitems = plug_in_make_menu (plug_in->subitems);
	  
	  tmp = next_item (tmp);
	  n++;
	}
      
      menu_items[n].label = NULL;

      free_list (items);
    }
  
  return menu_items;
}

/*
 * Strip off the first part of a title. That is, all the text
 *  before a slash.
 */

static char*
plug_in_strip_title (str)
     char *str;
{
  int count = 0;
  char *new_str;
  char *old_str;

  old_str = str;

  while (*str)
    {
      if (*str++ == '/')
	break;
      count++;
    }

  if (count)
    {
      new_str = xmalloc (count + 1);

      strncpy (new_str, old_str, count);
      new_str[count] = 0;

      return new_str;
    }
  else
    return NULL;
}

/*
 * The callback routine that gets called when a filter is
 *  selected from a menu.
 */

static void
plug_in_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  PlugInP plug_in;
  GDisplay *gdisplay;
  GImage *gimage;

  /* Make the plug-in.
   */
  plug_in = make_plug_in (client_data, NULL, gdisplay_active (w));

  gdisplay = gdisplay_active (w);
  gimage = gdisplay->gimage;
  gimage_add_plug_in (gimage, plug_in);
}

/*
 * This is a simple utility function that searches through
 *  a plug-ins list of dialogs and returns the one with
 *  id corresponding to "dialog_ID".
 */

static AutoDialog
plug_in_find_dialog (plug_in, dialog_ID)
     PlugInP plug_in;
     int dialog_ID;
{
  AutoDialog dialog;
  link_ptr tmp;

  tmp = plug_in->dialogs;  
  while (tmp)
    {
      dialog = tmp->data;
      if (dialog->dialog_ID == dialog_ID)
	  return dialog;

      tmp = next_item (tmp);
    }

  return NULL;
}

static PlugInParams
plug_in_find_params (title)
     char *title;
{
  PlugInParams params;
  link_ptr tmp;

  tmp = plug_in_params;
  while (tmp)
    {
      params = tmp->data;
      if (strcmp (params->prog, title) == 0)
	return params;

      tmp = next_item (tmp);
    }

  return NULL;
}

static void
plug_in_set_params (title, size, data)
     char *title;
     long size;
     void *data;
{
  PlugInParams params;

  params = plug_in_find_params (title);
  if (!params)
    {
      params = xmalloc (sizeof (_PlugInParams));

      params->prog = xstrdup (title);
      params->size = 0;
      params->data = NULL;
    }

  if (params->data)
    xfree (params->data);

  params->size = size;
  params->data = data;

  plug_in_params = add_to_list (plug_in_params, params);
}

static Boolean
plug_in_workproc (client_data)
     XtPointer client_data;
{
  PlugInP plug_in;
  
  plug_in = client_data;
  if (plug_in)
    {
      if (!plug_in->messages)
	{
	  if (plug_in->callback)
	    {
	      (* plug_in->callback) (plug_in->callback_data, (void*) (plug_in->destroy - 1));
	      plug_in->callback = NULL;
	      plug_in->callback_data = NULL;
	    }
	  plug_in->destroy = 0;
	  free_plug_in (plug_in);
	}
      else
	return False;
    }

  return True;
}

PlugInP
make_plug_in (name, image, display)
     char *name;
     void *image, *display;
{
  PlugInP plug_in;
  GDisplay *gdisplay;
  char *path;

  /* Don't even try to do anything if there isn't a search path.
   */
  if (!plug_in_path)
    return NULL;
  
  /* Find the filter.
   */
  path = search_in_path (plug_in_path, name);
  if (!path)
    {
      warning ("Unable to locate filter: \"%s\"", name);
      return 0;
    }
  
  /* Allocate space for the new plug-in.
   */
  plug_in = (PlugInP) xmalloc (sizeof (PlugIn));

  /* Set all fields to null values.
   */
  plug_in->open = 0;
  plug_in->destroy = 0;
  plug_in->pid = 0;
  plug_in->args = 0;
  plug_in->my_read = 0;
  plug_in->my_write = 0;
  plug_in->his_read = 0;
  plug_in->his_write = 0;
  plug_in->send_msg.info.type = 0;
  plug_in->send_msg.info.size = 0;
  plug_in->send_msg.data = 0;
  plug_in->recv_msg.info.type = 0;
  plug_in->recv_msg.info.size = 0;
  plug_in->recv_msg.data = 0;
  plug_in->input_id = 0;
  plug_in->dialogs = NULL;
  plug_in->messages = NULL;
  plug_in->images = NULL;
  plug_in->progress = NULL;
  plug_in->callback = NULL;
  plug_in->callback_data = NULL;
  plug_in->load_image = 0;

  /* Sometimes plug-ins are created without an input image, but with
   *  an input display. We'll take care of getting the image from the
   *  display here, if that is the case.
   */
  gdisplay = display;  
  if (!image && gdisplay)
    plug_in->image = gdisplay->gimage;
  else
    plug_in->image = image;
  plug_in->display = display;
  
  /* Create the command line arguments for the filter.
   * The first is, of course, the filter name. 
   * The second is "-gimp", which the filter uses to
   *  recognize as being called from the gimp.
   * The third and fourth arguments are the file descriptors
   *  the filter should use for communicating with the gimp.
   *  They are set when the plug-in is opened.
   */
  plug_in->args = (char**) xmalloc (sizeof (char*) * 5);
  plug_in->args[0] = xstrdup (path);
  plug_in->args[1] = xstrdup ("-gimp");
  plug_in->args[2] = (char*) xmalloc (sizeof (char) * 16);
  plug_in->args[3] = (char*) xmalloc (sizeof (char) * 16);
  plug_in->args[4] = NULL;
  
  return plug_in;
}

void
free_plug_in (plug_in)
     PlugInP plug_in;
{
  if (plug_in)
    {
      /* Make sure the plug-in is closed.
       * The '1' specifies that it should be killed if
       *  necessary.
       */
      plug_in_close (plug_in, 1);

      /* Free the various dynamically allocated space.
       */
      if (plug_in->args)
	{
	  if (plug_in->args[0])
	    xfree (plug_in->args[0]);
	  if (plug_in->args[1])
	    xfree (plug_in->args[1]);
	  if (plug_in->args[2])
	    xfree (plug_in->args[2]);
	  if (plug_in->args[3])
	    xfree (plug_in->args[3]);

	  xfree (plug_in->args);
	}

      /* Set values to NULL (just in case).
       */
      plug_in->args = NULL;

      /* Free the actual plug-in.
       */
      if (!plug_in->destroy)
	xfree (plug_in);
    }
}

int
plug_in_open (plug_in)
     PlugInP plug_in;
{
  int my_read[2];
  int my_write[2];

  if (plug_in)
    {
      /* Open two pipes. (Bidirectional communication).
       */
      if ((pipe (my_read) == -1) || (pipe (my_write) == -1))
	fatal_error ("Unable to open pipe");

      /* Remember the file descriptors for the pipes.
       */
      plug_in->my_read = my_read[0];
      plug_in->my_write = my_write[1];
      plug_in->his_read = my_write[0];
      plug_in->his_write = my_read[1];

      /* Set the rest of the command line arguments.
       */
      sprintf (plug_in->args[2], "%d", plug_in->his_read);
      sprintf (plug_in->args[3], "%d", plug_in->his_write);

      /* Fork another process. We'll remember the process id
       *  so that we can later use it to kill the filter if
       *  necessary.
       */
      plug_in->pid = fork ();

      if (plug_in->pid == 0)
	{
	  /* Execute the filter. The "_exit" call should never
	   *  be reached, unless some strange error condition
	   *  exists.
	   */
	  execvp (plug_in->args[0], plug_in->args);
	  _exit (1);
	}
      else if (plug_in->pid == -1)
	{
	  warning ("unable to run plug-in: %s\n", plug_in->args[0]);
	  free_plug_in (plug_in);
	  return 0;
	}

      /* Make Xt tell us when something has been written to our
       *  input pipe.
       */
      plug_in->input_id = XtAppAddInput (app_context, plug_in->my_read,
					 (XtPointer) XtInputReadMask,
					 plug_in_recv_message, plug_in);

      plug_in->open = 1;

      active_plug_ins = add_to_list (active_plug_ins, plug_in);

      return 1;
    }

  return 0;
}

void
plug_in_close (plug_in, kill_it)
     PlugInP plug_in;
     int kill_it;
{
  int status;

  if (plug_in && plug_in->open)
    {
      /* If necessary, kill the filter.
       */
      if (kill_it && plug_in->pid)
	status = kill (plug_in->pid, SIGKILL);
      
      /* Wait for the process to exit. This will happen
       *  immediately if it was just killed.
       */
      if (plug_in->pid)
	waitpid (plug_in->pid, &status, 0);

      /* Remove the input handler.
       */
      if (plug_in->input_id)
	XtRemoveInput (plug_in->input_id);

      /* Close the pipes.
       */
      if (plug_in->my_read)
	close (plug_in->my_read);
      if (plug_in->my_write)
	close (plug_in->my_write);
      if (plug_in->his_read)
	close (plug_in->his_read);
      if (plug_in->his_write)
	close (plug_in->his_write);

      /* Free any space that was allocated in order to recieve messages.
       */
      if ((plug_in->recv_msg.info.size) && (plug_in->recv_msg.data))
	xfree (plug_in->recv_msg.data);

      /* Set the fields to null values.
       */
      plug_in->pid = 0;
      plug_in->input_id = 0;
      plug_in->my_read = 0;
      plug_in->my_write = 0;
      plug_in->his_read = 0;
      plug_in->his_write = 0;
      plug_in->send_msg.info.type = 0;
      plug_in->send_msg.info.size = 0;
      plug_in->send_msg.data = 0;
      plug_in->recv_msg.info.type = 0;
      plug_in->recv_msg.info.size = 0;
      plug_in->recv_msg.data = 0;

      {
	link_ptr tmp;

	tmp = plug_in->dialogs;
	while (tmp)
	  {
	    dialog_close (tmp->data);
	    xfree (tmp->data);
	    tmp = next_item (tmp);
	  }

	plug_in->dialogs = free_list (plug_in->dialogs);
      }

      if (kill_it && plug_in->images)
	{
	  link_ptr tmp;

	  tmp = plug_in->images;
	  while (tmp)
	    {
	      gimage_delete (tmp->data);
	      tmp = next_item (tmp);
	    }

	  plug_in->images = free_list (plug_in->images);
	}

      if (plug_in->progress)
	{
	  progress_free (plug_in->progress);
	  plug_in->progress = NULL;
	}

      plug_in->image = NULL;
      plug_in->display = NULL;

      plug_in->open = 0;

      active_plug_ins = remove_from_list (active_plug_ins, plug_in);

      if (plug_in->callback)
	{
	  if (plug_in->messages)
	    {
	      plug_in->destroy = kill_it + 1;
	      XtAppAddWorkProc (app_context, plug_in_workproc, plug_in);
	    }
	  else
	    {
	      (* plug_in->callback) (plug_in->callback_data, (void*) kill_it);
	      plug_in->callback = NULL;
	      plug_in->callback_data = NULL;
	    }
	}
    }
}

void
plug_in_set_callback (plug_in, callback, callback_data)
     PlugInP plug_in;
     PlugInCallback callback;
     void *callback_data;
{
  if (plug_in)
    {
      plug_in->callback = callback;
      plug_in->callback_data = callback_data;
    }
}

void
plug_in_send_message (plug_in, type, size, data)
     PlugInP plug_in;
     long type, size;
     void *data;
{
  int err;

  if (plug_in)
    {
      /* If this is a LOAD message set the "load_image" flag.
       * That way, when the image is created we know to turn off
       *  the "dirty" flag.
       */
      if (type == LOAD)
	plug_in->load_image = 1;

      /* Send a message to the filter. This is done in two
       *  steps. The first is to write out the message header
       *  information. The second is to write out the message
       *  data.
       */
      plug_in->send_msg.info.type = type;
      plug_in->send_msg.info.size = size;
      plug_in->send_msg.data = data;

      err = plug_in_write (plug_in, sizeof (MsgHeader), &(plug_in->send_msg.info));
      err = plug_in_write (plug_in, plug_in->send_msg.info.size, plug_in->send_msg.data);
    }
}

void
plug_in_recv_message (client_data, source, id)
     XtPointer client_data;
     int * source;
     XtInputId *id;
{
  PlugInP plug_in;
  int err, size;

  plug_in = (PlugInP) client_data;

  if (plug_in)
    {
      /* There are two steps to receiving a message.
       * First, the message header is read in. Second,
       *  the message.
       */

      size = sizeof (MsgHeader);
      while (size)
	{
	  err = read (plug_in->my_read, 
		      &(plug_in->recv_msg.info), 
		      size);
	  size -= err;
	}

      if (plug_in->recv_msg.info.size)
	{
	  if (plug_in->recv_msg.data)
	    xfree (plug_in->recv_msg.data);

	  plug_in->recv_msg.data = plug_in_read (plug_in, plug_in->recv_msg.info.size);
	}

      if (plug_in->recv_msg.info.type < n_msg_procs)
	{
/*	  message ("%d", plug_in->recv_msg.info.type); */
	  if (msg_procs && msg_procs[plug_in->recv_msg.info.type])
	    (* (msg_procs[plug_in->recv_msg.info.type])) (plug_in, &plug_in->recv_msg);
	}
    }
}

static void*
plug_in_read (plug_in, size)
     PlugInP plug_in;
     long size;
{
  unsigned char *data;
  unsigned char *tmp;
  int bytes_read;
  
  data = xmalloc (size);
  tmp = data;
  
  while (size)
    {
      bytes_read = read (plug_in->my_read, tmp, size);
      tmp += bytes_read;
      size -= bytes_read;
    }
  
  return data;
}

static int
plug_in_write (plug_in, size, data)
     PlugInP plug_in;
     long size;
     void *data;
{
  return (write (plug_in->my_write, data, size));
}

static void
handle_quit (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  plug_in_close (plug_in, 0);
  free_plug_in (plug_in);
}

static void
handle_image_new (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgImage *image;
  GImage *gimage;
  int type;

  image = msg->data;

  switch (image->type)
    {
    case IMAGE_TYPE_RGB:
      type = RGB_GIMAGE;
      image->channels = 3;
      break;
    case IMAGE_TYPE_GRAY:
      type = GREY_GIMAGE;
      image->channels = 1;
      break;
    case IMAGE_TYPE_INDEXED:
      type = INDEXED_GIMAGE;
      image->channels = 1;
      break;
    case IMAGE_TYPE_ALL:
      warning ("plug-in specified IMAGE_TYPE_ALL for creating an image: %s", plug_in->args[0]);
      free_plug_in (plug_in);
    }

  if (image->width && image->height && image->channels)
    {
      gimage = gimage_new (image->width, image->height, type, image->channels);
      
      if (plug_in->load_image)
	gimage->dirty = 0;
    }
  else
    {
      free_plug_in (plug_in);
      return;
    }

  if (image->data)
    {
      if (gimage->filename)
	xfree (gimage->filename);
      gimage->filename = xstrdup (image->name);
      if (gimage->filename[0] == '/')
	gimage->has_filename = 1;
    }
  else
    {
      strcpy (image->name, gimage->filename);
    }

  switch (gimage->type)
    {
    case RGB_GIMAGE:
    case GREY_GIMAGE:
      image->colors = 0;
      break;
    case INDEXED_GIMAGE:
      image->colors = gimage->num_cols;
      memcpy (image->cmap, gimage->cmap, COLORMAP_SIZE);
      break;
    }  
  
  plug_in->images = add_to_list (plug_in->images, gimage);
  
  image->ID = gimage->ID;
  image->shmid = gimage->shmid;
  
  plug_in_send_message (plug_in, IMAGE_NEW, sizeof (MsgImage), image);
}

static void
handle_image_display (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgImage *image;
  GImage *gimage;
  GDisplay *gdisplay;
  unsigned short scale;

  image = msg->data;
  if (image->ID > 0)
    {
      gimage = gimage_get_ID (image->ID);
      switch (gimage->type)
	{
	case RGB_GIMAGE:
	case GREY_GIMAGE:
	  break;
	case INDEXED_GIMAGE:
	  gimage->num_cols = image->colors;
	  memcpy (gimage->cmap, image->cmap, COLORMAP_SIZE);
	  break;
	}

      scale = 0x0101;   /* 1:1 scale factor */
      if (plug_in->display)
	{
	  gdisplay = plug_in->display;
	  scale = gdisplay->scale;
	}
      
      gdisplay = gdisplay_gimage (gimage, scale);
    }
  
  plug_in_send_message (plug_in, msg->info.type, sizeof (MsgImage), image);
}

static void
handle_image_input_output (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  GImage *gimage;
  GDisplay *gdisplay;
  MsgImage *image;

  image = msg->data;
  if (image->ID)
    gimage = gimage_get_ID (image->ID);
  else if (plug_in->image)
    gimage = plug_in->image;
  else
    {
      free_plug_in (plug_in);
      return;
    }

  if (gimage)
    {
      strcpy (image->name, prune_filename (gimage->filename));
      image->width = gimage->width;
      image->height = gimage->height;
      image->channels = gimage->bpp;

      if (msg->info.type == IMAGE_INPUT)
	{
	  image->ID = gimage->ID;
	  image->shmid = gimage->shmid;
	}
      else if (msg->info.type == IMAGE_OUTPUT)
	{
	  image->ID = -gimage->ID;
	  image->shmid = gimage_get_shadow_ID (gimage);
	}
      else
	{
	  free_plug_in (plug_in);
	  return;
	}

      switch (gimage->type)
	{
	case RGB_GIMAGE:
	  image->type = IMAGE_TYPE_RGB;
	  image->colors = 0;
	  break;
	case GREY_GIMAGE:
	  image->type = IMAGE_TYPE_GRAY;
	  image->colors = 0;
	  break;
	case INDEXED_GIMAGE:
	  image->type = IMAGE_TYPE_INDEXED;
	  image->colors = gimage->num_cols;
	  memcpy (image->cmap, gimage->cmap, COLORMAP_SIZE);
	  break;
	}
    }
  else
    {
      free_plug_in (plug_in);
      return;
    }

  if (plug_in->display)
    {
      gdisplay = plug_in->display;

      if (!gdisplay_find_bounds (gdisplay, &image->x1, &image->y1, &image->x2, &image->y2))
	{
	  image->x1 = 0;
	  image->y1 = 0;
	  image->x2 = gdisplay->gimage->width;
	  image->y2 = gdisplay->gimage->height;
	}

      if ((gdisplay->select->float_buf) && (msg->info.type == IMAGE_INPUT))
	/*  If there is a floating buffer, we need to opaquely copy it to the gimage  */
	selection_stencil (gdisplay->select, gdisplay->gimage, 0, 0,
			   gdisplay->select->float_buf->width, 
			   gdisplay->select->float_buf->height,
			   COPY_STENCIL);
    }
  else
    {
      image->x1 = 0;
      image->y1 = 0;
      image->x2 = gimage->width;
      image->y2 = gimage->height;
    }

  plug_in_send_message (plug_in, msg->info.type, sizeof (MsgImage), image);
}

static void
handle_image_color (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgColor *color;

  color = msg->data;
  switch (color->type)
    {
    case COLOR_FOREGROUND:
      palette_get_foreground (&color->r, &color->g, &color->b);
      break;
    case COLOR_BACKGROUND:
      palette_get_background (&color->r, &color->g, &color->b);
      break;
    }

  plug_in_send_message (plug_in, msg->info.type, sizeof (MsgColor), color);
}

static void
handle_image_update (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgImage *image;
  GImage *gimage;
  short busy;

  image = msg->data;
  if (image->ID > 0)
    {
      gimage = gimage_get_ID (image->ID);
      busy = gimage->busy;
      gimage->busy = False;
      gdisplay_update_full (image->ID, 30);
      gimage->busy = busy;
    }
  else if (image->ID < 0)
    {
      GDisplay *gdisplay;
      Selection *select;

      gimage = gimage_get_ID (-image->ID);
      busy = gimage->busy;
      gimage->busy = False;

      if (gimage->type == INDEXED_GIMAGE)
	{
	  gimage->num_cols = image->colors;
	  memcpy (gimage->cmap, image->cmap, COLORMAP_SIZE);
	}
      
      gdisplay = plug_in->display;
      select = (gdisplay) ? gdisplay->select : NULL;
      gimage_merge_shadow (gimage, select);

      /*  If stingy memory management is enabled, free the shadow buffer  */
      if (app_data.stingy)
	gimage_free_shadow (gimage);

      if (gdisplay)
	{
	  int x1, y1, x2, y2;

	  if (gdisplay_find_bounds (gdisplay, &x1, &y1, &x2, &y2))
	    gdisplay_update (gimage->ID, x1, y1, x2 - x1, y2 - y1, 30);
	  else
	    gdisplay_update_full (gimage->ID, 30);
	}
      else
	gdisplay_update_full (gimage->ID, 30);

      gimage->busy = busy;
    }
  
  plug_in_send_message (plug_in, msg->info.type, sizeof (MsgImage), image);
}

static void
handle_load (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  message ("Gimp received a \"load\" message from a plug-in");
  message ("this shouldn't happen...killing the plug-in");
  free_plug_in (plug_in);
}

static void
handle_save (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  message ("Gimp received a \"save\" message from a plug-in");
  message ("this shouldn't happen...killing the plug-in");
  free_plug_in (plug_in);
}

static void
handle_params (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgParams *params;

  params = msg->data;
  if (params->type < n_param_procs)
    {
      if (param_procs && param_procs[params->type])
	(* (param_procs[params->type])) (plug_in, msg);
    }
  else
    {
      message ("unknown param message received: %d", params->type);
      free_plug_in (plug_in);
    }
}

static void
handle_progress (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgProgress *progress;

  progress = msg->data;

  if (!plug_in->progress)
    {
      if (strlen (progress->label) > 0)
	plug_in->progress = progress_new (prune_filename (plug_in->args[0]), 
					  progress->label,
					  handle_cancel_callback, 
					  plug_in);
      else
	plug_in->progress = progress_new (prune_filename (plug_in->args[0]), 
					  "working:",
					  handle_cancel_callback, 
					  plug_in);
    }

  progress_update (plug_in->progress, (progress->current * 100) / progress->max);
  if ((progress->current == 1) && (progress->max == 1))
    {
      progress_free (plug_in->progress);
      plug_in->progress = NULL;
    }

  plug_in_send_message (plug_in, PROGRESS, 0, 0);
}

static void
handle_message (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgMessage *message;
  MessageDialog dialog;
  Widget cancel;
  Widget help;
  XmString string;

  message = msg->data;
  if (message)
    {
      dialog = xmalloc (sizeof (_MessageDialog));
      dialog->plug_in = plug_in;
      
      dialog->dlg = XmCreateMessageDialog (toplevel, "pluginMessageDialog", NULL, 0);
      string = XmStringCreateLocalized (message->data);
      XtVaSetValues (dialog->dlg,
		     XmNmessageString, string,
		     XmNdefaultPosition, False,
		     XmNdeleteResponse, XmDESTROY,
		     NULL);
      cancel = XmMessageBoxGetChild (dialog->dlg, XmDIALOG_CANCEL_BUTTON);
      help = XmMessageBoxGetChild (dialog->dlg, XmDIALOG_HELP_BUTTON);
      XtUnmanageChild (cancel);
      XtUnmanageChild (help);
      XmStringFree (string);

      dialog->timer = XtAppAddTimeOut (app_context, MESSAGE_DELAY,
				       handle_message_dialog_timeout,
				       (void*) dialog);

      XtAddCallback (dialog->dlg, XmNokCallback, handle_message_dialog_callback, dialog);
      XtAddCallback (dialog->dlg, XmNdestroyCallback, handle_message_dialog_callback, dialog);
      XtAddCallback (dialog->dlg, XmNmapCallback, map_dialog, NULL);
      XtManageChild (dialog->dlg);

      plug_in->messages = add_to_list (plug_in->messages, dialog);
    }

  plug_in_send_message (plug_in, MESSAGE, 0, 0);
}

static void
handle_get_params (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgParams *params;
  PlugInParams the_params;

  params = msg->data;

  the_params = plug_in_find_params (plug_in->args[0]);
  if (the_params)
    {
      params->size = the_params->size;
      params->data = the_params->data;
    }

  plug_in_send_message (plug_in, PARAMS, sizeof (MsgParams), params);
  plug_in_write (plug_in, params->size, params->data);
}

static void
handle_set_params (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgParams *params;

  params = msg->data;
  if (params->size)
    {
      params->data = plug_in_read (plug_in, params->size);
      plug_in_set_params (plug_in->args[0], params->size, params->data);
    }

  plug_in_send_message (plug_in, PARAMS, sizeof (MsgParams), params);
}

static void
handle_dialog (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;

  dlg = msg->data;
  if (dlg->type < n_dialog_procs)
    {
      if (dialog_procs && dialog_procs[dlg->type])
	(* (dialog_procs[dlg->type])) (plug_in, msg);
    }
  else
    {
      message ("unknown dialog message received");
      free_plug_in (plug_in);
    }
}

static void
handle_dialog_new (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dlg->dialog.dialog_ID = -1;

  dialog = dialog_new (dlg->dialog.title, handle_dialog_callback, plug_in);
  plug_in->dialogs = add_to_list (plug_in->dialogs, dialog);
  dlg->dialog.dialog_ID = dialog->dialog_ID;
  
  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_show (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->dialog.dialog_ID);
  if (dialog)
    dialog_show (dialog);
  
  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_update (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->dialog.dialog_ID);
  if (dialog)
    {
      dialog_hide (dialog);
      dialog_show (dialog);
    }

  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_close (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->dialog.dialog_ID);
  if (dialog)
    {
      plug_in->dialogs = remove_from_list (plug_in->dialogs, dialog);
      dialog_close (dialog);
      xfree (dialog);
    }
    
  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_new_item (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;
  
  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->item.dialog_ID);
  if (dialog)
    dlg->item.item_ID = dialog_new_item (dialog, 
					 dlg->item.parent_ID, 
					 dlg->item.item_type,
					 dlg->item.data,
					 plug_in->image);
  
  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_show_item (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->item.dialog_ID);
  if (dialog)
    dialog_show_item (dialog, dlg->item.item_ID);

  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_hide_item (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->item.dialog_ID);
  if (dialog)
    dialog_hide_item (dialog, dlg->item.item_ID);

  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_change_item (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->item.dialog_ID);
  if (dialog)
    dialog_change_item (dialog, dlg->item.item_ID, dlg->item.data);

  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_dialog_delete_item (plug_in, msg)
     PlugInP plug_in;
     MsgP msg;
{
  MsgDialog *dlg;
  AutoDialog dialog;

  dlg = msg->data;
  dialog = plug_in_find_dialog (plug_in, dlg->item.dialog_ID);
  if (dialog)
    dialog_delete_item (dialog, dlg->item.item_ID);

  plug_in_send_message (plug_in, DIALOG, sizeof (MsgDialog), dlg);
}

static void
handle_cancel_callback (client_data, call_data)
     void *client_data;
     void *call_data;
{
  PlugInP plug_in;

  plug_in = (PlugInP) client_data;
  if (plug_in)
    {
      plug_in_close (plug_in, (int) call_data);
      free_plug_in (plug_in);
    }
}

static void
handle_dialog_callback (dialog_ID, item_ID, client_data, call_data)
     int dialog_ID, item_ID;
     void *client_data, *call_data;
{
  MsgDialog dlg;

  dlg.type = DIALOG_CALLBACK;
  dlg.callback.dialog_ID = dialog_ID;
  dlg.callback.item_ID = item_ID;
  memcpy (dlg.callback.data, call_data, 32);

  plug_in_send_message (client_data, DIALOG, sizeof (MsgDialog), &dlg);
}

static void 
handle_message_dialog_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  MessageDialog dialog;

  dialog = client_data;
  if (dialog)
    {
      if (dialog->timer)
	XtRemoveTimeOut (dialog->timer);
      dialog->plug_in->messages = remove_from_list (dialog->plug_in->messages, dialog);
      xfree (dialog);
    }
}

static void 
handle_message_dialog_timeout (client_data, call_data)
     XtPointer client_data;
     XtIntervalId * call_data;
{
  MessageDialog dialog;
  Widget temp;

  dialog = client_data;
  if (dialog && (dialog->timer == *call_data))
    {
      temp = dialog->dlg;
      dialog->dlg = 0;
      dialog->timer = 0;

      XtDestroyWidget (temp);
    }
}

