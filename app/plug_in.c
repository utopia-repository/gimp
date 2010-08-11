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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_IPC_H
#include <sys/ipc.h>
#endif

#ifdef HAVE_SHM_H
#include <sys/shm.h>
#endif

#include "libgimp/gimpprotocol.h"
#include "libgimp/gimpwire.h"

#include "app_procs.h"
#include "appenv.h"
#include "drawable.h"
#include "datafiles.h"
#include "errors.h"
#include "gdisplay.h"
#include "general.h"
#include "gimage.h"
#include "gimprc.h"
#include "interface.h"
#include "menus.h"
#include "plug_in.h"


#define SEPARATE_PROGRESS_BAR


typedef struct _PlugInBlocked  PlugInBlocked;

typedef enum
{
  RUN_INTERACTIVE    = 0x0,
  RUN_NONINTERACTIVE = 0x1,
  RUN_WITH_LAST_VALS = 0x2
} RunModeType;

struct _PlugInBlocked
{
  PlugIn *plug_in;
  char *proc_name;
};


static int  plug_in_write                 (int                fd,
					   guint8            *buf,
				           gulong             count);
static int  plug_in_flush                 (int                fd);
static void plug_in_push                  (PlugIn            *plug_in);
static void plug_in_pop                   (void);
static void plug_in_recv_message          (gpointer           data,
				           gint               id,
				           GdkInputCondition  cond);
static void plug_in_handle_message        (WireMessage       *msg);
static void plug_in_handle_quit           (void);
static void plug_in_handle_tile_req       (GPTileReq         *tile_req);
static void plug_in_handle_proc_run       (GPProcRun         *proc_run);
static void plug_in_handle_proc_return    (GPProcReturn      *proc_return);
static void plug_in_handle_proc_install   (GPProcInstall     *proc_install);
static void plug_in_handle_proc_uninstall (GPProcUninstall   *proc_uninstall);
static void plug_in_write_rc              (char              *filename);
static void plug_in_init_file             (char              *filename);
static void plug_in_query                 (char              *filename,
				           PlugInDef         *plug_in_def);
static void plug_in_add_to_db             (void);
static void plug_in_make_menu             (void);
static void plug_in_callback              (GtkWidget         *widget,
					   gpointer           client_data);
static void plug_in_proc_def_insert       (PlugInProcDef     *proc_def);
static void plug_in_proc_def_remove       (PlugInProcDef     *proc_def);
static void plug_in_proc_def_destroy      (PlugInProcDef     *proc_def,
					   int                data_only);

static Argument* plug_in_temp_run       (ProcRecord *proc_rec,
  					 Argument   *args);
static Argument* plug_in_params_to_args (GPParam   *params,
  					 int        nparams,
  					 int        full_copy);
static GPParam*  plug_in_args_to_params (Argument  *args,
  					 int        nargs,
  					 int        full_copy);
static void      plug_in_params_destroy (GPParam   *params,
  					 int        nparams,
  					 int        full_destroy);
static void      plug_in_args_destroy   (Argument  *args,
  					 int        nargs,
  					 int        full_destroy);

static Argument* progress_init_invoker   (Argument *args);
static Argument* progress_update_invoker (Argument *args);

static Argument* message_invoker         (Argument *args);

static Argument* message_handler_get_invoker (Argument *args);
static Argument* message_handler_set_invoker (Argument *args);


static GSList *plug_in_defs = NULL;
static GSList *gimprc_proc_defs = NULL;
static GSList *proc_defs = NULL;
static GSList *open_plug_ins = NULL;
static GSList *blocked_plug_ins = NULL;

static GSList *plug_in_stack = NULL;
static PlugIn *current_plug_in = NULL;
static int current_readfd = 0;
static int current_writefd = 0;
static int current_write_buffer_index = 0;
static char *current_write_buffer = NULL;
static Argument *current_return_vals = NULL;
static int current_return_nvals = 0;

static ProcRecord *last_plug_in = NULL;

static int shm_ID = -1;
static guchar *shm_addr = NULL;

static int write_pluginrc = FALSE;

static ProcArg progress_init_args[] =
{
  { PDB_STRING,
    "message",
    "Message to use in the progress dialog." }
};

static ProcRecord progress_init_proc =
{
  "gimp_progress_init",
  "Initializes the progress bar for the current plug-in",
  "Initializes the progress bar for the current plug-in. It is only valid to call this procedure from a plug-in.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  progress_init_args,
  0,
  NULL,
  { { progress_init_invoker } },
};

static ProcArg progress_update_args[] =
{
  { PDB_FLOAT,
    "percentage",
    "Percentage of progress completed" }
};

static ProcRecord progress_update_proc =
{
  "gimp_progress_update",
  "Updates the progress bar for the current plug-in",
  "Updates the progress bar for the current plug-in. It is only valid to call this procedure from a plug-in.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  progress_update_args,
  0,
  NULL,
  { { progress_update_invoker } },
};


static ProcArg message_args[] =
{
  { PDB_STRING,
    "message",
    "Message to display in the dialog." }
};

static ProcRecord message_proc =
{
  "gimp_message",
  "Displays a dialog box with a message",
  "Displays a dialog box with a message. Useful for status or error reporting.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  message_args,
  0,
  NULL,
  { { message_invoker } },
};


static ProcArg message_handler_get_out_args[] =
{
  { PDB_INT32,
    "handler",
    "the current handler type: { MESSAGE_BOX (0), CONSOLE (1) }" }
};

static ProcRecord message_handler_get_proc =
{
  "gimp_message_handler_get",
  "Returns the current state of where warning messages are displayed.",
  "This procedure returns the way g_message warnings are displayed. They can be shown in a dialog box or printed on the console where gimp was started.",
  "Manish Singh",
  "Manish Singh",
  "1998",
  PDB_INTERNAL,
  0,
  NULL,
  1,
  message_handler_get_out_args,
  { { message_handler_get_invoker } },
};

static ProcArg message_handler_set_args[] =
{
  { PDB_INT32,
    "handler",
    "the new handler type: { MESSAGE_BOX (0), CONSOLE (1) }" }
};

static ProcRecord message_handler_set_proc =
{
  "gimp_message_handler_set",
  "Controls where warning messages are displayed.",
  "This procedure controls how g_message warnings are displayed. They can be shown in a dialog box or printed on the console where gimp was started.",
  "Manish Singh",
  "Manish Singh",
  "1998",
  PDB_INTERNAL,
  1,
  message_handler_set_args,
  0,
  NULL,
  { { message_handler_set_invoker } },
};


void
plug_in_init ()
{
  extern int use_shm;
  char filename[MAXPATHLEN];
  GSList *tmp, *tmp2;
  PlugInDef *plug_in_def;
  PlugInProcDef *proc_def;
  gfloat nplugins, nth;

  /* initialize the progress init and update procedure db calls. */
  procedural_db_register (&progress_init_proc);
  procedural_db_register (&progress_update_proc);

  /* initialize the message box procedural db calls */
  procedural_db_register (&message_proc);
  procedural_db_register (&message_handler_get_proc);
  procedural_db_register (&message_handler_set_proc);

  /* initialize the gimp protocol library and set the read and
   *  write handlers.
   */
  gp_init ();
  wire_set_writer (plug_in_write);
  wire_set_flusher (plug_in_flush);

#ifdef HAVE_SHM_H
  /* allocate a piece of shared memory for use in transporting tiles
   *  to plug-ins. if we can't allocate a piece of shared memory then
   *  we'll fall back on sending the data over the pipe.
   */
  if (use_shm)
    {
      shm_ID = shmget (IPC_PRIVATE, TILE_WIDTH * TILE_HEIGHT * 4, IPC_CREAT | 0777);
      if (shm_ID == -1)
	g_message ("shmget failed...disabling shared memory tile transport\n");
      else
	{
	  shm_addr = (guchar*) shmat (shm_ID, 0, 0);
	  if (shm_addr == (guchar*) -1)
	    {
	      g_message ("shmat failed...disabling shared memory tile transport\n");
	      shm_ID = -1;
	    }

#ifdef	IPC_RMID_DEFERRED_RELEASE
	  if (shm_addr != (guchar*) -1)
	    shmctl (shm_ID, IPC_RMID, 0);
#endif
	}
    }
#endif

  /* search for binaries in the plug-in directory path */
  datafiles_read_directories (plug_in_path, plug_in_init_file, MODE_EXECUTABLE);

  /* read the pluginrc file for cached data */
  if (pluginrc_path)
    {
      if (*pluginrc_path == '/')
        strcpy(filename, pluginrc_path);
      else
        sprintf(filename, "%s/%s", gimp_directory(), pluginrc_path);
    }
  else
    sprintf (filename, "%s/pluginrc", gimp_directory ());

  app_init_update_status("Resource configuration", filename, -1);
  parse_gimprc_file (filename);

  /* query any plug-ins that have changed since we last wrote out
   *  the pluginrc file.
   */
  tmp = plug_in_defs;
  app_init_update_status("Plug-ins", "", 0);
  nplugins = g_slist_length(tmp);
  nth = 0;
  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      if (plug_in_def->query)
	{
	  write_pluginrc = TRUE;
	  if ((be_verbose == TRUE) || (no_splash == TRUE))
	    g_print ("query plug-in: \"%s\"\n", plug_in_def->prog);
	  plug_in_query (plug_in_def->prog, plug_in_def);
	}
      app_init_update_status(NULL, plug_in_def->prog, nth/nplugins);
      nth++;
    }

  /* insert the proc defs */
  tmp = gimprc_proc_defs;
  while (tmp)
    {
      proc_def = g_new (PlugInProcDef, 1);
      *proc_def = *((PlugInProcDef*) tmp->data);
      plug_in_proc_def_insert (proc_def);
      tmp = tmp->next;
    }

  tmp = plug_in_defs;
  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      tmp2 = plug_in_def->proc_defs;
      while (tmp2)
	{
	  proc_def = tmp2->data;
	  tmp2 = tmp2->next;

	  plug_in_proc_def_insert (proc_def);
	}
    }

  /* write the pluginrc file if necessary */
  if (write_pluginrc)
    {
      if ((be_verbose == TRUE) || (no_splash == TRUE))
	g_print ("writing \"%s\"\n", filename);
      plug_in_write_rc (filename);
    }

  /* add the plug-in procs to the procedure database */
  plug_in_add_to_db ();

  /* make the menu */
  plug_in_make_menu ();

  /* run the available extensions */
  tmp = proc_defs;
  if ((be_verbose == TRUE) || (no_splash == TRUE))
    g_print ("Starting extensions: ");
  app_init_update_status("Extensions", "", 0);
  nplugins = g_slist_length(tmp); nth = 0;

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (proc_def->prog &&
	  (proc_def->db_info.num_args == 0) &&
	  (proc_def->db_info.proc_type == PDB_EXTENSION))
	{
	  if ((be_verbose == TRUE) || (no_splash == TRUE))
	    g_print ("%s ", proc_def->db_info.name);
	  app_init_update_status(NULL, proc_def->db_info.name,
				 nth/nplugins);

	  plug_in_run (&proc_def->db_info, NULL, FALSE, TRUE);
	}
    }
  if ((be_verbose == TRUE) || (no_splash == TRUE))
    g_print ("\n");

  /* free up stuff */
  tmp = plug_in_defs;
  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      g_slist_free (plug_in_def->proc_defs);
      g_free (plug_in_def->prog);
      g_free (plug_in_def);
    }
  g_slist_free (plug_in_defs);
}

