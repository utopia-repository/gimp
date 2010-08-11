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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "callbacks.h"
#include "errors.h"

#define INTERACTIVE 0
#define STACK_TRACE 1

static void debug (int);
static void debug_stop (char **);
static void stack_trace (char **);
static void stack_trace_sigchld (int);

extern char *prog_name;

void
message (char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  printf ("%s: ", prog_name);
  vprintf (fmt, args);
  printf ("\n");
  va_end (args);
}

void
warning (char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  printf ("%s warning: ", prog_name);
  vprintf (fmt, args);
  printf ("\n");
  va_end (args);
}

void
fatal_error (char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  printf ("%s fatal error: ", prog_name);
  vprintf (fmt, args);
  printf ("\n");
  va_end (args);

/*  debug (INTERACTIVE); */
  app_exit (1);
}

static void
debug (method)
     int method;
{
  pid_t pid;
  char buf[16];
  char *args[4] = { "gdb", NULL, NULL, NULL };
  int x;
  
  sprintf (buf, "%d", (int) getpid ());

  args[1] = prog_name;
  args[2] = buf;

  pid = fork ();
  if (pid == 0)
    {
      switch (method)
	{
	case INTERACTIVE:
	  fprintf (stderr, "debug_stop\n");
	  debug_stop (args);
	  break;
	case STACK_TRACE:
	  fprintf (stderr, "stack_trace\n");
	  stack_trace (args);
	  break;
	}

      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("could not fork");
      return;
    }

  x = 1;
  while (x)
    ;
}

static void
debug_stop (args)
     char **args;
{
  execvp (args[0], args);
  perror ("exec failed");
  _exit (0);
}

static int stack_trace_done;

static void
stack_trace (args)
     char **args;
{
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  fd_set fdset;
  fd_set readset;
  struct timeval tv;
  int sel, index, state;
  char buffer[256];
  char c;

  stack_trace_done = 0;
  signal (SIGCHLD, stack_trace_sigchld);
  
  if ((pipe (in_fd) == -1) || (pipe (out_fd) == -1))
    {
      perror ("could open pipe");
      _exit (0);
    }

  pid = fork ();
  if (pid == 0)
    {
      close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
      close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
      close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */
      
      execvp (args[0], args);      /* exec gdb */
      perror ("exec failed");
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("could not fork");
      _exit (0);
    }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);

  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);
  
  index = 0;
  state = 0;
  
  while (1)
    {
      readset = fdset;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
      if (sel == -1)
	break;
      
      if ((sel > 0) && (FD_ISSET (out_fd[0], &readset)))
	{
	  if (read (out_fd[0], &c, 1))
	    {
	      switch (state)
		{
		case 0:
		  if (c == '#')
		    {
		      state = 1;
		      index = 0;
		      buffer[index++] = c;
		    }
		  break;
		case 1:
		  buffer[index++] = c;
		  if ((c == '\n') || (c == '\r'))
		    {
		      buffer[index] = 0;
		      fprintf (stderr, "%s", buffer);
		      state = 0;
		      index = 0;
		    }
		  break;
		default:
		  break;
		}
	    }
	}
      else if (stack_trace_done)
	break;
    }
  
  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
}

static void
stack_trace_sigchld (signum)
     int signum;
{
  stack_trace_done = 1;
}
