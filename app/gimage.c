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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include "appenv.h"
#include "general.h"
#include "gimage.h"
#include "widget.h"
#include "errors.h"
#include "plug_in.h"
#include "undo.h"

static void gimage_plug_in_callback (void *, void *);

/*
 *  Static variables
 */
static int gimage_ID = 1;
static char untitled_name[] = "Untitled";
link_ptr image_list = NULL;


/* static functions */

static void
gimage_allocate_shadow (gimage)
     GImage * gimage;
{
  long size;

  size = gimage->width * gimage->height * gimage->bpp;

  gimage->shadow_shmid = shmget (IPC_PRIVATE,
				 size,
				 IPC_CREAT|0777);
  
  if (gimage->shadow_shmid < 0)
    {
      perror ("shadow shmget failed");
      fatal_error ("shadow shmget failed!");
    }

  gimage->shadow_shmaddr = gimage->shadow_buf = 
    (unsigned char *) shmat (gimage->shadow_shmid, 0, 0);

  if (gimage->shadow_shmaddr < (void*) 0)
    fatal_error ("shadow shmat failed!");
}


/* function definitions */

static GImage *
gimage_create ()
{
  GImage *gimage;

  gimage = (GImage *) xmalloc (sizeof (GImage));

  gimage->has_filename = 0;
  gimage->num_cols = 0;
  gimage->cmap = NULL;
  gimage->raw_image = NULL;
  gimage->indexed_raw_image = NULL;
  gimage->ID = gimage_ID++;
  gimage->ref_count = 0;
  gimage->instance_count = 0;
  gimage->shadow_shmaddr = (void *) -1;
  gimage->shadow_shmid = 0;
  gimage->plug_ins = NULL;
  gimage->busy = False;
  gimage->dirty = 1;

  image_list = append_to_list (image_list, (void *) gimage);

  return gimage;
}


GImage *
gimage_new (width, height, type, bpp)
     long width, height;
     int type;
     int bpp;
{
  GImage *gimage;

  gimage = gimage_create ();

  gimage->filename = xstrdup (untitled_name);
  gimage->width = width;
  gimage->height = height;
  gimage->type = type;
  gimage->bpp = bpp;

  gimage_allocate (gimage, bpp * width * height);

  switch (gimage->type)
    {
    case RGB_GIMAGE : 
    case GREY_GIMAGE :
      break;
    case INDEXED_GIMAGE :
      /* always allocate 256 colors for the colormap */
      gimage->num_cols = 0;
      gimage->cmap = xmalloc (sizeof (unsigned char) * COLORMAP_SIZE);
      memset (gimage->cmap, 0, COLORMAP_SIZE);
      break;
    default :
      warning ("GImage type not supported.\n");
      break;
    }
  
  return gimage;
}


GImage *
gimage_copy (gimage)
     GImage *gimage;
{
  GImage *new;
  
  if (gimage)
    {    
      new = gimage_create ();
      
      new->type = gimage->type;
      new->bpp = gimage->bpp;
      
      new->filename = xstrdup (untitled_name);
      new->has_filename = False;
      
      new->width = gimage->width;
      new->height = gimage->height;
      
      return new;
    }
  else
    return NULL;
}


GImage *
gimage_dup (gimage)
     GImage *gimage;
{
  GImage *new;

  if (gimage)
    {
      new = gimage_new (gimage->width, gimage->height, gimage->type, gimage->bpp);
      new->filename = xstrdup (untitled_name);
      memcpy (new->shmaddr, gimage->shmaddr, new->width * new->height * new->bpp);

      /*  If the image type is indexed, and there is a colormap table, copy it  */
      if (new->cmap && (new->type == INDEXED_GIMAGE))
	{
	  memcpy (new->cmap, gimage->cmap, COLORMAP_SIZE);
	  new->num_cols = gimage->num_cols;
	}

      return new;
    }
  else
    return NULL;
}


GImage *
gimage_get_named (name)
     char *name;
{
  link_ptr tmp = image_list;
  GImage *gimage;
  char *str;

  while (tmp)
    {
      gimage = tmp->data;
      str = prune_filename (gimage->filename);
      if (strcmp (str, name) == 0)
	return gimage;
      
      tmp = next_item (tmp);
    }

  return NULL;
}


GImage *
gimage_get_ID (ID)
     int ID;
{
  link_ptr tmp = image_list;
  GImage *gimage;

  while (tmp)
    {
      gimage = (GImage *) tmp->data;
      if (gimage->ID == ID)
	return gimage;
      
      tmp = next_item (tmp);
    }

  return NULL;
}


