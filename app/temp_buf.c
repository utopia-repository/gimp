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
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include "appenv.h"
#include "errors.h"
#include "gdisplay.h"
#include "general.h"
#include "gimprc.h"
#include "paint_funcs.h"
#include "temp_buf.h"


static unsigned char *   temp_buf_allocate (unsigned int);
static void              temp_buf_to_color (TempBuf *, TempBuf *);
static void              temp_buf_to_grey (TempBuf *, TempBuf *);


/*  Memory management  */

static unsigned char *
temp_buf_allocate (size)
     unsigned int size;
{
  unsigned char * data;

  data = (unsigned char *) xmalloc (size);

  return data;
}


/*  The conversion routines  */

static void
temp_buf_to_color (src_buf, dest_buf)
     TempBuf * src_buf;
     TempBuf * dest_buf;
{
  unsigned char *src;
  unsigned char *dest;
  long num_bytes;

  src = temp_buf_data (src_buf);
  dest = temp_buf_data (dest_buf);

  num_bytes = src_buf->width * src_buf->height;

  while (num_bytes--)
    {
      *dest++ = *src;
      *dest++ = *src;
      *dest++ = *src++;
    }
}


static void
temp_buf_to_grey (src_buf, dest_buf)
     TempBuf * src_buf;
     TempBuf * dest_buf;
{
  unsigned char *src;
  unsigned char *dest;
  long num_bytes;
  float pix;

  src = temp_buf_data (src_buf);
  dest = temp_buf_data (dest_buf);

  num_bytes = src_buf->width * src_buf->height;

  while (num_bytes--)
    {
      pix =  0.30 * *src++;
      pix += 0.59 * *src++;
      pix += 0.11 * *src++;

      *dest++ = (unsigned char) pix;
    }
}


TempBuf * 
temp_buf_new (width, height, bytes, x, y, col)
     int width;
     int height;
     int bytes;
     int x, y;
     unsigned char * col;
{
  long i;
  int j;
  unsigned char * init, * data;
  TempBuf * temp;

  temp = (TempBuf *) xmalloc (sizeof (TempBuf));

  temp->width  = width;
  temp->height = height;
  temp->bytes  = bytes;
  temp->x      = x;
  temp->y      = y;
  temp->swapped = False;
  temp->filename = NULL;

  temp->data   = temp_buf_allocate (width * height * bytes);

  /*  initialize the data  */
  if (col)
    {
      i = width * height;
      data = temp->data;
      
      while (i--)
	{
	  j = bytes;
	  init = col;
	  while (j--)
	    *data++ = *init++;
	}
    }
  
  return temp;
}


TempBuf *
temp_buf_copy (src, dest)
     TempBuf * src;
     TempBuf * dest;
{
  TempBuf * new;
  long length;

  if (!src)
    {
      warning ("trying to copy a temp buf which is NULL.");
      return dest;
    }

  if (!dest)
    new = temp_buf_new (src->width, src->height, src->bytes, 0, 0, NULL);
  else
    {
      new = dest;
      if (dest->width != src->width || dest->height != src->height)
	warning ("In temp_buf_copy, the widths or heights don't match.");
    }

  /*  The temp buf is smart, and can translate between color and grey  */
  if (src->bytes != new->bytes)
    {
      if (src->bytes == 3)
	temp_buf_to_grey (src, new);
      else  /*  src->bytes == 1, new->bytes == 3  */
	temp_buf_to_color (src, new);
    }
  else
    {
      /* make the copy */
      length = src->width * src->height * src->bytes;
      memcpy (temp_buf_data (new), temp_buf_data (src), length);
    }

  return new;
}


TempBuf *
temp_buf_resize (buf, bytes, x, y, w, h)
     TempBuf * buf;
     int bytes;
     int x, y;
     int w, h;
{
  int size;

  /*  calculate the requested size  */
  size = w * h * bytes;

  /*  First, configure the canvas buffer  */
  if (!buf)
    buf = temp_buf_new (w, h, bytes, x, y, NULL);
  else 
    {
      if (size != (buf->width * buf->height * buf->bytes))
      {
	/*  Make sure the temp buf is unswapped  */
	temp_buf_unswap (buf);

	/*  Reallocate the data for it  */
	buf->data = xrealloc (buf->data, size);
      }

      /*  Make sure the temp buf fields are valid  */
      buf->x = x;
      buf->y = y;
      buf->width = w;
      buf->height = h;
      buf->bytes = bytes;
    }

  return buf;
}


