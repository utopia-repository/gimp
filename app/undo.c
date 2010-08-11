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
#include "errors.h"
#include "gdisplay.h"
#include "tools.h"
#include "undo.h"
#include "linked.h"
#include "paint_funcs.h"
#include "transform_core.h"

/*  Pop functions  */

Boolean      undo_pop_image            (int, int, void *);
Boolean      undo_pop_mod_sel          (int, int, void *);
Boolean      undo_pop_selection        (int, int, void *);
Boolean      undo_pop_region           (int, int, void *);
Boolean      undo_pop_move_sel         (int, int, void *);
Boolean      undo_pop_change_opacity   (int, int, void *);
Boolean      undo_pop_cut              (int, int, void *);
Boolean      undo_pop_copy             (int, int, void *);
Boolean      undo_pop_paste            (int, int, void *);
Boolean      undo_pop_transform        (int, int, void *);

/*  Free functions  */

void         undo_free_image           (int, void *);
void         undo_free_mod_sel         (int, void *);
void         undo_free_selection       (int, void *);
void         undo_free_region          (int, void *);
void         undo_free_move_sel        (int, void *);
void         undo_free_change_opacity  (int, void *);
void         undo_free_cut             (int, void *);
void         undo_free_paste           (int, void *);
void         undo_free_transform       (int, void *);


/*  the undo stack  */
static link_ptr undo_stack   = NULL;
static link_ptr redo_stack   = NULL;
static long     total_bytes  = 0;
static long     total_levels = 0;


#define UNDO    0
#define REDO    1
#define VERIFY  2

/*  external data  */
extern Selection * global_select;


static long
find_selection_size (select)
     Selection * select;
{
  long size;

  size = gregion_num_segments (select->region) * (sizeof (GSegment *) + sizeof (struct _link)) +
    select->region->extent * sizeof (GSegment *) +
      sizeof (Selection) + sizeof (GRegion);

  if (select->float_buf)
    size += sizeof (TempBuf) +
      select->float_buf->bytes * select->float_buf->width * select->float_buf->height;

  if (select->orig_buf)
    size += sizeof (TempBuf) +
      select->orig_buf->bytes * select->orig_buf->width * select->orig_buf->height;

  return size;
}


static void
undo_free_list (state, list)
     int state;
     link_ptr list;
{
  link_ptr orig;
  Undo * undo;

  orig = list;

  while (list)
    {
      undo = (Undo *) list->data;
      (* undo->free_func) (state, undo->data);
      total_bytes -= undo->bytes;
      xfree (undo);
      list = next_item (list);
    }
  
  free_list (orig);
}


static link_ptr
undo_verify_list (state, list)
     int state;
     link_ptr list;
{
  Undo * undo;

  if (!list)
    return NULL;

  undo = (Undo *) list->data;
  if (! (* undo->pop_func) (undo->ID, VERIFY, undo->data))
    {
      (* undo->free_func) (state, undo->data);
      total_bytes -= undo->bytes;
      xfree (undo);
      if (state == UNDO)
	total_levels --;
      return undo_verify_list (state, next_item (list));
    }
  else
    list->next = undo_verify_list (state, next_item (list));

  return list;
}


static long
find_bottom_size ()
{
  link_ptr list, next;

  list = undo_stack;

  while (list)
    {
      next = next_item (list);
      if (!next)
	return ((Undo *) list->data)->bytes;
      else
	list = next;
    }

  return 0;
}


static link_ptr
remove_stack_bottom ()
{
  link_ptr list, last;
  Undo * undo;

  list = undo_stack;

  last = NULL;
  while (list)
    {
      if (list->next == NULL)
	{
	  undo = (Undo *) list->data;
	  (* undo->free_func) (UNDO, undo->data);
	  total_bytes -= undo->bytes;
	  total_levels --;
	  xfree (undo);
	  if (last)
	    last->next = NULL;
	  free_list (list);
	  list = NULL;
	}
      else
	{
	  last = list;
	  list = next_item (list);
	}
    }

  if (last)
    return undo_stack;
  else
    return NULL;

}


static Boolean
undo_free_up_space (size)
     long size;
{
  long bottom_size;   /*  size of the undo element at the bottom of the stack  */

  /*  If there are 0 levels of undo, or there is not enough room in the
   *  entire stack for the push, return False.
   */
  if (app_data.levels_of_undo == 0 || (size > app_data.bytes_of_undo))
    return False;

  /*  Find the size of the element at the bottom of the undo case
   *  if we have reached the limit on levels of undo
   */
  bottom_size = (total_levels == app_data.levels_of_undo) ? find_bottom_size () : 0;
  if ((size + total_bytes - bottom_size) > app_data.bytes_of_undo)
    warning ("The levels of undo will be reduced to accomodate this operation.");
  
  /*  Delete the item on the bottom of the stack if we have the maximum
   *  levels of undo already
   */
  if (total_levels == app_data.levels_of_undo)
    undo_stack = remove_stack_bottom ();

  /*  Continue to remove items from the undo stack if we need to free up space  */
  while (undo_stack && ((size + total_bytes) > app_data.bytes_of_undo))
    undo_stack = remove_stack_bottom ();
  
  return True;
}



