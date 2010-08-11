/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * pixel_processor.c: Copyright (C) 1999 Jay Cox <jaycox@earthlink.net>
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

#ifdef ENABLE_MP
#include <pthread.h>
#endif

#include <string.h>

#include <glib-object.h>

#include "base-types.h"

#include "config/gimpbaseconfig.h"

#include "pixel-processor.h"
#include "pixel-region.h"

#ifdef ENABLE_MP
#include "tile.h"
#endif

#ifdef __GNUC__
#warning FIXME: extern GimpBaseConfig *base_config;
#endif
extern GimpBaseConfig *base_config;


typedef void (* p1_func) (gpointer     ,
			  PixelRegion *);
typedef void (* p2_func) (gpointer     ,
			  PixelRegion * ,
			  PixelRegion *);
typedef void (* p3_func) (gpointer     ,
			  PixelRegion *,
			  PixelRegion *,
			  PixelRegion *);
typedef void (* p4_func) (gpointer     ,
			  PixelRegion *,
			  PixelRegion *,
			  PixelRegion *,
			  PixelRegion *);


struct _PixelProcessor
{
  gpointer             data;
  p_func               f;
  PixelRegionIterator *PRI;

#ifdef ENABLE_MP
  pthread_mutex_t      mutex;
  gint                 nthreads;
#endif

  gint                 n_regions;
  PixelRegion         *r[4];

  void                *progress_report_data;
  ProgressReportFunc   progress_report_func;
};


#ifdef ENABLE_MP
static void *
do_parallel_regions (PixelProcessor *p_s)
{
  PixelRegion tr[4];
  gint        n_tiles = 0;
  gint        i;
  gint        cont = 1;

  pthread_mutex_lock (&p_s->mutex);

  if (p_s->nthreads != 0 && p_s->PRI)
    p_s->PRI = pixel_regions_process (p_s->PRI);

  if (p_s->PRI == NULL)
    {
      pthread_mutex_unlock (&p_s->mutex);
      return NULL;
    }

  p_s->nthreads++;

  do
    {
      for (i = 0; i < p_s->n_regions; i++)
	if (p_s->r[i])
	  {
	    memcpy(&tr[i], p_s->r[i], sizeof(PixelRegion));
	    if (tr[i].tiles)
	      tile_lock(tr[i].curtile);
	  }

      pthread_mutex_unlock (&p_s->mutex);
      n_tiles++;

      switch(p_s->n_regions)
	{
	case 1:
	  ((p1_func) p_s->f) (p_s->data,
                              p_s->r[0] ? &tr[0] : NULL);
	  break;

	case 2:
	  ((p2_func) p_s->f) (p_s->data,
                              p_s->r[0] ? &tr[0] : NULL,
                              p_s->r[1] ? &tr[1] : NULL);
	  break;

	case 3:
	  ((p3_func) p_s->f) (p_s->data,
                              p_s->r[0] ? &tr[0] : NULL,
                              p_s->r[1] ? &tr[1] : NULL,
                              p_s->r[2] ? &tr[2] : NULL);
	  break;

	case 4:
	  ((p4_func) p_s->f) (p_s->data,
                              p_s->r[0] ? &tr[0] : NULL,
                              p_s->r[1] ? &tr[1] : NULL,
                              p_s->r[2] ? &tr[2] : NULL,
                              p_s->r[3] ? &tr[3] : NULL);
	  break;

	default:
	  g_warning ("do_parallel_regions: Bad number of regions %d\n",
                     p_s->n_regions);
          break;
    }

    pthread_mutex_lock (&p_s->mutex);

    for (i = 0; i < p_s->n_regions; i++)
      if (p_s->r[i])
	{
	  if (tr[i].tiles)
	    tile_release (tr[i].curtile, tr[i].dirty);
	}

    if (p_s->progress_report_func && 
	!p_s->progress_report_func (p_s->progress_report_data,
                                    p_s->r[0]->x, p_s->r[0]->y, 
                                    p_s->r[0]->w, p_s->r[0]->h))
      cont = 0;

    } 

  while (cont && p_s->PRI &&
	 (p_s->PRI = pixel_regions_process (p_s->PRI)));

  p_s->nthreads--;

  pthread_mutex_unlock (&p_s->mutex);

  return NULL;
}
#endif

/*  do_parallel_regions_single is just like do_parallel_regions 
 *   except that all the mutex and tile locks have been removed
 *
 * If we are processing with only a single thread we don't need to do the
 * mutex locks etc. and aditional tile locks even if we were
 * configured --with-mp
 */

static gpointer
do_parallel_regions_single (PixelProcessor *p_s)
{
  gint cont = 1;

  do
    {
      switch (p_s->n_regions)
        {
        case 1:
          ((p1_func) p_s->f) (p_s->data,
                              p_s->r[0]);
          break;
          
        case 2:
          ((p2_func) p_s->f) (p_s->data,
                              p_s->r[0],
                              p_s->r[1]);
          break;

        case 3:
          ((p3_func) p_s->f) (p_s->data,
                              p_s->r[0],
                              p_s->r[1],
                              p_s->r[2]);
          break;
          
        case 4:
          ((p4_func) p_s->f) (p_s->data,
                              p_s->r[0],
                              p_s->r[1],
                              p_s->r[2],
                              p_s->r[3]);
          break;

        default:
          g_warning ("do_parallel_regions_single: Bad number of regions %d\n",
                     p_s->n_regions);
        }

      if (p_s->progress_report_func && 
          !p_s->progress_report_func (p_s->progress_report_data,
                                      p_s->r[0]->x, p_s->r[0]->y, 
                                      p_s->r[0]->w, p_s->r[0]->h))
        cont = 0;
    }

  while (cont && p_s->PRI &&
	 (p_s->PRI = pixel_regions_process (p_s->PRI)));

  return NULL;
}

