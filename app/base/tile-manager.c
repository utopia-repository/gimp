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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include "base-types.h"

#include "tile.h"
#include "tile-private.h"
#include "tile-cache.h"
#include "tile-manager.h"
#include "tile-manager-private.h"
#include "tile-swap.h"


static gint  tile_manager_get_tile_num (TileManager *tm,
                                        gint         xpixel,
                                        gint         ypixel);


TileManager *
tile_manager_new (gint toplevel_width,
		  gint toplevel_height,
		  gint bpp)
{
  TileManager *tm;
  gint         width;
  gint         height;

  g_return_val_if_fail (toplevel_width > 0, NULL);
  g_return_val_if_fail (toplevel_height > 0, NULL);
  g_return_val_if_fail (bpp  > 0, NULL);

  tm = g_new0 (TileManager, 1);

  width  = toplevel_width;
  height = toplevel_height;

  tm->ref_count     = 1;
  tm->x             = 0;
  tm->y             = 0;
  tm->width         = width;
  tm->height        = height;
  tm->bpp           = bpp;
  tm->ntile_rows    = (height + TILE_HEIGHT - 1) / TILE_HEIGHT;
  tm->ntile_cols    = (width  + TILE_WIDTH  - 1) / TILE_WIDTH;
  tm->tiles         = NULL;
  tm->validate_proc = NULL;

  tm->cached_num    = -1;
  tm->cached_tile   = NULL;

  tm->user_data     = NULL;

  return tm;
}

TileManager *
tile_manager_ref (TileManager *tm)
{
  g_return_val_if_fail (tm != NULL, NULL);

  tm->ref_count++;

  return tm;
}

void
tile_manager_unref (TileManager *tm)
{
  g_return_if_fail (tm != NULL);

  tm->ref_count--;

  if (tm->ref_count < 1)
    {
      gint ntiles;
      gint i;

      if (tm->cached_tile)
        tile_release (tm->cached_tile, FALSE);

      if (tm->tiles)
        {
          ntiles = tm->ntile_rows * tm->ntile_cols;

          for (i = 0; i < ntiles; i++)
            {
              TILE_MUTEX_LOCK (tm->tiles[i]);
              tile_detach (tm->tiles[i], tm, i);
            }

          g_free (tm->tiles);
        }

      g_free (tm);
    }
}


void
tile_manager_set_validate_proc (TileManager      *tm,
				TileValidateProc  proc)
{
  g_return_if_fail (tm != NULL);

  tm->validate_proc = proc;
}


Tile *
tile_manager_get_tile (TileManager *tm,
		       gint         xpixel,
		       gint         ypixel,
		       gint         wantread,
		       gint         wantwrite)
{
  gint tile_num;

  g_return_val_if_fail (tm != NULL, NULL);

  tile_num = tile_manager_get_tile_num (tm, xpixel, ypixel);
  if (tile_num < 0)
    return NULL;

  return tile_manager_get (tm, tile_num, wantread, wantwrite);
}

