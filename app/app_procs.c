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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "core/core-types.h"

#include "config/gimprc.h"

#include "base/base.h"

#include "core/gimp.h"

#include "file/file-open.h"
#include "file/file-utils.h"

#ifndef GIMP_CONSOLE_COMPILATION
#include "dialogs/user-install-dialog.h"

#include "gui/gui.h"
#endif

#include "app_procs.h"
#include "batch.h"
#include "errors.h"
#include "units.h"

#include "gimp-intl.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif


/*  local prototypes  */

static void       app_init_update_none    (const gchar *text1,
                                           const gchar *text2,
                                           gdouble      percentage);
static gboolean   app_exit_after_callback (Gimp        *gimp,
                                           gboolean     kill_it,
                                           GMainLoop   *loop);


/*  public functions  */

gboolean
app_libs_init (gboolean   *no_interface,
               gint       *argc,
               gchar    ***argv)
{
#ifdef GIMP_CONSOLE_COMPILATION
  *no_interface = TRUE;
#endif

  if (*no_interface)
    {
      gchar *basename;

      basename = g_path_get_basename ((*argv)[0]);
      g_set_prgname (basename);
      g_free (basename);

      g_type_init ();

      return TRUE;
    }
#ifndef GIMP_CONSOLE_COMPILATION
  else
    {
      return gui_libs_init (argc, argv);
    }
#endif

  return FALSE;
}

void
app_abort (gboolean     no_interface,
           const gchar *abort_message)
{
#ifndef GIMP_CONSOLE_COMPILATION
  if (no_interface)
#endif
    {
      g_print ("%s\n\n", abort_message);
    }
#ifndef GIMP_CONSOLE_COMPILATION
  else
    {
      gui_abort (abort_message);
    }
#endif

  app_exit (EXIT_FAILURE);
}

void
app_exit (gint status)
{
#ifdef G_OS_WIN32
  /* Give them time to read the message if it was printed in a
   * separate console window. I would really love to have
   * some way of asking for confirmation to close the console
   * window.
   */
  HANDLE console;
  DWORD  mode;

  console = GetStdHandle (STD_OUTPUT_HANDLE);
  if (GetConsoleMode (console, &mode) != 0)
    {
      g_print (_("(This console window will close in ten seconds)\n"));
      Sleep(10000);
    }
#endif

  exit (status);
}

