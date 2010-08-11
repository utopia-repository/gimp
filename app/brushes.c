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
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "appenv.h"
#include "brushes.h"
#include "brush_header.h"
#include "brush_select.h"
#include "buildmenu.h"
#include "errors.h"
#include "general.h"
#include "gimprc.h"
#include "linked.h"
#include "menus.h"


/*  global variables  */
GBrushP             active_brush = NULL;
link_ptr            brush_list = NULL;
int                 num_brushes = 0;

float               opacity = 1.0;
int                 interpolate = 1;
int                 paint_mode = 0;


BrushSelectP        brush_select_dialog = NULL;

/*  static variables  */
static Widget *     brush_widgets = NULL;
static int          have_default_brush = 0;

/*  static function prototypes  */
static link_ptr     insert_brush_in_list   (link_ptr, GBrushP);
static GBrushP      load_brush             (char *);
static void         free_brush             (GBrushP);
static void         create_menu_item       (GBrushP);
static Widget       get_brush_menu_widget  (int);
static void         brush_select_callback  (Widget, XtPointer, XtPointer);
static void         destroy_dialog         (Widget, XtPointer, XtPointer);

/*  function declarations  */
void
brushes_init ()
{
  DIR *dir;
  GBrushP brush;
  char path[256];
  char filename[256];
  char *home = NULL;
  char *local_path, *token;
  struct stat buf;
  int err, i;
  struct dirent * dir_ent;

  if (brush_list)
    brushes_free ();

  brush_list = NULL;
  num_brushes = 0;

  if (!home)
    home = getenv ("HOME");

  local_path = xstrdup (brush_path);
  token = strtok (local_path, ":");

  while (token)
    {
      if (*token == '~')
	sprintf (path, "%s%s", home, token + 1);
      else
	sprintf (path, "%s", token);

      /*  see if the directory exists and if it has any items in it  */
      err = stat (path, &buf);
      if (!err && S_ISDIR (buf.st_mode))
	{
	  if (token[strlen (token) - 1] != '/')
	    strcat (path, "/");

	  /*  open the brush directory  */
	  if (! (dir= opendir (path)))
	    warning ("error reading brushes from directory \"%s\"", path);
	  else
	    {
	      while ( (dir_ent = readdir (dir)) )
		{
		  sprintf (filename, "%s%s", path, dir_ent->d_name);
		  /*  double check the filename--especially that it is not a sub-dir  */
		  err = stat (filename, &buf);
		  if (!err && S_ISREG (buf.st_mode))
		    {
		      brush = load_brush (filename);
		      if (brush)
			/*  insert brush alphabetically  */
			brush_list = insert_brush_in_list (brush_list, brush);
		      /*  Check if the current brush is the default one  */
		      if (default_brush && dir_ent->d_name)
			if (strcmp (default_brush, dir_ent->d_name) == 0)
			  {
			    active_brush = brush;
			    have_default_brush = 1;
			  }
		    }
		}
	      closedir (dir);
	    }
	}

      token = strtok (NULL, ":");
    }

  /*  assign indexes to the loaded brushes  */
  {
    link_ptr list = brush_list;

    while (list)
      {
	/*  Set the brush index  */
	((GBrush *) list->data)->index = num_brushes++;
	list = next_item (list);
      }
  }

  /*  If there are any brushes, create a menu for them  */
  if (brush_list && !brush_widgets)
    {
      brush_widgets = (Widget *) xmalloc (sizeof (Widget) * num_brushes);
      for (i = 0; i < num_brushes; i++)
	brush_widgets [i] = NULL;
    }

  xfree (local_path);
}


void
brushes_free ()
{
  link_ptr list;
  GBrushP brush;
  int i;

  list = brush_list;

  while (list)
    {
      brush = (GBrushP) list->data;
      free_brush (brush);
      list = next_item (list);
    }

  if (brush_widgets)
    {
      for (i = 0; i < num_brushes; i++)
	if (brush_widgets [i])
	  XtDestroyWidget (brush_widgets [i]);

      xfree (brush_widgets);
      brush_widgets = NULL;
    }

  free_list (list);

  have_default_brush = 0;
  active_brush = NULL;
  num_brushes = 0;
  brush_list = NULL;
}