Tile *
tile_manager_get (TileManager *tm,
		  gint         tile_num,
		  gint         wantread,
		  gint         wantwrite)
{
  Tile **tiles;
  Tile **tile_ptr;
  gint   ntiles;
  gint   nrows, ncols;
  gint   right_tile;
  gint   bottom_tile;
  gint   i, j, k;

  g_return_val_if_fail (tm != NULL, NULL);

  ntiles = tm->ntile_rows * tm->ntile_cols;

  if ((tile_num < 0) || (tile_num >= ntiles))
    return NULL;

  if (!tm->tiles)
    {
      tm->tiles = g_new (Tile *, ntiles);
      tiles = tm->tiles;

      nrows = tm->ntile_rows;
      ncols = tm->ntile_cols;

      right_tile  = tm->width  - ((ncols - 1) * TILE_WIDTH);
      bottom_tile = tm->height - ((nrows - 1) * TILE_HEIGHT);

      for (i = 0, k = 0; i < nrows; i++)
	{
	  for (j = 0; j < ncols; j++, k++)
	    {
	      tiles[k] = g_new (Tile, 1);
	      tile_init (tiles[k], tm->bpp);
	      tile_attach (tiles[k], tm, k);

	      if (j == (ncols - 1))
		tiles[k]->ewidth = right_tile;

	      if (i == (nrows - 1))
		tiles[k]->eheight = bottom_tile;
	    }
	}
    }

  tile_ptr = &tm->tiles[tile_num];

  if (wantwrite && !wantread)
    {
      g_warning ("WRITE-ONLY TILE... UNTESTED!");
    }

#ifdef DEBUG_TILE_MANAGER
  if ((*tile_ptr)->share_count && (*tile_ptr)->write_count)
    g_printerr (">> MEEPITY %d,%d <<\n",
                (*tile_ptr)->share_count, (*tile_ptr)->write_count);
#endif

  if (wantread)
    {
      TILE_MUTEX_LOCK (*tile_ptr);
      if (wantwrite)
	{
	  if ((*tile_ptr)->share_count > 1)
	    {
	      /* Copy-on-write required */
	      Tile *newtile = g_new (Tile, 1);
              gint  newsize;

	      tile_init (newtile, (*tile_ptr)->bpp);

	      newtile->ewidth  = (*tile_ptr)->ewidth;
	      newtile->eheight = (*tile_ptr)->eheight;
	      newtile->valid   = (*tile_ptr)->valid;

              newsize = tile_size_inline (newtile);
	      newtile->data    = g_new (guchar, newsize);

	      if (!newtile->valid)
		g_warning ("Oh boy, r/w tile is invalid... we suck. "
                           "Please report.");

              if ((*tile_ptr)->rowhint)
                newtile->rowhint = g_memdup ((*tile_ptr)->rowhint,
                                             newtile->eheight *
                                             sizeof (TileRowHint));

	      if ((*tile_ptr)->data)
		{
		  memcpy (newtile->data, (*tile_ptr)->data, newsize);
		}
	      else
		{
		  tile_lock (*tile_ptr);
		  memcpy (newtile->data, (*tile_ptr)->data, newsize);
		  tile_release (*tile_ptr, FALSE);
		}

	      tile_detach (*tile_ptr, tm, tile_num);
	      TILE_MUTEX_LOCK (newtile);
	      tile_attach (newtile, tm, tile_num);
	      *tile_ptr = newtile;
	    }

	  (*tile_ptr)->write_count++;
	  (*tile_ptr)->dirty = TRUE;
	}
#ifdef DEBUG_TILE_MANAGER
      else
	{
	  if ((*tile_ptr)->write_count)
	    g_printerr ("STINK! r/o on r/w tile (%d)\n",
                        (*tile_ptr)->write_count);
	}
#endif

      TILE_MUTEX_UNLOCK (*tile_ptr);
      tile_lock (*tile_ptr);
    }

  return *tile_ptr;
}

void
tile_manager_get_async (TileManager *tm,
                        gint         xpixel,
                        gint         ypixel)
{
  Tile *tile_ptr;
  gint  tile_num;

  g_return_if_fail (tm != NULL);

  tile_num = tile_manager_get_tile_num (tm, xpixel, ypixel);
  if (tile_num < 0)
    return;

  tile_ptr = tm->tiles[tile_num];

  tile_swap_in_async (tile_ptr);
}

void
tile_manager_validate (TileManager *tm,
		       Tile        *tile)
{
  g_return_if_fail (tm != NULL);
  g_return_if_fail (tile != NULL);

  tile->valid = TRUE;

  if (tm->validate_proc)
    (* tm->validate_proc) (tm, tile);

#ifdef DEBUG_TILE_MANAGER
  g_printerr ("%c", tm->user_data ? 'V' : 'v');
#endif
}

void
tile_manager_invalidate_tiles (TileManager *tm,
			       Tile        *toplevel_tile)
{
  gdouble x, y;
  gint    row, col;
  gint    num;

  g_return_if_fail (tm != NULL);
  g_return_if_fail (toplevel_tile != NULL);

  col = toplevel_tile->tlink->tile_num % tm->ntile_cols;
  row = toplevel_tile->tlink->tile_num / tm->ntile_cols;

  x = ((col * TILE_WIDTH  + toplevel_tile->ewidth  / 2.0) /
       (gdouble) tm->width);
  y = ((row * TILE_HEIGHT + toplevel_tile->eheight / 2.0) /
       (gdouble) tm->height);

  if (tm->tiles)
    {
      col = x * tm->width / TILE_WIDTH;
      row = y * tm->height / TILE_HEIGHT;
      num = row * tm->ntile_cols + col;
      tile_invalidate (&tm->tiles[num], tm, num);
    }
}