static Undo *
undo_push (size)
     long size;
{
  Undo * new;

  size += sizeof (Undo);

  if (redo_stack)
    {
      undo_free_list (REDO, redo_stack);
      redo_stack = NULL;
    }

  if (! undo_free_up_space (size))
    return NULL;

  new = (Undo *) xmalloc (sizeof (Undo));

  new->bytes = size;
  total_bytes += size;
  total_levels ++;
  undo_stack = add_to_list (undo_stack, (void *) new);

  return new;
}


Boolean
undo_pop ()
{
  Undo * undo;
  link_ptr tmp;
  int status;

  while (undo_stack)
    {
      undo = (Undo *) undo_stack->data;

      status = (* undo->pop_func) (undo->ID, UNDO, undo->data);
      total_levels --;

      redo_stack = add_to_list (redo_stack, (void *) undo);

      tmp = undo_stack;
      undo_stack = next_item (undo_stack);
      tmp->next = NULL;
      free_list (tmp);

      if (status)
	return True;
    }

  return False;

}


Boolean
undo_redo ()
{
  Undo * redo;
  link_ptr tmp;
  int status;

  while (redo_stack)
    {
      redo = (Undo *) redo_stack->data;

      status = (* redo->pop_func) (redo->ID, REDO, redo->data);
      total_levels ++;

      undo_stack = add_to_list (undo_stack, (void *) redo);

      tmp = redo_stack;
      redo_stack = next_item (redo_stack);
      tmp->next = NULL;
      free_list (tmp);

      if (status)
	return True;
    }

  return False;

}


void
undo_clean_stack ()
{
  /*  Clean out any dated undo structures in the stack  */
  undo_stack = undo_verify_list (UNDO, undo_stack);
  redo_stack = undo_verify_list (REDO, redo_stack);
}


void
undo_free ()
{
  undo_free_list (UNDO, undo_stack);
  undo_free_list (REDO, redo_stack);
  undo_stack = NULL;
  redo_stack = NULL;
}



/*********************************/
/*  Image Undo functions         */
/*    includes undo_push_buffer  */


