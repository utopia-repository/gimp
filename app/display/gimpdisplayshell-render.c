/* GIMP - The GNU Image Manipulation Program
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "base/tile-manager.h"
#include "base/tile.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-colormap.h"
#include "core/gimpprojection.h"

#include "widgets/gimprender.h"

#include "gimpcanvas.h"
#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-filter.h"
#include "gimpdisplayshell-render.h"


typedef struct _RenderInfo  RenderInfo;

typedef void (* RenderFunc) (RenderInfo *info);

struct _RenderInfo
{
  GimpDisplayShell *shell;
  TileManager      *src_tiles;
  const guint      *alpha;
  const guchar     *src;
  guchar           *dest;
  gint              x, y;
  gint              w, h;
  gdouble           scalex;
  gdouble           scaley;
  gint              src_x;
  gint              src_y;
  gint              dest_bpp;
  gint              dest_bpl;
  gint              dest_width;

  gint              xstart;
  gint              xdelta;
  gint              yfraction;
};

static void  gimp_display_shell_render_info_scale   (RenderInfo       *info,
                                                     GimpDisplayShell *shell,
                                                     TileManager      *src_tiles,
                                                     gdouble           scale_x,
                                                     gdouble           scale_y);

static void  gimp_display_shell_render_setup_notify (GObject          *config,
                                                     GParamSpec       *param_spec,
                                                     Gimp             *gimp);


static guchar *tile_buf    = NULL;

static guint   check_mod   = 0;
static guint   check_shift = 0;


void
gimp_display_shell_render_init (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));
  g_return_if_fail (tile_buf == NULL);

  g_signal_connect (gimp->config, "notify::transparency-size",
                    G_CALLBACK (gimp_display_shell_render_setup_notify),
                    gimp);
  g_signal_connect (gimp->config, "notify::transparency-type",
                    G_CALLBACK (gimp_display_shell_render_setup_notify),
                    gimp);

  /*  allocate a buffer for arranging information from a row of tiles  */
  tile_buf = g_new (guchar, GIMP_RENDER_BUF_WIDTH * MAX_CHANNELS);

  gimp_display_shell_render_setup_notify (G_OBJECT (gimp->config), NULL, gimp);
}

void
gimp_display_shell_render_exit (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  g_signal_handlers_disconnect_by_func (gimp->config,
                                        gimp_display_shell_render_setup_notify,
                                        gimp);

  if (tile_buf)
    {
      g_free (tile_buf);
      tile_buf = NULL;
    }
}

static void
gimp_display_shell_render_setup_notify (GObject    *config,
                                        GParamSpec *param_spec,
                                        Gimp       *gimp)
{
  GimpCheckSize check_size;

  g_object_get (config,
                "transparency-size", &check_size,
                NULL);

  switch (check_size)
    {
    case GIMP_CHECK_SIZE_SMALL_CHECKS:
      check_mod   = 0x3;
      check_shift = 2;
      break;

    case GIMP_CHECK_SIZE_MEDIUM_CHECKS:
      check_mod   = 0x7;
      check_shift = 3;
      break;

    case GIMP_CHECK_SIZE_LARGE_CHECKS:
      check_mod   = 0xf;
      check_shift = 4;
      break;
    }
}


/*  Render Image functions  */

static void           render_image_rgb          (RenderInfo *info);
static void           render_image_rgb_a        (RenderInfo *info);
static void           render_image_gray         (RenderInfo *info);
static void           render_image_gray_a       (RenderInfo *info);
static void           render_image_indexed      (RenderInfo *info);
static void           render_image_indexed_a    (RenderInfo *info);

static const guint  * render_image_init_alpha   (gint        mult);

static const guchar * render_image_tile_fault   (RenderInfo *info);


static RenderFunc render_funcs[6] =
{
  render_image_rgb,
  render_image_rgb_a,
  render_image_gray,
  render_image_gray_a,
  render_image_indexed,
  render_image_indexed_a,
};


static void  gimp_display_shell_render_highlight (GimpDisplayShell *shell,
                                                  gint              x,
                                                  gint              y,
                                                  gint              w,
                                                  gint              h,
                                                  GdkRectangle     *highlight);
static void  gimp_display_shell_render_mask      (GimpDisplayShell *shell,
                                                  RenderInfo       *info);


/*****************************************************************/
/*  This function is the core of the display--it offsets and     */
/*  scales the image according to the current parameters in the  */
/*  display object.  It handles color, grayscale, 8, 15, 16, 24  */
/*  & 32 bit output depths.                                      */
/*****************************************************************/