void
tile_invalidate_tile (Tile        **tile_ptr,
		      TileManager  *tm,
		      gint          xpixel,
		      gint          ypixel)
{
  gint tile_num;

  g_return_if_fail (tile_ptr != NULL);
  g_return_if_fail (tm != NULL);

  tile_num = tile_manager_get_tile_num (tm, xpixel, ypixel);
  if (tile_num < 0)
    return;

  tile_invalidate (tile_ptr, tm, tile_num);
}


void
tile_invalidate (Tile        **tile_ptr,
		 TileManager  *tm,
		 gint          tile_num)
{
  Tile *tile = *tile_ptr;

  g_return_if_fail (tile_ptr != NULL);
  g_return_if_fail (tm != NULL);

  TILE_MUTEX_LOCK (tile);

  if (!tile->valid)
    goto leave;

  if (tile->share_count > 1)
    {
      /* This tile is shared.  Replace it with a new, invalid tile. */
      Tile *newtile = g_new (Tile, 1);

      g_print ("invalidating shared tile (executing buggy code!!!)\n");

      tile_init (newtile, tile->bpp);
      newtile->ewidth  = tile->ewidth;
      newtile->eheight = tile->eheight;
      tile_detach (tile, tm, tile_num);
      TILE_MUTEX_LOCK (newtile);
      tile_attach (newtile, tm, tile_num);
      tile = *tile_ptr = newtile;
    }

  if (tile->listhead)
    tile_cache_flush (tile);

  tile->valid = FALSE;
  if (tile->data)
    {
      g_free (tile->data);
      tile->data = NULL;
    }
  if (tile->swap_offset != -1)
    {
      /* If the tile is on disk, then delete its
       *  presence there.
       */
      tile_swap_delete (tile);
    }

leave:
  TILE_MUTEX_UNLOCK (tile);
}


void
tile_manager_map_tile (TileManager *tm,
		       gint         xpixel,
		       gint         ypixel,
		       Tile        *srctile)
{
  gint tile_row;
  gint tile_col;
  gint tile_num;

  g_return_if_fail (tm != NULL);
  g_return_if_fail (srctile != NULL);

  if ((xpixel < 0) || (xpixel >= tm->width) ||
      (ypixel < 0) || (ypixel >= tm->height))
    {
      g_warning ("tile_manager_map_tile: tile co-ord out of range.");
      return;
    }

  tile_row = ypixel / TILE_HEIGHT;
  tile_col = xpixel / TILE_WIDTH;
  tile_num = tile_row * tm->ntile_cols + tile_col;

  tile_manager_map (tm, tile_num, srctile);
}

