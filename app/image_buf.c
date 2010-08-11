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
#include <X11/Xlib.h>
#include "appenv.h"
#include "errors.h"
#include "gconvert.h"
#include "gdither.h"
#include "image_buf.h"
#include "visual.h"

/*  The X error code (from main.c)  */
extern int x_error_code;
extern int x_error_warnings;

typedef struct _ImageBufStd _ImageBufStd;
typedef struct _ImageBufShm _ImageBufShm;

struct _ImageBufStd {
  int type;
  int color;
  XImage *ximage;
};

struct _ImageBufShm {
  int type;
  int color;
  XImage *ximage;
  XShmSegmentInfo *x_shm_info;
};

static ImageBuf image_buf_create_ximage (int, int, int, int);
static void image_buf_draw_row_8 (ImageBuf, unsigned char *, int, int, int);
static void image_buf_draw_row_16 (ImageBuf, unsigned char *, int, int, int);
static void image_buf_draw_row_24 (ImageBuf, unsigned char *, int, int, int);


ImageBuf
image_buf_create_ximage (type, color, width, height)
     int type, color, width, height;
{
  _ImageBufStd *std_image;
  _ImageBufShm *shm_image;
  Visual * visual;
  int major, minor;
  Bool pixmaps;
  int depth;
  
  if (color == COLOR_BUF)
    {
      visual = color_visual;
      depth = color_depth;
    }
  else if (color == GREY_BUF)
    {
      visual = grey_visual;
      depth = grey_depth;
    }

  if (type == -1)
    type = (XShmQueryVersion (DISPLAY, &major, &minor, &pixmaps) && 
	    app_data.mit_shm);

  switch (type)
    {
    case 0:
      std_image = xmalloc (sizeof (_ImageBufStd));

      std_image->type = type;      
      std_image->color = color;
      std_image->ximage = XCreateImage (DISPLAY, visual, depth,
					ZPixmap, 0, 0, width, height, 32,
					0);
	

      std_image->ximage->data = xmalloc (std_image->ximage->bytes_per_line * std_image->ximage->height);

      return std_image;
      break;
    case 1:
      shm_image = xmalloc (sizeof (_ImageBufShm));

      shm_image->type = type;
      shm_image->color = color;
      shm_image->x_shm_info = xmalloc (sizeof (XShmSegmentInfo));

      shm_image->ximage = XShmCreateImage (DISPLAY, visual, depth,
					   ZPixmap, NULL, shm_image->x_shm_info,
					   (unsigned int) width, (unsigned int) height);
      
      shm_image->x_shm_info->shmid = shmget (IPC_PRIVATE,
					     shm_image->ximage->bytes_per_line * shm_image->ximage->height,
					     IPC_CREAT | 0777);
      if (shm_image->x_shm_info->shmid < 0)
	fatal_error ("shmget failed!");

      shm_image->x_shm_info->shmaddr = shm_image->ximage->data = shmat (shm_image->x_shm_info->shmid, 0, 0);
      shm_image->x_shm_info->readOnly = False;

      if (shm_image->x_shm_info->shmaddr < (char*) 0)
	fatal_error ("shmat failed!");

      /*  If the display is remote, then a shared memory attach
       *  operation will fail.  If the X11 error handler is called,
       *  then we'll give up on making this XImage shared...
       *  Since we're testing for a problem, turn off warnings...
       */
      x_error_code = 0;
      x_error_warnings = False;
      XShmAttach (DISPLAY, shm_image->x_shm_info);
      XSync (DISPLAY, False);
      x_error_warnings = True;   /*  turn warnings back on  */
      if (x_error_code == -1)
	{
	  XDestroyImage (shm_image->ximage);
	  shmdt (shm_image->x_shm_info->shmaddr);
	  shmctl (shm_image->x_shm_info->shmid, IPC_RMID, 0);
	  xfree (shm_image->x_shm_info);
	  xfree (shm_image);
      
	  return NULL;
	}

      return shm_image;
      break;
    }

  return NULL;
}


ImageBuf 
image_buf_create (type, color, width, height)
     int type, color, width, height;
{
  ImageBuf buf;

  buf = image_buf_create_ximage (type, color, width, height);

  /*  If a shared XImage was requested, but not created, try
   *  a standard XImage
   */
  if (type == 1 && !buf)
    buf = image_buf_create_ximage (0, color, width, height);

  return buf;
}

