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
#include "gdisplay.h"
#include "workprocs.h"
#include "cursorutil.h"
#include "tools.h"


Boolean
dither_work_proc (data)
     XtPointer data;
{
  GDisplay *gdisp;
  GDither *gdither;
  int j;
  short start;
  short amount;

  unsigned char *scalewalk;
  unsigned char *dither_from;
  unsigned char *src;

  gdisp = (GDisplay*) data;
  gdither = gdisp->gdither;

  if (!gdither)
      return True;

  if (timer_elapsed (gdisp->main_timer) < ((float) gdither->delay / 1e3))
    return False;

  timer_start (gdisp->line_timer);
  amount = 0;
  start = gdither->scanline;

  do {
    scalewalk = gdither->scale_data;
    dither_from = gdither->dither_from;
    src = gdither->src;

    j = gdither->width;
    while (j--)
      {
	*dither_from++ = *src;
	*dither_from++ = *(src+1);
	*dither_from++ = *(src+2);
	src += *scalewalk++;
      }

    amount++;

    (* gdither->engine) (gdither);

    if ( ! ((gdither->scanline + gdither->y + gdisp->offset_y) % SCALEDEST (gdisp)) )
      gdither->src += gdither->src_width * SCALESRC (gdisp);

  } while (!gdither->done && (timer_elapsed (gdisp->line_timer) < 0.2));

  gdisplay_put_image (gdisp, gdither->x, start + gdither->y, gdither->width, amount);

  if (gdisp->gdither->done)
    {
      /*  set new status  */
      gdisp->gdither->running = 0;

      return True;
    }
  else
      return False;
}
