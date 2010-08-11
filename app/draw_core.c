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
#include "appenv.h"
#include "draw_core.h"


DrawCore *
draw_core_new (draw_func)
     DrawCoreDraw draw_func;
{
  DrawCore * core;

  core = (DrawCore *) xmalloc (sizeof (DrawCore));

  core->draw_func    = draw_func;
  core->draw_state   = INVISIBLE;
  core->gc           = NULL;
  core->paused_count = 0;
  core->data         = NULL;
  core->line_width   = 1;

  return core;
}


void
draw_core_start (core, win, tool)
     DrawCore * core;
     Window win;
     Tool * tool;
{
  XGCValues gcv;

  if (core->draw_state != INVISIBLE)
    draw_core_stop (core, tool);

  core->win   = win;
  core->data  = (void *) tool;
  core->paused_count = 0;  /*  reset pause counter to 0  */

  /*  create a new graphics context  */
  if (core->gc)
    XFreeGC (DISPLAY, core->gc);

  gcv.function = GXinvert;
  gcv.foreground = 0xFFFFFFFF;
  gcv.background = 0x00000000;
  core->gc = XCreateGC (DISPLAY, core->win, GCFunction | GCForeground | GCBackground, &gcv);
  XSetLineAttributes (DISPLAY, core->gc, core->line_width, LineSolid, CapButt, JoinBevel);

  (* core->draw_func) (tool);

  core->draw_state = VISIBLE;
}


void
draw_core_stop (core, tool)
     DrawCore * core;
     Tool * tool;
{
  if (core->draw_state == INVISIBLE)
    return;

  (* core->draw_func) (tool);

  core->draw_state = INVISIBLE;
}


void
draw_core_resume (core, tool)
     DrawCore * core;
     Tool * tool;
{
  core->paused_count = (core->paused_count > 0) ? core->paused_count - 1 : 0;
  if (core->paused_count == 0)
    {
      core->draw_state = VISIBLE;
      (* core->draw_func) (tool);
    }
}


void
draw_core_pause (core, tool)
     DrawCore * core;
     Tool * tool;
{
  if (core->paused_count == 0)
    {
      core->draw_state = INVISIBLE;
      (* core->draw_func) (tool);
    }
  core->paused_count++;
}


void
draw_core_free (core)
     DrawCore * core;
{
  if (core)
    {
      if (core->gc)
	XFreeGC (DISPLAY, core->gc);
      xfree (core);
    }
}


