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
#ifndef __GXIMAGE_H__
#define __GXIMAGE_H__

#include <X11/extensions/XShm.h>

#define		MIT_SHM		0
#define		STANDARD	1

typedef struct _GXImage  GXImage;
typedef struct _MIT_SHM_Data  MIT_SHM_Data;
typedef struct _Standard_Data Standard_Data;

struct _MIT_SHM_Data
{
  XShmSegmentInfo *x_shm_info;  /*  Shared memory information for X */

  XImage *ximage;		/*  XImage pointer  */

};

struct _Standard_Data
{
  XImage *ximage;               /*  XImage pointer  */
};



struct _GXImage
{
  Widget shell;			/*  Shell that holds the canvas             */
  Widget canvas;		/*  canvas for image output                 */

  long width, height;		/*  width and height of ximage structure    */
  int  bits_per_pixel;          /*  Bits per pixel (ZPixmap format)         */
  long bytes_per_line;		/*  bytes per line of the ximage structure  */
  
  GC gc;			/*  graphics context                        */
  unsigned int depth;		/*  depth of our drawables                  */
  Visual *visual;		/*  visual appropriate to our depth         */

  unsigned char * data;		/*  actual ximage data buffer  */

  int type;			/*  Is this MIT_SHM or STANDARD  */

  void * private;		/*  private data  */

};


GXImage *	create_image (Widget, Widget, long, long, Visual *, int);
void		delete_image (GXImage *);
void		resize_image (GXImage *, long, long);
void		put_image    (GXImage *, long, long, long, long);


#endif  /*  __GXIMAGE_H__  */