TempBuf *
temp_buf_copy_area (src, dest, x, y, w, h)
     TempBuf * src;
     TempBuf * dest;
     int x, y;
     int w, h;
{
  TempBuf * new;
  PixelRegion srcR, destR;
  int x1, y1, x2, y2;

  if (!src)
    {
      warning ("trying to copy a temp buf which is NULL.");
      return dest;
    }

  /*  some bounds checking  */
  x1 = bounds (x, 0, src->width);
  y1 = bounds (y, 0, src->height);
  x2 = bounds (x + w, 0, src->width);
  y2 = bounds (y + h, 0, src->height);

  if (!(x2 - x1) || !(y2 - y1))
    return dest;

  if (!dest)
    new = temp_buf_new (w, h, src->bytes, x, y, NULL);
  else
    {
      new = dest;
      if (dest->bytes != src->bytes)
	warning ("In temp_buf_copy_area, the widths or heights or bytes don't match.");
    }

  /*  Copy the region  */
  srcR.bytes = src->bytes;
  srcR.w = (x2 - x1);
  srcR.h = (y2 - y1);
  srcR.rowstride = src->bytes * src->width;
  srcR.data = temp_buf_data (src) + y1 * srcR.rowstride + x1 * srcR.bytes;

  destR.rowstride = new->bytes * new->width;
  destR.data = temp_buf_data (new) + (y1 - y) * destR.rowstride + (x1 - x) * srcR.bytes;

  copy_region (&srcR, &destR);

  return new;
}


TempBuf *
temp_buf_load (temp, gimage_ptr, x, y, w, h)
     TempBuf * temp;
     void * gimage_ptr;
     int x, y;
     int w, h;
{
  GImage * gimage;
  int x1, y1, x2, y2;
  int bytes;
  PixelRegion srcPR, destPR;

  gimage = (GImage *) gimage_ptr;
  bytes  = gimage->bpp;

  /*  a little bounds checking here...  */
  x1 = bounds (x, 0, gimage->width);
  x2 = bounds (x + w, 0, gimage->width);
  y1 = bounds (y, 0, gimage->height);
  y2 = bounds (y + h, 0, gimage->height);

  w  = (x2 - x1);
  h  = (y2 - y1);

  /*  ignore degenerate case  */
  if (!w || !h)
    return temp;

  if (! temp)
    /*  Create a new temporary buffer  */
    temp = temp_buf_new (w, h, bytes, x1, y1, NULL);
  else
    {
      /*  set the origin of the temp buf  */
      temp->x = x;
      temp->y = y;
    }

  srcPR.w = w;
  srcPR.h = h;
  srcPR.bytes = bytes;

  srcPR.rowstride = bytes * gimage->width;
  destPR.rowstride = temp->bytes * temp->width;

  srcPR.data = gimage->raw_image + bytes * (y1 * gimage->width + x1);
  destPR.data = temp_buf_data (temp) + bytes * ((y1 - y) * temp->width + (x1 - x));

  copy_region (&srcPR, &destPR);

  return temp;
}


void
temp_buf_paste (temp, gimage_ptr, x, y, w, h)
     TempBuf * temp;
     void * gimage_ptr;
     int x, y;
     int w, h;
{
  GImage * gimage;
  int x1, y1, x2, y2;
  int bytes;
  PixelRegion srcPR, destPR;

  gimage = (GImage *) gimage_ptr;
  bytes  = gimage->bpp;

  /*  make sure the temp buffer and the gimage have the same number of bytes  */
  if (bytes != temp->bytes)
    {
      warning ("can't paste temp buffer because gimage and temp buf have different depths.");
      return;
    }

  /*  a little bounds checking here...  */
  x1 = bounds (x, 0, gimage->width);
  x2 = bounds (x + w, 0, gimage->width);
  y1 = bounds (y, 0, gimage->height);
  y2 = bounds (y + h, 0, gimage->height);

  w  = (x2 - x1);
  h  = (y2 - y1);

  /*  ignore degenerate case  */
  if (!w || !h)
    return;

  srcPR.w = w;
  srcPR.h = h;
  srcPR.bytes = bytes;

  srcPR.rowstride = bytes * temp->width;
  destPR.rowstride = bytes * gimage->width;

  srcPR.data = temp_buf_data (temp) + bytes * ((y1 - y) * temp->width + (x1 - x));
  destPR.data = gimage->raw_image + bytes * (y1 * gimage->width + x1);

  copy_region (&srcPR, &destPR);

  return;
}
     

