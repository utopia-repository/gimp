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

#include "errors.h"
#define __TIMER_TYPES__
#include "timer.h"

TIMER
make_timer ()
{
  TIMER t;
  struct timezone tz;

  t = (TIMER) malloc (sizeof (struct _TIMER));
  if (!t)
    {
      warning ("unable to allocate memory");
      return NULL;
    }

  gettimeofday (&t->start, &tz);
  t->activated = 1;

  return t;
}

void
free_timer (t)
     TIMER t;
{
  if (t)
    free (t);
}

void
timer_start (t)
     TIMER t;
{
  struct timezone tz;

  gettimeofday (&t->start, &tz);
  t->activated = 1;
}

void
timer_stop (t)
     TIMER t;
{
  struct timezone tz;

  gettimeofday (&t->end, &tz);
  t->activated = 0;
}

void
timer_reset (t)
     TIMER t;
{
  struct timezone tz;

  gettimeofday (&t->start, &tz);
  t->activated = 0;
}

float
timer_elapsed (t)
     TIMER t;
{
  struct timeval lapsed;
  struct timezone tz;
  float total;

  if (t->activated)
    gettimeofday (&t->end, &tz);

  if (t->start.tv_usec > t->end.tv_usec)
    {
      t->end.tv_usec += 1000000;
      t->end.tv_sec--;
    }
  lapsed.tv_usec = t->end.tv_usec - t->start.tv_usec;
  lapsed.tv_sec = t->end.tv_sec - t->start.tv_sec;

  total = lapsed.tv_sec + ((float) lapsed.tv_usec / 1e6);
  return total;
}