void
brush_select_dialog_free ()
{
  if (brush_select_dialog)
    {
      brush_select_free (brush_select_dialog);
      brush_select_dialog = NULL;
    }
}


GBrushP
get_active_brush ()
{
  if (have_default_brush)
    {
      have_default_brush = 0;
      if (!active_brush)
	fatal_error ("Specified default brush not found!");

      create_menu_item (active_brush);
      XtVaSetValues (get_brush_menu_widget (active_brush->index),
		     XmNset, True,
		     NULL);
    }
  else if (! active_brush && brush_list)
    {
      active_brush = (GBrushP) brush_list->data;
      create_menu_item (active_brush);
      XtVaSetValues (get_brush_menu_widget (active_brush->index),
		     XmNset, True,
		     NULL);
    }

  return active_brush;
}


static link_ptr
insert_brush_in_list (list, brush)
     link_ptr list;
     GBrushP brush;
{
  link_ptr tmp;
  link_ptr prev;
  link_ptr new_link;
  GBrushP b;
  int val;

  /* Insert the item in the list */
  if (list)
    {
      prev = NULL;
      tmp = list;
      do {
	  if (tmp)
	    {
	      b = (GBrushP) tmp->data;
	      
	      /* do the comparison needed for the insertion sort */
	      val = strcmp (brush->name, b->name);
	    }
	  else
	    val = -1;

          if (val <= 0)
            {
	      /* this is the place the item goes */
	      /* Insert the item into the list. We'll have to create
	       *  a new link and then do a little insertion. 
	       */
              new_link = alloc_list ();
	      if (!new_link)
		fatal_error ("Unable to allocate memory");

              new_link->data = brush;
              new_link->next = tmp;

              if (prev)
                prev->next = new_link;
              if (tmp == list)
                list = new_link;

	      return list;
            }
	  
	  /* Advance to the next item in the list.
	   */
          prev = tmp;
          tmp = next_item (tmp);
        } while (prev);
    }
  else
    /* There are no items in the brush list, so we'll just start
     *  one right now.
     */
    list = add_to_list (list, brush);

  return list;
}


static GBrushP
load_brush (filename)
     char * filename;
{
  GBrushP brush;
  FILE * fp;
  char * brush_name;
  int bn_size;
  unsigned char buf [sz_BrushHeader];
  BrushHeader header;
  unsigned long * hp;
  int i;

  brush = (GBrushP) xmalloc (sizeof (GBrush));

  brush->filename = xstrdup (filename);

  /*  Open the requested file  */
  if (! (fp = fopen (filename, "r")))
    {
      warning ("can't load brush \"%s\"\n", filename);
      return NULL;
    }

  /*  Read in the header size  */
  fread (buf, sz_BrushHeader, 1, fp);
  
  /*  rearrange the bytes in each unsigned long  */
  hp = (unsigned long *) &header;
  for (i = 0; i < 5; i++)
    hp [i] = (buf [i * 4] << 24) + (buf [i * 4 + 1] << 16) +
             (buf [i * 4 + 2] << 8) + (buf [i * 4 + 3]);

  /*  Get a new brush mask  */
  brush->mask = temp_buf_new (header.width, header.height, header.bytes, 0, 0, NULL);
  
  /*  Read in the brush name  */
  if ((bn_size = (header.header_size - sz_BrushHeader)))
    {
      brush_name = (char *) xmalloc (sizeof (char) * bn_size);
      fread (brush_name, bn_size, 1, fp);
      brush->name = brush_name;
    }
  else
    brush->name = xstrdup ("Generic");

  /*  Read the brush mask data  */
  fread (temp_buf_data (brush->mask), header.width * header.height * header.bytes, 1, fp);

  /*  Clean up  */
  fclose (fp);

  /*  Swap the brush to disk (if we're being stingy with memory) */
  if (app_data.stingy)
    temp_buf_swap (brush->mask);

  return brush;
}