void
tile_manager_map (TileManager *tm,
		  gint         tile_num,
		  Tile        *srctile)
{
  Tile **tiles;
  Tile **tile_ptr;
  gint   ntiles;
  gint   nrows, ncols;
  gint   right_tile;
  gint   bottom_tile;
  gint   i, j, k;

  g_return_if_fail (tm != NULL);
  g_return_if_fail (srctile != NULL);

  ntiles = tm->ntile_rows * tm->ntile_cols;

  if ((tile_num < 0) || (tile_num >= ntiles))
    {
      g_warning ("tile_manager_map: tile out of range.");
      return;
    }

  if (!tm->tiles)
    {
      g_warning ("tile_manager_map: empty tile level - init'ing.");

      tm->tiles = g_new (Tile *, ntiles);
      tiles = tm->tiles;

      nrows = tm->ntile_rows;
      ncols = tm->ntile_cols;

      right_tile  = tm->width  - ((ncols - 1) * TILE_WIDTH);
      bottom_tile = tm->height - ((nrows - 1) * TILE_HEIGHT);

      for (i = 0, k = 0; i < nrows; i++)
	{
	  for (j = 0; j < ncols; j++, k++)
	    {
#ifdef DEBUG_TILE_MANAGER
	      g_printerr (",");
#endif
	      tiles[k] = g_new (Tile, 1);
	      tile_init (tiles[k], tm->bpp);
	      tile_attach (tiles[k], tm, k);

	      if (j == (ncols - 1))
		tiles[k]->ewidth = right_tile;

	      if (i == (nrows - 1))
		tiles[k]->eheight = bottom_tile;
	    }
	}
    }

  tile_ptr = &tm->tiles[tile_num];

#ifdef DEBUG_TILE_MANAGER
  g_printerr (")");
#endif

  if (!srctile->valid)
    g_warning("tile_manager_map: srctile not validated yet!  please report.");

  TILE_MUTEX_LOCK (*tile_ptr);
  if ((*tile_ptr)->ewidth  != srctile->ewidth ||
      (*tile_ptr)->eheight != srctile->eheight ||
      (*tile_ptr)->bpp     != srctile->bpp)
    {
      g_warning ("tile_manager_map: nonconformant map (%p -> %p)",
		 srctile, *tile_ptr);
    }
  tile_detach (*tile_ptr, tm, tile_num);

#ifdef DEBUG_TILE_MANAGER
  g_printerr (">");
#endif

  TILE_MUTEX_LOCK (srctile);

#ifdef DEBUG_TILE_MANAGER
  g_printerr (" [src:%p tm:%p tn:%d] ", srctile, tm, tile_num);
#endif

  tile_attach (srctile, tm, tile_num);
  *tile_ptr = srctile;

  TILE_MUTEX_UNLOCK (srctile);

#ifdef DEBUG_TILE_MANAGER
  g_printerr ("}\n");
#endif
}

static gint
tile_manager_get_tile_num (TileManager *tm,
		           gint         xpixel,
		           gint         ypixel)
{
  gint tile_row;
  gint tile_col;
  gint tile_num;

  g_return_val_if_fail (tm != NULL, -1);

  if ((xpixel < 0) || (xpixel >= tm->width) ||
      (ypixel < 0) || (ypixel >= tm->height))
    return -1;

  tile_row = ypixel / TILE_HEIGHT;
  tile_col = xpixel / TILE_WIDTH;
  tile_num = tile_row * tm->ntile_cols + tile_col;

  return tile_num;
}

void
tile_manager_set_user_data (TileManager *tm,
			    gpointer     user_data)
{
  g_return_if_fail (tm != NULL);

  tm->user_data = user_data;
}

gpointer
tile_manager_get_user_data (const TileManager *tm)
{
  g_return_val_if_fail (tm != NULL, NULL);

  return tm->user_data;
}

gint
tile_manager_width  (const TileManager *tm)
{
  g_return_val_if_fail (tm != NULL, 0);

  return tm->width;
}

gint
tile_manager_height (const TileManager *tm)
{
  g_return_val_if_fail (tm != NULL, 0);

  return tm->height;
}

gint
tile_manager_bpp (const TileManager *tm)
{
  g_return_val_if_fail (tm != NULL, 0);

  return tm->bpp;
}

void
tile_manager_get_offsets (const TileManager *tm,
			  gint              *x,
			  gint              *y)
{
  g_return_if_fail (x!= NULL && y != NULL);

  *x = tm->x;
  *y = tm->y;
}

void
tile_manager_set_offsets (TileManager *tm,
			  gint         x,
			  gint         y)
{
  g_return_if_fail (tm != NULL);

  tm->x = x;
  tm->y = y;
}

gint64
tile_manager_get_memsize (const TileManager *tm,
                          gboolean           sparse)
{
  gint64 memsize;

  g_return_val_if_fail (tm != NULL, 0);

  /*  the tile manager itself  */
  memsize = sizeof (TileManager);

  /*  the array of tiles  */
  memsize += (gint64) tm->ntile_rows * tm->ntile_cols * (sizeof (Tile) +
                                                         sizeof (gpointer));

  /*  the memory allocated for the tiles   */
  if (sparse)
    {
      if (tm->tiles)
        {
          Tile   **tiles = tm->tiles;
          gint64   size  = TILE_WIDTH * TILE_HEIGHT * tm->bpp;
          gint     i, j;

          for (i = 0; i < tm->ntile_rows; i++)
            for (j = 0; j < tm->ntile_cols; j++, tiles++)
              {
                if (tile_is_valid (*tiles))
                  memsize += size;
              }
        }
    }
  else
    {
      memsize += (gint64) tm->width * tm->height * tm->bpp;
    }

  return memsize;
}

