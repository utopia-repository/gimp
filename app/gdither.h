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
#ifndef __GDITHER_H__
#define __GDITHER_H__

#include "gimage.h"
#include "timer.h"

#define REDMASK    0x00000E0
#define GREENMASK  0x00000E0
#define BLUEMASK   0x00000C0

#define REDMASK2   0xE0
#define GREENMASK2 0xE0
#define BLUEMASK2  0xC0

#define FIRSTBYTE(l)   (l & 0x000000FF)
#define SECONDBYTE(l)  ((l & 0x0000FF00)>>8)
#define THIRDBYTE(l)   ((l & 0x00FF0000)>>16)
#define SECONDWORD(l)  (l >> 16)

#define ORDERED          0
#define FLOYD_STEINBERG  1



/*
 *  Structures...
 *
 */

typedef struct _GDither GDither;
typedef void (*GDither_Func) (GDither *);
typedef void *GDither_Data;

struct _GDither
  {
    short done;                 /*  is dithering complete                               */
    short interrupted;          /*  flag for an interrupted dithering process           */
    short running;              /*  flag determining current status of process          */
    long scanline;		/*  current scanline of algorithm                       */
    long x, y;                  /*  x and y positions to start dithering at             */
    long width, height;         /*  width and height of portion of image to dither      */

    long delay;                 /*  milliseconds before dithering process starts        */

    GImage *gimage;             /*  the image to be dithered                            */
    unsigned char *dest;
    long dest_width;
    unsigned char *src;
    long src_width;

    unsigned char *scale_data;  /*  array for use in accelerated scaling                */
    unsigned char *dither_from; /*  dither this array                                   */

    GDither_Func engine;	/*  algorithm that actually does the work               */
    GDither_Data data;		/*  private dithering data specific to the dither_func  */
  };


/*

 *  Function declarations...
 *
 */

void dither_start_engine ();
void dither_shutdown_engine ();

GDither * dither_new ();
void dither_free (GDither *);

void dither_segment_floyd_steinberg (unsigned char *, unsigned char *, int, int, int);
void dither_segment_ordered (unsigned char *, unsigned char *, int, int, int);

void dither_floyd_steinberg (GDither *);
void dither_ordered (GDither *);
void dither_none (GDither *);


#endif /* __GDITHER_H__ */


