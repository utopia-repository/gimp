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

#ifndef __GIMP_BRUSH_CORE_H__
#define __GIMP_BRUSH_CORE_H__


#include "gimppaintcore.h"


#define BRUSH_CORE_SUBSAMPLE        4
#define BRUSH_CORE_SOLID_SUBSAMPLE  2

#define PRESSURE_SCALE              1.5


#define GIMP_TYPE_BRUSH_CORE            (gimp_brush_core_get_type ())
#define GIMP_BRUSH_CORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_BRUSH_CORE, GimpBrushCore))
#define GIMP_BRUSH_CORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_BRUSH_CORE, GimpBrushCoreClass))
#define GIMP_IS_BRUSH_CORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_BRUSH_CORE))
#define GIMP_IS_BRUSH_CORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_BRUSH_CORE))
#define GIMP_BRUSH_CORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_BRUSH_CORE, GimpBrushCoreClass))


typedef struct _GimpBrushCoreClass GimpBrushCoreClass;

struct _GimpBrushCore
{
  GimpPaintCore  parent_instance;

  GimpBrush     *main_brush;
  GimpBrush     *brush;
  gdouble        spacing;
  gdouble        scale;

  /*  brush buffers  */
  MaskBuf       *pressure_brush;

  MaskBuf       *solid_brushes[BRUSH_CORE_SOLID_SUBSAMPLE][BRUSH_CORE_SOLID_SUBSAMPLE];
  MaskBuf       *last_solid_brush;
  gboolean       solid_cache_invalid;

  MaskBuf       *scale_brush;
  MaskBuf       *last_scale_brush;
  gint           last_scale_width;
  gint           last_scale_height;

  MaskBuf       *scale_pixmap;
  MaskBuf       *last_scale_pixmap;
  gint           last_scale_pixmap_width;
  gint           last_scale_pixmap_height;

  MaskBuf       *kernel_brushes[BRUSH_CORE_SUBSAMPLE + 1][BRUSH_CORE_SUBSAMPLE + 1];

  MaskBuf       *last_brush_mask;
  gboolean       cache_invalid;

  gdouble        jitter;

  GRand         *rand;

  /*  don't use these...  */
  BoundSeg      *brush_bound_segs;
  gint           n_brush_bound_segs;
  gint           brush_bound_width;
  gint           brush_bound_height;
};

struct _GimpBrushCoreClass
{
  GimpPaintCoreClass  parent_class;

  /*  Set for tools that don't mind if the brush changes while painting  */
  gboolean            handles_changing_brush;

  /*  Scale the brush mask depending on pressure  */
  gboolean            use_scale;

  void (* set_brush) (GimpBrushCore *core,
                      GimpBrush     *brush);
};


GType   gimp_brush_core_get_type       (void) G_GNUC_CONST;

void    gimp_brush_core_set_brush      (GimpBrushCore            *core,
                                        GimpBrush                *brush);
void    gimp_brush_core_create_bound_segs (GimpBrushCore         *core,
                                           GimpPaintOptions      *options);

void    gimp_brush_core_paste_canvas   (GimpBrushCore            *core,
                                        GimpDrawable             *drawable,
                                        gdouble                   brush_opacity,
                                        gdouble                   image_opacity,
                                        GimpLayerModeEffects      paint_mode,
                                        GimpBrushApplicationMode  brush_hardness,
                                        GimpPaintApplicationMode  mode);
void    gimp_brush_core_replace_canvas (GimpBrushCore            *core,
                                        GimpDrawable                 *drawable,
                                        gdouble                   brush_opacity,
                                        gdouble                   image_opacity,
                                        GimpBrushApplicationMode  brush_hardness,
                                        GimpPaintApplicationMode  mode);

void    gimp_brush_core_color_area_with_pixmap
                                       (GimpBrushCore            *core,
                                        GimpImage                *dest,
                                        GimpDrawable             *drawable,
                                        TempBuf                  *area,
                                        gdouble                   scale,
                                        GimpBrushApplicationMode  mode);


#endif  /*  __GIMP_BRUSH_CORE_H__  */