void
plug_in_kill ()
{
  GSList *tmp;
  PlugIn *plug_in;

#ifdef HAVE_SHM_H
#ifndef	IPC_RMID_DEFERRED_RELEASE
  if (shm_ID != -1)
    {
      shmdt ((char*) shm_addr);
      shmctl (shm_ID, IPC_RMID, 0);
    }
#else	/* IPC_RMID_DEFERRED_RELEASE */
  if (shm_ID != -1)
    shmdt ((char*) shm_addr);
#endif
#endif

  tmp = open_plug_ins;
  while (tmp)
    {
      plug_in = tmp->data;
      tmp = tmp->next;

      plug_in_destroy (plug_in);
    }
}

void
plug_in_add (char *prog,
	     char *menu_path,
	     char *accelerator)
{
  PlugInProcDef *proc_def;
  GSList *tmp;

  if (strncmp ("plug_in_", prog, 8) != 0)
    {
      char *t = g_new (char, strlen (prog) + 9);
      sprintf (t, "plug_in_%s", prog);
      g_free (prog);
      prog = t;
    }

  tmp = gimprc_proc_defs;
  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, prog) == 0)
	{
	  if (proc_def->db_info.name)
	    g_free (proc_def->db_info.name);
	  if (proc_def->menu_path)
	    g_free (proc_def->menu_path);
	  if (proc_def->accelerator)
	    g_free (proc_def->accelerator);
	  if (proc_def->extensions)
	    g_free (proc_def->extensions);
	  if (proc_def->prefixes)
	    g_free (proc_def->prefixes);
	  if (proc_def->magics)
	    g_free (proc_def->magics);
	  if (proc_def->image_types)
	    g_free (proc_def->image_types);

	  proc_def->db_info.name = prog;
	  proc_def->menu_path = menu_path;
	  proc_def->accelerator = accelerator;
	  proc_def->prefixes = NULL;
	  proc_def->extensions = NULL;
	  proc_def->magics = NULL;
	  proc_def->image_types = NULL;
	  return;
	}
    }

  proc_def = g_new0 (PlugInProcDef, 1);
  proc_def->db_info.name = prog;
  proc_def->menu_path = menu_path;
  proc_def->accelerator = accelerator;

  gimprc_proc_defs = g_slist_prepend (gimprc_proc_defs, proc_def);
}

char*
plug_in_image_types (char *name)
{
  PlugInDef *plug_in_def;
  PlugInProcDef *proc_def;
  GSList *tmp;

  if (current_plug_in)
    {
      plug_in_def = current_plug_in->user_data;
      tmp = plug_in_def->proc_defs;
    }
  else
    {
      tmp = proc_defs;
    }

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, name) == 0)
	return proc_def->image_types;
    }

  return NULL;
}

GSList*
plug_in_extensions_parse (char *extensions)
{
  GSList *list;
  char *extension;
  list = NULL;
  /* EXTENSIONS can be NULL.  Avoid calling strtok if it is.  */
  if (extensions)
    {
      extensions = g_strdup (extensions);
      extension = strtok (extensions, " \t,");
      while (extension)
	{
	  list = g_slist_prepend (list, g_strdup (extension));
	  extension = strtok (NULL, " \t,");
	}
      g_free (extensions);
    }

  return g_slist_reverse (list);
}

void
plug_in_add_internal (PlugInProcDef *proc_def)
{
  proc_defs = g_slist_prepend (proc_defs, proc_def);
}

PlugInProcDef*
plug_in_file_handler (char *name,
		      char *extensions,
		      char *prefixes,
		      char *magics)
{
  PlugInDef *plug_in_def;
  PlugInProcDef *proc_def;
  GSList *tmp;

  if (current_plug_in)
    {
      plug_in_def = current_plug_in->user_data;
      tmp = plug_in_def->proc_defs;
    }
  else
    {
      tmp = proc_defs;
    }

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, name) == 0)
	{
	  /* EXTENSIONS can be proc_def->extensions  */
	  if (proc_def->extensions != extensions)
	    {
	      if (proc_def->extensions)
		g_free (proc_def->extensions);
	      proc_def->extensions = g_strdup (extensions);
	    }
	  proc_def->extensions_list = plug_in_extensions_parse (proc_def->extensions);

	  /* PREFIXES can be proc_def->prefixes  */
	  if (proc_def->prefixes != prefixes)
	    {
	      if (proc_def->prefixes)
		g_free (proc_def->prefixes);
	      proc_def->prefixes = g_strdup (prefixes);
	    }
	  proc_def->prefixes_list = plug_in_extensions_parse (proc_def->prefixes);

	  /* MAGICS can be proc_def->magics  */
	  if (proc_def->magics != magics)
	    {
	      if (proc_def->magics)
		g_free (proc_def->magics);
	      proc_def->magics = g_strdup (magics);
	    }
	  proc_def->magics_list = plug_in_extensions_parse (proc_def->magics);
	  return proc_def;
	}
    }

  return NULL;
}

void
plug_in_def_add (PlugInDef *plug_in_def)
{
  GSList *tmp;
  PlugInDef *tplug_in_def;
  char *t1, *t2;

  t1 = strrchr (plug_in_def->prog, '/');
  if (t1)
    t1 = t1 + 1;
  else
    t1 = plug_in_def->prog;

  tmp = plug_in_defs;
  while (tmp)
    {
      tplug_in_def = tmp->data;

      t2 = strrchr (tplug_in_def->prog, '/');
      if (t2)
	t2 = t2 + 1;
      else
	t2 = tplug_in_def->prog;

      if (strcmp (t1, t2) == 0)
	{
	  if ((strcmp (plug_in_def->prog, tplug_in_def->prog) == 0) &&
	      (plug_in_def->mtime == tplug_in_def->mtime))
	    {
	      /* Use cached plug-in entry */
	      tmp->data = plug_in_def;
	      g_free (tplug_in_def->prog);
	      g_free (tplug_in_def);
	    }
	  else
	    {
	      g_free (plug_in_def->prog);
	      g_free (plug_in_def);
	    }
	  return;
	}

      tmp = tmp->next;
    }

  write_pluginrc = TRUE;
  g_print ("\"%s\" executable not found\n", plug_in_def->prog);
  g_free (plug_in_def->prog);
  g_free (plug_in_def);
}

char*
plug_in_menu_path (char *name)
{
  PlugInDef *plug_in_def;
  PlugInProcDef *proc_def;
  GSList *tmp, *tmp2;

  tmp = plug_in_defs;
  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      tmp2 = plug_in_def->proc_defs;
      while (tmp2)
	{
	  proc_def = tmp2->data;
	  tmp2 = tmp2->next;

	  if (strcmp (proc_def->db_info.name, name) == 0)
	    return proc_def->menu_path;
	}
    }

  tmp = proc_defs;
  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, name) == 0)
	return proc_def->menu_path;
    }

  return NULL;
}

PlugIn*
plug_in_new (char *name)
{
  PlugIn *plug_in;
  char *path;

  if (name[0] != '/')
    {
      path = search_in_path (plug_in_path, name);
      if (!path)
	{
	  g_message ("unable to locate plug-in: \"%s\"", name);
	  return NULL;
	}
    }
  else
    {
      path = name;
    }

  plug_in = g_new (PlugIn, 1);

  plug_in->open = FALSE;
  plug_in->destroy = FALSE;
  plug_in->query = FALSE;
  plug_in->synchronous = FALSE;
  plug_in->recurse = FALSE;
  plug_in->busy = FALSE;
  plug_in->pid = 0;
  plug_in->args[0] = g_strdup (path);
  plug_in->args[1] = g_strdup ("-gimp");
  plug_in->args[2] = g_new (char, 16);
  plug_in->args[3] = g_new (char, 16);
  plug_in->args[4] = NULL;
  plug_in->args[5] = NULL;
  plug_in->args[6] = NULL;
  plug_in->my_read = 0;
  plug_in->my_write = 0;
  plug_in->his_read = 0;
  plug_in->his_write = 0;
  plug_in->input_id = 0;
  plug_in->write_buffer_index = 0;
  plug_in->temp_proc_defs = NULL;
  plug_in->progress = NULL;
  plug_in->progress_label = NULL;
  plug_in->progress_bar = NULL;
  plug_in->user_data = NULL;

  return plug_in;
}

void
plug_in_destroy (PlugIn *plug_in)
{
  if (plug_in)
    {
      plug_in_close (plug_in, TRUE);

      if (plug_in->args[0])
	g_free (plug_in->args[0]);
      if (plug_in->args[1])
	g_free (plug_in->args[1]);
      if (plug_in->args[2])
	g_free (plug_in->args[2]);
      if (plug_in->args[3])
	g_free (plug_in->args[3]);
      if (plug_in->args[4])
	g_free (plug_in->args[4]);
      if (plug_in->args[5])
	g_free (plug_in->args[5]);

      if (plug_in == current_plug_in)
	plug_in_pop ();

      if (!plug_in->destroy)
	g_free (plug_in);
    }
}

