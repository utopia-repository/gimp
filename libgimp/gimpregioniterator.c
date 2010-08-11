/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpregioniterator.c
 *
 * FIXME: fix the following comment:
 * Contains all kinds of miscellaneous routines factored out from different
 * plug-ins. They stay here until their API has crystalized a bit and we can
 * put them into the file where they belong (Maurits Rijk
 * <lpeek.mrijk@consunet.nl> if you want to blame someone for this mess)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdio.h>

#include <glib.h>

#include "gimp.h"
#include "gimpregioniterator.h"


struct _GimpRgnIterator
{
  GimpDrawable *drawable;
  gint          x1, y1, x2, y2;
};


static void  gimp_rgn_iterator_iter_single (GimpRgnIterator    *iter,
                                            GimpPixelRgn       *srcPR,
                                            GimpRgnFuncSrc      func,
                                            gpointer            data);
static void  gimp_rgn_render_row           (guchar             *src,
                                            guchar             *dest,
                                            gint                col,
                                            gint                bpp,
                                            GimpRgnFunc2        func,
                                            gpointer            data);
static void  gimp_rgn_render_region        (const GimpPixelRgn *srcPR,
                                            const GimpPixelRgn *destPR,
                                            GimpRgnFunc2        func,
                                            gpointer            data);


/**
 * gimp_rgn_iterator_new:
 * @drawable: a #GimpDrawable
 * @unused:   ignored
 *
 * Creates a new #GimpRgnIterator for @drawable. The #GimpRunMode
 * parameter is ignored.
 *
 * Return value: a newly allocated #GimpRgnIterator.
 **/
GimpRgnIterator *
gimp_rgn_iterator_new (GimpDrawable *drawable,
                       GimpRunMode   unused)
{
  GimpRgnIterator *iter = g_new (GimpRgnIterator, 1);

  iter->drawable = drawable;

  gimp_drawable_mask_bounds (drawable->drawable_id,
                             &iter->x1, &iter->y1,
                             &iter->x2, &iter->y2);

  return iter;
}

/**
 * gimp_rgn_iterator_free:
 * @iter: a #GimpRgnIterator
 *
 * Frees the resources allocated for @iter.
 **/
void
gimp_rgn_iterator_free (GimpRgnIterator *iter)
{
  g_free (iter);
}

void
gimp_rgn_iterator_src (GimpRgnIterator *iter,
                       GimpRgnFuncSrc   func,
                       gpointer         data)
{
  GimpPixelRgn srcPR;

  gimp_pixel_rgn_init (&srcPR, iter->drawable,
                       iter->x1, iter->y1,
                       iter->x2 - iter->x1, iter->y2 - iter->y1,
                       FALSE, FALSE);
  gimp_rgn_iterator_iter_single (iter, &srcPR, func, data);
}

