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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "appenv.h"
#include "gximage.h"
#include "visual.h"
#include "errors.h"


/*  The X error code (from main.c)  */
extern int x_error_code;
extern int x_error_warnings;

#define QUANTUM   32


/*  STATIC functions  */

static void
delete_image_shared (data)
     MIT_SHM_Data *data;
{
  XShmDetach (DISPLAY, data->x_shm_info);
  XDestroyImage (data->ximage);
  shmdt (data->x_shm_info->shmaddr);
  shmctl (data->x_shm_info->shmid, IPC_RMID, 0);
  xfree (data->x_shm_info);
  xfree (data);
}


static void
delete_image_standard (data)
     Standard_Data *data;
{
  XDestroyImage (data->ximage);
  xfree (data);
}


static void *
create_image_shared (gximage, width, height)
     GXImage *gximage;
     long width, height;
{
  MIT_SHM_Data *private;

  private = (MIT_SHM_Data *) xmalloc (sizeof (MIT_SHM_Data));

  private->x_shm_info = (XShmSegmentInfo *) xmalloc (sizeof (XShmSegmentInfo));

  private->ximage = XShmCreateImage (DISPLAY, gximage->visual, gximage->depth,
				     ZPixmap, NULL, private->x_shm_info,
				     (unsigned int) width, (unsigned int) height);

  private->x_shm_info->shmid = shmget (IPC_PRIVATE,
				       private->ximage->bytes_per_line*
				       private->ximage->height,
				       IPC_CREAT|0777);

  if (private->x_shm_info->shmid < 0)
    fatal_error ("shmget failed!");

  private->x_shm_info->shmaddr =
    private->ximage->data = shmat (private->x_shm_info->shmid, 0, 0);

  private->x_shm_info->readOnly = False;

  if (private->x_shm_info->shmaddr < (char*) 0)
    fatal_error ("shmat failed!");

  /*  If the display is remote, then a shared memory attach
   *  operation will fail.  If the X11 error handler is called,
   *  then we'll give up on making this XImage shared...
   *  Since we're testing for a problem, turn off warnings...
   */
  x_error_code = 0;
  x_error_warnings = False;
  XShmAttach (DISPLAY, private->x_shm_info);
  XSync (DISPLAY, False);
  x_error_warnings = True;   /*  turn warnings back on  */
  if (x_error_code == -1)
    {
      XDestroyImage (private->ximage);
      shmdt (private->x_shm_info->shmaddr);
      shmctl (private->x_shm_info->shmid, IPC_RMID, 0);
      xfree (private->x_shm_info);
      xfree (private);
      
      return NULL;
    }

  /*  set standard values in the GXImage structure  */

  gximage->width = private->ximage->width;
  gximage->height = private->ximage->height;
  gximage->bytes_per_line = private->ximage->bytes_per_line;
  gximage->bits_per_pixel = private->ximage->bits_per_pixel;
  gximage->data = (unsigned char*) private->ximage->data;

  return (void *) private;
}


static void *
create_image_standard (gximage, width, height)
     GXImage * gximage;
     long width;
     long height;
{
  Standard_Data * private;
  int bytes_per_line;
  unsigned char * data;
  int bpp;

  bpp = (gximage->depth == 8) ? 1 : 4;

  private = (Standard_Data *) xmalloc (sizeof (Standard_Data));

  bytes_per_line = QUANTUM * ((width*bpp + QUANTUM - 1) / QUANTUM);

  data = (unsigned char *) xmalloc (sizeof (unsigned char) * bytes_per_line * height);

  private->ximage = XCreateImage (DISPLAY, gximage->visual, gximage->depth,
				  ZPixmap, 0, (char*) data, width, height, QUANTUM,
				  bytes_per_line);


  /*  set standard values in the GXImage structure  */

  gximage->width = private->ximage->width;
  gximage->height = private->ximage->height;
  gximage->bytes_per_line = private->ximage->bytes_per_line;
  gximage->bits_per_pixel = private->ximage->bits_per_pixel;
  gximage->data = (unsigned char*) private->ximage->data;

  return (void *) private;
  
}