GBrushP
get_brush_by_index (index)
     int index;
{
  link_ptr list;
  GBrushP brush;

  list = brush_list;

  while (list)
    {
      brush = (GBrushP) list->data;
      if (brush->index == index)
	return brush;
      list = next_item (list);
    }

  return NULL;
}


XmStringTable
get_brush_names ()
{
  XmStringTable strings;
  GBrushP brush;
  link_ptr list;
  int i;

  strings = (XmStringTable) xmalloc (sizeof (XmString *) * num_brushes);

  list = brush_list;
  i = 0;
  while (list)
    {
      brush = (GBrushP) list->data;
      strings [i++] = XmStringCreateLocalized (brush->name);
      list = next_item (list);
    }

  return strings;
}


void
select_brush (brush)
     GBrushP brush;
{
  /*  Keep up appearances in the brush menu  */
  XtVaSetValues (get_brush_menu_widget (active_brush->index),
		 XmNset, False,
		 NULL);

  /*  If a menu item doesn't exist for this brush, create one  */
  if (!brush_widgets [brush->index])
    create_menu_item (brush);

  XtVaSetValues (get_brush_menu_widget (brush->index),
		 XmNset, True,
		 NULL);

  /*  Make sure the active brush is swapped before we get a new one... */
  if (app_data.stingy)
    temp_buf_swap (active_brush->mask);

  /*  Set the active brush  */
  active_brush = brush;

  /*  Make sure the active brush is unswapped... */
  if (app_data.stingy)
    temp_buf_unswap (brush->mask);

  /*  Keep up appearances in the brush dialog  */
  if (brush_select_dialog)
    brush_select_select (brush_select_dialog, brush->index);

}


void
create_brush_dialog ()
{
  if (!brush_select_dialog)
    {
      /*  Create the dialog...  */
      brush_select_dialog = brush_select_new ();
      XtAddCallback (brush_select_dialog->shell, XtNdestroyCallback, destroy_dialog, NULL);
    }
  else
    {
      /*  Popup the dialog  */
      XtPopup (brush_select_dialog->shell, XtGrabNone);
    }
}


static void
free_brush (brush)
     GBrushP brush;
{
  if (brush->mask)
    temp_buf_free (brush->mask);
  if (brush->filename)
    xfree (brush->filename);
  if (brush->name)
    xfree (brush->name);

  xfree (brush);
}


static Widget
get_brush_menu_widget (brush_num)
     int brush_num;
{
  return brush_widgets [brush_num];
}


static void
create_menu_item (brush)
     GBrushP brush;
{
  Widget menu;

  XtVaGetValues (mainmenu_widgets [BRUSHMENU], XmNsubMenuId, &menu, NULL);

  brush_widgets [brush->index] = 
    XtCreateManagedWidget (brush->name, xmToggleButtonWidgetClass, menu, NULL, 0);
  
  XtAddCallback (brush_widgets [brush->index], XmNvalueChangedCallback,
		 brush_select_callback, brush);
}


static void
brush_select_callback (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  GBrushP brush;

  brush = (GBrushP) client_data;

  select_brush (brush);
}


static void
destroy_dialog (w, client_data, call_data)
     Widget w;
     XtPointer client_data;
     XtPointer call_data;
{
  brush_select_dialog_free ();
}


int
get_brush_interpolate ()
{
  return interpolate;
}

float
get_brush_opacity ()
{
  return opacity;
}


int
get_brush_paint_mode ()
{
  return paint_mode;
}

void
set_brush_interpolate (interp)
     int interp;
{
  interpolate = interp;
}

void
set_brush_opacity (opac)
     float opac;
{
  opacity = opac;
}


void
set_brush_paint_mode (pm)
     int pm;
{
  paint_mode = pm;
}