void
temp_buf_free (temp_buf)
     TempBuf * temp_buf;
{
  if (temp_buf->data)
    xfree (temp_buf->data);
      

  if (temp_buf->swapped)
    temp_buf_swap_free (temp_buf);

  xfree (temp_buf);
}


unsigned char *
temp_buf_data (temp_buf)
     TempBuf * temp_buf;
{
  if (temp_buf->swapped)
    temp_buf_unswap (temp_buf);

  return temp_buf->data;
}


/******************************************************************
 *  Mask buffer functions                                         *
 ******************************************************************/


MaskBuf *
mask_buf_new (width, height)
     int width;
     int height;
{
  static unsigned char empty = 0;

  return (temp_buf_new (width, height, 1, 0, 0, &empty));
}


void
mask_buf_free (mask)
     MaskBuf * mask;
{
  temp_buf_free ((TempBuf *) mask);
}


MaskBuf *
mask_convert_from_region (region_ptr, minimize)
     void * region_ptr;
     int minimize;
{
  MaskBuf * new;
  GRegion * region;
  link_ptr list;
  GSegment * seg;
  unsigned char * data, * d;
  int x1, y1, x2, y2;
  int i, j;

  region = (GRegion *) region_ptr;

  /*  find the bounds  */
  if (minimize)
    gregion_find_bounds (region, &x1, &y1, &x2, &y2);
  else
    {
      x1 = 0; y1 = 0;
      x2 = region->width;
      y2 = region->extent;
    }

  /*  make sure there is a valid region defined  */
  if (!(x2 - x1) || !(y2 - y1))
    return NULL;

  /*  create the new mask  */
  new = mask_buf_new ((x2 - x1), (y2 - y1));

  data = mask_buf_data (new);

  for (i = y1; i < y2; i++)
    {
      list = region->segments[i];
      
      if (list)
	seg = (GSegment *) list->data;
      else
	seg = NULL;

      d = data;

      for (j = x1; j < x2; j++)
	{
	  *d++ = (seg && j >= seg->start && j < seg->end) ? seg->value : 0;
	  
	  if (seg && j == seg->end - 1) 
	    {
	      if ((list = next_item (list)))
		seg = (GSegment *) list->data;
	      else
		seg = NULL;
	    }

	}

      data += new->width;
    }

  return new;
}


void
mask_convert_to_region (mask, region_ptr)
     MaskBuf * mask;
     void * region_ptr;
{
  GRegion * region;
  unsigned char * data;
  int i, j, w;
  unsigned char last_data;

  region = (GRegion *) region_ptr;
  data = mask_buf_data (mask);
  
  for (i = 0; i < mask->height; i++)
    {
      w = 0;
      last_data = 0;
      for (j = 0; j < mask->width; j++)
	{
	  if (w && (*data != last_data))
	    {
	      gregion_add_segment (region, j - w, i, w, last_data);
	      w = 0;
	    }

	  if (*data)
	    w++;

	  last_data = *data++;
	}
      if (w)
	gregion_add_segment (region, j - w, i, w, last_data);
    }
}


unsigned char *
mask_buf_data (mask_buf)
     MaskBuf * mask_buf;
{
  if (mask_buf->swapped)
    temp_buf_unswap (mask_buf);

  return mask_buf->data;
}


/******************************************************************
 *  temp buffer disk caching functions                            *
 ******************************************************************/

/*  NOTES:
 *  Disk caching is setup as follows:
 *    On a call to temp_buf_swap, the TempBuf parameter is stored
 *    in a temporary variable called cached_in_memory.
 *    On the next call to temp_buf_swap, if cached_in_memory is non-null,
 *    cached_in_memory is moved to disk, and the latest TempBuf parameter
 *    is stored in cached_in_memory.  This method keeps the latest TempBuf
 *    structure in memory instead of moving it directly to disk as requested.
 *    On a call to temp_buf_unswap, if cached_in_memory is non-null, it is
 *    compared against the requested TempBuf.  If they are the same, nothing
 *    must be moved in from disk since it still resides in memory.  However,
 *    if the two pointers are different, the requested TempBuf is retrieved
 *    from disk.  In the former case, cached_in_memory is set to NULL;
 *    in the latter case, cached_in_memory is left unchanged.
 *    If temp_buf_swap_free is called, cached_in_memory must be checked
 *    against the temp buf being freed.  If they are the same, then cached_in_memory
 *    must be set to NULL;
 *
 *  In the case where memory usage is set to "stingy":
 *    temp bufs are not cached in memory at all, they go right to disk.
 */