int
plug_in_open (PlugIn *plug_in)
{
  int my_read[2];
  int my_write[2];

  if (plug_in)
    {
      /* Open two pipes. (Bidirectional communication).
       */
      if ((pipe (my_read) == -1) || (pipe (my_write) == -1))
	{
	  g_message ("unable to open pipe");
	  return 0;
	}

      plug_in->my_read = my_read[0];
      plug_in->my_write = my_write[1];
      plug_in->his_read = my_write[0];
      plug_in->his_write = my_read[1];

      /* Remember the file descriptors for the pipes.
       */
      sprintf (plug_in->args[2], "%d", plug_in->his_read);
      sprintf (plug_in->args[3], "%d", plug_in->his_write);

      /* Set the rest of the command line arguments.
       */
      if (plug_in->query)
	{
	  plug_in->args[4] = g_strdup ("-query");
	}
      else
	{
	  plug_in->args[4] = g_new (char, 16);
	  plug_in->args[5] = g_new (char, 16);

	  sprintf (plug_in->args[4], "%d", TILE_WIDTH);
	  sprintf (plug_in->args[5], "%d", TILE_WIDTH);
	}

      /* Fork another process. We'll remember the process id
       *  so that we can later use it to kill the filter if
       *  necessary.
       */
      plug_in->pid = fork ();

      if (plug_in->pid == 0)
	{
	  close(plug_in->my_read);
	  close(plug_in->my_write);
          /* Execute the filter. The "_exit" call should never
           *  be reached, unless some strange error condition
           *  exists.
           */
          execvp (plug_in->args[0], plug_in->args);
          _exit (1);
	}
      else if (plug_in->pid == -1)
	{
          g_message ("unable to run plug-in: %s\n", plug_in->args[0]);
          plug_in_destroy (plug_in);
          return 0;
	}

      close(plug_in->his_read);  plug_in->his_read  = -1;
      close(plug_in->his_write); plug_in->his_write = -1;

      if (!plug_in->synchronous)
	{
	  plug_in->input_id = gdk_input_add (plug_in->my_read,
					     GDK_INPUT_READ,
					     plug_in_recv_message,
					     plug_in);

	  open_plug_ins = g_slist_prepend (open_plug_ins, plug_in);
	}

      plug_in->open = TRUE;
      return 1;
    }

  return 0;
}

void
plug_in_close (PlugIn *plug_in,
	       int     kill_it)
{
  int status;
  struct timeval tv;

  if (plug_in && plug_in->open)
    {
      plug_in->open = FALSE;

      /* Ask the filter to exit gracefully
       */
      if (kill_it && plug_in->pid)
	{
	  plug_in_push (plug_in);
	  gp_quit_write (current_writefd);
	  plug_in_pop ();

	  /*  give the plug-in some time (10 ms)  */
	  tv.tv_sec = 0;
	  tv.tv_usec = 100;
	  select (0, NULL, NULL, NULL, &tv);
	}

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
        gdk_input_remove (plug_in->input_id);

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

      wire_clear_error();

      /* Destroy the progress dialog if it exists
       */
#ifdef SEPARATE_PROGRESS_BAR
      if (plug_in->progress)
	{
	  gtk_signal_disconnect_by_data (GTK_OBJECT (plug_in->progress), plug_in);
	  gtk_widget_destroy (plug_in->progress);

	  plug_in->progress = NULL;
	  plug_in->progress_label = NULL;
	  plug_in->progress_bar = NULL;
	}
#else
      if (plug_in->progress)
	{
	  progress_end ();
	  plug_in->progress = NULL;
	}
#endif

      /* Set the fields to null values.
       */
      plug_in->pid = 0;
      plug_in->input_id = 0;
      plug_in->my_read = 0;
      plug_in->my_write = 0;
      plug_in->his_read = 0;
      plug_in->his_write = 0;

      if (plug_in->recurse)
	gtk_main_quit ();

      plug_in->synchronous = FALSE;
      plug_in->recurse = FALSE;

      /* Unregister any temporary procedures
       */
      if (plug_in->temp_proc_defs)
	{
	  GSList *list;
	  PlugInProcDef *proc_def;

	  list = plug_in->temp_proc_defs;
	  while (list)
	    {
	      proc_def = (PlugInProcDef *) list->data;
	      plug_in_proc_def_remove (proc_def);
	      list = list->next;
	    }

	  g_slist_free (plug_in->temp_proc_defs);
	  plug_in->temp_proc_defs = NULL;
	}

      open_plug_ins = g_slist_remove (open_plug_ins, plug_in);
    }
}

static Argument *
plug_in_get_current_return_vals (ProcRecord *proc_rec)
{
  Argument *return_vals;
  int nargs;

  /* Return the status code plus the current return values. */
  nargs = proc_rec->num_values + 1;
  if (current_return_vals && current_return_nvals == nargs)
    return_vals = current_return_vals;
  else if (current_return_vals)
    {
      /* Allocate new return values of the correct size. */
      return_vals = procedural_db_return_args (proc_rec, FALSE);

      /* Copy all of the arguments we can. */
      memcpy (return_vals, current_return_vals,
	      sizeof (Argument) * MIN (current_return_nvals, nargs));

      /* Free the old argument pointer.  This will cause a memory leak
	 only if there were more values returned than we need (which
	 shouldn't ever happen). */
      g_free (current_return_vals);
    }
  else
    {
      /* Just return a dummy set of values. */
      return_vals = procedural_db_return_args (proc_rec, FALSE);
    }

  /* We have consumed any saved values, so clear them. */
  current_return_nvals = 0;
  current_return_vals = NULL;

  return return_vals;
}

Argument*
plug_in_run (ProcRecord *proc_rec,
	     Argument   *args,
	     int         synchronous,
	     int         destroy_values)
{
  GPConfig config;
  GPProcRun proc_run;
  Argument *return_vals;
  PlugIn *plug_in;

  return_vals = NULL;

  if (proc_rec->proc_type == PDB_TEMPORARY)
    {
      return_vals = plug_in_temp_run (proc_rec, args);
      goto done;
    }

  plug_in = plug_in_new (proc_rec->exec_method.plug_in.filename);

  if (plug_in)
    {
      if (plug_in_open (plug_in))
	{
	  plug_in->recurse = synchronous;

	  plug_in_push (plug_in);

	  config.version = GP_VERSION;
	  config.tile_width = TILE_WIDTH;
	  config.tile_height = TILE_HEIGHT;
	  config.shm_ID = shm_ID;
	  config.gamma = gamma_val;
	  config.install_cmap = install_cmap;
	  config.use_xshm = gdk_get_use_xshm ();
	  config.color_cube[0] = color_cube_shades[0];
	  config.color_cube[1] = color_cube_shades[1];
	  config.color_cube[2] = color_cube_shades[2];
	  config.color_cube[3] = color_cube_shades[3];

	  proc_run.name = proc_rec->name;
	  proc_run.nparams = proc_rec->num_args;
	  proc_run.params = plug_in_args_to_params (args, proc_rec->num_args, FALSE);

	  if (!gp_config_write (current_writefd, &config) ||
	      !gp_proc_run_write (current_writefd, &proc_run) ||
	      !wire_flush (current_writefd))
	    {
	      return_vals = procedural_db_return_args (proc_rec, FALSE);
	      goto done;
	    }

	  plug_in_pop ();

	  plug_in_params_destroy (proc_run.params, proc_run.nparams, FALSE);

	  /*
	   *  If this is an automatically installed extension, wait for an
	   *   installation-confirmation message
	   */
	  if ((proc_rec->proc_type == PDB_EXTENSION) && (proc_rec->num_args == 0))
	    gtk_main ();

	  if (plug_in->recurse)
	    {
	      gtk_main ();

	      return_vals = plug_in_get_current_return_vals (proc_rec);
	    }
	}
    }

done:
  if (return_vals && destroy_values)
    {
      procedural_db_destroy_args (return_vals, proc_rec->num_values);
      return_vals = NULL;
    }
  return return_vals;
}

void
plug_in_repeat (int with_interface)
{
  GDisplay *gdisplay;
  Argument *args;
  int i;

  if (last_plug_in)
    {
      gdisplay = gdisplay_active ();

      /* construct the procedures arguments */
      args = g_new (Argument, last_plug_in->num_args);
      memset (args, 0, (sizeof (Argument) * last_plug_in->num_args));

      /* initialize the argument types */
      for (i = 0; i < last_plug_in->num_args; i++)
	args[i].arg_type = last_plug_in->args[i].arg_type;

      /* initialize the first 3 plug-in arguments  */
      args[0].value.pdb_int = (with_interface ? RUN_INTERACTIVE : RUN_WITH_LAST_VALS);
      args[1].value.pdb_int = gdisplay->gimage->ID;
      args[2].value.pdb_int = drawable_ID (gimage_active_drawable (gdisplay->gimage));

      /* run the plug-in procedure */
      plug_in_run (last_plug_in, args, FALSE, TRUE);

      g_free (args);
    }
}

void
plug_in_set_menu_sensitivity (int base_type)
{
  PlugInProcDef *proc_def;
  GSList *tmp;
  int sensitive = FALSE;

  tmp = proc_defs;
  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (proc_def->image_types_val && proc_def->menu_path)
	{
	  switch (base_type)
	    {
	    case -1:
	      sensitive = FALSE;
	      break;
	    case RGB_GIMAGE:
	      sensitive = proc_def->image_types_val & RGB_IMAGE;
	      break;
	    case RGBA_GIMAGE:
	      sensitive = proc_def->image_types_val & RGBA_IMAGE;
	      break;
	    case GRAY_GIMAGE:
	      sensitive = proc_def->image_types_val & GRAY_IMAGE;
	      break;
	    case GRAYA_GIMAGE:
	      sensitive = proc_def->image_types_val & GRAYA_IMAGE;
	      break;
	    case INDEXED_GIMAGE:
	      sensitive = proc_def->image_types_val & INDEXED_IMAGE;
	      break;
	    case INDEXEDA_GIMAGE:
	      sensitive = proc_def->image_types_val & INDEXEDA_IMAGE;
	      break;
	    }

	  menus_set_sensitive (proc_def->menu_path, sensitive);
          if (last_plug_in && (last_plug_in == &(proc_def->db_info)))
	    {
	      menus_set_sensitive ("<Image>/Filters/Repeat last", sensitive);
	      menus_set_sensitive ("<Image>/Filters/Re-show last", sensitive);
	    }
	}
    }
}

static void
plug_in_recv_message (gpointer          data,
		      gint              id,
		      GdkInputCondition cond)
{
  WireMessage msg;

  plug_in_push ((PlugIn*) data);

  memset (&msg, 0, sizeof (WireMessage));
  if (!wire_read_msg (current_readfd, &msg))
    plug_in_close (current_plug_in, TRUE);
  else
    {
      plug_in_handle_message (&msg);
      wire_destroy (&msg);
    }

  if (!current_plug_in->open)
    plug_in_destroy (current_plug_in);
  else
    plug_in_pop ();
}