void
gimp_display_shell_render (GimpDisplayShell *shell,
                           gint              x,
                           gint              y,
                           gint              w,
                           gint              h,
                           GdkRectangle     *highlight)
{
  GimpProjection *projection;
  RenderInfo      info;
  GimpImageType   type;

  g_return_if_fail (w > 0 && h > 0);

  projection = shell->display->image->projection;

  /* Initialize RenderInfo with values that don't change during the
   * call of this function.
   */
  info.shell      = shell;

  info.x          = x + shell->offset_x;
  info.y          = y + shell->offset_y;
  info.w          = w;
  info.h          = h;

  info.dest_bpp   = 3;
  info.dest_bpl   = info.dest_bpp * GIMP_RENDER_BUF_WIDTH;
  info.dest_width = info.dest_bpp * info.w;

  if (GIMP_IMAGE_TYPE_HAS_ALPHA (gimp_projection_get_image_type (projection)))
    {
      gdouble opacity = gimp_projection_get_opacity (projection);

      info.alpha = render_image_init_alpha (opacity * 255.999);
    }

  /* Setup RenderInfo for rendering a GimpProjection level. */
  {
    TileManager *src_tiles;
    gint         level;

    level = gimp_projection_get_level (projection,
                                       shell->scale_x,
                                       shell->scale_y);

    src_tiles = gimp_projection_get_tiles_at_level (projection, level);

    gimp_display_shell_render_info_scale (&info,
                                          shell,
                                          src_tiles,
                                          shell->scale_x * (1 << level),
                                          shell->scale_y * (1 << level));
  }

  /* Currently, only RGBA and GRAYA projection types are used - the rest
   * are in case of future need.  -- austin, 28th Nov 1998.
   */
  type = gimp_projection_get_image_type (projection);

  if (G_UNLIKELY (type != GIMP_RGBA_IMAGE && type != GIMP_GRAYA_IMAGE))
    g_warning ("using untested projection type %d", type);

  (* render_funcs[type]) (&info);

  /*  apply filters to the rendered projection  */
  if (shell->filter_stack)
    gimp_color_display_stack_convert (shell->filter_stack,
                                      shell->render_buf,
                                      w, h,
                                      3,
                                      3 * GIMP_RENDER_BUF_WIDTH);

  /*  dim pixels outside the highlighted rectangle  */
  if (highlight)
    {
      gimp_display_shell_render_highlight (shell, x, y, w, h, highlight);
    }
  else if (shell->mask)
    {
      TileManager *src_tiles = gimp_drawable_get_tiles (shell->mask);

      /* The mask does not (yet) have an image pyramid, use the base scale of
       * the shell.
       */
      gimp_display_shell_render_info_scale (&info,
                                            shell,
                                            src_tiles,
                                            shell->scale_x,
                                            shell->scale_y);

      gimp_display_shell_render_mask (shell, &info);
    }

  /*  put it to the screen  */
  gimp_canvas_draw_rgb (GIMP_CANVAS (shell->canvas), GIMP_CANVAS_STYLE_RENDER,
                        x + shell->disp_xoffset, y + shell->disp_yoffset,
                        w, h,
                        shell->render_buf,
                        3 * GIMP_RENDER_BUF_WIDTH,
                        shell->offset_x, shell->offset_y);
}


#define GIMP_DISPLAY_SHELL_DIM_PIXEL(buf,x) \
{ \
  buf[3 * (x) + 0] >>= 1; \
  buf[3 * (x) + 1] >>= 1; \
  buf[3 * (x) + 2] >>= 1; \
}

/*  This function highlights the given area by dimming all pixels outside. */