int
gimage_get_shadow_ID (gimage)
     GImage * gimage;
{
  if ((gimage->shadow_shmaddr < (void*) 0) || (gimage->shadow_shmid == 0))
    gimage_allocate_shadow (gimage);

  return gimage->shadow_shmid;
}


unsigned char *
gimage_get_shadow_addr (gimage)
     GImage * gimage;
{
  if ((gimage->shadow_shmaddr < (void*) 0) || (gimage->shadow_shmid == 0))
    gimage_allocate_shadow (gimage);

  return gimage->shadow_buf;
}


void
gimage_allocate (gimage, size)
     GImage *gimage;
     int size;
{
  gimage->shmid = shmget (IPC_PRIVATE,
			  size,
			  IPC_CREAT|0777);
  
  if (gimage->shmid < 0)
    {
      perror ("shmget failed");
      fatal_error ("shmget failed!");
    }

  gimage->shmaddr = gimage->raw_image = 
    (unsigned char *) shmat (gimage->shmid, 0, 0);

  if (gimage->shmaddr < (void*) 0)
    fatal_error ("shmat failed!");
}


void
gimage_allocate_indexed (gimage, size)
     GImage *gimage;
     int size;
{
  if (!gimage->indexed_raw_image)
    gimage->indexed_raw_image = (unsigned char *) xmalloc(sizeof(unsigned char) * size);
}


void
gimage_free_shadow (gimage)
     GImage * gimage;
{
  /*  Free the shadow buffer from the specified gimage if it exists  */
  
  if (gimage->shadow_shmaddr >= (void*) 0)
    {
      shmdt (gimage->shadow_shmaddr);
      shmctl (gimage->shadow_shmid, IPC_RMID, 0);
    }

  gimage->shadow_shmaddr = (void *) -1;
  gimage->shadow_shmid = 0;
}


void
gimage_delete (gimage)
     GImage *gimage;
{
  gimage->ref_count--;

  if (!gimage->ref_count)
    {
      /*  remove this image from the global list  */
      image_list = remove_from_list (image_list, (void *) gimage);

      if (gimage->shmaddr >= (void*) 0)
	{
	  shmdt (gimage->shmaddr);
	  shmctl (gimage->shmid, IPC_RMID, 0);
	}

      if (gimage->indexed_raw_image)
	xfree (gimage->indexed_raw_image);

      if (gimage->cmap)
	xfree (gimage->cmap);

      gimage_free_shadow (gimage);
      
      xfree (gimage->filename);
      
      if (!gimage->busy)
	xfree (gimage);
    }
}

void
gimage_add_plug_in (gimage, data)
     GImage *gimage;
     void *data;
{
  gimage->plug_ins = append_to_list (gimage->plug_ins, data);
  
  if (!gimage->busy)
    gimage_plug_in_callback (gimage, 0);
}

void
gimage_plug_in_callback (client_data, call_data)
     void *client_data;
     void *call_data;
{
  GImage *gimage;
  link_ptr tmp_link;
  PlugInP plug_in;

  gimage = client_data;

  if (((int) call_data == 2) || (gimage->ref_count == 0))
    {
      tmp_link = gimage->plug_ins;
      while (tmp_link)
	{
	  free_plug_in (tmp_link->data);
	  tmp_link = tmp_link->next;
	}

      gimage->plug_ins = free_list (gimage->plug_ins);

      if (gimage->ref_count == 0)
	xfree (gimage);
    }

  if (gimage->plug_ins)
    {
      tmp_link = gimage->plug_ins;
      gimage->plug_ins = gimage->plug_ins->next;
      
      plug_in = tmp_link->data;
      tmp_link->next = NULL;
      free_list (tmp_link);

      plug_in_set_callback (plug_in, gimage_plug_in_callback, gimage);
      if (plug_in_open (plug_in))
	gimage->busy = 1;
      else
	gimage_plug_in_callback (client_data, call_data);
    }
  else
    gimage->busy = 0;
}

void
gimage_fill (gimage, r, g, b)
     GImage *gimage;
     int r, g, b;
{
  unsigned char *p;
  unsigned char c[3];
  int i, pixels;

  switch (gimage->type)
    {
    case RGB_GIMAGE:
      c[0] = r;
      c[1] = g;
      c[2] = b;
      break;
    case GREY_GIMAGE:
      c[0] = ((int) r + (int) g + (int) b) / 3;
      c[1] = c[0];
      c[2] = c[0];
      break;
    case INDEXED_GIMAGE:
      fatal_error ("we don't handle filling of indexed color images yet");
      break;
    }

  pixels = gimage->width * gimage->height * gimage->bpp;
  
  p = gimage->raw_image;
  for (i = 0; i < pixels; i++)
    *p++ = c[i % 3];
}