static void
plug_in_handle_message (WireMessage *msg)
{
  switch (msg->type)
    {
    case GP_QUIT:
      plug_in_handle_quit ();
      break;
    case GP_CONFIG:
      g_message ("plug_in_handle_message(): received a config message (should not happen)\n");
      plug_in_close (current_plug_in, TRUE);
      break;
    case GP_TILE_REQ:
      plug_in_handle_tile_req (msg->data);
      break;
    case GP_TILE_ACK:
      g_message ("plug_in_handle_message(): received a config message (should not happen)\n");
      plug_in_close (current_plug_in, TRUE);
      break;
    case GP_TILE_DATA:
      g_message ("plug_in_handle_message(): received a config message (should not happen)\n");
      plug_in_close (current_plug_in, TRUE);
      break;
    case GP_PROC_RUN:
      plug_in_handle_proc_run (msg->data);
      break;
    case GP_PROC_RETURN:
      plug_in_handle_proc_return (msg->data);
      plug_in_close (current_plug_in, FALSE);
      break;
    case GP_TEMP_PROC_RUN:
      g_message ("plug_in_handle_message(): received a temp proc run message (should not happen)\n");
      plug_in_close (current_plug_in, TRUE);
      break;
    case GP_TEMP_PROC_RETURN:
      plug_in_handle_proc_return (msg->data);
      gtk_main_quit ();
      break;
    case GP_PROC_INSTALL:
      plug_in_handle_proc_install (msg->data);
      break;
    case GP_PROC_UNINSTALL:
      plug_in_handle_proc_uninstall (msg->data);
      break;
    case GP_EXTENSION_ACK:
      gtk_main_quit ();
      break;
    }
}

static void
plug_in_handle_quit ()
{
  plug_in_close (current_plug_in, FALSE);
}