static void
gimp_display_shell_render_highlight (GimpDisplayShell *shell,
                                     gint              x,
                                     gint              y,
                                     gint              w,
                                     gint              h,
                                     GdkRectangle     *highlight)
{
  guchar       *buf  = shell->render_buf;
  GdkRectangle  rect;

  rect.x      = shell->offset_x + x;
  rect.y      = shell->offset_y + y;
  rect.width  = w;
  rect.height = h;

  if (gdk_rectangle_intersect (highlight, &rect, &rect))
    {
      rect.x -= shell->offset_x + x;
      rect.y -= shell->offset_y + y;

      for (y = 0; y < rect.y; y++)
        {
          for (x = 0; x < w; x++)
            GIMP_DISPLAY_SHELL_DIM_PIXEL (buf, x)

          buf += 3 * GIMP_RENDER_BUF_WIDTH;
        }

      for ( ; y < rect.y + rect.height; y++)
        {
          for (x = 0; x < rect.x; x++)
            GIMP_DISPLAY_SHELL_DIM_PIXEL (buf, x)

          for (x += rect.width; x < w; x++)
            GIMP_DISPLAY_SHELL_DIM_PIXEL (buf, x)

          buf += 3 * GIMP_RENDER_BUF_WIDTH;
        }

      for ( ; y < h; y++)
        {
          for (x = 0; x < w; x++)
            GIMP_DISPLAY_SHELL_DIM_PIXEL (buf, x)

          buf += 3 * GIMP_RENDER_BUF_WIDTH;
        }
    }
  else
    {
      for (y = 0; y < h; y++)
        {
          for (x = 0; x < w; x++)
            GIMP_DISPLAY_SHELL_DIM_PIXEL (buf, x)

          buf += 3 * GIMP_RENDER_BUF_WIDTH;
        }
    }
}

static void
gimp_display_shell_render_mask (GimpDisplayShell *shell,
                                RenderInfo       *info)
{
  gint      y, ye;
  gint      x, xe;
  gboolean  initial = TRUE;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          const guchar *src  = info->src;
          guchar       *dest = info->dest;

          switch (shell->mask_color)
            {
            case GIMP_RED_CHANNEL:
              for (x = info->x; x < xe; x++, src++, dest += 3)
                {
                  if (*src & 0x80)
                    continue;

                  dest[1] = dest[1] >> 2;
                  dest[2] = dest[2] >> 2;
                }
              break;

            case GIMP_GREEN_CHANNEL:
              for (x = info->x; x < xe; x++, src++, dest += 3)
                {
                  if (*src & 0x80)
                    continue;

                  dest[0] = dest[0] >> 2;
                  dest[2] = dest[2] >> 2;
                }
              break;

            case GIMP_BLUE_CHANNEL:
              for (x = info->x; x < xe; x++, src++, dest += 3)
                {
                  if (*src & 0x80)
                    continue;

                  dest[0] = dest[0] >> 2;
                  dest[1] = dest[1] >> 2;
                }
              break;

            default:
              break;
            }
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}


/*************************/
/*  8 Bit functions      */
/*************************/

static void
render_image_indexed (RenderInfo *info)
{
  const guchar *cmap;
  gint          y, ye;
  gint          x, xe;
  gboolean      initial = TRUE;

  cmap = gimp_image_get_colormap (info->shell->display->image);

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          const guchar *src  = info->src;
          guchar       *dest = info->dest;

          for (x = info->x; x < xe; x++)
            {
              guint  val = src[INDEXED_PIX] * 3;

              src += 1;

              dest[0] = cmap[val + 0];
              dest[1] = cmap[val + 1];
              dest[2] = cmap[val + 2];
              dest += 3;
            }
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}

static void
render_image_indexed_a (RenderInfo *info)
{
  const guint  *alpha = info->alpha;
  const guchar *cmap  = gimp_image_get_colormap (info->shell->display->image);
  gint          y, ye;
  gint          x, xe;
  gboolean      initial = TRUE;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0) && (y & check_mod))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          const guchar *src  = info->src;
          guchar       *dest = info->dest;
          guint         dark_light;

          dark_light = (y >> check_shift) + (info->x >> check_shift);

          for (x = info->x; x < xe; x++)
            {
              guint r, g, b, a = alpha[src[ALPHA_I_PIX]];
              guint val        = src[INDEXED_PIX] * 3;

              src += 2;

              if (dark_light & 0x1)
                {
                  r = gimp_render_blend_dark_check[(a | cmap[val + 0])];
                  g = gimp_render_blend_dark_check[(a | cmap[val + 1])];
                  b = gimp_render_blend_dark_check[(a | cmap[val + 2])];
                }
              else
                {
                  r = gimp_render_blend_light_check[(a | cmap[val + 0])];
                  g = gimp_render_blend_light_check[(a | cmap[val + 1])];
                  b = gimp_render_blend_light_check[(a | cmap[val + 2])];
                }

                dest[0] = r;
                dest[1] = g;
                dest[2] = b;
                dest += 3;

                if (((x + 1) & check_mod) == 0)
                  dark_light += 1;
              }
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}

static void
render_image_gray (RenderInfo *info)
{
  gint      y, ye;
  gint      x, xe;
  gboolean  initial = TRUE;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          const guchar *src  = info->src;
          guchar       *dest = info->dest;

          for (x = info->x; x < xe; x++)
            {
              guint val = src[GRAY_PIX];

              src += 1;

              dest[0] = val;
              dest[1] = val;
              dest[2] = val;
              dest += 3;
            }
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}

