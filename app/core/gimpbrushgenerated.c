/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimp_brush_generated module Copyright 1998 Jay Cox <jaycox@earthlink.net>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "base/temp-buf.h"

#include "gimpbrushgenerated.h"

#include "gimp-intl.h"


#define OVERSAMPLING 5


enum
{
  PROP_0,
  PROP_SHAPE,
  PROP_RADIUS,
  PROP_SPIKES,
  PROP_HARDNESS,
  PROP_ASPECT_RATIO,
  PROP_ANGLE
};


/*  local function prototypes  */

static void   gimp_brush_generated_class_init (GimpBrushGeneratedClass *klass);
static void   gimp_brush_generated_init       (GimpBrushGenerated      *brush);

static void       gimp_brush_generated_set_property  (GObject      *object,
                                                      guint         property_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec);
static void       gimp_brush_generated_get_property  (GObject      *object,
                                                      guint         property_id,
                                                      GValue       *value,
                                                      GParamSpec   *pspec);
static gboolean   gimp_brush_generated_save          (GimpData     *data,
                                                      GError      **error);
static void       gimp_brush_generated_dirty         (GimpData     *data);
static gchar    * gimp_brush_generated_get_extension (GimpData     *data);
static GimpData * gimp_brush_generated_duplicate     (GimpData     *data,
                                                      gboolean      stingy_memory_use);


static GimpBrushClass *parent_class = NULL;


GType
gimp_brush_generated_get_type (void)
{
  static GType brush_type = 0;

  if (! brush_type)
    {
      static const GTypeInfo brush_info =
      {
        sizeof (GimpBrushGeneratedClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_brush_generated_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpBrushGenerated),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_brush_generated_init,
      };

      brush_type = g_type_register_static (GIMP_TYPE_BRUSH,
                                           "GimpBrushGenerated",
                                           &brush_info, 0);
    }

  return brush_type;
}