static void
plug_in_handle_tile_req (GPTileReq *tile_req)
{
  GPTileData tile_data;
  GPTileData *tile_info;
  WireMessage msg;
  TileManager *tm;
  Tile *tile;

  if (tile_req->drawable_ID == -1)
    {
      tile_data.drawable_ID = -1;
      tile_data.tile_num = 0;
      tile_data.shadow = 0;
      tile_data.bpp = 0;
      tile_data.width = 0;
      tile_data.height = 0;
      tile_data.use_shm = (shm_ID == -1) ? FALSE : TRUE;
      tile_data.data = NULL;

      if (!gp_tile_data_write (current_writefd, &tile_data))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      if (!wire_read_msg (current_readfd, &msg))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      if (msg.type != GP_TILE_DATA)
	{
	  g_message ("expected tile data and received: %d\n", msg.type);
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile_info = msg.data;

      if (tile_info->shadow)
	tm = drawable_shadow (drawable_get_ID (tile_info->drawable_ID));
      else
	tm = drawable_data (drawable_get_ID (tile_info->drawable_ID));

      if (!tm)
	{
	  g_message ("plug-in requested invalid drawable (killing)\n");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile = tile_manager_get (tm, tile_info->tile_num, 0);
      if (!tile)
	{
	  g_message ("plug-in requested invalid tile (killing)\n");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile_ref (tile);

      if (tile_data.use_shm)
	memcpy (tile->data, shm_addr, tile_size (tile));
      else
	memcpy (tile->data, tile_info->data, tile_size (tile));

      tile_unref (tile, TRUE);

      wire_destroy (&msg);
      if (!gp_tile_ack_write (current_writefd))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}
    }
  else
    {
      if (tile_req->shadow)
	tm = drawable_shadow (drawable_get_ID (tile_req->drawable_ID));
      else
	tm = drawable_data (drawable_get_ID (tile_req->drawable_ID));

      if (!tm)
	{
	  g_message ("plug-in requested invalid drawable (killing)\n");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile = tile_manager_get (tm, tile_req->tile_num, 0);
      if (!tile)
	{
	  g_message ("plug-in requested invalid tile (killing)\n");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile_data.drawable_ID = tile_req->drawable_ID;
      tile_data.tile_num = tile_req->tile_num;
      tile_data.shadow = tile_req->shadow;
      tile_data.bpp = tile->bpp;
      tile_data.width = tile->ewidth;
      tile_data.height = tile->eheight;
      tile_data.use_shm = (shm_ID == -1) ? FALSE : TRUE;

      tile_ref (tile);

      if (tile_data.use_shm)
	memcpy (shm_addr, tile->data, tile_size (tile));
      else
	tile_data.data = tile->data;

      if (!gp_tile_data_write (current_writefd, &tile_data))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      tile_unref (tile, FALSE);

      if (!wire_read_msg (current_readfd, &msg))
	{
	  g_message ("plug_in_handle_tile_req: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      if (msg.type != GP_TILE_ACK)
	{
	  g_message ("expected tile ack and received: %d\n", msg.type);
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      wire_destroy (&msg);
    }
}

static void
plug_in_handle_proc_run (GPProcRun *proc_run)
{
  GPProcReturn proc_return;
  ProcRecord *proc_rec;
  Argument *args;
  Argument *return_vals;
  PlugInBlocked *blocked;

  args = plug_in_params_to_args (proc_run->params, proc_run->nparams, FALSE);
  proc_rec = procedural_db_lookup (proc_run->name);

  if (!proc_rec)
    {
      /* THIS IS PROBABLY NOT CORRECT -josh */
      g_message ("PDB lookup failed on %s\n", proc_run->name);
      plug_in_args_destroy (args, proc_run->nparams, FALSE);
      return;
    }

  return_vals = procedural_db_execute (proc_run->name, args);

  if (return_vals)
    {
      proc_return.name = proc_run->name;

      if (proc_rec)
	{
	  proc_return.nparams = proc_rec->num_values + 1;
	  proc_return.params = plug_in_args_to_params (return_vals, proc_rec->num_values + 1, FALSE);
	}
      else
	{
	  proc_return.nparams = 1;
	  proc_return.params = plug_in_args_to_params (return_vals, 1, FALSE);
	}

      if (!gp_proc_return_write (current_writefd, &proc_return))
	{
	  g_message ("plug_in_handle_proc_run: ERROR");
	  plug_in_close (current_plug_in, TRUE);
	  return;
	}

      plug_in_args_destroy (args, proc_run->nparams, FALSE);
      plug_in_args_destroy (return_vals, (proc_rec ? (proc_rec->num_values + 1) : 1), TRUE);
      plug_in_params_destroy (proc_return.params, proc_return.nparams, FALSE);
    }
  else
    {
      blocked = g_new (PlugInBlocked, 1);
      blocked->plug_in = current_plug_in;
      blocked->proc_name = g_strdup (proc_run->name);
      blocked_plug_ins = g_slist_prepend (blocked_plug_ins, blocked);
    }
}

static void
plug_in_handle_proc_return (GPProcReturn *proc_return)
{
  PlugInBlocked *blocked;
  GSList *tmp;

  if (current_plug_in->recurse)
    {
      current_return_vals = plug_in_params_to_args (proc_return->params,
						    proc_return->nparams,
						    TRUE);
      current_return_nvals = proc_return->nparams;
    }
  else
    {
      tmp = blocked_plug_ins;
      while (tmp)
	{
	  blocked = tmp->data;
	  tmp = tmp->next;

	  if (strcmp (blocked->proc_name, proc_return->name) == 0)
	    {
	      plug_in_push (blocked->plug_in);
	      if (!gp_proc_return_write (current_writefd, proc_return))
		{
		  g_message ("plug_in_handle_proc_run: ERROR");
		  plug_in_close (current_plug_in, TRUE);
		  return;
		}
	      plug_in_pop ();

	      blocked_plug_ins = g_slist_remove (blocked_plug_ins, blocked);
	      g_free (blocked->proc_name);
	      g_free (blocked);
	      break;
	    }
	}
    }
}

static void
plug_in_handle_proc_install (GPProcInstall *proc_install)
{
  PlugInDef *plug_in_def = NULL;
  PlugInProcDef *proc_def;
  ProcRecord *proc = NULL;
  GSList *tmp = NULL;
  GtkMenuEntry entry;
  char *prog = NULL;
  int add_proc_def;
  int i;

  /*
   *  Argument checking
   *   --only sanity check arguments when the procedure requests a menu path
   */

  if (proc_install->menu_path)
    {
      if (strncmp (proc_install->menu_path, "<Toolbox>", 9) == 0)
	{
	  if ((proc_install->nparams < 1) ||
	      (proc_install->params[0].type != PDB_INT32))
	    {
	      g_message ("plug-in \"%s\" attempted to install procedure \"%s\" which "
			 "does not take the standard plug-in args",
			 current_plug_in->args[0], proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Image>", 7) == 0)
	{
	  if ((proc_install->nparams < 3) ||
	      (proc_install->params[0].type != PDB_INT32) ||
	      (proc_install->params[1].type != PDB_IMAGE) ||
	      (proc_install->params[2].type != PDB_DRAWABLE))
	    {
	      g_message ("plug-in \"%s\" attempted to install procedure \"%s\" which "
			 "does not take the standard plug-in args",
			 current_plug_in->args[0], proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Load>", 6) == 0)
	{
	  if ((proc_install->nparams < 3) ||
	      (proc_install->params[0].type != PDB_INT32) ||
	      (proc_install->params[1].type != PDB_STRING) ||
	      (proc_install->params[2].type != PDB_STRING))
	    {
	      g_message ("plug-in \"%s\" attempted to install procedure \"%s\" which "
			 "does not take the standard plug-in args",
			 current_plug_in->args[0], proc_install->name);
	      return;
	    }
	}
      else if (strncmp (proc_install->menu_path, "<Save>", 6) == 0)
	{
	  if ((proc_install->nparams < 5) ||
	      (proc_install->params[0].type != PDB_INT32) ||
	      (proc_install->params[1].type != PDB_IMAGE) ||
	      (proc_install->params[2].type != PDB_DRAWABLE) ||
	      (proc_install->params[3].type != PDB_STRING) ||
	      (proc_install->params[4].type != PDB_STRING))
	    {
	      g_message ("plug-in \"%s\" attempted to install procedure \"%s\" which "
			 "does not take the standard plug-in args",
			 current_plug_in->args[0], proc_install->name);
	      return;
	    }
	}
      else
	{
	  g_message ("plug-in \"%s\" attempted to install procedure \"%s\" in "
		     "an invalid menu location.  Use either \"<Toolbox>\", \"<Image>\", "
		     "\"<Load>\", or \"<Save>\".",
		     current_plug_in->args[0], proc_install->name);
	  return;
	}
    }

  /*
   *  Sanity check for array arguments
   */

  for (i = 1; i < proc_install->nparams; i++)
    {
      if ((proc_install->params[i].type == PDB_INT32ARRAY ||
	   proc_install->params[i].type == PDB_INT8ARRAY ||
	   proc_install->params[i].type == PDB_FLOATARRAY ||
	   proc_install->params[i].type == PDB_STRINGARRAY) &&
	  proc_install->params[i-1].type != PDB_INT32)
	{
	  g_message ("plug_in \"%s\" attempted to install procedure \"%s\" "
		     "which fails to comply with the array parameter "
		     "passing standard.  Argument %d is noncompliant.",
		     current_plug_in->args[0], proc_install->name, i);
	  return;
	}
    }


  /*
   *  Initialization
   */
  proc_def = NULL;

  switch (proc_install->type)
    {
    case PDB_PLUGIN:
    case PDB_EXTENSION:
      plug_in_def = current_plug_in->user_data;
      prog = plug_in_def->prog;

      tmp = plug_in_def->proc_defs;
      break;

    case PDB_TEMPORARY:
      prog = "none";

      tmp = current_plug_in->temp_proc_defs;
      break;
    }

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, proc_install->name) == 0)
	{
	  if (proc_install->type == PDB_TEMPORARY)
	    plug_in_proc_def_remove (proc_def);
	  else
	    plug_in_proc_def_destroy (proc_def, TRUE);
	  break;
	}

      proc_def = NULL;
    }

  add_proc_def = FALSE;
  if (!proc_def)
    {
      add_proc_def = TRUE;
      proc_def = g_new (PlugInProcDef, 1);
    }

  proc_def->prog = g_strdup (prog);

  proc_def->menu_path = g_strdup (proc_install->menu_path);
  proc_def->accelerator = NULL;
  proc_def->extensions = NULL;
  proc_def->prefixes = NULL;
  proc_def->magics = NULL;
  proc_def->image_types = g_strdup (proc_install->image_types);
  proc_def->image_types_val = plug_in_image_types_parse (proc_def->image_types);

  proc = &proc_def->db_info;

  /*
   *  The procedural database procedure
   */
  proc->name = g_strdup (proc_install->name);
  proc->blurb = g_strdup (proc_install->blurb);
  proc->help = g_strdup (proc_install->help);
  proc->author = g_strdup (proc_install->author);
  proc->copyright = g_strdup (proc_install->copyright);
  proc->date = g_strdup (proc_install->date);
  proc->proc_type = proc_install->type;

  proc->num_args = proc_install->nparams;
  proc->num_values = proc_install->nreturn_vals;

  proc->args = g_new (ProcArg, proc->num_args);
  proc->values = g_new (ProcArg, proc->num_values);

  for (i = 0; i < proc->num_args; i++)
    {
      proc->args[i].arg_type = proc_install->params[i].type;
      proc->args[i].name = g_strdup (proc_install->params[i].name);
      proc->args[i].description = g_strdup (proc_install->params[i].description);
    }

  for (i = 0; i < proc->num_values; i++)
    {
      proc->values[i].arg_type = proc_install->return_vals[i].type;
      proc->values[i].name = g_strdup (proc_install->return_vals[i].name);
      proc->values[i].description = g_strdup (proc_install->return_vals[i].description);
    }

  switch (proc_install->type)
    {
    case PDB_PLUGIN:
    case PDB_EXTENSION:
      if (add_proc_def)
	plug_in_def->proc_defs = g_slist_prepend (plug_in_def->proc_defs, proc_def);
      break;

    case PDB_TEMPORARY:
      if (add_proc_def)
	current_plug_in->temp_proc_defs = g_slist_prepend (current_plug_in->temp_proc_defs, proc_def);

      proc_defs = g_slist_append (proc_defs, proc_def);
      proc->exec_method.temporary.plug_in = (void *) current_plug_in;
      procedural_db_register (proc);

      /*  If there is a menu path specified, create a menu entry  */
      if (proc_install->menu_path)
	{
	  entry.path = proc_install->menu_path;
	  entry.accelerator = NULL;
	  entry.callback = plug_in_callback;
	  entry.callback_data = proc;

	  menus_create (&entry, 1);
	}
      break;
    }
}

static void
plug_in_handle_proc_uninstall (GPProcUninstall *proc_uninstall)
{
  PlugInProcDef *proc_def;
  GSList *tmp;

  tmp = current_plug_in->temp_proc_defs;
  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (strcmp (proc_def->db_info.name, proc_uninstall->name) == 0)
	{
	  current_plug_in->temp_proc_defs = g_slist_remove (current_plug_in->temp_proc_defs, proc_def);
	  plug_in_proc_def_remove (proc_def);
	  break;
	}
    }
}

static int
plug_in_write (int     fd,
	       guint8 *buf,
	       gulong  count)
{
  gulong bytes;

  while (count > 0)
    {
      if ((current_write_buffer_index + count) >= WRITE_BUFFER_SIZE)
	{
	  bytes = WRITE_BUFFER_SIZE - current_write_buffer_index;
	  memcpy (&current_write_buffer[current_write_buffer_index], buf, bytes);
	  current_write_buffer_index += bytes;
	  if (!wire_flush (fd))
	    return FALSE;
	}
      else
	{
	  bytes = count;
	  memcpy (&current_write_buffer[current_write_buffer_index], buf, bytes);
	  current_write_buffer_index += bytes;
	}

      buf += bytes;
      count -= bytes;
    }

  return TRUE;
}

static int
plug_in_flush (int fd)
{
  int count;
  int bytes;

  if (current_write_buffer_index > 0)
    {
      count = 0;
      while (count != current_write_buffer_index)
        {
	  do {
	    bytes = write (fd, &current_write_buffer[count],
			   (current_write_buffer_index - count));
	  } while ((bytes == -1) && (errno == EAGAIN));

	  if (bytes == -1)
	    return FALSE;

          count += bytes;
        }

      current_write_buffer_index = 0;
    }

  return TRUE;
}

static void
plug_in_push (PlugIn *plug_in)
{
  if (plug_in)
    {
      current_plug_in = plug_in;
      plug_in_stack = g_slist_prepend (plug_in_stack, current_plug_in);

      current_readfd = current_plug_in->my_read;
      current_writefd = current_plug_in->my_write;
      current_write_buffer_index = current_plug_in->write_buffer_index;
      current_write_buffer = current_plug_in->write_buffer;
    }
  else
    {
      current_readfd = 0;
      current_writefd = 0;
      current_write_buffer_index = 0;
      current_write_buffer = NULL;
    }
}

static void
plug_in_pop ()
{
  GSList *tmp;

  if (current_plug_in)
    {
      current_plug_in->write_buffer_index = current_write_buffer_index;

      tmp = plug_in_stack;
      plug_in_stack = plug_in_stack->next;
      tmp->next = NULL;
      g_slist_free (tmp);
    }

  if (plug_in_stack)
    {
      current_plug_in = plug_in_stack->data;
      current_readfd = current_plug_in->my_read;
      current_writefd = current_plug_in->my_write;
      current_write_buffer_index = current_plug_in->write_buffer_index;
      current_write_buffer = current_plug_in->write_buffer;
    }
  else
    {
      current_plug_in = NULL;
      current_readfd = 0;
      current_writefd = 0;
      current_write_buffer_index = 0;
      current_write_buffer = NULL;
    }
}

static void
plug_in_write_rc_string (FILE *fp,
			 char *str)
{
  fputc ('"', fp);

  if (str)
    while (*str)
      {
	if ((*str == '"') || (*str == '\\'))
	  fputc ('\\', fp);
	fputc (*str, fp);
	str += 1;
      }

  fputc ('"', fp);
}

static void
plug_in_write_rc (char *filename)
{
  FILE *fp;
  PlugInDef *plug_in_def;
  PlugInProcDef *proc_def;
  GSList *tmp, *tmp2;
  int i;

  fp = fopen (filename, "w");
  if (!fp)
    return;

  tmp = plug_in_defs;
  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      if (plug_in_def->proc_defs)
	{
	  fprintf (fp, "(plug-in-def \"%s\" %ld",
		   plug_in_def->prog,
		   (long) plug_in_def->mtime);

	  tmp2 = plug_in_def->proc_defs;
	  if (tmp2)
	    fprintf (fp, "\n");

	  while (tmp2)
	    {
	      proc_def = tmp2->data;
	      tmp2 = tmp2->next;

	      fprintf (fp, "             (proc-def \"%s\" %d\n",
		       proc_def->db_info.name, proc_def->db_info.proc_type);
	      fprintf (fp, "                       ");
	      plug_in_write_rc_string (fp, proc_def->db_info.blurb);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->db_info.help);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->db_info.author);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->db_info.copyright);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->db_info.date);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->menu_path);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->extensions);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->prefixes);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->magics);
	      fprintf (fp, "\n                       ");
	      plug_in_write_rc_string (fp, proc_def->image_types);
	      fprintf (fp, "\n                       %d %d\n",
		       proc_def->db_info.num_args, proc_def->db_info.num_values);

	      for (i = 0; i < proc_def->db_info.num_args; i++)
		{
		  fprintf (fp, "                       (proc-arg %d ",
			   proc_def->db_info.args[i].arg_type);

		  plug_in_write_rc_string (fp, proc_def->db_info.args[i].name);
		  plug_in_write_rc_string (fp, proc_def->db_info.args[i].description);

		  fprintf (fp, ")%s",
			   (proc_def->db_info.num_values ||
			    (i < (proc_def->db_info.num_args - 1))) ? "\n" : "");
		}

	      for (i = 0; i < proc_def->db_info.num_values; i++)
		{
		  fprintf (fp, "                       (proc-arg %d ",
			   proc_def->db_info.values[i].arg_type);

		  plug_in_write_rc_string (fp, proc_def->db_info.values[i].name);
		  plug_in_write_rc_string (fp, proc_def->db_info.values[i].description);

		  fprintf (fp, ")%s", (i < (proc_def->db_info.num_values - 1)) ? "\n" : "");
		}

	      fprintf (fp, ")");

	      if (tmp2)
		fprintf (fp, "\n");
	    }

	  fprintf (fp, ")\n");

	  if (tmp)
	    fprintf (fp, "\n");
	}
    }

  fclose (fp);
}