static void
put_image_shared (gximage, x, y, w, h)
     GXImage *gximage;
     long x, y;
     long w, h;
{
  MIT_SHM_Data *private;

  private = (MIT_SHM_Data *) gximage->private;

  XShmPutImage (DISPLAY, XtWindow (gximage->canvas),
		gximage->gc, private->ximage, x, y, 
		x, y,
		(unsigned int) w,
		(unsigned int) h,
		False);

}


static void
put_image_standard (gximage, x, y, w, h)
     GXImage *gximage;
     long x, y;
     long w, h;
{
   Standard_Data * private;

   private = (Standard_Data *) gximage->private;

   XPutImage(DISPLAY, XtWindow(gximage->canvas),
	     gximage->gc, private->ximage, x, y,
	     x, y,
	     (unsigned int) w,
	     (unsigned int) h);

}

static GXImage *
create_gximage (shell, canvas, visual, depth)
     Widget shell;
     Widget canvas;
     Visual *visual;
     int depth;
{
  GXImage * gximage;
  XGCValues gcv;
  int major, minor;
  Bool pixmaps;
  Status status;

  gximage = (GXImage *) xmalloc (sizeof (GXImage));

  gximage->shell = shell;
  gximage->canvas = canvas;

  /*  create a new graphics context  */
  gcv.foreground = BlackPixelOfScreen (XtScreen (canvas));
  gximage->gc = XCreateGC (DISPLAY,
			   XtWindow (shell),
			   GCForeground, &gcv);

  gximage->depth = depth;

  gximage->visual = visual;

  /*  If the shared memory extension is available and requested...  */
  status = (XShmQueryVersion (DISPLAY, &major, &minor, &pixmaps) &&
	    app_data.mit_shm);

  switch (status)
    {
    case True:
      gximage->type = MIT_SHM;
      break;
    case False:
      gximage->type = STANDARD;
      break;
    }

  return gximage;

}



/****************************************************************/


/*  Function definitions  */

void
delete_image (gximage)
     GXImage *gximage;
{
  switch (gximage->type)
    {
    case MIT_SHM:
      delete_image_shared ((MIT_SHM_Data *) gximage->private);
      break;
    case STANDARD:
      delete_image_standard ((Standard_Data *) gximage->private);
      break;
    }
  
  XFreeGC (XtDisplay (gximage->canvas), gximage->gc);
  xfree (gximage);

}


GXImage *
create_image (shell, canvas, width, height, visual, depth)
     Widget shell;
     Widget canvas;
     long width, height;
     Visual *visual;
     int depth;
{
  GXImage *gximage;

  gximage = create_gximage (shell, canvas, visual, depth);

  switch (gximage->type)
    {
    case MIT_SHM:
      gximage->private = create_image_shared (gximage, width, height);

      /*  If the shared memory image wasn't created, try a normal one  */
      if (!gximage->private)
	{
	  gximage->private = create_image_standard (gximage, width, height);
	  gximage->type = STANDARD;
	}

      break;
    case STANDARD:
      gximage->private = create_image_standard (gximage, width, height);
      break;
    }

  return gximage;
}


void
resize_image (gximage, width, height)
     GXImage *gximage;
     long width;
     long height;
{
  /* Added this to insure no operations involving images remain on the
     X message queue.  */
  XSync (DISPLAY, False);

  /* delete old XImage */
  switch (gximage->type)
    {
    case MIT_SHM:
      delete_image_shared ((MIT_SHM_Data *) gximage->private);
      break;
    case STANDARD:
      delete_image_standard ((Standard_Data *) gximage->private);
      break;
    }

  /* create new XImage */
  switch (gximage->type)
    {
    case MIT_SHM:
      gximage->private = create_image_shared (gximage, width, height);
      break;
    case STANDARD:
      gximage->private = create_image_standard (gximage, width, height);
      break;
    }
  
}


void
put_image (gximage, x, y, w, h)
     GXImage *gximage;
     long x, y;
     long w, h;
{

  switch (gximage->type)
    {
    case MIT_SHM:
      put_image_shared (gximage, x, y, w, h);
      break;
    case STANDARD:
      put_image_standard (gximage, x, y, w, h);
      break;
    }

}