static void
gimp_brush_generated_class_init (GimpBrushGeneratedClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GimpDataClass *data_class   = GIMP_DATA_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gimp_brush_generated_set_property;
  object_class->get_property = gimp_brush_generated_get_property;

  data_class->save           = gimp_brush_generated_save;
  data_class->dirty          = gimp_brush_generated_dirty;
  data_class->get_extension  = gimp_brush_generated_get_extension;
  data_class->duplicate      = gimp_brush_generated_duplicate;

  g_object_class_install_property (object_class, PROP_SHAPE,
                                   g_param_spec_enum ("shape", NULL, NULL,
                                                      GIMP_TYPE_BRUSH_GENERATED_SHAPE,
                                                      GIMP_BRUSH_GENERATED_CIRCLE,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_RADIUS,
                                   g_param_spec_double ("radius", NULL, NULL,
                                                        0.1, 1000.0, 5.0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_SPIKES,
                                   g_param_spec_int    ("spikes", NULL, NULL,
                                                        2, 20, 2,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_HARDNESS,
                                   g_param_spec_double ("hardness", NULL, NULL,
                                                        0.0, 1.0, 0.0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_ASPECT_RATIO,
                                   g_param_spec_double ("aspect-ratio",
                                                        NULL, NULL,
                                                        1.0, 20.0, 1.0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_ANGLE,
                                   g_param_spec_double ("angle", NULL, NULL,
                                                        0.0, 180.0, 0.0,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
}

static void
gimp_brush_generated_init (GimpBrushGenerated *brush)
{
  brush->shape        = GIMP_BRUSH_GENERATED_CIRCLE;
  brush->radius       = 5.0;
  brush->hardness     = 0.0;
  brush->aspect_ratio = 1.0;
  brush->angle        = 0.0;
}

static void
gimp_brush_generated_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpBrushGenerated *brush = GIMP_BRUSH_GENERATED (object);

  switch (property_id)
    {
    case PROP_SHAPE:
      gimp_brush_generated_set_shape (brush, g_value_get_enum (value));
      break;
    case PROP_RADIUS:
      gimp_brush_generated_set_radius (brush, g_value_get_double (value));
      break;
    case PROP_SPIKES:
      gimp_brush_generated_set_spikes (brush, g_value_get_int (value));
      break;
    case PROP_HARDNESS:
      gimp_brush_generated_set_hardness (brush, g_value_get_double (value));
      break;
    case PROP_ASPECT_RATIO:
      gimp_brush_generated_set_aspect_ratio (brush, g_value_get_double (value));
      break;
    case PROP_ANGLE:
      gimp_brush_generated_set_angle (brush, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_brush_generated_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GimpBrushGenerated *brush = GIMP_BRUSH_GENERATED (object);

  switch (property_id)
    {
    case PROP_SHAPE:
      g_value_set_enum (value, brush->shape);
      break;
    case PROP_RADIUS:
      g_value_set_double (value, brush->radius);
      break;
    case PROP_SPIKES:
      g_value_set_int (value, brush->spikes);
      break;
    case PROP_HARDNESS:
      g_value_set_double (value, brush->hardness);
      break;
    case PROP_ASPECT_RATIO:
      g_value_set_double (value, brush->aspect_ratio);
      break;
    case PROP_ANGLE:
      g_value_set_double (value, brush->angle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
gimp_brush_generated_save (GimpData  *data,
                           GError   **error)
{
  GimpBrushGenerated *brush = GIMP_BRUSH_GENERATED (data);
  FILE               *file;
  gchar               buf[G_ASCII_DTOSTR_BUF_SIZE];
  gboolean            have_shape = FALSE;

  file  = fopen (data->filename, "wb");

  if (! file)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_OPEN,
                   _("Could not open '%s' for writing: %s"),
                   gimp_filename_to_utf8 (data->filename),
                   g_strerror (errno));
      return FALSE;
    }

  /* write magic header */
  fprintf (file, "GIMP-VBR\n");

  /* write version */
  if (brush->shape != GIMP_BRUSH_GENERATED_CIRCLE || brush->spikes > 2)
    {
      fprintf (file, "1.5\n");
      have_shape = TRUE;
    }
  else
    {
      fprintf (file, "1.0\n");
    }

  /* write name */
  fprintf (file, "%.255s\n", GIMP_OBJECT (brush)->name);

  if (have_shape)
    {
      GEnumClass *enum_class;
      GEnumValue *shape_val;

      enum_class = g_type_class_peek (GIMP_TYPE_BRUSH_GENERATED_SHAPE);

      /* write shape */
      shape_val = g_enum_get_value (enum_class, brush->shape);
      fprintf (file, "%s\n", shape_val->value_nick);
    }

  /* write brush spacing */
  fprintf (file, "%s\n",
           g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%f",
                            GIMP_BRUSH (brush)->spacing));

  /* write brush radius */
  fprintf (file, "%s\n",
           g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%f",
                            brush->radius));

  if (have_shape)
    {
      /* write brush spikes */
      fprintf (file, "%d\n", brush->spikes);
    }

  /* write brush hardness */
  fprintf (file, "%s\n",
           g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%f",
                            brush->hardness));

  /* write brush aspect_ratio */
  fprintf (file, "%s\n",
           g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%f",
                            brush->aspect_ratio));

  /* write brush angle */
  fprintf (file, "%s\n",
           g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%f",
                            brush->angle));

  fclose (file);

  return TRUE;
}

static gchar *
gimp_brush_generated_get_extension (GimpData *data)
{
  return GIMP_BRUSH_GENERATED_FILE_EXTENSION;
}

static GimpData *
gimp_brush_generated_duplicate (GimpData *data,
                                gboolean  stingy_memory_use)
{
  GimpBrushGenerated *brush = GIMP_BRUSH_GENERATED (data);

  return gimp_brush_generated_new (GIMP_OBJECT (brush)->name,
                                   brush->shape,
                                   brush->radius,
                                   brush->spikes,
                                   brush->hardness,
                                   brush->aspect_ratio,
                                   brush->angle,
                                   stingy_memory_use);
}

static gdouble
gauss (gdouble f)
{
  /* this aint' a real gauss function */
  if (f < -0.5)
    {
      f = -1.0 - f;
      return (2.0 * f*f);
    }

  if (f < 0.5)
    return (1.0 - 2.0 * f*f);

  f = 1.0 - f;
  return (2.0 * f*f);
}

static void
gimp_brush_generated_dirty (GimpData *data)
{
  GimpBrushGenerated *brush  = GIMP_BRUSH_GENERATED (data);
  GimpBrush          *gbrush = GIMP_BRUSH (brush);
  gint                x, y;
  guchar             *centerp;
  gdouble             d;
  gdouble             exponent;
  guchar              a;
  gint                length;
  gint                width  = 0;
  gint                height = 0;
  guchar             *lookup;
  gdouble             sum;
  gdouble             c, s, cs, ss;
  gdouble             short_radius;
  gdouble             buffer[OVERSAMPLING];

  if (gbrush->mask)
    temp_buf_free (gbrush->mask);

  s = sin (gimp_deg_to_rad (brush->angle));
  c = cos (gimp_deg_to_rad (brush->angle));

  short_radius = brush->radius / brush->aspect_ratio;

  gbrush->x_axis.x =        c * brush->radius;
  gbrush->x_axis.y = -1.0 * s * brush->radius;
  gbrush->y_axis.x =        s * short_radius;
  gbrush->y_axis.y =        c * short_radius;

  switch (brush->shape)
    {
    case GIMP_BRUSH_GENERATED_CIRCLE:
      width  = ceil (sqrt (gbrush->x_axis.x * gbrush->x_axis.x +
                           gbrush->y_axis.x * gbrush->y_axis.x));
      height = ceil (sqrt (gbrush->x_axis.y * gbrush->x_axis.y +
                           gbrush->y_axis.y * gbrush->y_axis.y));
      break;

    case GIMP_BRUSH_GENERATED_SQUARE:
      width  = ceil (fabs (gbrush->x_axis.x) + fabs (gbrush->y_axis.x));
      height = ceil (fabs (gbrush->x_axis.y) + fabs (gbrush->y_axis.y));
      break;

    case GIMP_BRUSH_GENERATED_DIAMOND:
      width  = ceil (MAX (fabs (gbrush->x_axis.x), fabs (gbrush->y_axis.x)));
      height = ceil (MAX (fabs (gbrush->x_axis.y), fabs (gbrush->y_axis.y)));
      break;

    default:
      g_return_if_reached ();
    }

  if (brush->spikes > 2)
    {
      /* could be optimized by respecting the angle */
      width = height = ceil (sqrt (brush->radius * brush->radius +
                                   short_radius * short_radius));
      gbrush->y_axis.x =        s * brush->radius;
      gbrush->y_axis.y =        c * brush->radius;
    }

  gbrush->mask = temp_buf_new (width  * 2 + 1,
                               height * 2 + 1,
                               1, width, height, NULL);

  centerp = temp_buf_data (gbrush->mask) + height * gbrush->mask->width + width;

  /* set up lookup table */
  length = OVERSAMPLING * ceil (1 + sqrt (2 *
                                          ceil (brush->radius + 1.0) *
                                          ceil (brush->radius + 1.0)));

  if ((1.0 - brush->hardness) < 0.0000004)
    exponent = 1000000.0;
  else
    exponent = 0.4 / (1.0 - brush->hardness);

  lookup = g_malloc (length);
  sum = 0.0;

  for (x = 0; x < OVERSAMPLING; x++)
    {
      d = fabs ((x + 0.5) / OVERSAMPLING - 0.5);

      if (d > brush->radius)
        buffer[x] = 0.0;
      else
        buffer[x] = gauss (pow (d / brush->radius, exponent));

      sum += buffer[x];
    }

  for (x = 0; d < brush->radius || sum > 0.00001; d += 1.0 / OVERSAMPLING)
    {
      sum -= buffer[x % OVERSAMPLING];

      if (d > brush->radius)
        buffer[x % OVERSAMPLING] = 0.0;
      else
        buffer[x % OVERSAMPLING] = gauss (pow (d / brush->radius, exponent));

      sum += buffer[x % OVERSAMPLING];
      lookup[x++] = RINT (sum * (255.0 / OVERSAMPLING));
    }

  while (x < length)
    {
      lookup[x++] = 0;
    }

  cs = cos (- 2 * G_PI / brush->spikes);
  ss = sin (- 2 * G_PI / brush->spikes);

  /* for an even number of spikes compute one half and mirror it */
  for (y = (brush->spikes % 2 ? -height : 0); y <= height; y++)
    {
      for (x = -width; x <= width; x++)
        {
          gdouble tx, ty, angle;

          tx = c*x - s*y;
          ty = fabs (s*x + c*y);

          if (brush->spikes > 2)
            {
              angle = atan2 (ty, tx);

              while (angle > G_PI / brush->spikes)
                {
                  gdouble sx = tx, sy = ty;

                  tx = cs * sx - ss * sy;
                  ty = ss * sx + cs * sy;

                  angle -= 2 * G_PI / brush->spikes;
                }
            }

          ty *= brush->aspect_ratio;
          switch (brush->shape)
            {
            case GIMP_BRUSH_GENERATED_CIRCLE:
              d = sqrt (tx*tx + ty*ty);
              break;
            case GIMP_BRUSH_GENERATED_SQUARE:
              d = MAX (fabs (tx), fabs (ty));
              break;
            case GIMP_BRUSH_GENERATED_DIAMOND:
              d = fabs (tx) + fabs (ty);
              break;
            }

          if (d < brush->radius + 1)
            a = lookup[(gint) RINT (d * OVERSAMPLING)];
          else
            a = 0;

          centerp[ y * gbrush->mask->width + x] = a;

          if (brush->spikes % 2 == 0)
            centerp[-1 * y * gbrush->mask->width - x] = a;
        }
    }

  g_free (lookup);

  if (GIMP_DATA_CLASS (parent_class)->dirty)
    GIMP_DATA_CLASS (parent_class)->dirty (data);
}

GimpData *
gimp_brush_generated_new (const gchar             *name,
                          GimpBrushGeneratedShape  shape,
                          gfloat                   radius,
                          gint                     spikes,
                          gfloat                   hardness,
                          gfloat                   aspect_ratio,
                          gfloat                   angle,
                          gboolean                 stingy_memory_use)
{
  GimpBrushGenerated *brush;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (*name != '\0', NULL);

  brush = g_object_new (GIMP_TYPE_BRUSH_GENERATED,
                        "name",         name,
                        "shape",        shape,
                        "radius",       radius,
                        "spikes",       spikes,
                        "hardness",     hardness,
                        "aspect-ratio", aspect_ratio,
                        "angle",        angle,
                        NULL);

  GIMP_BRUSH (brush)->spacing = 20;

  /* render brush mask */
  gimp_data_dirty (GIMP_DATA (brush));

  if (stingy_memory_use)
    temp_buf_swap (GIMP_BRUSH (brush)->mask);

  return GIMP_DATA (brush);
}

GList *
gimp_brush_generated_load (const gchar  *filename,
                           gboolean      stingy_memory_use,
                           GError      **error)
{
  GimpBrushGenerated      *brush;
  FILE                    *file;
  gchar                    string[256];
  gchar                   *name       = NULL;
  GimpBrushGeneratedShape  shape      = GIMP_BRUSH_GENERATED_CIRCLE;
  gboolean                 have_shape = FALSE;
  gint                     spikes     = 2;
  gdouble                  spacing;
  gdouble                  radius;
  gdouble                  hardness;
  gdouble                  aspect_ratio;
  gdouble                  angle;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  file = fopen (filename, "rb");

  if (! file)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_OPEN,
                   _("Could not open '%s' for reading: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      return NULL;
    }

  /* make sure the file we are reading is the right type */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;

  if (strncmp (string, "GIMP-VBR", 8) != 0)
    {
      g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                   _("Fatal parse error in brush file '%s': "
                     "Not a GIMP brush file."),
                   gimp_filename_to_utf8 (filename));
      goto failed;
    }

  /* make sure we are reading a compatible version */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;

  if (strncmp (string, "1.0", 3))
    {
      if (strncmp (string, "1.5", 3))
        {
          g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                       _("Fatal parse error in brush file '%s': "
                         "Unknown GIMP brush version."),
                       gimp_filename_to_utf8 (filename));
          goto failed;
        }
      else
        {
          have_shape = TRUE;
        }
    }

  /* read name */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;

  g_strstrip (string);
  name = gimp_any_to_utf8 (string, -1,
                           _("Invalid UTF-8 string in brush file '%s'."),
                           gimp_filename_to_utf8 (filename));

  if (have_shape)
    {
      GEnumClass *enum_class;
      GEnumValue *shape_val;

      enum_class = g_type_class_peek (GIMP_TYPE_BRUSH_GENERATED_SHAPE);

      /* read shape */
      errno = 0;
      if (! fgets (string, sizeof (string), file))
        goto failed;

      g_strstrip (string);
      shape_val = g_enum_get_value_by_nick (enum_class, string);

      if (!shape_val)
        {
          g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                       _("Fatal parse error in brush file '%s': "
                         "Unknown GIMP brush shape."),
                       gimp_filename_to_utf8 (filename));
          goto failed;
        }

      shape = shape_val->value;
    }

  /* read brush spacing */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;
  spacing = g_ascii_strtod (string, NULL);

  /* read brush radius */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;
  radius = g_ascii_strtod (string, NULL);

  if (have_shape)
    {
      /* read brush radius */
      errno = 0;
      if (! fgets (string, sizeof (string), file))
        goto failed;
      spikes = CLAMP (atoi (string), 2, 20);
    }

  /* read brush hardness */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;
  hardness = g_ascii_strtod (string, NULL);

  /* read brush aspect_ratio */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;
  aspect_ratio = g_ascii_strtod (string, NULL);

  /* read brush angle */
  errno = 0;
  if (! fgets (string, sizeof (string), file))
    goto failed;
  angle = g_ascii_strtod (string, NULL);

  fclose (file);

  /* create new brush */
  brush = g_object_new (GIMP_TYPE_BRUSH_GENERATED,
                        "name",         name,
                        "shape",        shape,
                        "radius",       radius,
                        "spikes",       spikes,
                        "hardness",     hardness,
                        "aspect-ratio", aspect_ratio,
                        "angle",        angle,
                        NULL);
  g_free (name);

  GIMP_BRUSH (brush)->spacing = spacing;

  /* render brush mask */
  gimp_data_dirty (GIMP_DATA (brush));

  if (stingy_memory_use)
    temp_buf_swap (GIMP_BRUSH (brush)->mask);

  return g_list_prepend (NULL, brush);

 failed:

  fclose (file);

  if (name)
    g_free (name);

  if (error && *error == NULL)
    g_set_error (error, GIMP_DATA_ERROR, GIMP_DATA_ERROR_READ,
                 _("Error while reading brush file '%s': %s"),
                 gimp_filename_to_utf8 (filename),
                 errno ? g_strerror (errno) : _("File is truncated"));

  return NULL;
}

GimpBrushGeneratedShape
gimp_brush_generated_set_shape (GimpBrushGenerated      *brush,
                                GimpBrushGeneratedShape  shape)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush),
                        GIMP_BRUSH_GENERATED_CIRCLE);

  if (brush->shape != shape)
    {
      brush->shape = shape;

      g_object_notify (G_OBJECT (brush), "shape");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->shape;
}

