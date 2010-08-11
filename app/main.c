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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef  WAIT_ANY
#define  WAIT_ANY -1
#endif   /*  WAIT_ANY  */

#include "../config.h"
#include "appenv.h"
#include "brushes.h"
#include "callbacks.h"
#include "fileops.h"
#include "resource.h"
#include "interface.h"
#include "gimage.h"
#include "gimprc.h"
#include "gdisplay.h"
#include "workprocs.h"
#include "cursorutil.h"
#include "plug_in.h"
#include "errors.h"

static RETSIGTYPE on_signal (int);
static RETSIGTYPE on_sig_child (int);
static int on_x_error (Display*, XErrorEvent*);
static int on_x_io_error (Display*);
static void on_xt_error (String);
static void on_xt_warning (String);


/* GLOBAL data */
char *prog_name;		/* The path name we are invoked with */
appdata app_data;		/* Options data */

/*  codes  */
int x_error_code;               /* set by X error handler */
int x_error_warnings;           /* print warnings or not  */

/* Top-level */
Widget toplevel;		/* main application widget */

XtAppContext app_context;

void
main (int argc, char **argv)
{
  /* Initialize variables */
  prog_name = argv[0];

  /* Handle some signals */
  signal (SIGHUP, on_signal);
  signal (SIGINT, on_signal);
  signal (SIGQUIT, on_signal);
  signal (SIGABRT, on_signal);
  signal (SIGBUS, on_signal);
  signal (SIGSEGV, on_signal);
  signal (SIGPIPE, on_signal);
  signal (SIGTERM, on_signal);

  /* Handle child exits */
  signal (SIGCHLD, on_sig_child);

  /* Handle X errors */
  XSetErrorHandler (on_x_error);
  XSetIOErrorHandler (on_x_io_error);

  /* Initialize X toolkit */
  toplevel = XtVaAppInitialize (&app_context,
				"Gimp",
				options, XtNumber (options),
				&argc, argv,
				fallbacks,
				NULL);

  /* Handle Xt errors */
  XtAppSetErrorHandler (app_context, on_xt_error);
  XtAppSetWarningHandler (app_context, on_xt_warning);

  /* Get application options */
  XtVaGetApplicationResources (
			       toplevel,
			       (XtPointer) & app_data,
			       resources,
			       XtNumber (resources),
			       NULL
			       );

  parse_gimprc ();  /*  parse the local GIMP configuration file  */
  brushes_init ();  /*  initialize the list of gimp brushes  */
  plug_in_init ();  /*  initialize the plug in structures  */
  file_io_init ();  /*  initialize the file types  */

  /* Create interface */
  create_interface (&app_data);

  argc--; argv++;
  if (argc > 0)
    while (argc--)
      file_open (*argv++, NULL);

  /* Main application loop */
  XtAppMainLoop (app_context);
}

static int caught_fatal_sig = 0;

static RETSIGTYPE
on_signal (sig_num)
     int sig_num;
{
  char *sig;
  
  if (caught_fatal_sig)
/*    raise (sig_num);*/
    kill (getpid (), sig_num);
  caught_fatal_sig = 1;

  switch (sig_num)
    {
    case SIGHUP:
      sig = "sighup";
      break;
    case SIGINT:
      sig = "sigint";
      break;
    case SIGQUIT:
      sig = "sigquit";
      break;
    case SIGABRT:
      sig = "sigabrt";
      break;
    case SIGBUS:
      sig = "sigbus";
      break;
    case SIGSEGV:
      sig = "sigsegv";
      break;
    case SIGPIPE:
      sig = "sigpipe";
      break;
    case SIGTERM:
      sig = "sigterm";
      break;
    default:
      sig = "unknown signal";
      break;
    }

  fatal_error ("%s caught", sig);
}

static RETSIGTYPE
on_sig_child (sig_num)
     int sig_num;
{
  int pid;
  int status;
  
  while (1)
    {
      pid = waitpid (WAIT_ANY, &status, WNOHANG);
      if (pid <= 0)
	break;
    }
}

static int
on_x_error (display, error)
     Display *display;
     XErrorEvent *error;
{
  char buf[64];

  if (x_error_warnings)
    {
      XGetErrorText (display, error->error_code, buf, 63);
      warning ("%s", buf);
    }
  x_error_code = -1;
  return 0;
}

static int
on_x_io_error (display)
     Display *display;
{
  fatal_error ("an io occurred with x");
  return 0;
}

static void
on_xt_error (str)
     String str;
{
  fatal_error ("%s", str);
}

static void
on_xt_warning (str)
     String str;
{
  warning ("%s", str);
}
