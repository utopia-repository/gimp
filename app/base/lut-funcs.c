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

#include <glib-object.h>

#include "libgimpmath/gimpmath.h"

#include "base-types.h"

#include "gimphistogram.h"
#include "gimplut.h"
#include "lut-funcs.h"


/* ---------- Brightness/Contrast -----------*/

typedef struct B_C_struct
{
  gdouble brightness;
  gdouble contrast;
} B_C_struct;

static gfloat
brightness_contrast_lut_func (B_C_struct *data,
			      gint        nchannels,
			      gint        channel,
			      gfloat      value)
{
  gfloat  nvalue;
  gdouble power;

  /* return the original value for the alpha channel */
  if ((nchannels == 2 || nchannels == 4) && channel == nchannels -1)
    return value;

  /* apply brightness */
  if (data->brightness < 0.0)
    value = value * (1.0 + data->brightness);
  else
    value = value + ((1.0 - value) * data->brightness);

  /* apply contrast */
  if (data->contrast < 0.0)
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;

      if (nvalue < 0.0)
	nvalue = 0.0;

      nvalue = 0.5 * pow (nvalue * 2.0 , (double) (1.0 + data->contrast));

      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }
  else
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;

      if (nvalue < 0.0)
	nvalue = 0.0;

      power = (data->contrast == 1.0) ? 127 : 1.0 / (1.0 - data->contrast);
      nvalue = 0.5 * pow (2.0 * nvalue, power);

      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }

  return value;
}

GimpLut *
brightness_contrast_lut_new (gdouble brightness,
			     gdouble contrast,
			     gint    n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  brightness_contrast_lut_setup (lut, brightness, contrast, n_channels);

  return lut;
}

void
brightness_contrast_lut_setup (GimpLut *lut,
			       gdouble  brightness,
			       gdouble  contrast,
			       gint     n_channels)
{
  B_C_struct data;

  g_return_if_fail (lut != NULL);

  data.brightness = brightness;
  data.contrast   = contrast;

  gimp_lut_setup (lut, (GimpLutFunc) brightness_contrast_lut_func,
		  (gpointer) &data, n_channels);
}

/* ---------------- invert ------------------ */

static gfloat
invert_lut_func (gpointer  unused,
		 gint      n_channels,
		 gint      channel,
		 gfloat    value)
{
  /* don't invert the alpha channel */
  if ((n_channels == 2 || n_channels == 4) && channel == n_channels -1)
    return value;

  return 1.0 - value;
}

GimpLut *
invert_lut_new (gint n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  invert_lut_setup (lut, n_channels);

  return lut;
}

void
invert_lut_setup (GimpLut *lut,
		  gint     n_channels)
{
  g_return_if_fail (lut != NULL);

  gimp_lut_setup_exact (lut, (GimpLutFunc) invert_lut_func,
			NULL , n_channels);
}

/* ---------------- add (or subract)------------------ */

static gfloat
add_lut_func (gdouble *amount,
	      gint     n_channels,
	      gint     channel,
	      gfloat   value)
{
  /* don't change the alpha channel */
  if ((n_channels == 2 || n_channels == 4) && channel == n_channels -1)
    return value;

  return (value + *amount);
}

GimpLut *
add_lut_new (gdouble amount,
	     gint    n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  add_lut_setup (lut, amount, n_channels);

  return lut;
}

void
add_lut_setup (GimpLut *lut,
	       gdouble  amount,
	       gint     n_channels)
{
  g_return_if_fail (lut != NULL);

  gimp_lut_setup (lut, (GimpLutFunc) add_lut_func,
		  (gpointer) &amount, n_channels);
}

/* ---------------- intersect (MIN (pixel, value)) ------------------ */

static gfloat
intersect_lut_func (gdouble *min,
		    gint     n_channels,
		    gint     channel,
		    gfloat   value)
{
  /* don't change the alpha channel */
  if ((n_channels == 2 || n_channels == 4) && channel == n_channels -1)
    return value;

  return MIN (value, *min);
}

GimpLut *
intersect_lut_new (gdouble value,
		   gint    n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  intersect_lut_setup (lut, value, n_channels);

  return lut;
}

void
intersect_lut_setup (GimpLut *lut,
		     gdouble  value,
		     gint     n_channels)
{
  g_return_if_fail (lut != NULL);

  gimp_lut_setup_exact (lut, (GimpLutFunc) intersect_lut_func,
			(gpointer) &value , n_channels);
}