#define MAX_THREADS 30

static void
pixel_regions_do_parallel (PixelProcessor *p_s)
{
#ifdef ENABLE_MP
  gint nthreads;

  nthreads = MIN (base_config->num_processors, MAX_THREADS);

  /* make sure we have at least one tile per thread */
  nthreads = MIN (nthreads,
		  (p_s->PRI->region_width * p_s->PRI->region_height)
		  / (TILE_WIDTH * TILE_HEIGHT));

  if (nthreads > 1)
    {
      gint           i;
      pthread_t      threads[MAX_THREADS];
      pthread_attr_t pthread_attr;

      pthread_attr_init (&pthread_attr);

      for (i = 0; i < nthreads; i++)
        {
	  pthread_create (&threads[i], &pthread_attr,
			  (void *(*)(void *)) do_parallel_regions,
			  p_s);
	}
      for (i = 0; i < nthreads; i++)
	{
	  gint ret;

	  if ((ret = pthread_join (threads[i], NULL)))
	    {
	      g_printerr ("pixel_regions_do_parallel: "
                          "pthread_join returned: %d\n", ret);
	    }
	}

      if (p_s->nthreads != 0)
	g_printerr ("pixel_regions_do_prarallel: we lost a thread\n");
    }
  else
#endif

    do_parallel_regions_single (p_s);
}

static PixelProcessor *
pixel_regions_real_process_parallel (p_func             f, 
				     gpointer           data,
				     ProgressReportFunc report_func,
				     gpointer           report_data,
				     gint               num_regions, 
				     va_list            ap)
{
  gint            i;
  PixelProcessor *p_s;

  p_s = g_new (PixelProcessor, 1);

  for (i = 0; i < num_regions; i++)
    p_s->r[i] = va_arg (ap, PixelRegion *);

  switch(num_regions)
    {
    case 1:
      p_s->PRI = pixel_regions_register (num_regions,
                                         p_s->r[0]);
      break;

    case 2:
      p_s->PRI = pixel_regions_register (num_regions,
                                         p_s->r[0],
                                         p_s->r[1]);
      break;

    case 3:
      p_s->PRI = pixel_regions_register (num_regions,
                                         p_s->r[0],
                                         p_s->r[1],
                                         p_s->r[2]);
      break;

    case 4:
      p_s->PRI = pixel_regions_register (num_regions,
                                         p_s->r[0],
                                         p_s->r[1],
                                         p_s->r[2],
                                         p_s->r[3]);
      break;

    default:
      g_warning ("pixel_regions_real_process_parallel:"
                 "Bad number of regions %d\n", p_s->n_regions);
  }

  if (!p_s->PRI)
    {
      pixel_processor_free (p_s);
      return NULL;
    }

  p_s->f = f;
  p_s->data = data;
  p_s->n_regions = num_regions;

#ifdef ENABLE_MP
  pthread_mutex_init (&p_s->mutex, NULL);
  p_s->nthreads = 0;
#endif

  p_s->progress_report_data = report_data;
  p_s->progress_report_func = report_func;

  pixel_regions_do_parallel (p_s);

  if (p_s->PRI)
    return p_s;

#ifdef ENABLE_MP
  pthread_mutex_destroy (&p_s->mutex);
#endif

  pixel_processor_free (p_s);

  return NULL;
}

void
pixel_regions_process_parallel (p_func   f, 
				gpointer data, 
				gint     num_regions, 
				...)
{
  va_list va;

  va_start (va, num_regions);

  pixel_regions_real_process_parallel (f, data, NULL, NULL, num_regions, va);

  va_end (va);
}

PixelProcessor *
pixel_regions_process_parallel_progress (p_func             f, 
					 gpointer           data,
					 ProgressReportFunc progress_func,
					 gpointer           progress_data, 
					 gint               num_regions,
					 ...)
{
  PixelProcessor *ret;
  va_list         va;

  va_start (va, num_regions);

  ret = pixel_regions_real_process_parallel (f, data,
					     progress_func, progress_data,
					     num_regions, va);

  va_end (va);

  return ret;
}

void
pixel_processor_stop (PixelProcessor *pp)
{
  if (!pp)
    return;

  if (pp->PRI)
    {
      pixel_regions_process_stop (pp->PRI);
      pp->PRI = NULL;
    }

  pixel_processor_free (pp);
}

PixelProcessor *
pixel_processor_cont (PixelProcessor *pp)
{
  pixel_regions_do_parallel (pp);

  if (pp->PRI)
    return pp;

  pixel_processor_free (pp);

  return NULL;
}

void
pixel_processor_free (PixelProcessor *pp)
{
  if (pp->PRI)
    pixel_processor_stop (pp);
  else
    g_free(pp);
}