Boolean
undo_push_image (ID, x1, y1, x2, y2)
     int ID;
     int x1, y1;
     int x2, y2;
{
  GImage * gimage;
  long size;
  Undo * new;
  TempBuf * buf;

  if (! (gimage = gimage_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gimage  */
  if (gimage->dirty < 0)
    gimage->dirty = 2;
  else
    gimage->dirty ++;

  /*  If we cannot create a new temp buf--either because our parameters are
   *  degenerate or something else failed, simply return an unsuccessful push.
   */
  if (! (buf = temp_buf_load (NULL, gimage, x1, y1, (x2 - x1), (y2 - y1))))
    return False;

  size = buf->width * buf->height * buf->bytes;

  if ((new = undo_push (size)))
    {
      new->ID        = ID;
      new->data      = buf;
      new->pop_func  = undo_pop_image;
      new->free_func = undo_free_image;

      /*  disk swap this temporary buffer -- in the interests of memory usage */
      temp_buf_swap ((TempBuf *) new->data);

      return True;
    }
  else
    return False;

}


Boolean
undo_push_buffer (ID, buf_ptr)
     int ID;
     void * buf_ptr;
{
  GImage * gimage;
  TempBuf * buf;
  long size;
  Undo * new;

  buf = (TempBuf *) buf_ptr;

  /* If the gimage ID is invalid or the buffer provided is invalid,
   * return an unsuccessful push.
   */
  if (! (gimage = gimage_get_ID (ID)) || !buf)
    return False;

  /*  increment the dirty flag for this gimage  */
  if (gimage->dirty < 0)
    gimage->dirty = 2;
  else
    gimage->dirty ++;

  size = buf->width * buf->height * buf->bytes;

  if ((new = undo_push (size)))
    {
      new->ID        = ID;
      new->data      = buf;
      new->pop_func  = undo_pop_image;
      new->free_func = undo_free_image;

      /*  disk swap this temporary buffer -- in the interests of memory usage */
      temp_buf_swap ((TempBuf *) new->data);

      return True;
    }
  else
    return False;

}


Boolean
undo_pop_image (ID, state, temp_buf_ptr)
     int ID;
     int state;
     void * temp_buf_ptr;
{
  GImage * gimage;
  TempBuf * temp_buf;
  unsigned char * data;
  unsigned char * temp_data;
  unsigned char * temp_line;
  long gimage_width;
  long temp_width;
  int i;

  if (! (gimage = gimage_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gimage->dirty--; break;
    case REDO : gimage->dirty++; break;
    default : break;
    }
    
  temp_buf = (TempBuf *) temp_buf_ptr;

  gimage_width = gimage->width * temp_buf->bytes;
  temp_width = temp_buf->width * temp_buf->bytes;

  /*  create the temporary scanline buffer--(needed for swapping scanlines)  */
  temp_line = (unsigned char *) xmalloc (sizeof (unsigned char) * temp_width);

  data = gimage->raw_image + temp_buf->bytes * 
    (temp_buf->y * gimage->width + temp_buf->x);

  /*  unswap the temporary buffer data  */
  temp_buf_unswap (temp_buf);

  temp_data = temp_buf_data (temp_buf);

  for (i = 0; i < temp_buf->height; i++)
    {
      if (i >= 0 || i < gimage->height)
	{
	  memcpy (temp_line, temp_data, temp_width);
	  memcpy (temp_data, data, temp_width);
	  memcpy (data, temp_line, temp_width);
	}

      data += gimage_width;
      temp_data += temp_width;
    }

  /*  disk swap this temporary buffer -- in the interests of memory usage */
  temp_buf_swap (temp_buf);

  /*  free up the temporary scanline buffer  */
  xfree (temp_line);

  /*  update the display  */
  gdisplay_update (gimage->ID, temp_buf->x, temp_buf->y,
		   temp_buf->width, temp_buf->height, 0);

  return True;
}


void
undo_free_image (state, temp_buf_ptr)
     int state;
     void * temp_buf_ptr;
{
  temp_buf_free ((TempBuf *) temp_buf_ptr);
}


/******************************/
/*  Selection Undo functions  */


Boolean
undo_push_mod_sel (ID, buf_ptr)
     int ID;
     void * buf_ptr;
{
  GDisplay * gdisp;
  TempBuf * buf;
  long size;
  Undo * new;

  buf = (TempBuf *) buf_ptr;

  /* If the gdisplay ID is invalid or the buffer provided is invalid,
   * return an unsuccessful push.
   */
  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gimage  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty ++;

  if (!buf)
    return False;

  size = buf->width * buf->height * buf->bytes;

  if ((new = undo_push (size)))
    {
      new->ID        = ID;
      new->data      = buf;
      new->pop_func  = undo_pop_mod_sel;
      new->free_func = undo_free_mod_sel;

      /*  disk swap this temporary buffer -- in the interests of memory usage */
      temp_buf_swap (buf);

      return True;
    }
  else
    return False;

}


Boolean
undo_pop_mod_sel (ID, state, buf_ptr)
     int ID;
     int state;
     void * buf_ptr;
{
  GDisplay * gdisplay;
  Selection * select;
  TempBuf * buf;
  PixelRegion srcR, destR;
  int x1, y1, x2, y2;
  int sm;

  if (! (gdisplay = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisplay->gimage->dirty--; break;
    case REDO : gdisplay->gimage->dirty++; break;
    default : break;
    }

  select = gdisplay->select;
  buf = (TempBuf *) buf_ptr;

  /*  unswap this temporary buffer  */
  temp_buf_unswap (buf);

  /*  exchange the contents of the backup temp buf with the current float buf  */
  srcR.bytes = buf->bytes;
  srcR.w = buf->width;
  srcR.h = buf->height;
  srcR.rowstride = srcR.w * srcR.bytes;
  srcR.data = temp_buf_data (buf);

  destR.rowstride = select->float_buf->width * select->float_buf->bytes;
  destR.data = temp_buf_data (select->float_buf) + 
    buf->y * destR.rowstride + buf->x * select->float_buf->bytes;

  swap_region (&srcR, &destR);

  /*  disk swap this temporary buffer -- in the interests of memory usage */
  temp_buf_swap (buf);

  /*  find the bounds to update  */
  selection_find_bounds (select, &x1, &y1, &x2, &y2);

  /*  paste the floating buffer down on the gimage */
  selection_stencil (select, (void *) gdisplay->gimage, buf->x, buf->y,
		     buf->x + buf->width, buf->y + buf->height, BLEND_STENCIL);
    
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisplay, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisplay->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);
  return True;
}


void
undo_free_mod_sel (state, buf_ptr)
     int state;
     void * buf_ptr;
{
  temp_buf_free ((TempBuf *) buf_ptr);

}


/******************************/
/*  Selection Undo functions  */

Boolean
undo_push_selection (ID, select_ptr)
     int ID;
     void * select_ptr;
{
  Undo * new;
  Selection * select, * new_sel;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  select = (Selection *) select_ptr;

  /*  increment the dirty flag for this gdisplay  */
  if (select->float_buf != NULL) /*  this is the case where we're anchoring a selection  */
    {
      if (gdisp->gimage->dirty < 0)
	gdisp->gimage->dirty = 2;
      else
	gdisp->gimage->dirty++;
    }

  /*  accounts for everything in the Selection structure  */

  if ((new = undo_push (find_selection_size (select))))
    {
      new_sel = selection_generic_new (select->region, select->offset_x, 
				       select->offset_y, select->float_buf);
      new_sel->orig_buf  = select->orig_buf;
      new_sel->opacity   = select->opacity;

      new->ID            = ID;
      new->data          = (void *) new_sel;
      new->pop_func      = undo_pop_selection;
      new->free_func     = undo_free_selection;

      /*  disk swap this selection's temporary buffers */
      temp_buf_swap (select->float_buf);
      temp_buf_swap (select->orig_buf);

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_selection (ID, state, select_ptr)
     int ID;
     int state;
     void * select_ptr;
{
  GDisplay * gdisp;
  Selection * select;

  select = (Selection *) select_ptr;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : 
      if (select->float_buf != NULL)
	gdisp->gimage->dirty--;  /*  this is the case where we're unanchoring a selection  */
      break;
    case REDO : 
      if (gdisp->select->float_buf != NULL)
	gdisp->gimage->dirty++;  /*  this is the case where we're anchoring a selection  */
      break;
    default : break;
    }

  /*  hide the current selection  */
  selection_invis (gdisp->select);

   /* freeze the active tool */
  active_tool_control (PAUSE, (void *) gdisp);

  /*  unswap the temp buffers which came from the undo stack  */
  temp_buf_unswap (select->float_buf);
  temp_buf_unswap (select->orig_buf);

  selection_swap (gdisp->select, select);

  /*  swap the temp buffers which are now being placed back on the undo stack  */
  temp_buf_swap (select->float_buf);
  temp_buf_swap (select->orig_buf);
  
  /* re-enable the active tool */
  active_tool_control (RESUME, (void *) gdisp);

  /*  recalculate the current selection  */
  selection_start (gdisp->select, 0, True);

  return True;

}


void
undo_free_selection (state, select_ptr)
     int state;
     void * select_ptr;
{
  Selection * select;

  select = (Selection *) select_ptr;
  selection_generic_free (select);
}



/******************************/
/*  Region Undo functions  */

Boolean
undo_push_region (ID, region_ptr)
     int ID;
     void * region_ptr;
{
  Undo * new;
  GRegion ** region;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  if ((new = undo_push (sizeof (GRegion *))))
    {
      region             = (GRegion **) xmalloc (sizeof (GRegion *));
      region [0]         = (GRegion *) region_ptr;
      new->ID            = ID;
      new->data          = (void *) region;
      new->pop_func      = undo_pop_region;
      new->free_func     = undo_free_region;

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_region (ID, state, region_ptr)
     int ID;
     int state;
     void * region_ptr;
{
  GDisplay * gdisp;
  GRegion ** region;
  GRegion * tmp_reg;

  region = (GRegion **) region_ptr;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : break;  /*  modifying regions doesn't mess with the dirty flag  */
    case REDO : break;
    default : break;
    }

  /*  hide the current selection  */
  selection_invis (gdisp->select);

   /* freeze the active tool */
  active_tool_control (PAUSE, (void *) gdisp);

  /*  swap regions  */
  tmp_reg = gdisp->select->region;
  gdisp->select->region = region[0];
  region[0] = tmp_reg;

  /* re-enable the active tool */
  active_tool_control (RESUME, (void *) gdisp);

  /*  recalculate the current selection  */
  selection_start (gdisp->select, 0, True);

  return True;

}


void
undo_free_region (state, region_ptr)
     int state;
     void * region_ptr;
{
  GRegion ** region;

  region = (GRegion **) region_ptr;
  gregion_free (region [0]);
}



/***********************************/
/*  Move selection Undo functions  */

Boolean
undo_push_move_sel (ID, offset_x, offset_y, initial_move)
     int ID;
     int offset_x, offset_y;
     int initial_move;
{
  long size;
  int * offsets;
  Undo * new;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gdisplay  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty++;

  size = sizeof (int) * 3;

  if ((new = undo_push (size)))
    {
      offsets = (int *) xmalloc (size);

      offsets[0] = offset_x;
      offsets[1] = offset_y;
      offsets[2] = initial_move;  /*  did this move cut the selection?  */

      new->ID        = ID;
      new->data      = offsets;
      new->pop_func  = undo_pop_move_sel;
      new->free_func = undo_free_move_sel;

      return True;
    }
  else
    return False;
}


Boolean
undo_pop_move_sel (ID, state, offsets_ptr)
     int ID;
     int state;
     void * offsets_ptr;
{
  GDisplay * gdisp;
  int * offsets;
  int x1, y1, x2, y2;
  int bx1, by1, bx2, by2;
  int sm;  /*  selection margin  */

  offsets = (int *) offsets_ptr;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisp->gimage->dirty--; break;
    case REDO : gdisp->gimage->dirty++; break;
    default : break;
    }

  x1 = gdisp->select->offset_x,
  y1 = gdisp->select->offset_y,
  x2 = offsets[0];
  y2 = offsets[1];

  offsets[0] = gdisp->select->offset_x;
  offsets[1] = gdisp->select->offset_y;

  selection_cut_floating (gdisp->select, (void *) gdisp->gimage);

  gdisp->select->offset_x = x2;
  gdisp->select->offset_y = y2;

  /*  If we're undo'ing and this was the initial move which actually cut
   *  out the selected region, make sure we paste it completely back in
   *  minus any fuzzy region blends, etc...
   */
  if (state == UNDO && offsets[2] == 1)
    {
      selection_paste_floating (gdisp->select, (void *) gdisp->gimage, COPY_STENCIL);

      /*  since we're undo'ing this all the way to the time it was initially cut,
       *  we should free the buffers also and let them get recreated on a REDO op
       */
      if (gdisp->select->float_buf)
	temp_buf_free (gdisp->select->float_buf);
      if (gdisp->select->orig_buf)
	temp_buf_free (gdisp->select->orig_buf);
      gdisp->select->float_buf = gdisp->select->orig_buf = NULL;
    }
  else
    selection_paste_floating (gdisp->select, (void *) gdisp->gimage, BLEND_STENCIL);
    
  /*  Find bounds on area to update  */
  gregion_find_bounds (gdisp->select->region, &bx1, &by1, &bx2, &by2);
      
  bx1 = MINIMUM ((x1 + bx1), (x2 + bx1));
  bx2 = MAXIMUM ((x1 + bx2), (x2 + bx2));
  by1 = MINIMUM ((y1 + by1), (y2 + by1));
  by2 = MAXIMUM ((y1 + by2), (y2 + by2));

  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, bx1, by1, (bx2 - bx1 + sm), (by2 - by1 + sm), 0);

  return True;
}


void
undo_free_move_sel (state, offsets_ptr)
     int state;
     void * offsets_ptr;
{
  xfree (offsets_ptr);
}


/***********************************/
/*  Change Opacity Undo functions  */

Boolean
undo_push_change_opacity (ID, old_opacity, new_opacity, initial_move)
     int ID;
     double old_opacity;
     double new_opacity;
     int initial_move;
{
  long size;
  double * opacity_ptr;
  Undo * new;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gdisplay  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty++;

  size = sizeof (double) * 3;

  if ((new = undo_push (size)))
    {
      opacity_ptr = (double *) xmalloc (size);

      opacity_ptr[0] = old_opacity;
      opacity_ptr[1] = new_opacity;
      opacity_ptr[2] = (double) initial_move;

      new->ID        = ID;
      new->data      = opacity_ptr;
      new->pop_func  = undo_pop_change_opacity;
      new->free_func = undo_free_change_opacity;

      return True;
    }
  else
    return False;
}


Boolean
undo_pop_change_opacity (ID, state, opacity_ptr)
     int ID;
     int state;
     void * opacity_ptr;
{
  GDisplay * gdisp;
  double * opacity, tmp;
  int x1, y1, x2, y2;
  int sm;  /*  selection margin  */

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisp->gimage->dirty--; break;
    case REDO : gdisp->gimage->dirty++; break;
    default : break;
    }

  opacity = (double *) opacity_ptr;

  /*  switch opacities  */
  tmp = opacity [0];
  opacity [0] = opacity [1];
  opacity [1] = tmp;

  selection_set_opacity (gdisp->select, opacity [1]);

  selection_cut_floating (gdisp->select, (void *) gdisp->gimage);

  /*  If we're undo'ing and this was the initial move which actually cut
   *  out the selected region, make sure we paste it completely back in
   *  minus any fuzzy region blends, etc...
   */
  if (state == UNDO && (int) opacity[2] == 1)
    {
      selection_paste_floating (gdisp->select, (void *) gdisp->gimage, COPY_STENCIL);
      /*  since we're undo'ing this all the way to the time it was initially cut,
       *  we should free the buffers also and let them get recreated on a REDO op
       */
      if (gdisp->select->float_buf)
	temp_buf_free (gdisp->select->float_buf);
      if (gdisp->select->orig_buf)
	temp_buf_free (gdisp->select->orig_buf);
      gdisp->select->float_buf = gdisp->select->orig_buf = NULL;
    }
  else
    selection_paste_floating (gdisp->select, (void *) gdisp->gimage, BLEND_STENCIL);
    
  /*  Find bounds on area to update  */
  selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2);
      
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);

  return True;
}