void
tile_manager_get_tile_coordinates (TileManager *tm,
				   Tile        *tile,
				   gint        *x,
				   gint        *y)
{
  TileLink *tl;

  g_return_if_fail (tm != NULL);
  g_return_if_fail (x != NULL && y != NULL);

  for (tl = tile->tlink; tl; tl = tl->next)
    {
      if (tl->tm == tm) break;
    }

  if (tl == NULL)
    {
      g_warning ("tile_manager_get_tile_coordinates: "
                 "tile not attached to manager");
      return;
    }

  *x = TILE_WIDTH * (tl->tile_num % tm->ntile_cols);
  *y = TILE_HEIGHT * (tl->tile_num / tm->ntile_cols);
}


void
tile_manager_map_over_tile (TileManager *tm,
			    Tile        *tile,
			    Tile        *srctile)
{
  TileLink *tl;

  g_return_if_fail (tm != NULL);
  g_return_if_fail (tile != NULL);
  g_return_if_fail (srctile != NULL);

  for (tl = tile->tlink; tl; tl = tl->next)
    {
      if (tl->tm == tm) break;
    }

  if (tl == NULL)
    {
      g_warning ("tile_manager_map_over_tile: tile not attached to manager");
      return;
    }

  tile_manager_map (tm, tl->tile_num, srctile);
}

PixelDataHandle *
request_pixel_data (TileManager *tm,
		    gint         x1,
		    gint	 y1,
		    gint	 x2,
		    gint	 y2,
		    gboolean	 wantread,
		    gboolean     wantwrite)
{
  PixelDataHandlePrivate *pdh;
  guint tile_num_1, tile_num_2;
  guint w, h;

  pdh = g_new (PixelDataHandlePrivate, 1);
  pdh->tm = tm;
  pdh->readable  = wantread;
  pdh->writeable = wantwrite;
  pdh->x1 = x1;
  pdh->y1 = y1;
  pdh->x2 = x2;
  pdh->y2 = y2;
  pdh->public.width = w = (x2-x1)+1;
  pdh->public.height = h = (y2-y1)+1;

  tile_num_1 = tile_manager_get_tile_num (tm, x1, y1);
  tile_num_2 = tile_manager_get_tile_num (tm, x2, y2);

  if (tile_num_1 == tile_num_2)
    {
      pdh->tile = tile_manager_get (tm, tile_num_1, wantread, wantwrite);
      pdh->public.data = tile_data_pointer (pdh->tile,
                                            x1 % TILE_WIDTH,
                                            y1 % TILE_HEIGHT);
      pdh->public.stride = tile_bpp (pdh->tile) * tile_ewidth (pdh->tile);
      pdh->local_buffer = FALSE;
    }
  else
    {
      pdh->public.data = g_new (guchar, w * h * tm->bpp);
      pdh->public.stride = tm->bpp * w;
      pdh->local_buffer = TRUE;
      pdh->tile = NULL;

      if (wantread)
	read_pixel_data (tm, x1, y1, x2, y2,
			 pdh->public.data, pdh->public.stride);
    }
  return (PixelDataHandle *) pdh;
}

void
release_pixel_data (PixelDataHandle *xpdh)
{
  PixelDataHandlePrivate *pdh = (PixelDataHandlePrivate *)(xpdh);

  if (pdh->local_buffer)
    {
      if (pdh->writeable)
	write_pixel_data (pdh->tm, pdh->x1, pdh->y1, pdh->x2, pdh->y2,
			  pdh->public.data, pdh->public.stride);
      g_free (pdh->public.data);
    }
  else
    {
      tile_release (pdh->tile, pdh->writeable);
    }

  g_free (pdh);
}