gfloat
gimp_brush_generated_set_radius (GimpBrushGenerated *brush,
                                 gfloat              radius)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  radius = CLAMP (radius, 0.0, 32767.0);

  if (brush->radius != radius)
    {
      brush->radius = radius;

      g_object_notify (G_OBJECT (brush), "radius");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->radius;
}

gint
gimp_brush_generated_set_spikes (GimpBrushGenerated *brush,
                                 gint                spikes)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1);

  spikes = CLAMP (spikes, 2, 20);

  if (brush->spikes != spikes)
    {
      brush->spikes = spikes;

      g_object_notify (G_OBJECT (brush), "spikes");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->spikes;
}

gfloat
gimp_brush_generated_set_hardness (GimpBrushGenerated *brush,
                                   gfloat              hardness)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  hardness = CLAMP (hardness, 0.0, 1.0);

  if (brush->hardness != hardness)
    {
      brush->hardness = hardness;

      g_object_notify (G_OBJECT (brush), "hardness");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->hardness;
}

gfloat
gimp_brush_generated_set_aspect_ratio (GimpBrushGenerated *brush,
                                       gfloat              ratio)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  ratio = CLAMP (ratio, 1.0, 1000.0);

  if (brush->aspect_ratio != ratio)
    {
      brush->aspect_ratio = ratio;

      g_object_notify (G_OBJECT (brush), "aspect-ratio");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->aspect_ratio;
}

gfloat
gimp_brush_generated_set_angle (GimpBrushGenerated *brush,
                                gfloat              angle)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  if (angle < 0.0)
    angle = -1.0 * fmod (angle, 180.0);
  else if (angle > 180.0)
    angle = fmod (angle, 180.0);

  if (brush->angle != angle)
    {
      brush->angle = angle;

      g_object_notify (G_OBJECT (brush), "angle");
      gimp_data_dirty (GIMP_DATA (brush));
    }

  return brush->angle;
}

GimpBrushGeneratedShape
gimp_brush_generated_get_shape (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush),
                        GIMP_BRUSH_GENERATED_CIRCLE);

  return brush->shape;
}

gfloat
gimp_brush_generated_get_radius (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->radius;
}

gint
gimp_brush_generated_get_spikes (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1);

  return brush->spikes;
}

gfloat
gimp_brush_generated_get_hardness (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->hardness;
}

gfloat
gimp_brush_generated_get_aspect_ratio (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->aspect_ratio;
}

gfloat
gimp_brush_generated_get_angle (const GimpBrushGenerated *brush)
{
  g_return_val_if_fail (GIMP_IS_BRUSH_GENERATED (brush), -1.0);

  return brush->angle;
}