static void
render_image_gray_a (RenderInfo *info)
{
  const guint *alpha = info->alpha;
  gint         y, ye;
  gint         x, xe;
  gboolean     initial = TRUE;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0) && (y & check_mod))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          const guchar *src  = info->src;
          guchar       *dest = info->dest;
          guint         dark_light;

          dark_light = (y >> check_shift) + (info->x >> check_shift);

          for (x = info->x; x < xe; x++)
            {
              guint a = alpha[src[ALPHA_G_PIX]];
              guint val;

              if (dark_light & 0x1)
                val = gimp_render_blend_dark_check[(a | src[GRAY_PIX])];
              else
                val = gimp_render_blend_light_check[(a | src[GRAY_PIX])];

              src += 2;

              dest[0] = val;
              dest[1] = val;
              dest[2] = val;
              dest += 3;

              if (((x + 1) & check_mod) == 0)
                dark_light += 1;
            }
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}

static void
render_image_rgb (RenderInfo *info)
{
  gint      y, ye;
  gint      xe;
  gboolean  initial = TRUE;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      if (!initial && (error == 0))
        {
          memcpy (info->dest, info->dest - info->dest_bpl, info->dest_width);
        }
      else
        {
          memcpy (info->dest, info->src, 3 * info->w);
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      if (error)
        {
          info->src_y += error;
          info->yfraction = 256 * fmod (y/info->scaley, 1.0);
          info->src = render_image_tile_fault (info);

          initial = TRUE;
        }
      else
        {
          initial = FALSE;
        }
    }
}

static void
render_image_rgb_a (RenderInfo *info)
{
  const guint *alpha = info->alpha;
  gint         y, ye;
  gint         x, xe;

  y  = info->y;
  ye = info->y + info->h;
  xe = info->x + info->w;

  info->yfraction = 256 * fmod (y/info->scaley, 1.0);
  info->src = render_image_tile_fault (info);

  while (TRUE)
    {
      gint error = floor ((y + 1) / info->scaley) - floor (y / info->scaley);

      const guchar *src  = info->src;
      guchar       *dest = info->dest;
      guint         dark_light;

      dark_light = (y >> check_shift) + (info->x >> check_shift);

      for (x = info->x; x < xe; x++)
        {
          guint r, g, b, a = alpha[src[ALPHA_PIX]];

          if (dark_light & 0x1)
            {
              r = gimp_render_blend_dark_check[(a | src[RED_PIX])];
              g = gimp_render_blend_dark_check[(a | src[GREEN_PIX])];
              b = gimp_render_blend_dark_check[(a | src[BLUE_PIX])];
            }
          else
            {
              r = gimp_render_blend_light_check[(a | src[RED_PIX])];
              g = gimp_render_blend_light_check[(a | src[GREEN_PIX])];
              b = gimp_render_blend_light_check[(a | src[BLUE_PIX])];
            }

          src += 4;

          dest[0] = r;
          dest[1] = g;
          dest[2] = b;
          dest += 3;

          if (((x + 1) & check_mod) == 0)
            dark_light += 1;
        }

      if (++y == ye)
        break;

      info->dest += info->dest_bpl;

      info->yfraction = 256 * fmod (y/info->scaley, 1.0);
      info->src_y += error;
      info->src = render_image_tile_fault (info);
    }
}

static void
gimp_display_shell_render_info_scale (RenderInfo       *info,
                                      GimpDisplayShell *shell,
                                      TileManager      *src_tiles,
                                      gdouble           scale_x,
                                      gdouble           scale_y)
{
  info->src_tiles = src_tiles;

  /* We must reset info->dest because this member is modified in render
   * functions.
   */
  info->dest      = shell->render_buf;

  info->scalex    = scale_x;
  info->scaley    = scale_y;

  info->src_x     = (gdouble) info->x / info->scalex;
  info->src_y     = (gdouble) info->y / info->scaley;

  info->xstart    = (info->src_x +
                     ((info->x / info->scalex) -
                      floor (info->x / info->scalex))) * 65536.0;

  info->xdelta    = (1 << 16) * (1.0 / info->scalex);
}

static const guint *
render_image_init_alpha (gint mult)
{
  static guint *alpha_mult = NULL;
  static gint   alpha_val  = -1;

  gint i;

  if (alpha_val != mult)
    {
      if (!alpha_mult)
        alpha_mult = g_new (guint, 256);

      alpha_val = mult;
      for (i = 0; i < 256; i++)
        alpha_mult[i] = ((mult * i) / 255) << 8;
    }

  return alpha_mult;
}