void
app_run (const gchar         *full_prog_name,
         gint                 gimp_argc,
         gchar              **gimp_argv,
         const gchar         *alternate_system_gimprc,
         const gchar         *alternate_gimprc,
         const gchar         *session_name,
         const gchar         *batch_interpreter,
         const gchar        **batch_commands,
         gboolean             no_interface,
         gboolean             no_data,
         gboolean             no_fonts,
         gboolean             no_splash,
         gboolean             be_verbose,
         gboolean             use_shm,
         gboolean             use_cpu_accel,
         gboolean             console_messages,
         GimpStackTraceMode   stack_trace_mode,
         GimpPDBCompatMode    pdb_compat_mode)
{
  GimpInitStatusFunc  update_status_func = NULL;
  Gimp               *gimp;
  GMainLoop          *loop;
  gboolean            swap_is_ok;
  gint                i;

  const gchar *log_domains[] =
  {
    "Gimp",
    "Gimp-Actions",
    "Gimp-Base",
    "Gimp-Composite",
    "Gimp-Config",
    "Gimp-Core",
    "Gimp-Dialogs",
    "Gimp-Display",
    "Gimp-File",
    "Gimp-GUI",
    "Gimp-Menus",
    "Gimp-PDB",
    "Gimp-Paint-Funcs",
    "Gimp-Plug-In",
    "Gimp-Text",
    "Gimp-Tools",
    "Gimp-Vectors",
    "Gimp-Widgets",
    "Gimp-XCF"
  };

  /*  Create an instance of the "Gimp" object which is the root of the
   *  core object system
   */
  gimp = gimp_new (full_prog_name,
                   session_name,
                   be_verbose,
                   no_data,
                   no_fonts,
                   no_interface,
                   use_shm,
                   console_messages,
                   stack_trace_mode,
                   pdb_compat_mode);

  for (i = 0; i < G_N_ELEMENTS (log_domains); i++)
    g_log_set_handler (log_domains[i],
                       G_LOG_LEVEL_MESSAGE,
                       gimp_message_log_func, &gimp);

  g_log_set_handler (NULL,
		     G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL,
		     gimp_error_log_func, &gimp);

  units_init (gimp);

  /*  Check if the user's gimp_directory exists
   */
  if (! g_file_test (gimp_directory (), G_FILE_TEST_IS_DIR))
    {
      /*  not properly installed  */

#ifndef GIMP_CONSOLE_COMPILATION
      if (no_interface)
#endif
	{
          const gchar *msg;

          msg = _("GIMP is not properly installed for the current user.\n"
                  "User installation was skipped because the '--no-interface' flag was used.\n"
                  "To perform user installation, run the GIMP without the '--no-interface' flag.");

          g_printerr ("%s\n\n", msg);
	}
#ifndef GIMP_CONSOLE_COMPILATION
      else
	{
          user_install_dialog_run (alternate_system_gimprc,
                                   alternate_gimprc,
                                   be_verbose);
	}
#endif
    }

#if defined G_OS_WIN32 && !defined GIMP_CONSOLE_COMPILATION
  /* Common windoze apps don't have a console at all. So does Gimp
   * - if appropiate. This allows to compile as console application
   * with all it's benefits (like inheriting the console) but hide
   * it, if the user doesn't want it.
   */
  if (!no_interface && !be_verbose && !console_messages)
    FreeConsole ();
#endif

  gimp_load_config (gimp, alternate_system_gimprc, alternate_gimprc);

  /*  initialize lowlevel stuff  */
  swap_is_ok = base_init (GIMP_BASE_CONFIG (gimp->config),
                          be_verbose, use_cpu_accel);

#ifndef GIMP_CONSOLE_COMPILATION
  if (! no_interface)
    update_status_func = gui_init (gimp, no_splash);
#endif

  if (! update_status_func)
    update_status_func = app_init_update_none;

  /*  Create all members of the global Gimp instance which need an already
   *  parsed gimprc, e.g. the data factories
   */
  gimp_initialize (gimp, update_status_func);

  /*  Load all data files
   */
  gimp_restore (gimp, update_status_func);

  /* display a warning when no test swap file could be generated */
  if (! swap_is_ok)
    g_message (_("Unable to open a test swap file. To avoid data loss "
                 "please check the location and permissions of the swap "
                 "directory defined in your Preferences "
                 "(currently \"%s\")."),
               GIMP_BASE_CONFIG (gimp->config)->swap_path);

  /*  enable autosave late so we don't autosave when the
   *  monitor resolution is set in gui_init()
   */
  gimp_rc_set_autosave (GIMP_RC (gimp->edit_config), TRUE);

  /*  Parse the rest of the command line arguments as images to load
   */
  if (gimp_argc > 0)
    {
      gint i;

      for (i = 0; i < gimp_argc; i++)
        {
          if (gimp_argv[i])
            {
              GError *error = NULL;
              gchar  *uri;

              /*  first try if we got a file uri  */
              uri = g_filename_from_uri (gimp_argv[i], NULL, NULL);

              if (uri)
                {
                  g_free (uri);
                  uri = g_strdup (gimp_argv[i]);
                }
              else
                {
                  uri = file_utils_filename_to_uri (gimp->load_procs,
                                                    gimp_argv[i], &error);
                }

              if (! uri)
                {
                  g_printerr ("conversion filename -> uri failed: %s\n",
                              error->message);
                  g_clear_error (&error);
                }
              else
                {
                  GimpImage         *gimage;
                  GimpPDBStatusType  status;

                  gimage = file_open_with_display (gimp,
                                                   gimp_get_user_context (gimp),
                                                   NULL,
                                                   uri,
                                                   &status, &error);

                  if (! gimage && status != GIMP_PDB_CANCEL)
                    {
                      gchar *filename = file_utils_uri_to_utf8_filename (uri);

                      g_message (_("Opening '%s' failed: %s"),
                                 filename, error->message);
                      g_clear_error (&error);

                      g_free (filename);
                   }

                  g_free (uri);
                }
            }
        }
    }

#ifndef GIMP_CONSOLE_COMPILATION
  if (! no_interface)
    gui_post_init (gimp);
#endif

  batch_run (gimp, batch_interpreter, batch_commands);

  loop = g_main_loop_new (NULL, FALSE);

  g_signal_connect_after (gimp, "exit",
                          G_CALLBACK (app_exit_after_callback),
                          loop);

  gimp_threads_leave (gimp);
  g_main_loop_run (loop);
  gimp_threads_enter (gimp);

  g_main_loop_unref (loop);

  g_object_unref (gimp);
  base_exit ();
}


/*  private functions  */

static void
app_init_update_none (const gchar *text1,
                      const gchar *text2,
                      gdouble      percentage)
{
}

static gboolean
app_exit_after_callback (Gimp      *gimp,
                         gboolean   kill_it,
                         GMainLoop *loop)
{
  if (gimp->be_verbose)
    g_print ("EXIT: app_exit_after_callback\n");

  /*
   *  In stable releases, we simply call exit() here. This speeds up
   *  the process of quitting GIMP and also works around the problem
   *  that plug-ins might still be running.
   *
   *  In unstable releases, we shut down GIMP properly in an attempt
   *  to catch possible problems in our finalizers.
   */

#ifdef GIMP_UNSTABLE
  g_main_loop_quit (loop);
#else
  /*  make sure that the swap file is removed before we quit */
  base_exit ();
  exit (EXIT_SUCCESS);
#endif

  return FALSE;
}
