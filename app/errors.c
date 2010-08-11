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

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "core/core-types.h"

#include "core/gimp.h"

#include "errors.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif


/*  private variables  */

static gboolean             use_debug_handler = FALSE;
static GimpStackTraceMode   stack_trace_mode  = GIMP_STACK_TRACE_QUERY;
static gchar               *full_prog_name    = NULL;


/*  local function prototypes  */

static G_GNUC_NORETURN void  gimp_eek (const gchar *reason,
                                       const gchar *message,
                                       gboolean     use_handler);


/*  public functions  */

void
gimp_errors_init (const gchar        *_full_prog_name,
                  gboolean            _use_debug_handler,
                  GimpStackTraceMode  _stack_trace_mode)
{
  g_return_if_fail (_full_prog_name != NULL);
  g_return_if_fail (full_prog_name == NULL);

#ifdef GIMP_UNSTABLE
  g_printerr ("This is a development version of GIMP.  "
	      "Debug messages may appear here.\n\n");

#ifdef G_OS_WIN32
  g_printerr ("You can minimize this window, but don't close it.\n\n");
#endif
#endif /* GIMP_UNSTABLE */

  use_debug_handler = _use_debug_handler ? TRUE : FALSE;
  stack_trace_mode  = _stack_trace_mode;
  full_prog_name    = g_strdup (_full_prog_name);
}

void
gimp_message_log_func (const gchar    *log_domain,
		       GLogLevelFlags  flags,
		       const gchar    *message,
		       gpointer        data)
{
  Gimp **gimp = (Gimp **) data;

  if (gimp && GIMP_IS_GIMP (*gimp))
    {
      gimp_message (*gimp, NULL, message);
      return;
    }

  g_printerr ("%s: %s\n\n", gimp_filename_to_utf8 (full_prog_name), message);
}

void
gimp_error_log_func (const gchar    *domain,
		     GLogLevelFlags  flags,
		     const gchar    *message,
		     gpointer        data)
{
  gimp_fatal_error (message);
}

void
gimp_fatal_error (const gchar *fmt, ...)
{
  va_list  args;
  gchar   *message;

  va_start (args, fmt);
  message = g_strdup_vprintf (fmt, args);
  va_end (args);

  gimp_eek ("fatal error", message, TRUE);
}

void
gimp_terminate (const gchar *fmt, ...)
{
  va_list  args;
  gchar   *message;

  va_start (args, fmt);
  message = g_strdup_vprintf (fmt, args);
  va_end (args);

  gimp_eek ("terminated", message, use_debug_handler);
}


/*  private functions  */

static void
gimp_eek (const gchar *reason,
          const gchar *message,
          gboolean     use_handler)
{
#ifndef G_OS_WIN32

  g_printerr ("%s: %s: %s\n", gimp_filename_to_utf8 (full_prog_name),
              reason, message);

  if (use_handler)
    {
      switch (stack_trace_mode)
	{
	case GIMP_STACK_TRACE_NEVER:
	  break;

	case GIMP_STACK_TRACE_QUERY:
	  {
	    sigset_t sigset;

	    sigemptyset (&sigset);
	    sigprocmask (SIG_SETMASK, &sigset, NULL);
	    g_on_error_query (full_prog_name);
	  }
	  break;

	case GIMP_STACK_TRACE_ALWAYS:
	  {
	    sigset_t sigset;

	    sigemptyset (&sigset);
	    sigprocmask (SIG_SETMASK, &sigset, NULL);
	    g_on_error_stack_trace (full_prog_name);
	  }
	  break;

	default:
	  break;
	}
    }

#else

  /* g_on_error_* don't do anything reasonable on Win32. */

  MessageBox (NULL, g_strdup_printf ("%s: %s", reason, message),
              full_prog_name, MB_OK|MB_ICONERROR);

#endif /* ! G_OS_WIN32 */

  exit (EXIT_FAILURE);
}