static inline void
mix_pixels (gint          dx,
            gint          dy,
            const guchar *src0,
            const guchar *src1,
            const guchar *src2,
            const guchar *src3,
            guchar       *dst,
            gint          components)
{
  /* FIXME: make sure this is dealing correctly with the alpha */
#define DO_COMPONENT(iter) \
  { \
    gint m0, m1; \
    m0        = ((256 - dx) * src0[iter] + dx * src1[iter]) >> 8; \
    m1        = ((256 - dx) * src2[iter] + dx * src3[iter]) >> 8; \
    dst[iter] = (((256 - dy) * m0 + dy * m1)) >> 8; \
  }

  /* Adjust the weights of the bilinear mixing in favour of the least important
   * neighbours, this will both slightly blur the result as well as make sure
   * that we rather display than throw away information from the original
   * source data.
   */
  dx = (dx-128) * 0.50 + 128;
  dy = (dy-128) * 0.50 + 128;

  switch (components)
    {
      gint i;
      case 4:
#define ALPHA 3
        DO_COMPONENT(ALPHA);
        if (dst[ALPHA])
          {
            for (i= 0; i < ALPHA; i++)
              {
                gint m0 = ((256-dx) * src0[i] * src0[ALPHA] + dx * src1[i] * src1[ALPHA]) >> 8;
                gint m1 = ((256-dx) * src2[i] * src2[ALPHA] + dx * src3[i] * src3[ALPHA]) >> 8;
                gint r = (((256-dy) * m0 + dy * m1)/dst[ALPHA]) >> 8;

                if (r<0)
                  dst[i]=0;
                else if (r>255)
                  dst[i]=255;
                else
                dst[i]=r;
              }
          }
#undef ALPHA
        break;
      case 3:
        DO_COMPONENT (0);
        DO_COMPONENT (1);
        DO_COMPONENT (2);
        break;
      case 2:
#define ALPHA 1
        DO_COMPONENT(ALPHA);
        if (dst[ALPHA])
          {
            for (i= 0; i < ALPHA; i++)
              {
                gint m0 = ((256-dx) * src0[i] * src0[ALPHA] + dx * src1[i] * src1[ALPHA]) >> 8;
                gint m1 = ((256-dx) * src2[i] * src2[ALPHA] + dx * src3[i] * src3[ALPHA]) >> 8;
                gint r = (((256-dy) * m0 + dy * m1)/dst[ALPHA]) >> 8;

                if (r<0)
                  dst[i]=0;
                else if (r>255)
                  dst[i]=255;
                else
                dst[i]=r;
              }
          }
#undef ALPHA
        break;
      case 1:
        DO_COMPONENT (0);
        break;
    }
}


/* this function results in the correct data residing where dest points, if
 * scale_x and|or scale_y have a scale factor >100% the respective dimension
 * will use nearest neighbour instead of bilinear interpolation.
 */
static inline void
compute_sample (gdouble       scale_x,
                gdouble       scale_y,
                gint          dx,
                gint          dy,
                const guchar *src0,
                const guchar *src1,
                const guchar *src2,
                const guchar *src3,
                guchar       *dest,
                gint          bpp)
{
  if (scale_x < 1.0 &&
      scale_y < 1.0)
    {
      mix_pixels (dx >> 8,
                  dy >> 8,
                  src0, src1,
                  src2, src3,
                  dest,
                  bpp);
      dest += bpp;
    }
  else if (scale_x < 1.0 &&
           scale_y >= 1.0)
    {
      mix_pixels (dx >> 8,
                  0,
                  src0, src1,
                  src2, src3,
                  dest,
                  bpp);
      dest += bpp;
    }
  else if (scale_x >= 1.0 &&
           scale_y < 1.0)
    {
      mix_pixels (0,
                  dy >> 8,
                  src0, src1,
                  src2, src3,
                  dest,
                  bpp);
      dest += bpp;
    }
  else
    {
      const guchar *s = src0;

      switch (bpp)
        {
          case 4:
            *dest++ = *s++;
          case 3:
            *dest++ = *s++;
          case 2:
            *dest++ = *s++;
          case 1:
            *dest++ = *s++;
        }
    }
}

/* fast paths */
static const guchar * render_image_tile_fault_one_row  (RenderInfo *info);
static const guchar * render_image_tile_fault_nearest  (RenderInfo *info);