#define MAX_FILENAME    256
#define COMMAND_LENGTH  262

/*  a static counter for generating unique filenames  */
static int swap_index = 0;
static char buf[MAX_FILENAME];

/*  a static pointer which keeps track of the last request for a swapped buffer  */
static TempBuf * cached_in_memory = NULL;

/*  external error number  */
extern int errno;


static char *
generate_unique_filename ()
{
  pid_t pid;

  pid = getpid ();

  sprintf (buf, "%s/gimp%d.%d", swap_path, (int) pid, swap_index++);

  return xstrdup (buf);
}


void
temp_buf_swap (buf)
     TempBuf * buf;
{
  TempBuf * swap;
  char * filename;
  struct stat stat_buf;
  int err;
  FILE * fp;

  if (!buf || buf->swapped)
    return;

  /*  Set the swapped flag  */
  buf->swapped = True;

  if (app_data.stingy)
    swap = buf;
  else
    {
      swap = cached_in_memory;
      cached_in_memory = buf;
    }

  /*  For the case where there is no temp buf ready to be moved to disk, return  */
  if (!swap)
    return;

  /*  Get a unique filename for caching the data to a UNIX file  */
  filename = generate_unique_filename ();

  /*  Check if generated filename is valid  */
  err = stat (filename, &stat_buf);
  if (!err)
    {
      if (stat_buf.st_mode & S_IFDIR)
	{
	  warning ("Error in temp buf caching: \"%s\" is a directory (cannot overwrite)", 
		   filename);
	  xfree (filename);
	  return;
	}
    }

  /*  Open file for overwrite  */
  if ((fp = fopen (filename, "w")))
    {
      fwrite (swap->data, swap->width * swap->height * swap->bytes, 1, fp);
      fclose (fp);
    }
  else
    {
      perror ("Error in temp buf caching");
      warning ("Cannot write \"%s\"", filename);
      xfree (filename);
      return;
    }
  /*  Finally, free the buffer's data  */
  xfree (swap->data);
  swap->data = NULL;

  swap->filename = filename;
}


void
temp_buf_unswap (buf)
     TempBuf * buf;
{
  struct stat stat_buf;
  FILE * fp;
  char command[COMMAND_LENGTH];

  if (!buf || !buf->swapped)
    return;

  /*  Set the swapped flag  */
  buf->swapped = False;

  /*  If the requested temp buf is still in memory, simply return  */
  if (cached_in_memory == buf)
    {
      cached_in_memory = NULL;
      return;
    }

  /*  Allocate memory for the buffer's data  */
  buf->data   = temp_buf_allocate (buf->width * buf->height * buf->bytes);

  /*  Find out if the filename of the swapped data is an existing file... */
  if (!stat (buf->filename, &stat_buf))
    {
      if ((fp = fopen (buf->filename, "r")))
	{
	  fread (buf->data, buf->width * buf->height * buf->bytes, 1, fp);
	  fclose (fp);
	}
      else
	perror ("Error in temp buf caching");

      /*  Delete the swap file  */
      sprintf (command, "rm -f %s", buf->filename);
      system (command);
    }
  else
    warning ("Error in temp buf caching: information swapped to disk was lost!");

  xfree (buf->filename);   /*  free filename  */
  buf->filename = NULL;
}


void
temp_buf_swap_free (buf)
     TempBuf * buf;
{
  struct stat stat_buf;
  char command[COMMAND_LENGTH];

  if (!buf->swapped)
    return;

  /*  Set the swapped flag  */
  buf->swapped = False;

  /*  If the requested temp buf is cached in memory...  */
  if (cached_in_memory == buf)
    {
      cached_in_memory = NULL;
      return;
    }

  /*  Find out if the filename of the swapped data is an existing file... */
  if (!stat (buf->filename, &stat_buf))
    {
      /*  Delete the swap file  */
      sprintf (command, "rm -f %s", buf->filename);
      system (command);
    }
  else
    warning ("Error in temp buf disk swapping: information swapped to disk was lost!");

  xfree (buf->filename);   /*  free filename  */
  buf->filename = NULL;
}


void
swapping_free ()
{
  if (cached_in_memory)
    temp_buf_free (cached_in_memory);
}





