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
#ifndef __TIMER_H__
#define __TIMER_H__

#ifdef __TIMER_TYPES__

#include <sys/time.h>

typedef struct _TIMER *TIMER;
struct _TIMER
  {
    struct timeval start;
    struct timeval end;
    short activated;
  };

#else

typedef void *TIMER;

#endif /* __TIMER_TYPES__ */

TIMER make_timer (void);
void free_timer (TIMER);

void timer_start (TIMER);
void timer_stop (TIMER);
void timer_reset (TIMER);
float timer_elapsed (TIMER);

#endif /* __TIMER_H__ */
