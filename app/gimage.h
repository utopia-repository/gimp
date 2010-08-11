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
#ifndef __GIMAGE_H__
#define __GIMAGE_H__

#include "linked.h"

/* the image types */
#define RGB_GIMAGE       0
#define GREY_GIMAGE      1
#define INDEXED_GIMAGE   2

#define COLORMAP_SIZE    768

/* structure declarations */

typedef struct _GImage GImage;

struct _GImage
  {
    char *filename;		        /*  original filename            */
    int has_filename;                   /*  has a valid filename         */

    long width, height;		        /*  width and height attributes  */

    int type;                           /*  RGB color, grey, or indexed  */
    int bpp;                            /*  bytes per pixel              */

    unsigned char * cmap;               /*  colormap--for indexed        */
    int num_cols;                       /*  number of cols--for indexed  */

    unsigned char *raw_image;	        /*  raw r,g,b values             */
    unsigned char *indexed_raw_image;   /*  indexed raw image            */
    unsigned char *shadow_buf;          /*  shadow buffer                */

    int dirty;                          /*  dirty flag -- # of ops       */

    int instance_count;                 /*  number of instances          */
    int ref_count;                      /*  number of references         */

    int shmid;                          /*  Shared memory ID             */
    void * shmaddr;                     /*  Shared memory address        */

    int shadow_shmid;                   /*  Shm ID of shadow buf         */
    void * shadow_shmaddr;              /*  Shm addr of shadow buf       */

    int ID;                             /*  Unique gimage identifier     */

    link_ptr plug_ins;                  /*  a list of plug-ins to be run */
    short busy;                         /*  is this image in use         */
  };


/* function declarations */

GImage *        gimage_new (long, long, int, int);
GImage *        gimage_copy (GImage *);
GImage *        gimage_dup (GImage *);
GImage *        gimage_get_named (char *);
GImage *        gimage_get_ID (int);
int             gimage_get_shadow_ID (GImage *);
unsigned char * gimage_get_shadow_addr (GImage *);
void            gimage_allocate (GImage *, int);
void            gimage_allocate_indexed (GImage *, int);
void            gimage_free_shadow  (GImage *);
void            gimage_delete (GImage *);
void            gimage_add_plug_in (GImage *, void *);
void            gimage_fill (GImage *, int, int, int);

#endif /* __GIMAGE_H__ */