static void
plug_in_init_file (char *filename)
{
  GSList *tmp;
  PlugInDef *plug_in_def;
  char *plug_in_name;
  char *name;

  name = strrchr (filename, '/');
  if (name)
    name = name + 1;
  else
    name = filename;

  plug_in_def = NULL;
  tmp = plug_in_defs;

  while (tmp)
    {
      plug_in_def = tmp->data;
      tmp = tmp->next;

      plug_in_name = strrchr (plug_in_def->prog, '/');
      if (plug_in_name)
	plug_in_name = plug_in_name + 1;
      else
	plug_in_name = plug_in_def->prog;

      if (strcmp (name, plug_in_name) == 0)
	{
	  g_print ("duplicate plug-in: \"%s\" (skipping)\n", filename);
	  return;
	}

      plug_in_def = NULL;
    }

  plug_in_def = g_new (PlugInDef, 1);
  plug_in_def->prog = g_strdup (filename);
  plug_in_def->proc_defs = NULL;
  plug_in_def->mtime = datafile_mtime ();
  plug_in_def->query = TRUE;

  plug_in_defs = g_slist_append (plug_in_defs, plug_in_def);
}

static void
plug_in_query (char      *filename,
	       PlugInDef *plug_in_def)
{
  PlugIn *plug_in;
  WireMessage msg;

  plug_in = plug_in_new (filename);
  if (plug_in)
    {
      plug_in->query = TRUE;
      plug_in->synchronous = TRUE;
      plug_in->user_data = plug_in_def;

      if (plug_in_open (plug_in))
	{
	  plug_in_push (plug_in);

	  while (plug_in->open)
	    {
	      if (!wire_read_msg (current_readfd, &msg))
		plug_in_close (current_plug_in, TRUE);
	      else
		{
		  plug_in_handle_message (&msg);
		  wire_destroy (&msg);
		}
	    }

	  plug_in_pop ();
	  plug_in_destroy (plug_in);
	}
    }
}

static void
plug_in_add_to_db ()
{
  PlugInProcDef *proc_def;
  Argument args[4];
  Argument *return_vals;
  GSList *tmp;

  tmp = proc_defs;

  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (proc_def->prog && (proc_def->db_info.proc_type != PDB_INTERNAL))
	{
	  proc_def->db_info.exec_method.plug_in.filename = proc_def->prog;
	  procedural_db_register (&proc_def->db_info);
	}
    }

  for (tmp = proc_defs; tmp; tmp = tmp->next)
    {
      proc_def = tmp->data;

      if (proc_def->extensions || proc_def->prefixes || proc_def->magics)
        {
          args[0].arg_type = PDB_STRING;
          args[0].value.pdb_pointer = proc_def->db_info.name;

          args[1].arg_type = PDB_STRING;
          args[1].value.pdb_pointer = proc_def->extensions;

	  args[2].arg_type = PDB_STRING;
	  args[2].value.pdb_pointer = proc_def->prefixes;

	  args[3].arg_type = PDB_STRING;
	  args[3].value.pdb_pointer = proc_def->magics;

          if (proc_def->image_types)
            {
              return_vals = procedural_db_execute ("gimp_register_save_handler", args);
              g_free (return_vals);
            }
          else
            {
              return_vals = procedural_db_execute ("gimp_register_magic_load_handler", args);
              g_free (return_vals);
            }
	}
    }
}

static void
plug_in_make_menu ()
{
  GtkMenuEntry entry;
  PlugInProcDef *proc_def;
  GSList *tmp;

  tmp = proc_defs;
  while (tmp)
    {
      proc_def = tmp->data;
      tmp = tmp->next;

      if (proc_def->prog && proc_def->menu_path && (!proc_def->extensions &&
						    !proc_def->prefixes &&
						    !proc_def->magics))
	{
	  entry.path = proc_def->menu_path;
	  entry.accelerator = proc_def->accelerator;
	  entry.callback = plug_in_callback;
	  entry.callback_data = &proc_def->db_info;

	  menus_create (&entry, 1);
	}
    }
}

static void
plug_in_callback (GtkWidget *widget,
		  gpointer   client_data)
{
  GDisplay *gdisplay;
  ProcRecord *proc_rec;
  Argument *args;
  int i;

  /* get the active gdisplay */
  gdisplay = gdisplay_active ();

  proc_rec = (ProcRecord*) client_data;

  /* construct the procedures arguments */
  args = g_new (Argument, proc_rec->num_args);
  memset (args, 0, (sizeof (Argument) * proc_rec->num_args));

  /* initialize the argument types */
  for (i = 0; i < proc_rec->num_args; i++)
    args[i].arg_type = proc_rec->args[i].arg_type;

  switch (proc_rec->proc_type)
    {
    case PDB_EXTENSION:
      /* initialize the first argument  */
      args[0].value.pdb_int = RUN_INTERACTIVE;
      break;

    case PDB_PLUGIN:
      if (gdisplay)
	{
	  /* initialize the first 3 plug-in arguments  */
	  args[0].value.pdb_int = RUN_INTERACTIVE;
	  args[1].value.pdb_int = gdisplay->gimage->ID;
	  args[2].value.pdb_int = drawable_ID (gimage_active_drawable (gdisplay->gimage));
	}
      else
	{
	  g_message ("Uh-oh, no active gdisplay for the plug-in!");
	  g_free (args);
	  return;
	}
      break;

    case PDB_TEMPORARY:
      args[0].value.pdb_int = RUN_INTERACTIVE;
      if (proc_rec->num_args >= 3 &&
	  proc_rec->args[1].arg_type == PDB_IMAGE &&
	  proc_rec->args[2].arg_type == PDB_DRAWABLE)
	{
	  if (gdisplay)
	    {
	      args[1].value.pdb_int = gdisplay->gimage->ID;
	      args[2].value.pdb_int = drawable_ID (gimage_active_drawable (gdisplay->gimage));
	    }
	  else
	    {
	      g_message ("Uh-oh, no active gdisplay for the temporary procedure!");
	      g_free (args);
	      return;
	    }
	}
      break;

    default:
      g_error ("Unknown procedure type.");
      break;
    }

  /* run the plug-in procedure */
  plug_in_run (proc_rec, args, FALSE, TRUE);

  if (proc_rec->proc_type == PDB_PLUGIN)
    last_plug_in = proc_rec;

  g_free (args);
}

static void
plug_in_proc_def_insert (PlugInProcDef *proc_def)
{
  PlugInProcDef *tmp_proc_def;
  GSList *tmp, *prev;
  GSList *list;

  prev = NULL;
  tmp = proc_defs;

  while (tmp)
    {
      tmp_proc_def = tmp->data;

      if (strcmp (proc_def->db_info.name, tmp_proc_def->db_info.name) == 0)
	{
	  tmp->data = proc_def;

	  if (proc_def->menu_path)
	    g_free (proc_def->menu_path);
	  if (proc_def->accelerator)
	    g_free (proc_def->accelerator);

	  proc_def->menu_path = tmp_proc_def->menu_path;
	  proc_def->accelerator = tmp_proc_def->accelerator;

	  tmp_proc_def->menu_path = NULL;
	  tmp_proc_def->accelerator = NULL;

	  plug_in_proc_def_destroy (tmp_proc_def, FALSE);
	  return;
	}
      else if (!proc_def->menu_path ||
	       (tmp_proc_def->menu_path &&
		(strcmp (proc_def->menu_path, tmp_proc_def->menu_path) < 0)))
	{
	  list = g_slist_alloc ();
	  list->data = proc_def;

	  list->next = tmp;
	  if (prev)
	    prev->next = list;
	  else
	    proc_defs = list;
	  return;
	}

      prev = tmp;
      tmp = tmp->next;
    }

  proc_defs = g_slist_append (proc_defs, proc_def);
}

static void
plug_in_proc_def_remove (PlugInProcDef *proc_def)
{
  /*  Destroy the menu item  */
  if (proc_def->menu_path)
    menus_destroy (proc_def->menu_path);

  /*  Unregister the procedural database entry  */
  procedural_db_unregister (proc_def->db_info.name);

  /*  Remove the defintion from the global list  */
  proc_defs = g_slist_remove (proc_defs, proc_def);

  /*  Destroy the definition  */
  plug_in_proc_def_destroy (proc_def, TRUE);
}

static void
plug_in_proc_def_destroy (PlugInProcDef *proc_def,
			  int            data_only)
{
  int i;

  if (proc_def->prog)
    g_free (proc_def->prog);
  if (proc_def->menu_path)
    g_free (proc_def->menu_path);
  if (proc_def->accelerator)
    g_free (proc_def->accelerator);
  if (proc_def->extensions)
    g_free (proc_def->extensions);
  if (proc_def->prefixes)
    g_free (proc_def->prefixes);
  if (proc_def->magics)
    g_free (proc_def->magics);
  if (proc_def->image_types)
    g_free (proc_def->image_types);
  if (proc_def->db_info.name)
    g_free (proc_def->db_info.name);
  if (proc_def->db_info.blurb)
    g_free (proc_def->db_info.blurb);
  if (proc_def->db_info.help)
    g_free (proc_def->db_info.help);
  if (proc_def->db_info.author)
    g_free (proc_def->db_info.author);
  if (proc_def->db_info.copyright)
    g_free (proc_def->db_info.copyright);
  if (proc_def->db_info.date)
    g_free (proc_def->db_info.date);

  for (i = 0; i < proc_def->db_info.num_args; i++)
    {
      if (proc_def->db_info.args[i].name)
	g_free (proc_def->db_info.args[i].name);
      if (proc_def->db_info.args[i].description)
	g_free (proc_def->db_info.args[i].description);
    }

  for (i = 0; i < proc_def->db_info.num_values; i++)
    {
      if (proc_def->db_info.values[i].name)
	g_free (proc_def->db_info.values[i].name);
      if (proc_def->db_info.values[i].description)
	g_free (proc_def->db_info.values[i].description);
    }

  if (proc_def->db_info.args)
    g_free (proc_def->db_info.args);
  if (proc_def->db_info.values)
    g_free (proc_def->db_info.values);

  if (!data_only)
    g_free (proc_def);
}