void
read_pixel_data (TileManager *tm,
		 gint         x1,
		 gint         y1,
		 gint         x2,
		 gint         y2,
		 guchar      *buffer,
		 guint        stride)
{
  Tile   *t;
  guchar *s, *d;
  guint   x, y;
  guint   rows, cols;
  guint   srcstride;

  for (y = y1; y <= y2; y += TILE_HEIGHT - (y % TILE_HEIGHT))
    for (x = x1; x <= x2; x += TILE_WIDTH - (x % TILE_WIDTH))
      {
	t = tile_manager_get_tile (tm, x, y, TRUE, FALSE);
	s = tile_data_pointer (t, x % TILE_WIDTH, y % TILE_HEIGHT);
	d = buffer + stride * (y - y1) + tm->bpp * (x - x1);
	rows = tile_eheight (t) - y % TILE_HEIGHT;
	if (rows > (y2 - y + 1))
	  rows = y2 - y + 1;

	cols = tile_ewidth (t) - x % TILE_WIDTH;
	if (cols > (x2 - x + 1))
	  cols = x2 - x + 1;

	srcstride = tile_ewidth (t) * tile_bpp (t);

	while (rows --)
	  {
	    memcpy (d, s, cols * tm->bpp);
	    s += srcstride;
	    d += stride;
	  }

	tile_release (t, FALSE);
      }
}

void
write_pixel_data (TileManager *tm,
		  gint         x1,
		  gint         y1,
		  gint         x2,
		  gint         y2,
		  guchar      *buffer,
		  guint        stride)
{
  Tile   *t;
  guchar *s, *d;
  guint   x, y;
  guint   rows, cols;
  guint   dststride;

  for (y = y1; y <= y2; y += TILE_HEIGHT - (y % TILE_HEIGHT))
    for (x = x1; x <= x2; x += TILE_WIDTH - (x % TILE_WIDTH))
      {
	t = tile_manager_get_tile (tm, x, y, TRUE, TRUE);
	s = buffer + stride * (y - y1) + tm->bpp * (x - x1);
	d = tile_data_pointer (t, x % TILE_WIDTH, y % TILE_HEIGHT);
	rows = tile_eheight (t) - y % TILE_HEIGHT;
	if (rows > (y2 - y + 1))
	  rows = y2 - y + 1;

	cols = tile_ewidth (t) - x % TILE_WIDTH;
	if (cols > (x2 - x + 1))
	  cols = x2 - x + 1;

	dststride = tile_ewidth (t) * tile_bpp (t);

	while (rows --)
	  {
	    memcpy (d, s, cols * tm->bpp);
	    s += stride;
	    d += dststride;
	  }

	tile_release (t, TRUE);
      }
}

void
read_pixel_data_1 (TileManager *tm,
		   gint	         x,
		   gint          y,
		   guchar       *buffer)
{
  if (x >= 0 && y >= 0 && x < tm->width && y < tm->height)
    {
      gint num = tile_manager_get_tile_num (tm, x, y);

      if (num != tm->cached_num)    /* must fetch a new tile */
        {
           if (tm->cached_tile)
             tile_release (tm->cached_tile, FALSE);

           tm->cached_num = num;
           tm->cached_tile = tile_manager_get (tm, num, TRUE, FALSE);
        }

      if (tm->cached_tile)
        {
           guchar *data = tile_data_pointer (tm->cached_tile,
                                             x % TILE_WIDTH, y % TILE_HEIGHT);
           switch (tm->bpp)
             {
             case 4:
               *(guint32*)buffer = *(guint32*)data;
               break;
             default:
               {
                 gint i;
                 for (i = 0; i < tm->bpp; i++)
                   buffer[i] = data[i];
               }
               break;
             }
        }
    }
}

void
write_pixel_data_1 (TileManager *tm,
		    gint	  x,
		    gint	  y,
		    guchar       *buffer)
{
  Tile   *t;
  guchar *d;

  t = tile_manager_get_tile (tm, x, y, TRUE, TRUE);
  d = tile_data_pointer (t, x % TILE_WIDTH, y % TILE_HEIGHT);
  memcpy (d, buffer, tm->bpp);
  tile_release (t, TRUE);
}