/* function to render a horizontal line of view data */
static const guchar *
render_image_tile_fault (RenderInfo *info)
{
  Tile         *tile0;
  Tile         *tile1;
  Tile         *tile2;
  Tile         *tile3;

  const guchar *src0;
  const guchar *src1;
  const guchar *src2;
  const guchar *src3;

  guchar       *dest;
  gint          width;
  gint          tilex0;   /* the current x-tile indice used for the left
                             sample pair*/
  gint          tilex1;   /* the current x-tile indice used for the right
                             sample pair */
  gint          xdelta;   /* fixed point amount to increment source x
                             coordinas for each horizontal integer destination
                             pixel increment */
  gint          bpp;
  glong         x;


  /* dispatch to fast path functions on special conditions */
  if ((info->scalex == 1.0 &&
       info->scaley == 1.0) ||
      (info->shell->scale_x > 1.0 &&
       info->shell->scale_y > 1.0))
    {
      /* use nearest neighbour interpolation when the desired scale
       * is 1:1 with the available pyramid.
       */
      return render_image_tile_fault_nearest (info);
    }
  else if (((info->src_y)     & ~(TILE_WIDTH -1)) ==
           ((info->src_y + 1) & ~(TILE_WIDTH -1)))
    {
      /* all the tiles needed are in a single row */
      return render_image_tile_fault_one_row (info);
    }

  tile0 = tile_manager_get_tile (info->src_tiles,
                                 info->src_x, info->src_y, TRUE, FALSE);
  tile2 = tile_manager_get_tile (info->src_tiles,
                                 info->src_x, info->src_y+1, TRUE, FALSE);
  tile1 = tile_manager_get_tile (info->src_tiles,
                                 info->src_x+1, info->src_y, TRUE, FALSE);
  tile3 = tile_manager_get_tile (info->src_tiles,
                                 info->src_x+1, info->src_y+1, TRUE, FALSE);

  g_return_val_if_fail (tile0 != NULL, tile_buf);

  src0 = tile_data_pointer (tile0,
                            info->src_x % TILE_WIDTH,
                            info->src_y % TILE_HEIGHT);
  if (tile1)
    {
      src1 = tile_data_pointer (tile1,
                                (info->src_x + 1)% TILE_WIDTH,
                                info->src_y % TILE_HEIGHT);
    }
  else
    {
      src1 = src0;  /* reusing existing pixel data */
    }

  if (tile2)
    {
      src2 = tile_data_pointer (tile2,
                                info->src_x % TILE_WIDTH,
                                (info->src_y + 1) % TILE_HEIGHT);
    }
  else
    {
      src2 = src0;  /* reusing existing pixel data */
    }

  if (tile3)
    {
      src3 = tile_data_pointer (tile3,
                                (info->src_x + 1) % TILE_WIDTH,
                                (info->src_y + 1) % TILE_HEIGHT);
    }
  else
    {
      src3 = src1;  /* reusing existing pixel data */
    }

  bpp    = tile_manager_bpp (info->src_tiles);
  dest   = tile_buf;

  x      = info->xstart;

  width  = info->w;

  tilex0 = info->src_x / TILE_WIDTH;
  tilex1 = (info->src_x + 1) / TILE_WIDTH;

  xdelta = info->xdelta;

  do
    {
      gint  src_x = x >> 16;
      gint  skipped;

      compute_sample (info->shell->scale_x,
                      info->shell->scale_y,
                      (x >> 8) & 0xff,
                      info->yfraction,
                      src0, src1,
                      src2, src3,
                      dest,
                      bpp);
      dest += bpp;

      x += xdelta;

      skipped = (x >> 16) - src_x;

      if (skipped)
        {
          /* if we changed integer source pixel coordinates in the source
           * buffer, make sure the src pointers (and their backing tiles) are
           * correct
           */ 
          src0 += skipped * bpp;
          src1 += skipped * bpp;
          src2 += skipped * bpp;
          src3 += skipped * bpp;

          src_x += skipped;

          if ((src_x / TILE_WIDTH) != tilex0)
            {
              tile_release (tile0, FALSE);

              if (tile2)
                tile_release (tile2, FALSE);

              tilex0 += 1;

              tile0 = tile_manager_get_tile (info->src_tiles,
                                             src_x, info->src_y, TRUE, FALSE);
              tile2 = tile_manager_get_tile (info->src_tiles,
                                             src_x, info->src_y+1, TRUE, FALSE);
              if (!tile0)
                goto done;

              if (!tile2)
                goto done;

              src0 = tile_data_pointer (tile0,
                                        src_x % TILE_WIDTH,
                                        info->src_y % TILE_HEIGHT);
              src2 = tile_data_pointer (tile2,
                                        src_x % TILE_WIDTH,
                                        (info->src_y + 1)% TILE_HEIGHT);
            }

          if (((src_x+1) / TILE_WIDTH) != tilex1)
            {
              if (tile1)
                tile_release (tile1, FALSE);

              if (tile3)
                tile_release (tile3, FALSE);

              tilex1 += 1;

              tile1 = tile_manager_get_tile (info->src_tiles,
                                             src_x + 1, info->src_y,
                                             TRUE, FALSE);
              tile3 = tile_manager_get_tile (info->src_tiles,
                                             src_x + 1, info->src_y + 1,
                                             TRUE, FALSE);

              if (!tile1)
                {
                  src1 = src0;
                }
              else
                {
                  src1 = tile_data_pointer (tile1,
                                            (src_x + 1) % TILE_WIDTH,
                                            info->src_y % TILE_HEIGHT);
                }

              if (!tile3)
                {
                  src3 = src2;
                }
              else
                {
                  src3 = tile_data_pointer (tile3,
                                            (src_x + 1) % TILE_WIDTH,
                                            (info->src_y+1) % TILE_HEIGHT);
                }
            }
        }
    }
  while (--width);

done:
  if (tile0)
    tile_release (tile0, FALSE);

  if (tile2)
    tile_release (tile2, FALSE);

  if (tile1)
    tile_release (tile1, FALSE);

  if (tile3)
    tile_release (tile3, FALSE);

  return tile_buf;
}