void
undo_free_change_opacity (state, opacity_ptr)
     int state;
     void * opacity_ptr;
{
  xfree (opacity_ptr);
}


/*****************************/
/*  Edit Cut Undo functions  */


Boolean
undo_push_cut (ID, old_select, new_select, initial_move)
     int ID;
     void * old_select;
     void * new_select;
     int initial_move;
{
  Selection ** selections;
  long size;
  Undo * new;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gdisplay  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty++;

  size = find_selection_size ((Selection *) new_select);
  size += sizeof (Selection *) * 3;
  
  if ((new = undo_push (size)))
    {
      selections         = (Selection **) xmalloc (sizeof (Selection *) * 3);
      selections[0]      = old_select;
      selections[1]      = new_select;
      selections[2]      = (Selection *) initial_move;

      new->ID            = ID;
      new->data          = (void *) selections;
      new->pop_func      = undo_pop_cut;
      new->free_func     = undo_free_cut;

      /*  In the interests of disk space, swap the old selection's edit buffers  */
      if (old_select)
	{
	  temp_buf_swap (((Selection *) old_select)->float_buf);
	  temp_buf_swap (((Selection *) old_select)->orig_buf);
	}

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_cut (ID, state, selections_ptr)
     int ID;
     int state;
     void * selections_ptr;
{
  GDisplay * gdisp;
  Selection ** selections;
  Selection * select, * old_select, * new_select, *cur_select;
  int x1, y1, x2, y2;
  int sm;   /*  selection margin  */

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisp->gimage->dirty--; break;
    case REDO : gdisp->gimage->dirty++; break;
    default : break;
    }

  selections = (Selection **) selections_ptr;
  old_select = selections[0];
  new_select = selections[1];
  select = gdisp->select;

  /*  swap selections  */
  selection_swap (select, new_select);

  if (state == UNDO)
    {
      cur_select = select;

      /*  unswap the old buffer's edit buffer  */
      if (old_select)
	{
	  temp_buf_unswap (old_select->float_buf);
	  temp_buf_unswap (old_select->orig_buf);
	}
      /*  swap the new buffer's edit buf  */
      temp_buf_swap (new_select->float_buf);
      temp_buf_swap (new_select->orig_buf);

      /*  put the old global buffer back into the global_select variable  */
      global_select = old_select;

      if ((int) selections[2] == 1)
	{
	  selection_paste_floating (cur_select, (void *) gdisp->gimage, COPY_STENCIL);
	  
	  /*  since we're undo'ing this all the way to the time it was initially cut,
	   *  we should free the buffers also and let them get recreated on a REDO op
	   */
	  if (cur_select->float_buf)
	    temp_buf_free (cur_select->float_buf);
	  if (cur_select->orig_buf)
	    temp_buf_free (cur_select->orig_buf);
	  cur_select->float_buf = cur_select->orig_buf = NULL;
	}
      else
	selection_paste_floating (cur_select, (void *) gdisp->gimage, BLEND_STENCIL);

    }
  else if (state == REDO)
    {
      cur_select = new_select;

      /*  unswap the new buffer's edit buffer  */
      temp_buf_unswap (new_select->float_buf);
      temp_buf_unswap (new_select->orig_buf);
      /*  swap the old buffer's edit buf  */
      if (old_select)
	{
	  temp_buf_swap (old_select->float_buf);
	  temp_buf_swap (old_select->orig_buf);
	}

      /*  put the cut section back into the global_select variable  */
      global_select = new_select;

      /*  cut the selection out  */
      selection_cut_floating (cur_select, (void *) gdisp->gimage);
    }

  selection_find_bounds (cur_select, &x1, &y1, &x2, &y2);

  /* update the gdisplays  */
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);

  return True;
}