static Argument *
plug_in_temp_run (ProcRecord *proc_rec,
		  Argument   *args)
{
  Argument *return_vals;
  PlugIn *plug_in;
  GPProcRun proc_run;
  gint old_recurse;

  return_vals = NULL;

  plug_in = (PlugIn *) proc_rec->exec_method.temporary.plug_in;

  if (plug_in)
    {
      if (plug_in->busy)
	{
	  return_vals = procedural_db_return_args (proc_rec, FALSE);
	  goto done;
	}

      plug_in->busy = TRUE;
      plug_in_push (plug_in);

      proc_run.name = proc_rec->name;
      proc_run.nparams = proc_rec->num_args;
      proc_run.params = plug_in_args_to_params (args, proc_rec->num_args, FALSE);

      if (!gp_temp_proc_run_write (current_writefd, &proc_run) ||
	  !wire_flush (current_writefd))
	{
	  return_vals = procedural_db_return_args (proc_rec, FALSE);
	  goto done;
	}

      plug_in_pop ();

      plug_in_params_destroy (proc_run.params, proc_run.nparams, FALSE);

      old_recurse = plug_in->recurse;
      plug_in->recurse = TRUE;

      gtk_main ();

      return_vals = plug_in_get_current_return_vals (proc_rec);
      plug_in->recurse = old_recurse;
      plug_in->busy = FALSE;
    }

done:
  return return_vals;
}

static Argument*
plug_in_params_to_args (GPParam *params,
			int      nparams,
			int      full_copy)
{
  Argument *args;
  gchar **stringarray;
  guchar *colorarray;
  int count;
  int i, j;

  if (nparams == 0)
    return NULL;

  args = g_new (Argument, nparams);

  for (i = 0; i < nparams; i++)
    {
      args[i].arg_type = params[i].type;

      switch (args[i].arg_type)
	{
	case PDB_INT32:
	  args[i].value.pdb_int = params[i].data.d_int32;
	  break;
	case PDB_INT16:
	  args[i].value.pdb_int = params[i].data.d_int16;
	  break;
	case PDB_INT8:
	  args[i].value.pdb_int = params[i].data.d_int8;
	  break;
	case PDB_FLOAT:
	  args[i].value.pdb_float = params[i].data.d_float;
	  break;
	case PDB_STRING:
	  if (full_copy)
	    args[i].value.pdb_pointer = g_strdup (params[i].data.d_string);
	  else
	    args[i].value.pdb_pointer = params[i].data.d_string;
	  break;
	case PDB_INT32ARRAY:
	  if (full_copy)
	    {
	      count = args[i-1].value.pdb_int;
	      args[i].value.pdb_pointer = g_new (gint32, count);
	      memcpy (args[i].value.pdb_pointer, params[i].data.d_int32array, count * 4);
	    }
	  else
	    {
	      args[i].value.pdb_pointer = params[i].data.d_int32array;
	    }
	  break;
	case PDB_INT16ARRAY:
	  if (full_copy)
	    {
	      count = args[i-1].value.pdb_int;
	      args[i].value.pdb_pointer = g_new (gint16, count);
	      memcpy (args[i].value.pdb_pointer, params[i].data.d_int16array, count * 2);
	    }
	  else
	    {
	      args[i].value.pdb_pointer = params[i].data.d_int16array;
	    }
	  break;
	case PDB_INT8ARRAY:
	  if (full_copy)
	    {
	      count = args[i-1].value.pdb_int;
	      args[i].value.pdb_pointer = g_new (gint8, count);
	      memcpy (args[i].value.pdb_pointer, params[i].data.d_int8array, count);
	    }
	  else
	    {
	      args[i].value.pdb_pointer = params[i].data.d_int8array;
	    }
	  break;
	case PDB_FLOATARRAY:
	  if (full_copy)
	    {
	      count = args[i-1].value.pdb_int;
	      args[i].value.pdb_pointer = g_new (gdouble, count);
	      memcpy (args[i].value.pdb_pointer, params[i].data.d_floatarray, count * 8);
	    }
	  else
	    {
	      args[i].value.pdb_pointer = params[i].data.d_floatarray;
	    }
	  break;
	case PDB_STRINGARRAY:
	  if (full_copy)
	    {
	      args[i].value.pdb_pointer = g_new (gchar*, args[i-1].value.pdb_int);
	      stringarray = args[i].value.pdb_pointer;

	      for (j = 0; j < args[i-1].value.pdb_int; j++)
		stringarray[j] = g_strdup (params[i].data.d_stringarray[j]);
	    }
	  else
	    {
	      args[i].value.pdb_pointer = params[i].data.d_stringarray;
	    }
	  break;
	case PDB_COLOR:
	  args[i].value.pdb_pointer = g_new (guchar, 3);
	  colorarray = args[i].value.pdb_pointer;
	  colorarray[0] = params[i].data.d_color.red;
	  colorarray[1] = params[i].data.d_color.green;
	  colorarray[2] = params[i].data.d_color.blue;
	  break;
	case PDB_REGION:
	  g_message ("the \"region\" arg type is not currently supported");
	  break;
	case PDB_DISPLAY:
	  args[i].value.pdb_int = params[i].data.d_display;
	  break;
	case PDB_IMAGE:
	  args[i].value.pdb_int = params[i].data.d_image;
	  break;
	case PDB_LAYER:
	  args[i].value.pdb_int = params[i].data.d_layer;
	  break;
	case PDB_CHANNEL:
	  args[i].value.pdb_int = params[i].data.d_channel;
	  break;
	case PDB_DRAWABLE:
	  args[i].value.pdb_int = params[i].data.d_drawable;
	  break;
	case PDB_SELECTION:
	  args[i].value.pdb_int = params[i].data.d_selection;
	  break;
	case PDB_BOUNDARY:
	  args[i].value.pdb_int = params[i].data.d_boundary;
	  break;
	case PDB_PATH:
	  args[i].value.pdb_int = params[i].data.d_path;
	  break;
	case PDB_STATUS:
	  args[i].value.pdb_int = params[i].data.d_status;
	  break;
	case PDB_END:
	  break;
	}
    }

  return args;
}

static GPParam*
plug_in_args_to_params (Argument *args,
			int       nargs,
			int       full_copy)
{
  GPParam *params;
  gchar **stringarray;
  guchar *colorarray;
  int i, j;

  if (nargs == 0)
    return NULL;

  params = g_new (GPParam, nargs);

  for (i = 0; i < nargs; i++)
    {
      params[i].type = args[i].arg_type;

      switch (args[i].arg_type)
	{
	case PDB_INT32:
	  params[i].data.d_int32 = args[i].value.pdb_int;
	  break;
	case PDB_INT16:
	  params[i].data.d_int16 = args[i].value.pdb_int;
	  break;
	case PDB_INT8:
	  params[i].data.d_int8 = args[i].value.pdb_int;
	  break;
	case PDB_FLOAT:
	  params[i].data.d_float = args[i].value.pdb_float;
	  break;
	case PDB_STRING:
	  if (full_copy)
	    params[i].data.d_string = g_strdup (args[i].value.pdb_pointer);
	  else
	    params[i].data.d_string = args[i].value.pdb_pointer;
	  break;
	case PDB_INT32ARRAY:
	  if (full_copy)
	    {
	      params[i].data.d_int32array = g_new (gint32, params[i-1].data.d_int32);
	      memcpy (params[i].data.d_int32array,
		      args[i].value.pdb_pointer,
		      params[i-1].data.d_int32 * 4);
	    }
	  else
	    {
	      params[i].data.d_int32array = args[i].value.pdb_pointer;
	    }
	  break;
	case PDB_INT16ARRAY:
	  if (full_copy)
	    {
	      params[i].data.d_int16array = g_new (gint16, params[i-1].data.d_int32);
	      memcpy (params[i].data.d_int16array,
		      args[i].value.pdb_pointer,
		      params[i-1].data.d_int32 * 2);
	    }
	  else
	    {
	      params[i].data.d_int16array = args[i].value.pdb_pointer;
	    }
	  break;
	case PDB_INT8ARRAY:
	  if (full_copy)
	    {
	      params[i].data.d_int8array = g_new (gint8, params[i-1].data.d_int32);
	      memcpy (params[i].data.d_int8array,
		      args[i].value.pdb_pointer,
		      params[i-1].data.d_int32);
	    }
	  else
	    {
	      params[i].data.d_int8array = args[i].value.pdb_pointer;
	    }
	  break;
	case PDB_FLOATARRAY:
	  if (full_copy)
	    {
	      params[i].data.d_floatarray = g_new (gdouble, params[i-1].data.d_int32);
	      memcpy (params[i].data.d_floatarray,
		      args[i].value.pdb_pointer,
		      params[i-1].data.d_int32 * 8);
	    }
	  else
	    {
	      params[i].data.d_floatarray = args[i].value.pdb_pointer;
	    }
	  break;
	case PDB_STRINGARRAY:
	  if (full_copy)
	    {
	      params[i].data.d_stringarray = g_new (gchar*, params[i-1].data.d_int32);
	      stringarray = args[i].value.pdb_pointer;

	      for (j = 0; j < params[i-1].data.d_int32; j++)
		params[i].data.d_stringarray[j] = g_strdup (stringarray[j]);
	    }
	  else
	    {
	      params[i].data.d_stringarray = args[i].value.pdb_pointer;
	    }
	  break;
	case PDB_COLOR:
	  colorarray = args[i].value.pdb_pointer;
	  if( colorarray )
	    {
	      params[i].data.d_color.red = colorarray[0];
	      params[i].data.d_color.green = colorarray[1];
	      params[i].data.d_color.blue = colorarray[2];
	    }
	  else
	    {
	      params[i].data.d_color.red = 0;
	      params[i].data.d_color.green = 0;
	      params[i].data.d_color.blue = 0;
	    }
	  break;
	case PDB_REGION:
	  g_message ("the \"region\" arg type is not currently supported");
	  break;
	case PDB_DISPLAY:
	  params[i].data.d_display = args[i].value.pdb_int;
	  break;
	case PDB_IMAGE:
	  params[i].data.d_image = args[i].value.pdb_int;
	  break;
	case PDB_LAYER:
	  params[i].data.d_layer = args[i].value.pdb_int;
	  break;
	case PDB_CHANNEL:
	  params[i].data.d_channel = args[i].value.pdb_int;
	  break;
	case PDB_DRAWABLE:
	  params[i].data.d_drawable = args[i].value.pdb_int;
	  break;
	case PDB_SELECTION:
	  params[i].data.d_selection = args[i].value.pdb_int;
	  break;
	case PDB_BOUNDARY:
	  params[i].data.d_boundary = args[i].value.pdb_int;
	  break;
	case PDB_PATH:
	  params[i].data.d_path = args[i].value.pdb_int;
	  break;
	case PDB_STATUS:
	  params[i].data.d_status = args[i].value.pdb_int;
	  break;
	case PDB_END:
	  break;
	}
    }

  return params;
}