/* function to render a horizontal line of view data */
static const guchar *
render_image_tile_fault_nearest (RenderInfo *info)
{
  Tile         *tile;
  const guchar *src;
  guchar       *dest;
  gint          width;
  gint          tilex;
  gint          xdelta;
  gint          bpp;
  glong         x;

  tile = tile_manager_get_tile (info->src_tiles,
                                info->src_x, info->src_y, TRUE, FALSE);

  g_return_val_if_fail (tile != NULL, tile_buf);

  src = tile_data_pointer (tile,
                           info->src_x % TILE_WIDTH,
                           info->src_y % TILE_HEIGHT);

  bpp   = tile_manager_bpp (info->src_tiles);
  dest  = tile_buf;

  x     = info->xstart;
  

  width = info->w;

  tilex = info->src_x / TILE_WIDTH;

  xdelta = info->xdelta;

  do
    {
      const guchar *s  = src;
      gint  src_x = x >> 16;
      gint  skipped;

      switch (bpp)
        {
        case 4:
          *dest++ = *s++;
        case 3:
          *dest++ = *s++;
        case 2:
          *dest++ = *s++;
        case 1:
          *dest++ = *s++;
        }

      x += xdelta;
      skipped = (x >> 16) - src_x;
      if (skipped)
        {
          src += skipped * bpp;
          src_x += skipped;

          if ((src_x / TILE_WIDTH) != tilex)
            {
              tile_release (tile, FALSE);
              tilex += 1;

              tile = tile_manager_get_tile (info->src_tiles,
                                            src_x, info->src_y, TRUE, FALSE);
              if (!tile)
                return tile_buf;

              src = tile_data_pointer (tile,
                                       src_x % TILE_WIDTH,
                                       info->src_y % TILE_HEIGHT);
            }
        }
    }
  while (--width);

  tile_release (tile, FALSE);

  return tile_buf;
}


/* optimized renderer for a line of view data that assumes source data
 * to lie in the same row of tiles.
 */