void
undo_free_cut (state, selections_ptr)
     int state;
     void * selections_ptr;
{
  Selection ** selections;
  Selection * select;

  selections = (Selection **) selections_ptr;
  
  /*  This is confusing:  If we're freeing this up from the undo stack,
      it means that we're removing it because we're out of room.  In this case,
      The selection we want to free up is the "old" selection--the selection
      which was in the global buffer before we "cut" but is now obsolete */
  if (state == UNDO)
    select = selections[0];

  /*  If we're freeing this from the REDO stack, it means that we cut, then
      undo'd.  In this case, we have the "old" selection back in the global
      edit buffer.  So we can just trash the "new" selection.  */
  else
    select = selections[1];

  /*  Free up some space!  */
  if (select)
    selection_generic_free (select);

  xfree (selections);
}


/*****************************/
/*  Edit Copy Undo functions  */


Boolean
undo_push_copy (ID, old_select, new_select)
     int ID;
     void * old_select;
     void * new_select;
{
  Selection ** selections;
  long size;
  Undo * new;

  size = find_selection_size ((Selection *) new_select);
  size += sizeof (Selection *) * 2;
  
  if ((new = undo_push (size)))
    {
      selections         = (Selection **) xmalloc (sizeof (Selection *) * 2);
      selections[0]      = old_select;
      selections[1]      = new_select;

      new->ID            = ID;
      new->data          = (void *) selections;
      new->pop_func      = undo_pop_copy;

      new->free_func     = undo_free_cut;

      /*  In the interests of disk space, swap the old selection's edit buffer  */
      if (old_select)
	{
	  temp_buf_swap (((Selection *) old_select)->float_buf);
	  temp_buf_swap (((Selection *) old_select)->orig_buf);
	}

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_copy (ID, state, selections_ptr)
     int ID;
     int state;
     void * selections_ptr;
{
  Selection ** selections;

  selections = (Selection **) selections_ptr;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : break;  /*  copying doesn't mess with any dirty bits  */
    case REDO : break;
    default : break;
    }

  if (state == UNDO)
    {
      /*  unswap the old buffer's edit buffer  */
      if (selections[0])
	{
	  temp_buf_unswap (selections[0]->float_buf);
	  temp_buf_unswap (selections[0]->orig_buf);
	}
      /*  swap the new buffer's edit buf  */
      temp_buf_swap (selections[1]->float_buf);
      temp_buf_swap (selections[1]->orig_buf);

      /*  put the old global buffer back into the global_select variable  */
      global_select = selections[0];
    }
  else if (state == REDO)
    {
      /*  unswap the new buffer's edit buffer  */
      temp_buf_unswap (selections[1]->float_buf);
      temp_buf_unswap (selections[1]->orig_buf);
      /*  swap the old buffer's edit buf  */
      if (selections[0])
	{
	  temp_buf_swap (selections[0]->float_buf);
	  temp_buf_swap (selections[0]->orig_buf);
	}

      /*  put the cut section back into the global_select variable  */
      global_select = selections[1];
    }

  return True;
}


/*****************************/
/*  Edit Paste Undo functions  */


Boolean
undo_push_paste (ID, old_select)
     int ID;
     void * old_select;
{
  Undo * new;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gdisplay  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty++;

  if ((new = undo_push (find_selection_size ((Selection *) old_select))))
    {
      new->ID            = ID;
      new->data          = (void *) old_select;
      new->pop_func      = undo_pop_paste;
      new->free_func     = undo_free_paste;

      /*  In the interests of disk space, swap the old selection's edit buffer  */
      temp_buf_swap (((Selection *) old_select)->float_buf);
      temp_buf_swap (((Selection *) old_select)->orig_buf);

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_paste (ID, state, select_ptr)
     int ID;
     int state;
     void * select_ptr;
{
  GDisplay * gdisp;
  Selection * select;
  Selection * cur_select;
  Selection * old_select;
  int x1, y1, x2, y2;
  int x3, y3, x4, y4;
  int sm;   /*  selection margin  */

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisp->gimage->dirty--; break;
    case REDO : gdisp->gimage->dirty++; break;
    default : break;
    }

  select = (Selection *) select_ptr;

  /*  unswap the edit buffers of the selection being removed from the stack  */
  temp_buf_unswap (select->float_buf);
  temp_buf_unswap (select->orig_buf);

  /*  swap selections  */
  selection_swap (gdisp->select, select);

  /*  swap the edit buffer of the selection being place on the undo stack  */
  temp_buf_swap (select->float_buf);
  temp_buf_swap (select->orig_buf);

  /*  If we're undo'ing, that means we're cutting a selection we
   *  recently pasted down.  We don't have to paste the old one,
   *  because it's already there behind the one we need to cut.
   */
  if (state == UNDO)
    {
      cur_select = select;
      old_select = gdisp->select;
      selection_cut_floating (cur_select, (void *) gdisp->gimage);
    }
  /*  if we're redo'ing, it means we're re-pasting the global edit
   *  selection to the gimage.  
   */
  else
    {
      cur_select = gdisp->select;
      old_select = select;
      selection_paste_floating (cur_select, (void *) gdisp->gimage, BLEND_STENCIL);
    }

  selection_find_bounds (cur_select, &x1, &y1, &x2, &y2);

  /*  possibly expand the bounding box if the old selection is non-empty  */
  if (selection_find_bounds (old_select, &x3, &y3, &x4, &y4))
    {
      x1 = MINIMUM (x1, x3);
      y1 = MINIMUM (y1, y3);
      x2 = MAXIMUM (x2, x4);
      y2 = MAXIMUM (y2, y4);
    }

  /* update the gdisplays  */
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);

  return True;
}


void
undo_free_paste (state, select_ptr)
     int state;
     void * select_ptr;
{
  Selection * select;

  select = (Selection *) select_ptr;

  if (select)
    selection_generic_free (select);
}


/****************************************/
/*  Selection Transform Undo functions  */


Boolean
undo_push_transform (ID, trans_undo_ptr)
     int ID;
     void * trans_undo_ptr;
{
  Undo * new;
  TransformUndo * trans_undo;
  long size;
  GDisplay * gdisp;

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  /*  increment the dirty flag for this gdisplay  */
  if (gdisp->gimage->dirty < 0)
    gdisp->gimage->dirty = 2;
  else
    gdisp->gimage->dirty++;

  trans_undo = (TransformUndo *) trans_undo_ptr;

  size = sizeof (TransformUndo) + 
    find_selection_size ((Selection *) trans_undo->old_select);

  if ((new = undo_push (size)))
    {
      new->ID            = ID;
      new->data          = trans_undo_ptr;
      new->pop_func      = undo_pop_transform;
      new->free_func     = undo_free_transform;

      /*  In the interests of disk space, swap the old selection's edit buffer  */
      temp_buf_swap (((Selection *) trans_undo->old_select)->float_buf);
      temp_buf_swap (((Selection *) trans_undo->old_select)->orig_buf);

      return True;
    }
  else
    return False;
  
}


Boolean
undo_pop_transform (ID, state, trans_undo_ptr)
     int ID;
     int state;
     void * trans_undo_ptr;
{
  GDisplay * gdisp;
  TransformUndo * trans_undo;
  TransformCore * active;
  Selection * select;
  int i;
  double tmp;
  int x1, y1, x2, y2;
  int x3, y3, x4, y4;
  int sm;   /*  selection margin  */

  if (! (gdisp = gdisplay_get_ID (ID)))
    return False;

  switch (state)
    {
    case VERIFY : return True; break;
    case UNDO : gdisp->gimage->dirty--; break;
    case REDO : gdisp->gimage->dirty++; break;
    default : break;
    }

  trans_undo = (TransformUndo *) trans_undo_ptr;
  select = (Selection *) trans_undo->old_select;

  /*  get the current selection's bounds  */
  selection_find_bounds (gdisp->select, &x3, &y3, &x4, &y4);

  /*  cut the current floating selection  */
  selection_cut_floating (gdisp->select, (void *) gdisp->gimage);

  /*  unswap the old selection edit buffer so we can use it  */
  temp_buf_unswap (select->float_buf);
  temp_buf_unswap (select->orig_buf);

  /*  swap selections  */
  selection_swap (gdisp->select, select);

  /*  swap the edit buffer of the selection being placed on the undo stack  */
  temp_buf_swap (select->float_buf);
  temp_buf_swap (select->orig_buf);

  if (state == UNDO && trans_undo->initial_move == 1)
    {
      selection_paste_floating (gdisp->select, (void *) gdisp->gimage, COPY_STENCIL);
	  
      /*  since we're undo'ing this all the way to the time it was initially cut,
       *  we should free the buffers also and let them get recreated on a REDO op
       */
      if (gdisp->select->float_buf)
	temp_buf_free (gdisp->select->float_buf);
      if (gdisp->select->orig_buf)
	temp_buf_free (gdisp->select->orig_buf);
      gdisp->select->float_buf = gdisp->select->orig_buf = NULL;
    }
  else
    selection_paste_floating (gdisp->select, (void *) gdisp->gimage, BLEND_STENCIL);

  /*  get the new current selection's bounds  */
  selection_find_bounds (gdisp->select, &x1, &y1, &x2, &y2);

  /*  find the maximum extents  */
  x1 = MINIMUM (x1, x3);
  y1 = MINIMUM (y1, y3);
  x2 = MAXIMUM (x2, x4);
  y2 = MAXIMUM (y2, y4);

  /* if the active tool is the same as the one which was responsible
   * for the original transformation, restore the transformation info
   */
  if (active_tool->type == trans_undo->tool_type && gdisp == active_tool->gdisp_ptr)
    {
      active = (TransformCore *) active_tool->private;
      select = (Selection *) active->select_ptr;

      for (i = 0; i < TRAN_INFO_SIZE; i++)
	{
	  tmp = active->trans_info [i];
	  active->trans_info [i] = trans_undo->trans_info [i];
	  trans_undo->trans_info [i] = tmp;
	}

      active->gregion_id = gdisp->select->region->id;

      if (trans_undo->first)
	/*  if this was the first transformation in a string of transforms,
	 *  reset the active tool
	 */
	transform_core_reset (active_tool, active_tool->gdisp_ptr);
      
    }

  /* update the gdisplays  */
  /*  Make sure to account for the fact that selection bounds extend 1 pixel extra  */
  sm = UNSCALE (gdisp, 1);
  if (sm < 1) sm = 1;
  gdisplay_update (gdisp->gimage->ID, x1, y1, (x2 - x1 + sm), (y2 - y1 + sm), 0);

  return True;
}


void
undo_free_transform (state, trans_undo_ptr)
     int state;
     void * trans_undo_ptr;
{
  Selection * select;
  TransformUndo * trans_undo;

  trans_undo = (TransformUndo *) trans_undo_ptr;
  select = (Selection *) trans_undo->old_select;

  if (select)
    selection_generic_free (select);

  xfree (trans_undo);
}