static void
plug_in_params_destroy (GPParam *params,
			int      nparams,
			int      full_destroy)
{
  int i, j;

  for (i = 0; i < nparams; i++)
    {
      switch (params[i].type)
	{
	case PDB_INT32:
	case PDB_INT16:
	case PDB_INT8:
	case PDB_FLOAT:
	  break;
	case PDB_STRING:
	  if (full_destroy)
	    g_free (params[i].data.d_string);
	  break;
	case PDB_INT32ARRAY:
	  if (full_destroy)
	    g_free (params[i].data.d_int32array);
	  break;
	case PDB_INT16ARRAY:
	  if (full_destroy)
	    g_free (params[i].data.d_int16array);
	  break;
	case PDB_INT8ARRAY:
	  if (full_destroy)
	    g_free (params[i].data.d_int8array);
	  break;
	case PDB_FLOATARRAY:
	  if (full_destroy)
	    g_free (params[i].data.d_floatarray);
	  break;
	case PDB_STRINGARRAY:
	  if (full_destroy)
	    {
	      for (j = 0; j < params[i-1].data.d_int32; j++)
		g_free (params[i].data.d_stringarray[j]);
	      g_free (params[i].data.d_stringarray);
	    }
	  break;
	case PDB_COLOR:
	  break;
	case PDB_REGION:
	  g_message ("the \"region\" arg type is not currently supported");
	  break;
	case PDB_DISPLAY:
	case PDB_IMAGE:
	case PDB_LAYER:
	case PDB_CHANNEL:
	case PDB_DRAWABLE:
	case PDB_SELECTION:
	case PDB_BOUNDARY:
	case PDB_PATH:
	case PDB_STATUS:
	  break;
	case PDB_END:
	  break;
	}
    }

  g_free (params);
}

static void
plug_in_args_destroy (Argument *args,
		      int       nargs,
		      int       full_destroy)
{
  gchar **stringarray;
  int count;
  int i, j;

  for (i = 0; i < nargs; i++)
    {
      switch (args[i].arg_type)
	{
	case PDB_INT32:
	case PDB_INT16:
	case PDB_INT8:
	case PDB_FLOAT:
	  break;
	case PDB_STRING:
	  if (full_destroy)
	    g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_INT32ARRAY:
	  if (full_destroy)
	    g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_INT16ARRAY:
	  if (full_destroy)
	    g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_INT8ARRAY:
	  if (full_destroy)
	    g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_FLOATARRAY:
	  if (full_destroy)
	    g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_STRINGARRAY:
	  if (full_destroy)
	    {
	      count = args[i-1].value.pdb_int;
	      stringarray = args[i].value.pdb_pointer;

	      for (j = 0; j < count; j++)
		g_free (stringarray[j]);

	      g_free (args[i].value.pdb_pointer);
	    }
	  break;
	case PDB_COLOR:
	  g_free (args[i].value.pdb_pointer);
	  break;
	case PDB_REGION:
	  g_message ("the \"region\" arg type is not currently supported");
	  break;
	case PDB_DISPLAY:
	case PDB_IMAGE:
	case PDB_LAYER:
	case PDB_CHANNEL:
	case PDB_DRAWABLE:
	case PDB_SELECTION:
	case PDB_BOUNDARY:
	case PDB_PATH:
	case PDB_STATUS:
	  break;
	case PDB_END:
	  break;
	}
    }

  g_free (args);
}

int
plug_in_image_types_parse (char *image_types)
{
  int types;

  if (!image_types)
    return (RGB_IMAGE | GRAY_IMAGE | INDEXED_IMAGE);

  types = 0;

  while (*image_types)
    {
      while (*image_types &&
	     ((*image_types == ' ') ||
	      (*image_types == '\t') ||
	      (*image_types == ',')))
	image_types++;

      if (*image_types)
	{
	  if (strncmp (image_types, "RGBA", 4) == 0)
	    {
	      types |= RGBA_IMAGE;
	      image_types += 4;
	    }
	  else if (strncmp (image_types, "RGB*", 4) == 0)
	    {
	      types |= RGB_IMAGE | RGBA_IMAGE;
	      image_types += 4;
	    }
	  else if (strncmp (image_types, "RGB", 3) == 0)
	    {
	      types |= RGB_IMAGE;
	      image_types += 3;
	    }
	  else if (strncmp (image_types, "GRAYA", 5) == 0)
	    {
	      types |= GRAYA_IMAGE;
	      image_types += 5;
	    }
	  else if (strncmp (image_types, "GRAY*", 5) == 0)
	    {
	      types |= GRAY_IMAGE | GRAYA_IMAGE;
	      image_types += 5;
	    }
	  else if (strncmp (image_types, "GRAY", 4) == 0)
	    {
	      types |= GRAY_IMAGE;
	      image_types += 4;
	    }
	  else if (strncmp (image_types, "INDEXEDA", 8) == 0)
	    {
	      types |= INDEXEDA_IMAGE;
	      image_types += 8;
	    }
	  else if (strncmp (image_types, "INDEXED*", 8) == 0)
	    {
	      types |= INDEXED_IMAGE | INDEXEDA_IMAGE;
	      image_types += 8;
	    }
	  else if (strncmp (image_types, "INDEXED", 7) == 0)
	    {
	      types |= INDEXED_IMAGE;
	      image_types += 7;
	    }
	  else
	    {
	      while (*image_types &&
		     (*image_types != 'R') &&
		     (*image_types != 'G') &&
		     (*image_types != 'I'))
		image_types++;
	    }
	}
    }

  return types;
}

static void
plug_in_progress_cancel (GtkWidget *widget,
			 PlugIn    *plug_in)
{
  plug_in->progress = NULL;
  plug_in_destroy (plug_in);
}

static void
plug_in_progress_init (PlugIn *plug_in,
		       char   *message)
{
  GtkWidget *vbox;
  GtkWidget *button;

  if (!message)
    message = plug_in->args[0];

#ifdef SEPARATE_PROGRESS_BAR
  if (!plug_in->progress)
    {
      plug_in->progress = gtk_dialog_new ();
      gtk_window_set_wmclass (GTK_WINDOW (plug_in->progress), "plug_in_progress", "Gimp");
      gtk_window_set_title (GTK_WINDOW (plug_in->progress), prune_filename (plug_in->args[0]));
      gtk_widget_set_uposition (plug_in->progress, progress_x, progress_y);
      gtk_signal_connect (GTK_OBJECT (plug_in->progress), "destroy",
			  (GtkSignalFunc) plug_in_progress_cancel,
			  plug_in);
      gtk_container_border_width (GTK_CONTAINER (GTK_DIALOG (plug_in->progress)->action_area), 2);

      vbox = gtk_vbox_new (FALSE, 2);
      gtk_container_border_width (GTK_CONTAINER (vbox), 2);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (plug_in->progress)->vbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);

      plug_in->progress_label = gtk_label_new (message);
      gtk_misc_set_alignment (GTK_MISC (plug_in->progress_label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), plug_in->progress_label, FALSE, TRUE, 0);
      gtk_widget_show (plug_in->progress_label);

      plug_in->progress_bar = gtk_progress_bar_new ();
      gtk_widget_set_usize (plug_in->progress_bar, 150, 20);
      gtk_box_pack_start (GTK_BOX (vbox), plug_in->progress_bar, TRUE, TRUE, 0);
      gtk_widget_show (plug_in->progress_bar);

      button = gtk_button_new_with_label ("Cancel");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 (GtkSignalFunc) gtk_widget_destroy,
                                 GTK_OBJECT (plug_in->progress));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (plug_in->progress)->action_area), button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      gtk_widget_show (plug_in->progress);
    }
  else
    {
      gtk_label_set (GTK_LABEL (plug_in->progress_label), message);
    }
#else
  if (!plug_in->progress)
    {
      plug_in->progress = 0x1;
      progress_update (0.0);
      progress_start ();
    }
#endif
}

static void
plug_in_progress_update (PlugIn *plug_in,
			 double  percentage)
{
#ifdef SEPARATE_PROGRESS_BAR
  if (!(percentage >= 0.0 && percentage <= 1.0))
    return;

  if (!plug_in->progress)
    plug_in_progress_init (plug_in, NULL);

  gtk_progress_bar_update (GTK_PROGRESS_BAR (plug_in->progress_bar), percentage);
#else
  progress_update (percentage);
#endif
}

static Argument*
progress_init_invoker (Argument *args)
{
  int success = FALSE;

  if (current_plug_in && current_plug_in->open)
    {
      success = TRUE;
      if (no_interface == FALSE)
	plug_in_progress_init (current_plug_in, args[0].value.pdb_pointer);
    }

  return procedural_db_return_args (&progress_init_proc, success);
}

static Argument*
progress_update_invoker (Argument *args)
{
  int success = FALSE;

  if (current_plug_in && current_plug_in->open)
    {
      success = TRUE;
      if (no_interface == FALSE)
	plug_in_progress_update (current_plug_in, args[0].value.pdb_float);
    }

  return procedural_db_return_args (&progress_update_proc, success);
}

static Argument*
message_invoker (Argument *args)
{
  g_message (args[0].value.pdb_pointer, NULL, NULL);
  return procedural_db_return_args (&message_proc, TRUE);
}

static Argument*
message_handler_get_invoker (Argument *args)
{
  Argument *return_args;

  return_args = procedural_db_return_args (&message_handler_get_proc, TRUE);
  return_args[1].value.pdb_int = message_handler;
  return return_args;
}

static Argument*
message_handler_set_invoker (Argument *args)
{
  int success = TRUE;

  if ((args[0].value.pdb_int >= MESSAGE_BOX) &&
      (args[0].value.pdb_int <= CONSOLE))
    message_handler = args[0].value.pdb_int;
  else
    success = FALSE;

  return procedural_db_return_args (&message_handler_set_proc, success);
}