static const guchar *
render_image_tile_fault_one_row (RenderInfo *info)
{
  Tile         *current_tile;
  Tile         *next_tile=NULL;  /* only used when crossing over right edge
                                    of tiles */
  const guchar *src0;
  const guchar *src1;
  const guchar *src2;
  const guchar *src3;
  guchar       *dest;
  gint          width;
  gint          tilex0;
  gint          tilex1;
  gint          xdelta;
  gint          bpp;
  glong         x;

  current_tile = tile_manager_get_tile (info->src_tiles,
                                        info->src_x, info->src_y, TRUE, FALSE);

  g_return_val_if_fail (current_tile != NULL, tile_buf);


  src0 = tile_data_pointer (current_tile,
                            info->src_x % TILE_WIDTH,
                            info->src_y % TILE_HEIGHT);
  src2 = tile_data_pointer (current_tile,
                            info->src_x % TILE_WIDTH,
                            (info->src_y + 1) % TILE_HEIGHT);

  if ((info->src_x & ~(TILE_WIDTH -1)) !=
      ((info->src_x + 1) & ~(TILE_WIDTH -1))) /* need two tiles */
    {
      next_tile = tile_manager_get_tile (info->src_tiles,
                                         info->src_x+1, info->src_y,
                                         TRUE, FALSE);
    }

  if (next_tile != NULL)  
    {
      src1 = tile_data_pointer (next_tile,
                                (info->src_x + 1)% TILE_WIDTH,
                                info->src_y % TILE_HEIGHT);
      src3 = tile_data_pointer (next_tile,
                                (info->src_x + 1) % TILE_WIDTH,
                                (info->src_y + 1) % TILE_HEIGHT);
    }
  else if (((info->src_x & ~(TILE_WIDTH -1)) ==
           ((info->src_x + 1) & ~(TILE_WIDTH -1))))
    /* the common case get all data from the same tile */
    {
      src1 = tile_data_pointer (current_tile,
                                (info->src_x + 1)% TILE_WIDTH,
                                info->src_y % TILE_HEIGHT);
      src3 = tile_data_pointer (current_tile,
                                (info->src_x + 1) % TILE_WIDTH,
                                (info->src_y + 1) % TILE_HEIGHT);
    }
  else
    {
      /* we should have gotten the rightmost pair of samples from a
       * separate tile that we couldn't retrieve
       */
      src1 = src0;
      src3 = src2;
    }

  bpp   = tile_manager_bpp (info->src_tiles);
  dest  = tile_buf;

  x     = info->xstart;

  width = info->w;

  tilex0= info->src_x / TILE_WIDTH;
  tilex1= (info->src_x+1)/ TILE_WIDTH;

  xdelta = info->xdelta;

  do
    {
      gint  src_x = x >> 16;
      gint  skipped;

      compute_sample (info->shell->scale_x,
                      info->shell->scale_y,
                      (x >> 8) & 0xff,
                      info->yfraction,
                      src0, src1,
                      src2, src3,
                      dest,
                      bpp);
      dest += bpp;

      if (next_tile)  /* we should only need two tiles for one of
                         the rounds when generating data
                       */
        {
          tile_release (next_tile, FALSE);
          next_tile = NULL;
        }

      x += xdelta;

      skipped = (x >> 16) - src_x;

      if (skipped)
        {
          src0 += skipped * bpp;
          src1 += skipped * bpp;
          src2 += skipped * bpp;
          src3 += skipped * bpp;

          src_x += skipped;

          /* check if src0 or src2 was pushed out of the
           * current tile
           */
          if ((src_x / TILE_WIDTH) != tilex0)
            {
              tile_release (current_tile, FALSE);
              tilex0 += 1;

              if (next_tile)
                { /* reuse tile1 fetched for src0 and src2 if available */
                  current_tile=next_tile;
                  next_tile=NULL;
                }
              else
                {
                  current_tile = tile_manager_get_tile (info->src_tiles,
                                                        src_x, info->src_y,
                                                        TRUE, FALSE);
                }

              if (!current_tile)
                goto done;

              src0 = tile_data_pointer (current_tile,
                                        src_x % TILE_WIDTH,
                                        info->src_y % TILE_HEIGHT);
              src2 = tile_data_pointer (current_tile,
                                        src_x % TILE_WIDTH,
                                        (info->src_y + 1)% TILE_HEIGHT);
            }

          if (((src_x+1) / TILE_WIDTH) != tilex1)
            {
              if (next_tile)
                tile_release (next_tile, FALSE);

              tilex1 += 1;

              next_tile = tile_manager_get_tile (info->src_tiles,
                                             src_x + 1, info->src_y,
                                             TRUE, FALSE);
              if (!next_tile)
                { /* use the data pointers from src0 and src2 when
                     we're outside the defined region */
                  src1 = src0;
                  src3 = src2;
                }
              else
                {
                  src1 = tile_data_pointer (next_tile,
                                            (src_x + 1) % TILE_WIDTH,
                                            info->src_y % TILE_HEIGHT);
                  src3 = tile_data_pointer (next_tile,
                                            (src_x + 1) % TILE_WIDTH,
                                            (info->src_y+1) % TILE_HEIGHT);
                }
            }
        }
    }
  while (--width);

done:
  if (current_tile!=NULL)
    tile_release (current_tile, FALSE);

  if (next_tile!=NULL)
    tile_release (next_tile, FALSE);

  return tile_buf;
}