void 
image_buf_destroy (image)
     ImageBuf image;
{
  _ImageBufStd *std_image;
  _ImageBufShm *shm_image;

  std_image = image;
  shm_image = image;

  switch (std_image->type)
    {
    case 0:
      XDestroyImage (std_image->ximage);
      xfree (std_image);
      break;
    case 1:
      XShmDetach (DISPLAY, shm_image->x_shm_info);
      XDestroyImage (shm_image->ximage);
      shmdt (shm_image->x_shm_info->shmaddr);
      shmctl (shm_image->x_shm_info->shmid, IPC_RMID, 0);
      xfree (shm_image->x_shm_info);
      xfree (shm_image);
      break;
    }
}

void 
image_buf_put (image, drawable, gc)
     ImageBuf image;
     Drawable drawable;
     GC gc;
{
  image_buf_put_area (image, drawable, gc, 0, 0,
		      image_buf_width (image), 
		      image_buf_height (image));
}

void
image_buf_put_area (image, drawable, gc, x, y, w, h)
     ImageBuf image;
     Drawable drawable;
     GC gc;
     int x, y, w, h;
{
  _ImageBufStd *std_image;
  _ImageBufShm *shm_image;

  std_image = image;
  shm_image = image;

  switch (std_image->type)
    {
    case 0:
      XPutImage (DISPLAY, drawable, gc, std_image->ximage, x, y, x, y, w, h);
      break;
    case 1:
      XShmPutImage (DISPLAY, drawable, gc, shm_image->ximage, x, y, x, y, w, h, True);
      break;
    }
}

void 
image_buf_draw_row (image, row, x, y, w)
     ImageBuf image;
     unsigned char *row;
     int x, y;
     int w;
{
  if (image)
    {
       switch (image_buf_depth (image))
	 {
	 case 8:
	   image_buf_draw_row_8 (image, row, x, y, w);
	   break;
	 case 12:
	 case 15:
	 case 16:
	   image_buf_draw_row_16 (image, row, x, y, w);
	   break;
	 case 24:
	 case 32:
	   image_buf_draw_row_24 (image, row, x, y, w);
	   break;
	 default:
	   fatal_error ("unable to handle %d bit in color selections", image_buf_depth (image));
	   break;
	 }
    }
}

void * 
image_buf_data (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->ximage->data;
}

int 
image_buf_width (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->ximage->width;
}

int 
image_buf_height (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->ximage->height;
}

int 
image_buf_depth (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->ximage->depth;
}

int 
image_buf_color (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->color;
}

int
image_buf_bytes (image)
     ImageBuf image;
{
  return (((_ImageBufStd*) image)->ximage->bits_per_pixel >> 3);
  
}

int 
image_buf_row_bytes (image)
     ImageBuf image;
{
  return ((_ImageBufStd*) image)->ximage->bytes_per_line;
}

static void 
image_buf_draw_row_8 (image, row, x, y, w)
     ImageBuf image;
     unsigned char *row;
     int x, y;
     int w;
{
  unsigned char *src;
  unsigned char *dest;

  src = row;
  dest = image_buf_data (image);
  dest += image_buf_row_bytes (image) * y + x;

  switch (image_buf_color (image))
    {
    case COLOR_BUF :
      dither_segment_ordered (src, dest, 0, y, w);
      break;
    case GREY_BUF :
      greyscale_8 (src, dest, w);
      break;
    }
}

static void 
image_buf_draw_row_16 (image, row, x, y, w)
     ImageBuf image;
     unsigned char *row;
     int x, y;
     int w;
{
  unsigned char *src;
  unsigned short *dest;
  int i, r, g, b;
  
  src = row;
  dest = image_buf_data (image);
  dest += ((image_buf_row_bytes (image) * y) >> 1) + x;

  switch (image_buf_color (image))
    {
    case COLOR_BUF :
      for (i = 0; i < w; i++)
	{
	  r = *src++;
	  g = *src++;
	  b = *src++;
      
	  *dest++ = COLOR_COMPOSE (r, g, b);
	}
      break;
    case GREY_BUF :
      greyscale_16 (src, dest, w);
      break;
    }
}

static void 
image_buf_draw_row_24 (image, row, x, y, w)
     ImageBuf image;
     unsigned char *row;
     int x, y;
     int w;
{
  unsigned char *src;
  unsigned long *dest;
  int i, r, g, b;
  
  src = row;
  dest = image_buf_data (image);
  dest += ((image_buf_row_bytes (image) * y) >> 2) + x;

  switch (image_buf_color (image))
    {
    case COLOR_BUF :
      for (i = 0; i < w; i++)
	{
	  r = *src++;
	  g = *src++;
	  b = *src++;
      
	  *dest++ = COLOR_COMPOSE (r, g, b);
	}
      break;
    case GREY_BUF :
      greyscale_24 (src, dest, w);
      break;
    }
}