/* ---------------- Threshold ------------------ */

static gfloat
threshold_lut_func (gdouble *min,
		    gint     n_channels,
		    gint     channel,
		    gfloat   value)
{
  /* don't change the alpha channel */
  if ((n_channels == 2 || n_channels == 4) && channel == n_channels -1)
    return value;

  if (value < *min)
    return 0.0;

  return 1.0;
}

GimpLut *
threshold_lut_new (gdouble value,
		   gint    n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  threshold_lut_setup (lut, value, n_channels);

  return lut;
}

void
threshold_lut_setup (GimpLut *lut,
		     gdouble  value,
		     gint     n_channels)
{
  g_return_if_fail (lut != NULL);

  gimp_lut_setup_exact (lut, (GimpLutFunc) threshold_lut_func,
			(gpointer) &value , n_channels);
}

/* --------------- posterize ---------------- */

static gfloat
posterize_lut_func (gint   *ilevels,
		    gint    n_channels,
		    gint    channel,
		    gfloat  value)
{
  gint levels;

  /* don't posterize the alpha channel */
  if ((n_channels == 2 || n_channels == 4) && channel == n_channels -1)
    return value;

  if (*ilevels < 2)
    levels = 2;
  else
    levels = *ilevels;

  value = RINT (value * (levels - 1.0)) / (levels - 1.0);

  return value;
}

GimpLut *
posterize_lut_new (gint levels,
		   gint n_channels)
{
  GimpLut *lut;

  lut = gimp_lut_new ();

  posterize_lut_setup (lut, levels, n_channels);

  return lut;
}

void
posterize_lut_setup (GimpLut *lut,
		     gint     levels,
		     gint     n_channels)
{
  g_return_if_fail (lut != NULL);

  gimp_lut_setup_exact (lut, (GimpLutFunc) posterize_lut_func,
			(gpointer) &levels, n_channels);
}

/* --------------- equalize ------------- */

typedef struct
{
  GimpHistogram *histogram;
  gint           part[5][257];
} hist_lut_struct;

static gfloat
equalize_lut_func (hist_lut_struct *hlut,
		   gint             n_channels,
		   gint             channel,
		   gfloat           value)
{
  gint i = 0;
  gint j;

  j = (gint) (value * 255.0 + 0.5);

  while (hlut->part[channel][i + 1] <= j)
    i++;

  return i / 255.0;
}

GimpLut *
eq_histogram_lut_new (GimpHistogram *histogram,
		      gint           n_channels)
{
  GimpLut *lut;

  g_return_val_if_fail (histogram != NULL, NULL);

  lut = gimp_lut_new ();

  eq_histogram_lut_setup (lut, histogram, n_channels);

  return lut;
}

void
eq_histogram_lut_setup (GimpLut       *lut,
			GimpHistogram *hist,
			gint           n_channels)
{
  gint            i, k, j;
  hist_lut_struct hlut;
  gdouble         pixels_per_value;
  gdouble         desired;
  gdouble         sum, dif;

  g_return_if_fail (lut != NULL);
  g_return_if_fail (hist != NULL);

  /* Find partition points */
  pixels_per_value = gimp_histogram_get_count (hist,
                                               GIMP_HISTOGRAM_VALUE,
                                               0, 255) / 256.0;

  for (k = 0; k < n_channels; k++)
    {
      /* First and last points in partition */
      hlut.part[k][0]   = 0;
      hlut.part[k][256] = 256;

      /* Find intermediate points */
      j   = 0;
      sum = (gimp_histogram_get_channel (hist, k, 0) +
	     gimp_histogram_get_channel (hist, k, 1));

      for (i = 1; i < 256; i++)
	{
	  desired = i * pixels_per_value;

	  while (sum <= desired)
	    {
	      j++;
	      sum += gimp_histogram_get_channel (hist, k, j + 1);
	    }

	  /* Nearest sum */
	  dif = sum - gimp_histogram_get_channel (hist, k, j);

	  if ((sum - desired) > (dif / 2.0))
	    hlut.part[k][i] = j;
	  else
	    hlut.part[k][i] = j + 1;
	}
    }

  gimp_lut_setup (lut, (GimpLutFunc) equalize_lut_func,
		  (gpointer) &hlut, n_channels);
}