void
gimp_rgn_iterator_src_dest (GimpRgnIterator    *iter,
                            GimpRgnFuncSrcDest  func,
                            gpointer            data)
{
  GimpPixelRgn  srcPR, destPR;
  gint          x1, y1, x2, y2;
  gint          bpp;
  gpointer      pr;
  gint          total_area;
  gint          area_so_far;

  x1 = iter->x1;
  y1 = iter->y1;
  x2 = iter->x2;
  y2 = iter->y2;

  total_area = (x2 - x1) * (y2 - y1);
  area_so_far   = 0;

  gimp_pixel_rgn_init (&srcPR, iter->drawable, x1, y1, x2 - x1, y2 - y1,
                       FALSE, FALSE);
  gimp_pixel_rgn_init (&destPR, iter->drawable, x1, y1, x2 - x1, y2 - y1,
                       TRUE, TRUE);

  bpp = srcPR.bpp;

  for (pr = gimp_pixel_rgns_register (2, &srcPR, &destPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      gint    y;
      guchar* src  = srcPR.data;
      guchar* dest = destPR.data;

      for (y = srcPR.y; y < srcPR.y + srcPR.h; y++)
        {
          gint x;
          guchar *s = src;
          guchar *d = dest;

          for (x = srcPR.x; x < srcPR.x + srcPR.w; x++)
            {
              func (x, y, s, d, bpp, data);
              s += bpp;
              d += bpp;
            }

          src  += srcPR.rowstride;
          dest += destPR.rowstride;
        }

      area_so_far += srcPR.w * srcPR.h;
      gimp_progress_update ((gdouble) area_so_far / (gdouble) total_area);
    }

  gimp_drawable_flush (iter->drawable);
  gimp_drawable_merge_shadow (iter->drawable->drawable_id, TRUE);
  gimp_drawable_update (iter->drawable->drawable_id,
                        x1, y1, x2 - x1, y2 - y1);
}

void
gimp_rgn_iterator_dest (GimpRgnIterator *iter,
                        GimpRgnFuncDest  func,
                        gpointer         data)
{
  GimpPixelRgn destPR;

  gimp_pixel_rgn_init (&destPR, iter->drawable,
                       iter->x1, iter->y1,
                       iter->x2 - iter->x1, iter->y2 - iter->y1,
                       TRUE, TRUE);
  gimp_rgn_iterator_iter_single (iter, &destPR, (GimpRgnFuncSrc) func, data);

  /*  update the processed region  */
  gimp_drawable_flush (iter->drawable);
  gimp_drawable_merge_shadow (iter->drawable->drawable_id, TRUE);
  gimp_drawable_update (iter->drawable->drawable_id,
                        iter->x1, iter->y1,
                        iter->x2 - iter->x1, iter->y2 - iter->y1);
}


void
gimp_rgn_iterate1 (GimpDrawable *drawable,
                   GimpRunMode   unused,
                   GimpRgnFunc1  func,
                   gpointer      data)
{
  GimpPixelRgn  srcPR;
  gint          x1, y1, x2, y2;
  gpointer      pr;
  gint          total_area;
  gint          area_so_far;
  gint          progress_skip;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  total_area = (x2 - x1) * (y2 - y1);

  if (total_area <= 0)
    return;

  area_so_far   = 0;
  progress_skip = 0;

  gimp_pixel_rgn_init (&srcPR, drawable,
                       x1, y1, (x2 - x1), (y2 - y1), FALSE, FALSE);

  for (pr = gimp_pixel_rgns_register (1, &srcPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      guchar *src = srcPR.data;
      gint    y;

      for (y = 0; y < srcPR.h; y++)
        {
          guchar *s = src;
          gint    x;

          for (x = 0; x < srcPR.w; x++)
            {
              func (s, srcPR.bpp, data);
              s += srcPR.bpp;
            }

          src += srcPR.rowstride;
        }

      area_so_far += srcPR.w * srcPR.h;

      if (((progress_skip++) % 10) == 0)
        gimp_progress_update ((gdouble) area_so_far / (gdouble) total_area);
    }
}

void
gimp_rgn_iterate2 (GimpDrawable *drawable,
                   GimpRunMode   unused,
                   GimpRgnFunc2  func,
                   gpointer      data)
{
  GimpPixelRgn  srcPR, destPR;
  gint          x1, y1, x2, y2;
  gpointer      pr;
  gint          total_area;
  gint          area_so_far;
  gint          progress_skip;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  total_area = (x2 - x1) * (y2 - y1);

  if (total_area <= 0)
    return;

  area_so_far   = 0;
  progress_skip = 0;

  /* Initialize the pixel regions. */
  gimp_pixel_rgn_init (&srcPR, drawable, x1, y1, (x2 - x1), (y2 - y1),
                       FALSE, FALSE);
  gimp_pixel_rgn_init (&destPR, drawable, x1, y1, (x2 - x1), (y2 - y1),
                       TRUE, TRUE);

  for (pr = gimp_pixel_rgns_register (2, &srcPR, &destPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      gimp_rgn_render_region (&srcPR, &destPR, func, data);

      area_so_far += srcPR.w * srcPR.h;

      if (((progress_skip++) % 10) == 0)
        gimp_progress_update ((gdouble) area_so_far / (gdouble) total_area);
    }

  /*  update the processed region  */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));
}

static void
gimp_rgn_iterator_iter_single (GimpRgnIterator *iter,
                               GimpPixelRgn    *srcPR,
                               GimpRgnFuncSrc   func,
                               gpointer         data)
{
  gpointer  pr;
  gint      total_area;
  gint      area_so_far;

  total_area = (iter->x2 - iter->x1) * (iter->y2 - iter->y1);
  area_so_far   = 0;

  for (pr = gimp_pixel_rgns_register (1, srcPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      guchar *src = srcPR->data;
      gint    y;

      for (y = srcPR->y; y < srcPR->y + srcPR->h; y++)
        {
          guchar *s = src;
          gint x;

          for (x = srcPR->x; x < srcPR->x + srcPR->w; x++)
            {
              func (x, y, s, srcPR->bpp, data);
              s += srcPR->bpp;
            }

          src += srcPR->rowstride;
        }

      area_so_far += srcPR->w * srcPR->h;
      gimp_progress_update ((gdouble) area_so_far / (gdouble) total_area);
    }
}

static void
gimp_rgn_render_row (guchar       *src,
                     guchar       *dest,
                     gint          col,    /* row width in pixels */
                     gint          bpp,
                     GimpRgnFunc2  func,
                     gpointer      data)
{
  while (col--)
    {
      func (src, dest, bpp, data);
      src += bpp;
      dest += bpp;
    }
}

static void
gimp_rgn_render_region (const GimpPixelRgn *srcPR,
                        const GimpPixelRgn *destPR,
                        GimpRgnFunc2        func,
                        gpointer            data)
{
  guchar *src  = srcPR->data;
  guchar *dest = destPR->data;
  gint    row;

  for (row = 0; row < srcPR->h; row++)
    {
      gimp_rgn_render_row (src, dest, srcPR->w, srcPR->bpp, func, data);

      src  += srcPR->rowstride;
      dest += destPR->rowstride;
    }
}
