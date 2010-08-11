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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "appenv.h"
#include "gimprc.h"
#include "paint_funcs.h"
#include "boundary.h"

#define STD_BUF_SIZE       1021
#define MAXDIFF            195076
#define HASH_TABLE_SIZE    1021
#define RANDOM_TABLE_SIZE  4096
#define RANDOM_SEED        314159265
#define EPSILON            0.0001

#define INT_MULT(a,b,t)  ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))

typedef enum
{
  MinifyX_MinifyY,
  MinifyX_MagnifyY,
  MagnifyX_MinifyY,
  MagnifyX_MagnifyY
} ScaleType;

/*  Layer modes information  */
typedef struct _LayerMode LayerMode;
struct _LayerMode
{
  int affect_alpha;   /*  does the layer mode affect the alpha channel  */
  char *name;         /*  layer mode specification  */
};

LayerMode layer_modes[] =
{
  { 1, "Normal" },
  { 1, "Dissolve" },
  { 1, "Behind" },
  { 0, "Multiply" },
  { 0, "Screen" },
  { 0, "Overlay" },
  { 0, "Difference" },
  { 0, "Addition" },
  { 0, "Subtraction" },
  { 0, "Darken Only" },
  { 0, "Lighten Only" },
  { 0, "Hue" },
  { 0, "Saturation" },
  { 0, "Color" },
  { 0, "Value" },
  { 1, "Erase" },
  { 1, "Replace" }
};

/*  ColorHash structure  */
typedef struct _ColorHash ColorHash;

struct _ColorHash
{
  int pixel;           /*  R << 16 | G << 8 | B  */
  int index;           /*  colormap index        */
  int colormap_ID;     /*  colormap ID           */
};

static ColorHash color_hash_table [HASH_TABLE_SIZE];
static int random_table [RANDOM_TABLE_SIZE];
static int color_hash_misses;
static int color_hash_hits;
static unsigned char * tmp_buffer;  /* temporary buffer available upon request */
static int tmp_buffer_size;
static unsigned char no_mask = OPAQUE_OPACITY;


/*******************************/
/*  Local function prototypes  */
static int *  make_curve         (double, int *);
static void   run_length_encode  (unsigned char *, int *, int, int);
static void   draw_segments      (PixelRegion *, BoundSeg *, int, int, int, int);
static double cubic              (double, int, int, int, int);
static void apply_layer_mode_replace (unsigned char *, unsigned char *,
				      unsigned char *, unsigned char *,
				      int, int, int,
				      int, int, int, int *);


static unsigned char *
paint_funcs_get_buffer (int size)
{
  if (size > tmp_buffer_size)
    {
      tmp_buffer_size = size;
      tmp_buffer = (unsigned char *) g_realloc (tmp_buffer, size);
    }

  return tmp_buffer;
}


/*
 * The equations: g(r) = exp (- r^2 / (2 * sigma^2))
 *                   r = sqrt (x^2 + y ^2)
 */

static int *
make_curve (double  sigma,
	    int    *length)
{
  int *curve;
  double sigma2;
  double l;
  int temp;
  int i, n;

  sigma2 = 2 * sigma * sigma;
  l = sqrt (-sigma2 * log (1.0 / 255.0));

  n = ceil (l) * 2;
  if ((n % 2) == 0)
    n += 1;

  curve = g_malloc (sizeof (int) * n);

  *length = n / 2;
  curve += *length;
  curve[0] = 255;

  for (i = 1; i <= *length; i++)
    {
      temp = (int) (exp (- (i * i) / sigma2) * 255);
      curve[-i] = temp;
      curve[i] = temp;
    }

  return curve;
}


static void
run_length_encode (unsigned char *src,
		   int           *dest,
		   int            w,
		   int            bytes)
{
  int start;
  int i;
  int j;
  unsigned char last;

  last = *src;
  src += bytes;
  start = 0;

  for (i = 1; i < w; i++)
    {
      if (*src != last)
	{
	  for (j = start; j < i; j++)
	    {
	      *dest++ = (i - j);
	      *dest++ = last;
	    }
	  start = i;
	  last = *src;
	}
      src += bytes;
    }

  for (j = start; j < i; j++)
    {
      *dest++ = (i - j);
      *dest++ = last;
    }
}


static void
draw_segments (PixelRegion *destPR,
	       BoundSeg    *bs,
	       int          num_segs,
	       int          off_x,
	       int          off_y,
	       int          opacity)
{
  int x1, y1, x2, y2;
  int tmp, i, length;
  unsigned char *line;

  length = MAXIMUM (destPR->w, destPR->h);
  line = paint_funcs_get_buffer (length);
  memset (line, opacity, length);

  for (i = 0; i < num_segs; i++)
    {
      x1 = bs[i].x1 + off_x;
      y1 = bs[i].y1 + off_y;
      x2 = bs[i].x2 + off_x;
      y2 = bs[i].y2 + off_y;

      if (bs[i].open == 0)
	{
	  /*  If it is vertical  */
	  if (x1 == x2)
	    {
	      x1 -= 1;
	      x2 -= 1;
	    }
	  else
	    {
	      y1 -= 1;
	      y2 -= 1;
	    }
	}

      /*  render segment  */
      x1 = BOUNDS (x1, 0, destPR->w - 1);
      y1 = BOUNDS (y1, 0, destPR->h - 1);
      x2 = BOUNDS (x2, 0, destPR->w - 1);
      y2 = BOUNDS (y2, 0, destPR->h - 1);

      if (x1 == x2)
	{
	  if (y2 < y1)
	    {
	      tmp = y1;
	      y1 = y2;
	      y2 = tmp;
	    }
	  pixel_region_set_col (destPR, x1, y1, (y2 - y1), line);
	}
      else
	{
	  if (x2 < x1)
	    {
	      tmp = x1;
	      x1 = x2;
	      x2 = tmp;
	    }
	  pixel_region_set_row (destPR, x1, y1, (x2 - x1), line);
	}
    }
}


static double
cubic (double dx,
       int    jm1,
       int    j,
       int    jp1,
       int    jp2)
{
  double dx1, dx2, dx3;
  double h1, h2, h3, h4;
  double result;

  /*  constraint parameter = -1  */
  dx1 = fabs (dx);
  dx2 = dx1 * dx1;
  dx3 = dx2 * dx1;
  h1 = dx3 - 2 * dx2 + 1;
  result = h1 * j;

  dx1 = fabs (dx - 1.0);
  dx2 = dx1 * dx1;
  dx3 = dx2 * dx1;
  h2 = dx3 - 2 * dx2 + 1;
  result += h2 * jp1;

  dx1 = fabs (dx - 2.0);
  dx2 = dx1 * dx1;
  dx3 = dx2 * dx1;
  h3 = -dx3 + 5 * dx2 - 8 * dx1 + 4;
  result += h3 * jp2;

  dx1 = fabs (dx + 1.0);
  dx2 = dx1 * dx1;
  dx3 = dx2 * dx1;
  h4 = -dx3 + 5 * dx2 - 8 * dx1 + 4;
  result += h4 * jm1;

  if (result < 0.0)
    result = 0.0;
  if (result > 255.0)
    result = 255.0;

  return result;
}

/*********************/
/*  FUNCTIONS        */
/*********************/

void
paint_funcs_setup ()
{
  int i;

  /*  allocate the temporary buffer  */
  tmp_buffer = (unsigned char *) g_malloc (STD_BUF_SIZE);
  tmp_buffer_size = STD_BUF_SIZE;

  /*  initialize the color hash table--invalidate all entries  */
  for (i = 0; i < HASH_TABLE_SIZE; i++)
    color_hash_table[i].colormap_ID = -1;
  color_hash_misses = 0;
  color_hash_hits = 0;

  /*  generate a table of random seeds  */
  srand (RANDOM_SEED);
  for (i = 0; i < RANDOM_TABLE_SIZE; i++)
    random_table[i] = rand ();

  for (i = 0; i < RANDOM_TABLE_SIZE; i++)
    {
      int tmp;
      int swap = i + rand () % (RANDOM_TABLE_SIZE - i);
      tmp = random_table[i];
      random_table[i] = random_table[swap];
      random_table[swap] = tmp;
    }
}


void
paint_funcs_free ()
{
  /*  free the temporary buffer  */
  g_free (tmp_buffer);

  /*  print out the hash table statistics
      printf ("RGB->indexed hash table lookups: %d\n", color_hash_hits + color_hash_misses);
      printf ("RGB->indexed hash table hits: %d\n", color_hash_hits);
      printf ("RGB->indexed hash table misses: %d\n", color_hash_misses);
      printf ("RGB->indexed hash table hit rate: %f\n",
      100.0 * color_hash_hits / (color_hash_hits + color_hash_misses));
      */
}


void
color_pixels (unsigned char *dest,
	      unsigned char *color,
	      int            w,
	      int            bytes)
{
  int b;

  while (w--)
    {
      for (b = 0; b < bytes; b++)
	dest[b] = color[b];

      dest += bytes;
    }
}


void
blend_pixels (unsigned char *src1,
	      unsigned char *src2,
	      unsigned char *dest,
	      int            blend,
	      int            w,
	      int            bytes,
	      int            has_alpha)
{
  int alpha, b;
  unsigned char blend2 = (255 - blend);

  alpha = (has_alpha) ? bytes - 1 : bytes;
  while (w --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = (src1[b] * blend2 + src2[b] * blend) / 255;

      if (has_alpha)
	dest[alpha] = src1[alpha];  /*  alpha channel--assume src2 has none  */

      src1 += bytes;
      src2 += bytes;
      dest += bytes;
    }
}


void
shade_pixels (unsigned char *src,
	      unsigned char *dest,
	      unsigned char *col,
	      int            blend,
	      int            w,
	      int            bytes,
	      int            has_alpha)
{
  int alpha, b;
  unsigned char blend2 = (255 - blend);

  alpha = (has_alpha) ? bytes - 1 : bytes;
  while (w --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = (src[b] * blend2 + col[b] * blend) / 255;

      if (has_alpha)
	dest[alpha] = src[alpha];  /* alpha channel */

      src += bytes;
      dest += bytes;
    }
}


void
extract_alpha_pixels (unsigned char *src,
		      unsigned char *mask,
		      unsigned char *dest,
		      int            w,
		      int            bytes)
{
  int alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  alpha = bytes - 1;
  while (w --)
    {
      *dest++ = (src[alpha] * *m) / 255;

      if (mask)
	m++;
      src += bytes;
    }
}


void
darken_pixels (unsigned char *src1,
	       unsigned char *src2,
	       unsigned char *dest,
	       int            length,
	       int            b1,
	       int            b2,
	       int            ha1,
	       int            ha2)
{
  int b, alpha;
  unsigned char s1, s2;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length--)
    {
      for (b = 0; b < alpha; b++)
	{
	  s1 = src1[b];
	  s2 = src2[b];
	  dest[b] = (s1 < s2) ? s1 : s2;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
lighten_pixels (unsigned char *src1,
		unsigned char *src2,
		unsigned char *dest,
		int            length,
		int            b1,
		int            b2,
		int            ha1,
		int            ha2)
{
  int b, alpha;
  unsigned char s1, s2;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length--)
    {
      for (b = 0; b < alpha; b++)
	{
	  s1 = src1[b];
	  s2 = src2[b];
	  dest[b] = (s1 < s2) ? s2 : s1;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
hsv_only_pixels (unsigned char *src1,
		 unsigned char *src2,
		 unsigned char *dest,
		 int            mode,
		 int            length,
		 int            bytes1,
		 int            bytes2,
		 int            ha1,
		 int            ha2)
{
  int r1, g1, b1;
  int r2, g2, b2;

  /*  assumes inputs are only 4 byte RGBA pixels  */
  while (length--)
    {
      r1 = src1[0]; g1 = src1[1]; b1 = src1[2];
      r2 = src2[0]; g2 = src2[1]; b2 = src2[2];
      rgb_to_hsv (&r1, &g1, &b1);
      rgb_to_hsv (&r2, &g2, &b2);

      switch (mode)
	{
	case HUE_MODE:
	  r1 = r2;
	  break;
	case SATURATION_MODE:
	  g1 = g2;
	  break;
	case VALUE_MODE:
	  b1 = b2;
	  break;
	}

      /*  set the destination  */
      hsv_to_rgb (&r1, &g1, &b1);

      dest[0] = r1; dest[1] = g1; dest[2] = b1;

      if (ha1 && ha2)
	dest[3] = MIN (src1[3], src2[3]);
      else if (ha2)
	dest[3] = src2[3];

      src1 += bytes1;
      src2 += bytes2;
      dest += bytes2;
    }
}


void
color_only_pixels (unsigned char *src1,
		   unsigned char *src2,
		   unsigned char *dest,
		   int            mode,
		   int            length,
		   int            bytes1,
		   int            bytes2,
		   int            ha1,
		   int            ha2)
{
  int r1, g1, b1;
  int r2, g2, b2;

  /*  assumes inputs are only 4 byte RGBA pixels  */
  while (length--)
    {
      r1 = src1[0]; g1 = src1[1]; b1 = src1[2];
      r2 = src2[0]; g2 = src2[1]; b2 = src2[2];
      rgb_to_hls (&r1, &g1, &b1);
      rgb_to_hls (&r2, &g2, &b2);

      /*  transfer hue and saturation to the source pixel  */
      r1 = r2;
      b1 = b2;

      /*  set the destination  */
      hls_to_rgb (&r1, &g1, &b1);

      dest[0] = r1; dest[1] = g1; dest[2] = b1;

      if (ha1 && ha2)
	dest[3] = MIN (src1[3], src2[3]);
      else if (ha2)
	dest[3] = src2[3];

      src1 += bytes1;
      src2 += bytes2;
      dest += bytes2;
    }
}


void
multiply_pixels (unsigned char *src1,
		 unsigned char *src2,
		 unsigned char *dest,
		 int            length,
		 int            b1,
		 int            b2,
		 int            ha1,
		 int            ha2)
{
  int alpha, b;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = (src1[b] * src2[b]) / 255;

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
screen_pixels (unsigned char *src1,
	       unsigned char *src2,
	       unsigned char *dest,
	       int            length,
	       int            b1,
	       int            b2,
	       int            ha1,
	       int            ha2)
{
  int alpha, b;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = 255 - ((255 - src1[b]) * (255 - src2[b])) / 255;

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
overlay_pixels (unsigned char *src1,
		unsigned char *src2,
		unsigned char *dest,
		int            length,
		int            b1,
		int            b2,
		int            ha1,
		int            ha2)
{
  int alpha, b;
  int screen, mult;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	{
	  screen = 255 - ((255 - src1[b]) * (255 - src2[b])) / 255;
	  mult = (src1[b] * src2[b]) / 255;
	  dest[b] = (screen * src1[b] + mult * (255 - src1[b])) / 255;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
add_pixels (unsigned char *src1,
	    unsigned char *src2,
	    unsigned char *dest,
	    int            length,
	    int            b1,
	    int            b2,
	    int            ha1,
	    int            ha2)
{
  int alpha, b;
  int sum;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	{
	  sum = src1[b] + src2[b];
	  dest[b] = (sum > 255) ? 255 : sum;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
subtract_pixels (unsigned char *src1,
		 unsigned char *src2,
		 unsigned char *dest,
		 int            length,
		 int            b1,
		 int            b2,
		 int            ha1,
		 int            ha2)
{
  int alpha, b;
  int diff;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	{
	  diff = src1[b] - src2[b];
	  dest[b] = (diff < 0) ? 0 : diff;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
difference_pixels (unsigned char *src1,
		   unsigned char *src2,
		   unsigned char *dest,
		   int            length,
		   int            b1,
		   int            b2,
		   int            ha1,
		   int            ha2)
{
  int alpha, b;
  int diff;

  alpha = (ha1 || ha2) ? MAXIMUM (b1, b2) - 1 : b1;

  while (length --)
    {
      for (b = 0; b < alpha; b++)
	{
	  diff = src1[b] - src2[b];
	  dest[b] = (diff < 0) ? -diff : diff;
	}

      if (ha1 && ha2)
	dest[alpha] = MIN (src1[alpha], src2[alpha]);
      else if (ha2)
	dest[alpha] = src2[alpha];

      src1 += b1;
      src2 += b2;
      dest += b2;
    }
}


void
dissolve_pixels (unsigned char *src,
		 unsigned char *dest,
		 int            x,
		 int            y,
		 int            opacity,
		 int            length,
		 int            sb,
		 int            db,
		 int            has_alpha)
{
  int alpha, b;
  int rand_val;

  /*  Set up the random number generator  */
  srand (random_table [y % RANDOM_TABLE_SIZE]);
  for (b = 0; b < x; b++)
    rand ();

  alpha = db - 1;

  while (length --)
    {
      /*  preserve the intensity values  */
      for (b = 0; b < alpha; b++)
	dest[b] = src[b];

      /*  dissolve if random value is > opacity  */
      rand_val = (rand() & 0xFF);

      if (has_alpha)
	dest[alpha] = (rand_val > opacity) ? 0 : src[alpha];
      else
	dest[alpha] = (rand_val > opacity) ? 0 : OPAQUE_OPACITY;

      dest += db;
      src += sb;
    }
}

void
replace_pixels (unsigned char *src1,
		unsigned char *src2,
		unsigned char *dest,
		unsigned char *mask,
		int            length,
		int            opacity,
		int           *affect,
		int            b1,
		int            b2)
{
  int alpha;
  int b;
  double a_val, a_recip, mask_val;
  double norm_opacity;
  int s1_a, s2_a;
  int new_val;

  if (b1 != b2)
    {
      g_message ("replace_pixels only works on commensurate pixel regions");
      return;
    }

  alpha = b1 - 1;
  norm_opacity = opacity * (1.0 / 65025.0);

  while (length --)
    {
      mask_val = mask[0] * norm_opacity;
      /* calculate new alpha first. */
      s1_a = src1[alpha];
      s2_a = src2[alpha];
      a_val = s1_a + mask_val * (s2_a - s1_a);
      if (a_val == 0)
	a_recip = 0;
      else
	a_recip = 1.0 / a_val;
      /* possible optimization: fold a_recip into s1_a and s2_a */
      for (b = 0; b < alpha; b++)
	{
	  new_val = 0.5 + a_recip * (src1[b] * s1_a + mask_val *
				     (src2[b] * s2_a - src1[b] * s1_a));
	  dest[b] = affect[b] ? MIN (new_val, 255) : src1[b];
	}
      dest[alpha] = affect[alpha] ? a_val + 0.5: s1_a;
      src1 += b1;
      src2 += b2;
      dest += b2;
      mask++;
    }
}


void
swap_pixels (unsigned char *src,
	     unsigned char *dest,
	     int            length)
{
  while (length--)
    {
      *src = *src ^ *dest;
      *dest = *dest ^ *src;
      *src = *src ^ *dest;
      src++;
      dest++;
    }
}


void
scale_pixels (unsigned char *src,
	      unsigned char *dest,
	      int            length,
	      int            scale)
{
  while (length --)
    *dest++ = (unsigned char) ((*src++ * scale) / 255);
}


void
add_alpha_pixels (unsigned char *src,
		  unsigned char *dest,
		  int            length,
		  int            bytes)
{
  int alpha, b;

  alpha = bytes + 1;
  while (length --)
    {
      for (b = 0; b < bytes; b++)
	dest[b] = src[b];

      dest[b] = OPAQUE_OPACITY;

      src += bytes;
      dest += alpha;
    }
}


void
flatten_pixels (unsigned char *src,
		unsigned char *dest,
		unsigned char *bg,
		int            length,
		int            bytes)
{
  int alpha, b;
  int t1, t2;

  alpha = bytes - 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = INT_MULT (src[b], src[alpha], t1) + INT_MULT (bg[b], (255 - src[alpha]), t2);

      src += bytes;
      dest += alpha;
    }
}


void
gray_to_rgb_pixels (unsigned char *src,
		    unsigned char *dest,
		    int            length,
		    int            bytes)
{
  int b;
  int dest_bytes;
  int has_alpha;

  has_alpha = (bytes == 2) ? 1 : 0;
  dest_bytes = (has_alpha) ? 4 : 3;

  while (length --)
    {
      for (b = 0; b < bytes; b++)
	dest[b] = src[0];

      if (has_alpha)
	dest[3] = src[1];

      src += bytes;
      dest += dest_bytes;
    }
}


void
apply_mask_to_alpha_channel (unsigned char *src,
			     unsigned char *mask,
			     int            opacity,
			     int            length,
			     int            bytes)
{
  int alpha;

  alpha = bytes - 1;
  while (length --)
    {
      src[alpha] = (src[alpha] * *mask++ * opacity) / 65025;
      src += bytes;
    }
}


void
combine_mask_and_alpha_channel (unsigned char *src,
				unsigned char *mask,
				int            opacity,
				int            length,
				int            bytes)
{
  unsigned char mask_val;
  int alpha;

  alpha = bytes - 1;
  while (length --)
    {
      mask_val = (*mask++ * opacity) / 255;
      src[alpha] = src[alpha] + ((255 - src[alpha]) * mask_val) / 255;
      src += bytes;
    }
}


void
copy_gray_to_inten_a_pixels (unsigned char *src,
			     unsigned char *dest,
			     int            length,
			     int            bytes)
{
  int b;
  int alpha;

  alpha = bytes - 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = *src;
      dest[b] = OPAQUE_OPACITY;

      src ++;
      dest += bytes;
    }
}


void
initial_channel_pixels (unsigned char *src,
			unsigned char *dest,
			int            length,
			int            bytes)
{
  int alpha, b;

  alpha = bytes - 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = src[0];

      dest[alpha] = OPAQUE_OPACITY;

      dest += bytes;
      src ++;
    }
}


void
initial_indexed_pixels (unsigned char *src,
			unsigned char *dest,
			unsigned char *cmap,
			int            length)
{
  int col_index;

  /*  This function assumes always that we're mapping from
   *  an RGB colormap to an RGBA image...
   */
  while (length--)
    {
      col_index = *src++ * 3;
      *dest++ = cmap[col_index++];
      *dest++ = cmap[col_index++];
      *dest++ = cmap[col_index++];
      *dest++ = OPAQUE_OPACITY;
    }
}


void
initial_indexed_a_pixels (unsigned char *src,
			  unsigned char *dest,
			  unsigned char *mask,
			  unsigned char *cmap,
			  int            opacity,
			  int            length)
{
  int col_index;
  unsigned char new_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  while (length --)
    {
      col_index = *src++ * 3;
      new_alpha = (*src++ * *m * opacity) / 65025;

      *dest++ = cmap[col_index++];
      *dest++ = cmap[col_index++];
      *dest++ = cmap[col_index++];
      /*  Set the alpha channel  */
      *dest++ = (new_alpha > 127) ? OPAQUE_OPACITY : TRANSPARENT_OPACITY;

      if (mask)
	m++;
    }
}


void
initial_inten_pixels (unsigned char *src,
		      unsigned char *dest,
		      unsigned char *mask,
		      int            opacity,
		      int           *affect,
		      int            length,
		      int            bytes)
{
  int b, dest_bytes;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  /*  This function assumes the source has no alpha channel and
   *  the destination has an alpha channel.  So dest_bytes = bytes + 1
   */
  dest_bytes = bytes + 1;

  if (mask)
    {
      while (length --)
	{
	  for (b = 0; b < bytes; b++)
	    dest [b] = affect [b] ? src [b] : 0;
	    
	  /*  Set the alpha channel  */
	  dest[b] = affect [b] ? (opacity * *m) / 255 : 0;
	    
	  m++;
	  dest += dest_bytes;
	  src += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  for (b = 0; b < bytes; b++)
	    dest [b] = affect [b] ? src [b] : 0;
	    
	  /*  Set the alpha channel  */
	  dest[b] = affect [b] ? opacity : 0;
	    
	  dest += dest_bytes;
	  src += bytes;
	}
    }
}


void
initial_inten_a_pixels (unsigned char *src,
			unsigned char *dest,
			unsigned char *mask,
			int            opacity,
			int           *affect,
			int            length,
			int            bytes)
{
  int alpha, b;
  unsigned char * m;

  alpha = bytes - 1;
  if (mask)
    {
      m = mask;
      while (length --)
	{
	  for (b = 0; b < alpha; b++)
	    dest[b] = src[b] * affect[b];
	  
	  /*  Set the alpha channel  */
	  dest[alpha] = affect [alpha] ? (opacity * src[alpha] * *m) / 65025 : 0;
	  
	  m++;
	  
	  dest += bytes;
	  src += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  for (b = 0; b < alpha; b++)
	    dest[b] = src[b] * affect[b];
	  
	  /*  Set the alpha channel  */
	  dest[alpha] = affect [alpha] ? (opacity * src[alpha]) / 255 : 0;
	  
	  dest += bytes;
	  src += bytes;
	}
    }
}


void
combine_indexed_and_indexed_pixels (unsigned char *src1,
				    unsigned char *src2,
				    unsigned char *dest,
				    unsigned char *mask,
				    int            opacity,
				    int           *affect,
				    int            length,
				    int            bytes)
{
  int b;
  unsigned char new_alpha;
  unsigned char * m;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  new_alpha = (*m * opacity) / 255;
	  
	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];
	  
	  m++;
	  
	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = opacity;
	  
	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];
	  
	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
}


void
combine_indexed_and_indexed_a_pixels (unsigned char *src1,
				      unsigned char *src2,
				      unsigned char *dest,
				      unsigned char *mask,
				      int            opacity,
				      int           *affect,
				      int            length,
				      int            bytes)
{
  int b, alpha;
  unsigned char new_alpha;
  unsigned char * m;
  int src2_bytes;

  alpha = 1;
  src2_bytes = 2;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  new_alpha = (src2[alpha] * *m * opacity) / 65025;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];

	  m++;

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = (src2[alpha] * opacity) / 255;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
}


void
combine_indexed_a_and_indexed_a_pixels (unsigned char *src1,
					unsigned char *src2,
					unsigned char *dest,
					unsigned char *mask,
					int            opacity,
					int           *affect,
					int            length,
					int            bytes)
{
  int b, alpha;
  unsigned char new_alpha;
  unsigned char * m;

  alpha = 1;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  new_alpha = (src2[alpha] * *m * opacity) / 65025;

	  for (b = 0; b < alpha; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];

	  dest[alpha] = (affect[alpha] && new_alpha > 127) ? OPAQUE_OPACITY : src1[alpha];

	  m++;

	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = (src2[alpha] * opacity) / 255;

	  for (b = 0; b < alpha; b++)
	    dest[b] = (affect[b] && new_alpha > 127) ? src2[b] : src1[b];

	  dest[alpha] = (affect[alpha] && new_alpha > 127) ? OPAQUE_OPACITY : src1[alpha];

	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
}


void
combine_inten_a_and_indexed_a_pixels (unsigned char *src1,
				      unsigned char *src2,
				      unsigned char *dest,
				      unsigned char *mask,
				      unsigned char *cmap,
				      int            opacity,
				      int            length,
				      int            bytes)
{
  int b, alpha;
  unsigned char new_alpha;
  int src2_bytes;
  int index;

  alpha = 1;
  src2_bytes = 2;

  if (mask)
    {
      unsigned char *m = mask;
      while (length --)
	{
	  new_alpha = (src2[alpha] * *m * opacity) / 65025;

	  index = src2[0] * 3;

	  for (b = 0; b < bytes-1; b++)
	    dest[b] = (new_alpha > 127) ? cmap[index + b] : src1[b];

	  dest[b] = (new_alpha > 127) ? OPAQUE_OPACITY : src1[b];  /*  alpha channel is opaque  */

	  m++;

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = (src2[alpha] * opacity) / 255;

	  index = src2[0] * 3;

	  for (b = 0; b < bytes-1; b++)
	    dest[b] = (new_alpha > 127) ? cmap[index + b] : src1[b];

	  dest[b] = (new_alpha > 127) ? OPAQUE_OPACITY : src1[b];  /*  alpha channel is opaque  */

	  /* m++; /Per */

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
}


void
combine_inten_and_inten_pixels (unsigned char *src1,
				unsigned char *src2,
				unsigned char *dest,
				unsigned char *mask,
				int            opacity,
				int           *affect,
				int            length,
				int            bytes)
{
  int b;
  unsigned char new_alpha;
  unsigned char * m;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  new_alpha = (*m * opacity) / 255;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b]) ?
	      (src2[b] * new_alpha + src1[b] * (255 - new_alpha)) / 255 :
	    src1[b];

	  m++;

	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = opacity;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b]) ?
	      (src2[b] * new_alpha + src1[b] * (255 - new_alpha)) / 255 :
	    src1[b];

	  src1 += bytes;
	  src2 += bytes;
	  dest += bytes;
	}
    }
}


void
combine_inten_and_inten_a_pixels (unsigned char *src1,
				  unsigned char *src2,
				  unsigned char *dest,
				  unsigned char *mask,
				  int            opacity,
				  int           *affect,
				  int            length,
				  int            bytes)
{
  int alpha, b;
  int src2_bytes;
  unsigned char new_alpha;
  unsigned char * m;

  alpha = bytes;
  src2_bytes = bytes + 1;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  new_alpha = (src2[alpha] * *m * opacity) / 65025;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b]) ?
	      (src2[b] * new_alpha + src1[b] * (255 - new_alpha)) / 255 :
	    src1[b];

	  m++;
	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  new_alpha = (src2[alpha] * opacity) / 255;

	  for (b = 0; b < bytes; b++)
	    dest[b] = (affect[b]) ?
	      (src2[b] * new_alpha + src1[b] * (255 - new_alpha)) / 255 :
	    src1[b];

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
}

#define alphify(src2_alpha,new_alpha) \
	if (new_alpha == 0 || src2_alpha == 0)							\
	  {											\
	    for (b = 0; b < alpha; b++)								\
	      dest[b] = src1 [b];								\
	  }											\
	else if (src2_alpha == new_alpha){							\
	  for (b = 0; b < alpha; b++)								\
	    dest [b] = affect [b] ? src2 [b] : src1 [b];					\
	} else {										\
	  ratio = (float) src2_alpha / new_alpha;						\
	  compl_ratio = 1.0 - ratio;								\
	  											\
	  for (b = 0; b < alpha; b++)								\
	    dest[b] = affect[b] ?								\
	      (unsigned char) (src2[b] * ratio + src1[b] * compl_ratio + EPSILON) : src1[b];	\
	}
	
void
combine_inten_a_and_inten_pixels (unsigned char *src1,
				  unsigned char *src2,
				  unsigned char *dest,
				  unsigned char *mask,
				  int            opacity,
				  int           *affect,
				  int            mode_affect,  /*  how does the combination mode affect alpha?  */
				  int            length,
				  int            bytes)  /*  4 or 2 depending on RGBA or GRAYA  */
{
  int alpha, b;
  int src2_bytes;
  unsigned char src2_alpha;
  unsigned char new_alpha;
  unsigned char * m;
  float ratio, compl_ratio;

  src2_bytes = bytes - 1;
  alpha = bytes - 1;

  if (mask)
    {
      m = mask;
      while (length --)
	{
	  src2_alpha = (*m * opacity) / 255;
	  new_alpha = src1[alpha] + ((255 - src1[alpha]) * src2_alpha) / 255;
	  alphify (src2_alpha, new_alpha);
	  
	  if (mode_affect)
	    dest[alpha] = (affect[alpha]) ? new_alpha : src1[alpha];
	  else
	    dest[alpha] = (src1[alpha]) ? src1[alpha] : (affect[alpha] ? new_alpha : src1[alpha]);

	  m++;

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
  else
    {
      while (length --)
	{
	  src2_alpha = opacity;
	  new_alpha = src1[alpha] + ((255 - src1[alpha]) * src2_alpha) / 255;
	  alphify (src2_alpha, new_alpha);
	  
	  if (mode_affect)
	    dest[alpha] = (affect[alpha]) ? new_alpha : src1[alpha];
	  else
	    dest[alpha] = (src1[alpha]) ? src1[alpha] : (affect[alpha] ? new_alpha : src1[alpha]);

	  src1 += bytes;
	  src2 += src2_bytes;
	  dest += bytes;
	}
    }
}


void
combine_inten_a_and_inten_a_pixels (unsigned char *src1,
				    unsigned char *src2,
				    unsigned char *dest,
				    unsigned char *mask,
				    int           opacity,
				    int          *affect,
				    int           mode_affect,  /*  how does the combination mode affect alpha?  */
				    int           length,
				    int           bytes)  /*  4 or 2 depending on RGBA or GRAYA  */
{
  int alpha, b;
  unsigned char src2_alpha;
  unsigned char new_alpha;
  unsigned char * m;
  float ratio, compl_ratio;

  alpha = bytes - 1;
  if (mask){
    m = mask;
    while (length --)
      {
	src2_alpha = (src2[alpha] * *m * opacity) / 65025;
	new_alpha = src1[alpha] + ((255 - src1[alpha]) * src2_alpha) / 255;

	alphify (src2_alpha, new_alpha);
	
	if (mode_affect)
	  dest[alpha] = (affect[alpha]) ? new_alpha : src1[alpha];
	else
	  dest[alpha] = (src1[alpha]) ? src1[alpha] : (affect[alpha] ? new_alpha : src1[alpha]);
	
	m++;
	
	src1 += bytes;
	src2 += bytes;
	dest += bytes;
      }
  } else {
    while (length --)
      {
	src2_alpha = (src2[alpha] * opacity) / 255;
	new_alpha = src1[alpha] + ((255 - src1[alpha]) * src2_alpha) / 255;

	alphify (src2_alpha, new_alpha);
	
	if (mode_affect)
	  dest[alpha] = (affect[alpha]) ? new_alpha : src1[alpha];
	else
	  dest[alpha] = (src1[alpha]) ? src1[alpha] : (affect[alpha] ? new_alpha : src1[alpha]);

	src1 += bytes;
	src2 += bytes;
	dest += bytes;
      }
  }
}
#undef alphify

void
combine_inten_a_and_channel_mask_pixels (unsigned char *src,
					 unsigned char *channel,
					 unsigned char *dest,
					 unsigned char *col,
					 int            opacity,
					 int            length,
					 int            bytes)
{
  int alpha, b;
  unsigned char channel_alpha;
  unsigned char new_alpha;
  unsigned char compl_alpha;
  int t, s;

  alpha = bytes - 1;
  while (length --)
    {
      channel_alpha = INT_MULT (255 - *channel, opacity, t);
      if (channel_alpha)
	{
	  new_alpha = src[alpha] + INT_MULT ((255 - src[alpha]), channel_alpha, t);

	  if (new_alpha != 255)
	    channel_alpha = (channel_alpha * 255) / new_alpha;
	  compl_alpha = 255 - channel_alpha;

	  for (b = 0; b < alpha; b++)
	    dest[b] = INT_MULT (col[b], channel_alpha, t) +
	      INT_MULT (src[b], compl_alpha, s);
	  dest[b] = new_alpha;
	}
      else
	memcpy(dest, src, bytes);

      /*  advance pointers  */
      src+=bytes;
      dest+=bytes;
      channel++;
    }
}


void
combine_inten_a_and_channel_selection_pixels (unsigned char *src,
					      unsigned char *channel,
					      unsigned char *dest,
					      unsigned char *col,
					      int            opacity,
					      int            length,
					      int            bytes)
{
  int alpha, b;
  unsigned char channel_alpha;
  unsigned char new_alpha;
  unsigned char compl_alpha;
  int t, s;

  alpha = bytes - 1;
  while (length --)
    {
      channel_alpha = INT_MULT (*channel, opacity, t);
      if (channel_alpha)
	{
	  new_alpha = src[alpha] + INT_MULT ((255 - src[alpha]), channel_alpha, t);

	  if (new_alpha != 255)
	    channel_alpha = (channel_alpha * 255) / new_alpha;
	  compl_alpha = 255 - channel_alpha;

	  for (b = 0; b < alpha; b++)
	    dest[b] = INT_MULT (col[b], channel_alpha, t) +
	      INT_MULT (src[b], compl_alpha, s);
	  dest[b] = new_alpha;
	}
      else
	memcpy(dest, src, bytes);

      /*  advance pointers  */
      src+=bytes;
      dest+=bytes;
      channel++;
    }
}


void
behind_inten_pixels (unsigned char *src1,
		     unsigned char *src2,
		     unsigned char *dest,
		     unsigned char *mask,
		     int            opacity,
		     int           *affect,
		     int            length,
		     int            b1,
		     int            b2,
		     int            ha1,
		     int            ha2)
{
  int alpha, b;
  unsigned char src1_alpha;
  unsigned char src2_alpha;
  unsigned char new_alpha;
  unsigned char * m;
  float ratio, compl_ratio;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  /*  the alpha channel  */
  alpha = b1 - 1;

  while (length --)
    {
      src1_alpha = src1[alpha];
      src2_alpha = (src2[alpha] * *m * opacity) / 65025;
      new_alpha = src2_alpha + ((255 - src2_alpha) * src1_alpha) / 255;
      if (new_alpha)
	ratio = (float) src1_alpha / new_alpha;
      else
	ratio = 0.0;
      compl_ratio = 1.0 - ratio;

      for (b = 0; b < alpha; b++)
	dest[b] = (affect[b]) ?
	  (unsigned char) (src1[b] * ratio + src2[b] * compl_ratio + EPSILON) :
	src1[b];

      dest[alpha] = (affect[alpha]) ? new_alpha : src1[alpha];

      if (mask)
	m++;

      src1 += b1;
      src2 += b2;
      dest += b1;
    }
}


void
behind_indexed_pixels (unsigned char *src1,
		       unsigned char *src2,
		       unsigned char *dest,
		       unsigned char *mask,
		       int            opacity,
		       int           *affect,
		       int            length,
		       int            b1,
		       int            b2,
		       int            ha1,
		       int            ha2)
{
  int alpha, b;
  unsigned char src1_alpha;
  unsigned char src2_alpha;
  unsigned char new_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  /*  the alpha channel  */
  alpha = b1 - 1;

  while (length --)
    {
      src1_alpha = src1[alpha];
      src2_alpha = (src2[alpha] * *m * opacity) / 65025;
      new_alpha = (src2_alpha > 127) ? OPAQUE_OPACITY : TRANSPARENT_OPACITY;

      for (b = 0; b < b1; b++)
	dest[b] = (affect[b] && new_alpha == OPAQUE_OPACITY && (src1_alpha > 127)) ?
	  src2[b] : src1[b];

      if (mask)
	m++;

      src1 += b1;
      src2 += b2;
      dest += b1;
    }
}


void
replace_inten_pixels (unsigned char *src1,
		      unsigned char *src2,
		      unsigned char *dest,
		      unsigned char *mask,
		      int            opacity,
		      int           *affect,
		      int            length,
		      int            b1,
		      int            b2,
		      int            ha1,
		      int            ha2)
{
  int bytes, b;
  unsigned char mask_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  bytes = MINIMUM (b1, b2);
  while (length --)
    {
      mask_alpha = (*m * opacity) / 255;

      for (b = 0; b < bytes; b++)
	dest[b] = (affect[b]) ?
	  (src2[b] * mask_alpha + src1[b] * (255 - mask_alpha)) / 255 :
	src1[b];

      if (ha1 && !ha2)
	dest[b] = src1[b];

      if (mask)
	m++;

      src1 += b1;
      src2 += b2;
      dest += b1;
    }
}


void
replace_indexed_pixels (unsigned char *src1,
			unsigned char *src2,
			unsigned char *dest,
			unsigned char *mask,
			int            opacity,
			int           *affect,
			int            length,
			int            b1,
			int            b2,
			int            ha1,
			int            ha2)
{
  int bytes, b;
  unsigned char mask_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  bytes = MINIMUM (b1, b2);
  while (length --)
    {
      mask_alpha = (*m * opacity) / 255;

      for (b = 0; b < bytes; b++)
	dest[b] = (affect[b] && mask_alpha) ?
	  src2[b] : src1[b];

      if (ha1 && !ha2)
	dest[b] = src1[b];

      if (mask)
	m++;

      src1 += b1;
      src2 += b2;
      dest += b1;
    }
}


void
erase_inten_pixels (unsigned char *src1,
		    unsigned char *src2,
		    unsigned char *dest,
		    unsigned char *mask,
		    int            opacity,
		    int           *affect,
		    int            length,
		    int            bytes)
{
  int alpha, b;
  unsigned char src2_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  alpha = bytes - 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = src1[b];

      src2_alpha = (src2[alpha] * *m * opacity) / 65025;
      dest[alpha] = src1[alpha] - (src1[alpha] * src2_alpha) / 255;

      if (mask)
	m++;

      src1 += bytes;
      src2 += bytes;
      dest += bytes;
    }
}


void
erase_indexed_pixels (unsigned char *src1,
		      unsigned char *src2,
		      unsigned char *dest,
		      unsigned char *mask,
		      int            opacity,
		      int           *affect,
		      int            length,
		      int            bytes)
{
  int alpha, b;
  unsigned char src2_alpha;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  alpha = bytes - 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = src1[b];

      src2_alpha = (src2[alpha] * *m * opacity) / 65025;
      dest[alpha] = (src2_alpha > 127) ? TRANSPARENT_OPACITY : src1[alpha];

      if (mask)
	m++;

      src1 += bytes;
      src2 += bytes;
      dest += bytes;
    }
}


void
extract_from_inten_pixels (unsigned char *src,
			   unsigned char *dest,
			   unsigned char *mask,
			   unsigned char *bg,
			   int            cut,
			   int            length,
			   int            bytes,
			   int            has_alpha)
{
  int b, alpha;
  int dest_bytes;
  unsigned char * m;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  alpha = (has_alpha) ? bytes - 1 : bytes;
  dest_bytes = (has_alpha) ? bytes : bytes + 1;
  while (length --)
    {
      for (b = 0; b < alpha; b++)
	dest[b] = src[b];

      if (has_alpha)
	{
	  dest[alpha] = (*m * src[alpha]) / 255;
	  if (cut)
	    src[alpha] = ((255 - *m) * src[alpha]) / 255;
	}
      else
	{
	  dest[alpha] = *m;
	  if (cut)
	    for (b = 0; b < bytes; b++)
	      src[b] = (*m * bg[b] + (255 - *m) * src[b]) / 255;
	}

      if (mask)
	m++;

      src += bytes;
      dest += dest_bytes;
    }
}


void
extract_from_indexed_pixels (unsigned char *src,
			     unsigned char *dest,
			     unsigned char *mask,
			     unsigned char *cmap,
			     unsigned char *bg,
			     int            cut,
			     int            length,
			     int            bytes,
			     int            has_alpha)
{
  int b;
  int index;
  unsigned char * m;
  int t;

  if (mask)
    m = mask;
  else
    m = &no_mask;

  while (length --)
    {
      index = src[0] * 3;
      for (b = 0; b < 3; b++)
	dest[b] = cmap[index + b];

      if (has_alpha)
	{
	  dest[3] = INT_MULT (*m, src[1], t);
	  if (cut)
	    src[1] = INT_MULT ((255 - *m), src[1], t);
	}
      else
	{
	  dest[3] = *m;
	  if (cut)
	    src[0] = (*m > 127) ? bg[0] : src[0];
	}

      if (mask)
	m++;

      src += bytes;
      dest += 4;
    }
}


void
map_to_color (int            src_type,
	      unsigned char *cmap,
	      unsigned char *src,
	      unsigned char *rgb)
{
  switch (src_type)
    {
    case 0:  /*  RGB      */
      /*  Straight copy  */
      *rgb++ = *src++;
      *rgb++ = *src++;
      *rgb   = *src;
      break;
    case 1:  /*  GRAY     */
      *rgb++ = *src;
      *rgb++ = *src;
      *rgb   = *src;
      break;
    case 2:  /*  INDEXED  */
      {
	int index = *src * 3;
	*rgb++ = cmap [index++];
	*rgb++ = cmap [index++];
	*rgb   = cmap [index++];
      }
      break;
    }
}


int
map_rgb_to_indexed (unsigned char *cmap,
		    int            num_cols,
		    int            ID,
		    int            r,
		    int            g,
		    int            b)
{
  unsigned int pixel;
  int hash_index;
  int cmap_index;

  pixel = (r << 16) | (g << 8) | b;
  hash_index = pixel % HASH_TABLE_SIZE;

  /*  Hash table lookup hit  */
  if (color_hash_table[hash_index].colormap_ID == ID &&
      color_hash_table[hash_index].pixel == pixel)
    {
      cmap_index = color_hash_table[hash_index].index;
      color_hash_hits++;
    }
  /*  Hash table lookup miss  */
  else
    {
      unsigned char *col;
      int diff, sum, max;
      int i;

      max = MAXDIFF;
      cmap_index = 0;

      col = cmap;
      for (i = 0; i < num_cols; i++)
	{
	  diff = r - *col++;
	  sum = diff * diff;
	  diff = g - *col++;
	  sum += diff * diff;
	  diff = b - *col++;
	  sum += diff * diff;

	  if (sum < max)
	    {
	      cmap_index = i;
	      max = sum;
	    }
	}

      /*  update the hash table  */
      color_hash_table[hash_index].pixel = pixel;
      color_hash_table[hash_index].index = cmap_index;
      color_hash_table[hash_index].colormap_ID = ID;
      color_hash_misses++;
    }

  return cmap_index;
}



/**************************************************/
/*    REGION FUNCTIONS                            */
/**************************************************/


void
color_region (PixelRegion   *src,
	      unsigned char *col)
{
  int h;
  unsigned char * s;
  void * pr;

  for (pr = pixel_regions_register (1, src); pr != NULL; pr = pixel_regions_process (pr))
    {
      h = src->h;
      s = src->data;

      while (h--)
	{
	  color_pixels (s, col, src->w, src->bytes);
	  s += src->rowstride;
	}
    }
}


void
blend_region (PixelRegion *src1,
	      PixelRegion *src2,
	      PixelRegion *dest,
	      int          blend)
{
  int h;
  unsigned char * s1, * s2, * d;

  s1 = src1->data;
  s2 = src2->data;
  d = dest->data;
  h = src1->h;

  while (h --)
    {
/*      blend_pixels (s1, s2, d, blend, src1->w, src1->bytes);*/
      s1 += src1->rowstride;
      s2 += src2->rowstride;
      d += dest->rowstride;
    }
}


void
shade_region (PixelRegion   *src,
	      PixelRegion   *dest,
	      unsigned char *col,
	      int            blend)
{
  int h;
  unsigned char * s, * d;

  s = src->data;
  d = dest->data;
  h = src->h;

  while (h --)
    {
/*      blend_pixels (s, d, col, blend, src->w, src->bytes);*/
      s += src->rowstride;
      d += dest->rowstride;
    }
}


void
copy_region (PixelRegion *src,
	     PixelRegion *dest)
{
  int h;
  int pixelwidth;
  unsigned char * s, * d;
  void * pr;

  for (pr = pixel_regions_register (2, src, dest); pr != NULL; pr = pixel_regions_process (pr))
    {
      pixelwidth = src->w * src->bytes;
      s = src->data;
      d = dest->data;
      h = src->h;

      while (h --)
	{
	  memcpy (d, s, pixelwidth);
	  s += src->rowstride;
	  d += dest->rowstride;
	}
    }
}


void
add_alpha_region (PixelRegion *src,
		  PixelRegion *dest)
{
  int h;
  unsigned char * s, * d;
  void * pr;

  for (pr = pixel_regions_register (2, src, dest); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      d = dest->data;
      h = src->h;

      while (h --)
	{
	  add_alpha_pixels (s, d, src->w, src->bytes);
	  s += src->rowstride;
	  d += dest->rowstride;
	}
    }
}


void
flatten_region (PixelRegion   *src,
		PixelRegion   *dest,
		unsigned char *bg)
{
  int h;
  unsigned char * s, * d;

  s = src->data;
  d = dest->data;
  h = src->h;

  while (h --)
    {
      flatten_pixels (s, d, bg, src->w, src->bytes);
      s += src->rowstride;
      d += dest->rowstride;
    }
}


void
extract_alpha_region (PixelRegion *src,
		      PixelRegion *mask,
		      PixelRegion *dest)
{
  int h;
  unsigned char * s, * m, * d;
  void * pr;

  for (pr = pixel_regions_register (3, src, mask, dest); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      d = dest->data;
      if (mask)
	m = mask->data;
      else
	m = NULL;

      h = src->h;
      while (h --)
	{
	  extract_alpha_pixels (s, m, d, src->w, src->bytes);
	  s += src->rowstride;
	  d += dest->rowstride;
	  if (mask)
	    m += mask->rowstride;
	}
    }
}


void
extract_from_region (PixelRegion   *src,
		     PixelRegion   *dest,
		     PixelRegion   *mask,
		     unsigned char *cmap,
		     unsigned char *bg,
		     int            type,
		     int            has_alpha,
		     int            cut)
{
  int h;
  unsigned char * s, * d, * m;
  void * pr;

  for (pr = pixel_regions_register (3, src, dest, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      d = dest->data;
      m = (mask) ? mask->data : NULL;
      h = src->h;

      while (h --)
	{
	  switch (type)
	    {
	    case 0:  /*  RGB      */
	    case 1:  /*  GRAY     */
	      extract_from_inten_pixels (s, d, m, bg, cut, src->w,
					 src->bytes, has_alpha);
	      break;
	    case 2:  /*  INDEXED  */
	      extract_from_indexed_pixels (s, d, m, cmap, bg, cut, src->w,
					   src->bytes, has_alpha);
	      break;
	    }

	  s += src->rowstride;
	  d += dest->rowstride;
	  if (mask)
	    m += mask->rowstride;
	}
    }
}


void
convolve_region (PixelRegion *srcR,
		 PixelRegion *destR,
		 int         *matrix,
		 int          size,
		 int          divisor,
		 int          mode)
{
  /*  Convolve the src image using the convolution matrix, writing to dest  */
  /*  Convolve is not tile-enabled--use accordingly  */
  unsigned char *src, *s_row, * s;
  unsigned char *dest, * d;
  int * m;
  int total [4];
  int b, bytes;
  int length;
  int wraparound;
  int margin;      /*  margin imposed by size of conv. matrix  */
  int i, j;
  int x, y;
  int offset;

  /*  If the mode is NEGATIVE, the offset should be 128  */
  if (mode == NEGATIVE)
    {
      offset = 128;
      mode = 0;
    }
  else
    offset = 0;

  /*  check for the boundary cases  */
  if (srcR->w < (size - 1) || srcR->h < (size - 1))
    return;

  /*  Initialize some values  */
  bytes = srcR->bytes;
  length = bytes * srcR->w;
  margin = size / 2;
  src = srcR->data;
  dest = destR->data;

  /*  calculate the source wraparound value  */
  wraparound = srcR->rowstride - size * bytes;

  /* copy the first (size / 2) scanlines of the src image... */
  for (i = 0; i < margin; i++)
    {
      memcpy (dest, src, length);
      src += srcR->rowstride;
      dest += destR->rowstride;
    }

  src = srcR->data;

  for (y = margin; y < srcR->h - margin; y++)
    {
      s_row = src;
      s = s_row + srcR->rowstride*margin;
      d = dest;

      /* handle the first margin pixels... */
      b = bytes * margin;
      while (b --)
	*d++ = *s++;

      /* now, handle the center pixels */
      x = srcR->w - margin*2;
      while (x--)
	{
	  s = s_row;

	  m = matrix;
	  total [0] = total [1] = total [2] = total [3] = 0;
	  i = size;
	  while (i --)
	    {
	      j = size;
	      while (j --)
		{
		  for (b = 0; b < bytes; b++)
		    total [b] += *m * *s++;
		  m ++;
		}

	      s += wraparound;
	    }

	  for (b = 0; b < bytes; b++)
	    {
	      total [b] = total [b] / divisor + offset;

	      /*  only if mode was ABSOLUTE will mode by non-zero here  */
	      if (total [b] < 0 && mode)
		total [b] = - total [b];

	      if (total [b] < 0)
		*d++ = 0;
	      else
		*d++ = (total [b] > 255) ? 255 : (unsigned char) total [b];
	    }

	  s_row += bytes;

	}

      /* handle the last pixel... */
      s = s_row + (srcR->rowstride + bytes) * margin;
      b = bytes * margin;
      while (b --)
	*d++ = *s++;

      /* set the memory pointers */
      src += srcR->rowstride;
      dest += destR->rowstride;
    }

  src += srcR->rowstride*margin;

  /* copy the last (margin) scanlines of the src image... */
  for (i = 0; i < margin; i++)
    {
      memcpy (dest, src, length);
      src += srcR->rowstride;
      dest += destR->rowstride;
    }
}

/* Convert from separated alpha to premultiplied alpha. Only works on
   non-tiled regions! */
void
multiply_alpha_region (PixelRegion *srcR)
{
  unsigned char *src, *s;
  int x, y;
  int width, height;
  int b, bytes;
  double alpha_val;

  width = srcR->w;
  height = srcR->h;
  bytes = srcR->bytes;

  src = srcR->data;

  for (y = 0; y < height; y++)
    {
      s = src;
      for (x = 0; x < width; x++)
	{
	  alpha_val = s[bytes - 1] * (1.0 / 255.0);
	  for (b = 0; b < bytes - 1; b++)
	    s[b] = 0.5 + s[b] * alpha_val;
	  s += bytes;
	}
      src += srcR->rowstride;
    }
}

/* Convert from premultiplied alpha to separated alpha. Only works on
   non-tiled regions! */
void
separate_alpha_region (PixelRegion *srcR)
{
  unsigned char *src, *s;
  int x, y;
  int width, height;
  int b, bytes;
  double alpha_recip;
  int new_val;

  width = srcR->w;
  height = srcR->h;
  bytes = srcR->bytes;

  src = srcR->data;

  for (y = 0; y < height; y++)
    {
      s = src;
      for (x = 0; x < width; x++)
	{
	  /* predicate is equivalent to:
	     (((s[bytes - 1] - 1) & 255) + 2) & 256
	     */
	  if (s[bytes - 1] != 0 && s[bytes - 1] != 255)
	    {
	      alpha_recip = 255.0 / s[bytes - 1];
	      for (b = 0; b < bytes - 1; b++)
		{
		  new_val = 0.5 + s[b] * alpha_recip;
		  new_val = MIN (new_val, 255);
		  s[b] = new_val;
		}
	    }
	  s += bytes;
	}
      src += srcR->rowstride;
    }
}

void
gaussian_blur_region (PixelRegion *srcR,
		      double       radius)
{
  double std_dev;
  long width, height;
  int bytes;
  unsigned char *src, *sp;
  unsigned char *dest, *dp;
  unsigned char *data;
  int *buf, *b;
  int pixels;
  int total;
  int i, row, col;
  int start, end;
  int *curve;
  int *sum;
  int val;
  int length;
  int alpha;
  int initial_p, initial_m;

  if (radius == 0.0) return;		/* zero blur is a no-op */

  /*  allocate the result buffer  */
  length = MAXIMUM (srcR->w, srcR->h) * srcR->bytes;
  data = paint_funcs_get_buffer (length * 2);
  src = data;
  dest = data + length;

  std_dev = sqrt (-(radius * radius) / (2 * log (1.0 / 255.0)));

  curve = make_curve (std_dev, &length);
  sum = g_malloc (sizeof (int) * (2 * length + 1));

  sum[0] = 0;

  for (i = 1; i <= length*2; i++)
    sum[i] = curve[i-length-1] + sum[i-1];
  sum += length;

  width = srcR->w;
  height = srcR->h;
  bytes = srcR->bytes;
  alpha = bytes - 1;

  buf = g_malloc (sizeof (int) * MAXIMUM (width, height) * 2);

  total = sum[length] - sum[-length];

  for (col = 0; col < width; col++)
    {
      pixel_region_get_col (srcR, col + srcR->x, srcR->y, height, src, 1);
      sp = src + alpha;

      initial_p = sp[0];
      initial_m = sp[(height-1) * bytes];

      /*  Determine a run-length encoded version of the column  */
      run_length_encode (sp, buf, height, bytes);

      for (row = 0; row < height; row++)
	{
	  start = (row < length) ? -row : -length;
	  end = (height <= (row + length)) ? (height - row - 1) : length;

	  val = 0;
	  i = start;
	  b = buf + (row + i) * 2;

	  if (start != -length)
	    val += initial_p * (sum[start] - sum[-length]);

	  while (i < end)
	    {
	      pixels = b[0];
	      i += pixels;
	      if (i > end)
		i = end;
	      val += b[1] * (sum[i] - sum[start]);
	      b += (pixels * 2);
	      start = i;
	    }

	  if (end != length)
	    val += initial_m * (sum[length] - sum[end]);

	  sp[row * bytes] = val / total;
	}

      pixel_region_set_col (srcR, col + srcR->x, srcR->y, height, src);
    }

  for (row = 0; row < height; row++)
    {
      pixel_region_get_row (srcR, srcR->x, row + srcR->y, width, src, 1);
      sp = src + alpha;
      dp = dest + alpha;

      initial_p = sp[0];
      initial_m = sp[(width-1) * bytes];

      /*  Determine a run-length encoded version of the row  */
      run_length_encode (sp, buf, width, bytes);

      for (col = 0; col < width; col++)
	{
	  start = (col < length) ? -col : -length;
	  end = (width <= (col + length)) ? (width - col - 1) : length;

	  val = 0;
	  i = start;
	  b = buf + (col + i) * 2;

	  if (start != -length)
	    val += initial_p * (sum[start] - sum[-length]);

	  while (i < end)
	    {
	      pixels = b[0];
	      i += pixels;
	      if (i > end)
		i = end;
	      val += b[1] * (sum[i] - sum[start]);
	      b += (pixels * 2);
	      start = i;
	    }

	  if (end != length)
	    val += initial_m * (sum[length] - sum[end]);

	  val = val / total;

	  dp[col * bytes] = val;
	}
      
      pixel_region_set_row (srcR, srcR->x, row + srcR->y, width, dest);
    }

  g_free (buf);
  g_free (sum - length);
  g_free (curve - length);
}


void
border_region (PixelRegion *destPR,
	       void        *bs_ptr,
	       int          bs_segs,
	       int          radius)
{
  BoundSeg * bs;
  unsigned char opacity;
  int r, i, j;

  bs = (BoundSeg *) bs_ptr;

  /*  draw the border  */
  for (r = 0; r <= radius; r++)
    {
      opacity = 255 * (r + 1) / (radius + 1);
      j = radius - r;

      for (i = 0; i <= j; i++)
	{
	  /*  northwest  */
	  draw_segments (destPR, bs, bs_segs, -i, -(j - i), opacity);

	  /*  only draw the rest if they are different  */
	  if (j)
	    {
	      /*  northeast  */
	      draw_segments (destPR, bs, bs_segs, i, -(j - i), opacity);
	      /*  southeast  */
	      draw_segments (destPR, bs, bs_segs, i, (j - i), opacity);
	      /*  southwest  */
	      draw_segments (destPR, bs, bs_segs, -i, (j - i), opacity);
	    }
	}
    }
}


/* non-interpolating scale_region.  [adam]
 */
void
scale_region_no_resample (PixelRegion *srcPR,
			  PixelRegion *destPR)
{
  int * x_src_offsets;
  int * y_src_offsets;
  unsigned char * src;
  unsigned char * dest;
  int width, height, orig_width, orig_height;
  int last_src_y;
  int row_bytes;
  int x,y,b;
  char bytes;

  orig_width = srcPR->w;
  orig_height = srcPR->h;

  width = destPR->w;
  height = destPR->h;

  bytes = srcPR->bytes;

  /*  the data pointers...  */
  x_src_offsets = (int *) g_malloc (width * bytes * sizeof(int));
  y_src_offsets = (int *) g_malloc (height * sizeof(int));
  src  = (unsigned char *) g_malloc (orig_width * bytes);
  dest = (unsigned char *) g_malloc (width * bytes);
  
  /*  pre-calc the scale tables  */
  for (b = 0; b < bytes; b++)
    {
      for (x = 0; x < width; x++)
	{
	  x_src_offsets [b + x * bytes] = b + bytes * ((x * orig_width + orig_width / 2) / width);
	}
    }
  for (y = 0; y < height; y++)
    {
      y_src_offsets [y] = (y * orig_height + orig_height / 2) / height;
    }
  
  /*  do the scaling  */
  row_bytes = width * bytes;
  last_src_y = -1;
  for (y = 0; y < height; y++)
    {
      /* if the source of this line was the same as the source
       *  of the last line, there's no point in re-rescaling.
       */
      if (y_src_offsets[y] != last_src_y)
	{
	  pixel_region_get_row (srcPR, 0, y_src_offsets[y], orig_width, src, 1);
	  for (x = 0; x < row_bytes ; x++)
	    {
	      dest[x] = src[x_src_offsets[x]];
	    }
	  last_src_y = y_src_offsets[y];
	}

      pixel_region_set_row (destPR, 0, y, width, dest);
    }
  
  g_free (x_src_offsets);
  g_free (y_src_offsets);
  g_free (src);
  g_free (dest);
}


void
scale_region (PixelRegion *srcPR,
	      PixelRegion *destPR)
{
  unsigned char * src_m1, * src, * src_p1, * src_p2;
  unsigned char * s_m1, * s, * s_p1, * s_p2;
  unsigned char * dest, * d;
  double * row, * r;
  int src_row, src_col;
  int bytes, b;
  int width, height;
  int orig_width, orig_height;
  double x_rat, y_rat;
  double x_cum, y_cum;
  double x_last, y_last;
  double * x_frac, y_frac, tot_frac;
  float dx, dy;
  int i, j;
  int frac;
  int advance_dest_x, advance_dest_y;
  int minus_x, plus_x, plus2_x;
  ScaleType scale_type;

  orig_width = srcPR->w;
  orig_height = srcPR->h;

  width = destPR->w;
  height = destPR->h;

  /*  Some calculations...  */
  bytes = destPR->bytes;

  /*  the data pointers...  */
  src_m1 = (unsigned char *) g_malloc (orig_width * bytes);
  src    = (unsigned char *) g_malloc (orig_width * bytes);
  src_p1 = (unsigned char *) g_malloc (orig_width * bytes);
  src_p2 = (unsigned char *) g_malloc (orig_width * bytes);
  dest   = (unsigned char *) g_malloc (width * bytes);

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (double) orig_width / (double) width;
  y_rat = (double) orig_height / (double) height;

  /*  determine the scale type  */
  if (x_rat < 1.0 && y_rat < 1.0)
    scale_type = MagnifyX_MagnifyY;
  else if (x_rat < 1.0 && y_rat >= 1.0)
    scale_type = MagnifyX_MinifyY;
  else if (x_rat >= 1.0 && y_rat < 1.0)
    scale_type = MinifyX_MagnifyY;
  else
    scale_type = MinifyX_MinifyY;

  /*  allocate an array to help with the calculations  */
  row    = (double *) g_malloc (sizeof (double) * width * bytes);
  x_frac = (double *) g_malloc (sizeof (double) * (width + orig_width));

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = 0;
  x_cum = (double) src_col;
  x_last = x_cum;

  for (i = 0; i < width + orig_width; i++)
    {
      if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (double) * width * bytes);

  /*  counters...  */
  src_row = 0;
  y_cum = (double) src_row;
  y_last = y_cum;

  /*  Get the first src row  */
  pixel_region_get_row (srcPR, 0, src_row, orig_width, src, 1);
  /*  Get the next two if possible  */
  if (src_row < (orig_height - 1))
    pixel_region_get_row (srcPR, 0, (src_row + 1), orig_width, src_p1, 1);
  if ((src_row + 1) < (orig_height - 1))
    pixel_region_get_row (srcPR, 0, (src_row + 2), orig_width, src_p2, 1);

  /*  Scale the selected region  */
  i = height;
  while (i)
    {
      src_col = 0;
      x_cum = (double) src_col;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= (src_row + 1 + EPSILON))
	{
	  y_cum += y_rat;
	  dy = y_cum - src_row;
	  y_frac = y_cum - y_last;
	  advance_dest_y = TRUE;
	}
      else
	{
	  y_frac = (src_row + 1) - y_last;
	  dy = 1.0;
	  advance_dest_y = FALSE;
	}

      y_last += y_frac;

      s = src;
      s_m1 = (src_row > 0) ? src_m1 : src;
      s_p1 = (src_row < (orig_height - 1)) ? src_p1 : src;
      s_p2 = ((src_row + 1) < (orig_height - 1)) ? src_p2 : s_p1;

      r = row;

      frac = 0;
      j = width;

      while (j)
	{
	  if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	    {
	      x_cum += x_rat;
	      dx = x_cum - src_col;
	      advance_dest_x = TRUE;
	    }
	  else
	    {
	      dx = 1.0;
	      advance_dest_x = FALSE;
	    }

	  tot_frac = x_frac[frac++] * y_frac;

	  minus_x = (src_col > 0) ? -bytes : 0;
	  plus_x = (src_col < (orig_width - 1)) ? bytes : 0;
	  plus2_x = ((src_col + 1) < (orig_width - 1)) ? bytes * 2 : plus_x;

	  if (cubic_interpolation)
	    switch (scale_type)
	      {
	      case MagnifyX_MagnifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += cubic (dy, cubic (dx, s_m1[b+minus_x], s_m1[b], s_m1[b+plus_x], s_m1[b+plus2_x]),
				 cubic (dx, s[b+minus_x], s[b],	s[b+plus_x], s[b+plus2_x]),
				 cubic (dx, s_p1[b+minus_x], s_p1[b], s_p1[b+plus_x], s_p1[b+plus2_x]),
				 cubic (dx, s_p2[b+minus_x], s_p2[b], s_p2[b+plus_x], s_p2[b+plus2_x])) * tot_frac;
		break;
	      case MagnifyX_MinifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += cubic (dx, s[b+minus_x], s[b], s[b+plus_x], s[b+plus2_x]) * tot_frac;
		break;
	      case MinifyX_MagnifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += cubic (dy, s_m1[b], s[b], s_p1[b], s_p2[b]) * tot_frac;
		break;
	      case MinifyX_MinifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += s[b] * tot_frac;
		break;
	      }
	  else
	    switch (scale_type)
	      {
	      case MagnifyX_MagnifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += ((1 - dy) * ((1 - dx) * s[b] + dx * s[b+plus_x]) +
			   dy  * ((1 - dx) * s_p1[b] + dx * s_p1[b+plus_x])) * tot_frac;
		break;
	      case MagnifyX_MinifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += (s[b] * (1 - dx) + s[b+plus_x] * dx) * tot_frac;
		break;
	      case MinifyX_MagnifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += (s[b] * (1 - dy) + s_p1[b] * dy) * tot_frac;
		break;
	      case MinifyX_MinifyY:
		for (b = 0; b < bytes; b++)
		  r[b] += s[b] * tot_frac;
		break;
	      }

	  if (advance_dest_x)
	    {
	      r += bytes;
	      j--;
	    }
	  else
	    {
	      s_m1 += bytes;
	      s    += bytes;
	      s_p1 += bytes;
	      s_p2 += bytes;
	      src_col++;
	    }
	}

      if (advance_dest_y)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;

	  j = width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (unsigned char) (*r++ * tot_frac);
	    }

	  /*  set the pixel region span  */
	  pixel_region_set_row (destPR, 0, (height - i), width, dest);

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (double) * width * bytes);

	  i--;
	}
      else
	{
	  /*  Shuffle pointers  */
	  s = src_m1;
	  src_m1 = src;
	  src = src_p1;
	  src_p1 = src_p2;
	  src_p2 = s;

	  src_row++;
	  if ((src_row + 1) < (orig_height - 1))
	    pixel_region_get_row (srcPR, 0, (src_row + 2), orig_width, src_p2, 1);
	}
    }

  /*  free up temporary arrays  */
  g_free (row);
  g_free (x_frac);
  g_free (src_m1);
  g_free (src);
  g_free (src_p1);
  g_free (src_p2);
  g_free (dest);
}


void
subsample_region (PixelRegion *srcPR,
		  PixelRegion *destPR,
		  int          subsample)
{
  unsigned char * src, * s;
  unsigned char * dest, * d;
  double * row, * r;
  int destwidth;
  int src_row, src_col;
  int bytes, b;
  int width, height;
  int orig_width, orig_height;
  double x_rat, y_rat;
  double x_cum, y_cum;
  double x_last, y_last;
  double * x_frac, y_frac, tot_frac;
  int i, j;
  int frac;
  int advance_dest;

  orig_width = srcPR->w / subsample;
  orig_height = srcPR->h / subsample;
  width = destPR->w;
  height = destPR->h;

  /*  Some calculations...  */
  bytes = destPR->bytes;
  destwidth = destPR->rowstride;

  /*  the data pointers...  */
  src = (unsigned char *) g_malloc (orig_width * bytes);
  dest = destPR->data;

  /*  find the ratios of old x to new x and old y to new y  */
  x_rat = (double) orig_width / (double) width;
  y_rat = (double) orig_height / (double) height;

  /*  allocate an array to help with the calculations  */
  row    = (double *) g_malloc (sizeof (double) * width * bytes);
  x_frac = (double *) g_malloc (sizeof (double) * (width + orig_width));

  /*  initialize the pre-calculated pixel fraction array  */
  src_col = 0;
  x_cum = (double) src_col;
  x_last = x_cum;

  for (i = 0; i < width + orig_width; i++)
    {
      if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	{
	  x_cum += x_rat;
	  x_frac[i] = x_cum - x_last;
	}
      else
	{
	  src_col ++;
	  x_frac[i] = src_col - x_last;
	}
      x_last += x_frac[i];
    }

  /*  clear the "row" array  */
  memset (row, 0, sizeof (double) * width * bytes);

  /*  counters...  */
  src_row = 0;
  y_cum = (double) src_row;
  y_last = y_cum;

  pixel_region_get_row (srcPR, 0, src_row * subsample, orig_width * subsample, src, subsample);

  /*  Scale the selected region  */
  for (i = 0; i < height; )
    {
      src_col = 0;
      x_cum = (double) src_col;

      /* determine the fraction of the src pixel we are using for y */
      if (y_cum + y_rat <= (src_row + 1 + EPSILON))
	{
	  y_cum += y_rat;
	  y_frac = y_cum - y_last;
	  advance_dest = TRUE;
	}
      else
	{
	  src_row ++;
	  y_frac = src_row - y_last;
	  advance_dest = FALSE;
	}

      y_last += y_frac;

      s = src;
      r = row;

      frac = 0;
      j = width;

      while (j)
	{
	  tot_frac = x_frac[frac++] * y_frac;

	  for (b = 0; b < bytes; b++)
	    r[b] += s[b] * tot_frac;

	  /*  increment the destination  */
	  if (x_cum + x_rat <= (src_col + 1 + EPSILON))
	    {
	      r += bytes;
	      x_cum += x_rat;
	      j--;
	    }

	  /* increment the source */
	  else
	    {
	      s += bytes;
	      src_col++;
	    }
	}

      if (advance_dest)
	{
	  tot_frac = 1.0 / (x_rat * y_rat);

	  /*  copy "row" to "dest"  */
	  d = dest;
	  r = row;

	  j = width;
	  while (j--)
	    {
	      b = bytes;
	      while (b--)
		*d++ = (unsigned char) (*r++ * tot_frac);
	    }

	  dest += destwidth;

	  /*  clear the "row" array  */
	  memset (row, 0, sizeof (double) * destwidth);

	  i++;
	}
      else
	pixel_region_get_row (srcPR, 0, src_row * subsample, orig_width * subsample, src, subsample);
    }

  /*  free up temporary arrays  */
  g_free (row);
  g_free (x_frac);
  g_free (src);
}


float
shapeburst_region (PixelRegion *srcPR,
		   PixelRegion *distPR)
{
  Tile *tile;
  unsigned char *tile_data;
  float max_iterations;
  float *distp_cur;
  float *distp_prev;
  float *tmp;
  float min_prev;
  float float_tmp;
  int min;
  int min_left;
  int length;
  int i, j, k;
  int src;
  int fraction;
  int prev_frac;
  int x, y;
  int end;
  int boundary;
  int inc;

  src = 0;

  max_iterations = 0.0;

  length = distPR->w + 1;
  distp_prev = (float *) paint_funcs_get_buffer (sizeof (float) * length * 2);
  for (i = 0; i < length; i++)
    distp_prev[i] = 0.0;

  distp_prev += 1;
  distp_cur = distp_prev + length;

  for (i = 0; i < srcPR->h; i++)
    {
      /*  set the current dist row to 0's  */
      memset(distp_cur - 1, 0, sizeof(float) * (length - 1));

      for (j = 0; j < srcPR->w; j++)
	{
	  min_prev = MINIMUM (distp_cur[j-1], distp_prev[j]);
	  min_left = MINIMUM ((srcPR->w - j - 1), (srcPR->h - i - 1));
	  min = (int) MINIMUM (min_left, min_prev);
	  fraction = 255;

	  /*  This might need to be changed to 0 instead of k = (min) ? (min - 1) : 0  */
	  for (k = (min) ? (min - 1) : 0; k <= min; k++)
	    {
	      x = j;
	      y = i + k;
	      end = y - k;

	      while (y >= end)
		{
		  tile = tile_manager_get_tile (srcPR->tiles, x, y, 0);
		  tile_ref (tile);
		  tile_data = tile->data + (tile->ewidth * (y % TILE_HEIGHT) + (x % TILE_WIDTH));
		  boundary = MINIMUM ((y % TILE_HEIGHT), (tile->ewidth - (x % TILE_WIDTH) - 1));
		  boundary = MINIMUM (boundary, (y - end)) + 1;
		  inc = 1 - tile->ewidth;

		  while (boundary--)
		    {
		      src = *tile_data;
		      if (src == 0)
			{
			  min = k;
			  y = -1;
			  break;
			}
		      if (src < fraction)
			fraction = src;

		      x++;
		      y--;
		      tile_data += inc;
		    }

		  tile_unref (tile, FALSE);
		}
	    }

	  if (src != 0)
	    {
	      /*  If min_left != min_prev use the previous fraction
	       *   if it is less than the one found
	       */
	      if (min_left != min)
		{
		  prev_frac = (int) (255 * (min_prev - min));
		  if (prev_frac == 255)
		    prev_frac = 0;
		  fraction = MINIMUM (fraction, prev_frac);
		}
	      min++;
	    }

	  float_tmp = distp_cur[j] = min + fraction / 256.0;

	  if (float_tmp > max_iterations)
	    max_iterations = float_tmp;
	}

      /*  set the dist row  */
      pixel_region_set_row (distPR, distPR->x, distPR->y + i, distPR->w, (unsigned char *) distp_cur);

      /*  swap pointers around  */
      tmp = distp_prev;
      distp_prev = distp_cur;
      distp_cur = tmp;
    }

  return max_iterations;
}


int
thin_region (PixelRegion *src,
	     int          type)
{
  int i, j;
  int found_one;
  unsigned char *destp;
  unsigned char *prev_row;
  unsigned char *cur_row;
  unsigned char *next_row;
  unsigned char *temp_row;

  /*  feed both the prev and cur row buffers from the paint func buffer  */
  prev_row = paint_funcs_get_buffer ((src->w + 2) * 3 + src->w);

  /*  set all values to 0  */
  memset (prev_row, 0, (src->w + 2) * 3);

  prev_row += 1;
  cur_row = prev_row + src->w + 2;
  next_row = cur_row + src->w + 2;
  destp = next_row + src->w + 1;

  pixel_region_get_row (src, src->x, src->y, src->w, cur_row, 1);
  found_one = FALSE;

  for (i = 0; i < src->h; i++)
    {
      if (i < (src->h - 1))
	pixel_region_get_row (src, src->x, src->y + i + 1, src->w, next_row, 1);
      else
	memset (next_row, 0, src->w);

      for (j = 0; j < src->w; j++)
	{
	  if (type == SHRINK_REGION)
	    {
	      if (cur_row[j] != 0)
		{
		  found_one = TRUE;
		  destp[j] = cur_row[j];

		  if (cur_row[j - 1] < destp[j])
		    destp[j] = cur_row[j - 1];
		  if (cur_row[j + 1] < destp[j])
		    destp[j] = cur_row[j + 1];
		  if (prev_row[j] < destp[j])
		    destp[j] = prev_row[j];
		  if (next_row[j] < destp[j])
		    destp[j] = next_row[j];
		}
	      else
		destp[j] = cur_row[j];
	    }
	  else
	    {
	      if (cur_row[j] != 255)
		{
		  found_one = TRUE;
		  destp[j] = cur_row[j];

		  if (cur_row[j - 1] > destp[j])
		    destp[j] = cur_row[j - 1];
		  if (cur_row[j + 1] > destp[j])
		    destp[j] = cur_row[j + 1];
		  if (prev_row[j] > destp[j])
		    destp[j] = prev_row[j];
		  if (next_row[j] > destp[j])
		    destp[j] = next_row[j];
		}
	      else
		destp[j] = cur_row[j];
	    }
	}

      pixel_region_set_row (src, src->x, src->y + i, src->w, destp);

      temp_row = prev_row;
      prev_row = cur_row;
      cur_row = next_row;
      next_row = temp_row;
    }

  return (found_one == FALSE);  /*  is the area empty yet?  */
}


void
swap_region (PixelRegion *src,
	     PixelRegion *dest)
{
  int h;
  int length;
  unsigned char * s, * d;
  void * pr;

  for (pr = pixel_regions_register (2, src, dest); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      h = src->h;
      d = dest->data;
      length = src->w * src->bytes;

      while (h --)
	{
	  swap_pixels (s, d, length);
	  s += src->rowstride;
	  d += dest->rowstride;
	}
    }
}


void
apply_mask_to_region (PixelRegion *src,
		      PixelRegion *mask,
		      int          opacity)
{
  int h;
  unsigned char * s, * m;
  void * pr;

  for (pr = pixel_regions_register (2, src, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      m = mask->data;
      h = src->h;

      while (h --)
	{
	  apply_mask_to_alpha_channel (s, m, opacity, src->w, src->bytes);
	  s += src->rowstride;
	  m += mask->rowstride;
	}
    }
}


void
combine_mask_and_region (PixelRegion *src,
			 PixelRegion *mask,
			 int          opacity)
{
  int h;
  unsigned char * s, * m;
  void * pr;

  for (pr = pixel_regions_register (2, src, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      s = src->data;
      m = mask->data;
      h = src->h;

      while (h --)
	{
	  combine_mask_and_alpha_channel (s, m, opacity, src->w, src->bytes);
	  s += src->rowstride;
	  m += mask->rowstride;
	}
    }
}


void
copy_gray_to_region (PixelRegion *src,
		     PixelRegion *dest)
{
  int h;
  unsigned char * s, * d;
  void * pr;

  for (pr = pixel_regions_register (2, src, dest); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      d = dest->data;
      h = src->h;

      while (h --)
	{
	  copy_gray_to_inten_a_pixels (s, d, src->w, dest->bytes);
	  s += src->rowstride;
	  d += dest->rowstride;
	}
    }
}


void
initial_region (PixelRegion   *src,
		PixelRegion   *dest,
		PixelRegion   *mask,
		unsigned char *data,
		int            opacity,
		int            mode,
		int           *affect,
		int            type)
{
  int h;
  unsigned char * s, * d, * m;
  unsigned char * buf;
  void * pr;

  buf = paint_funcs_get_buffer (src->w * (src->bytes + 1));

  for (pr = pixel_regions_register (3, src, dest, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s = src->data;
      d = dest->data;
      m = (mask) ? mask->data : NULL;

      for (h = 0; h < src->h; h++)
	{
	  /*  based on the type of the initial image...  */
	  switch (type)
	    {
	    case INITIAL_CHANNEL_MASK:
	    case INITIAL_CHANNEL_SELECTION:
	      initial_channel_pixels (s, d, src->w, dest->bytes);
	      break;

	    case INITIAL_INDEXED:
	      initial_indexed_pixels (s, d, data, src->w);
	      break;

	    case INITIAL_INDEXED_ALPHA:
	      initial_indexed_a_pixels (s, d, m, data, opacity, src->w);
	      break;

	    case INITIAL_INTENSITY:
	      if (mode == DISSOLVE_MODE)
		{
		  dissolve_pixels (s, buf, src->x, src->y + h, opacity, src->w, src->bytes,
				   src->bytes + 1, 0);
		  initial_inten_pixels (buf, d, m, opacity, affect,
					src->w, src->bytes);
		}
	      else
		initial_inten_pixels (s, d, m, opacity, affect, src->w, src->bytes);
	      break;

	    case INITIAL_INTENSITY_ALPHA:
	      if (mode == DISSOLVE_MODE)
		{
		  dissolve_pixels (s, buf, src->x, src->y + h, opacity, src->w, src->bytes,
				   src->bytes, 1);
		  initial_inten_a_pixels (buf, d, m, opacity, affect,
					  src->w, src->bytes);
		}
	      else
		initial_inten_a_pixels (s, d, m, opacity, affect, src->w, src->bytes);
	      break;
	    }

	  s += src->rowstride;
	  d += dest->rowstride;
	  if (mask)
	    m += mask->rowstride;
	}
    }
}


void
combine_regions (PixelRegion   *src1,
		 PixelRegion   *src2,
		 PixelRegion   *dest,
		 PixelRegion   *mask,
		 unsigned char *data,
		 int            opacity,
		 int            mode,
		 int           *affect,
		 int            type)
{
  int h;
  int ha1, ha2;
  int combine;
  int mode_affect;
  unsigned char * s, * s1, * s2;
  unsigned char * d, * m;
  unsigned char * buf;
  void * pr;

  combine = 0;

  /*  Determine which sources have alpha channels  */
  switch (type)
    {
    case COMBINE_INTEN_INTEN:
    case COMBINE_INDEXED_INDEXED:
      ha1 = ha2 = 0;
      break;
    case COMBINE_INTEN_A_INTEN:
      ha1 = 1;
      ha2 = 0;
      break;
    case COMBINE_INTEN_INTEN_A:
    case COMBINE_INDEXED_INDEXED_A:
      ha1 = 0;
      ha2 = 1;
      break;
    case COMBINE_INTEN_A_INTEN_A:
    case COMBINE_INDEXED_A_INDEXED_A:
      ha1 = ha2 = 1;
      break;
    default:
      ha1 = ha2 = 0;
    }

  buf = paint_funcs_get_buffer (src1->w * (src1->bytes + 1));

  for (pr = pixel_regions_register (4, src1, src2, dest, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s1 = src1->data;
      s2 = src2->data;
      d = dest->data;
      m = (mask) ? mask->data : NULL;

      for (h = 0; h < src1->h; h++)
	{
	  s = buf;

	  /*  apply the paint mode based on the combination type & mode  */
	  switch (type)
	    {
	    case COMBINE_INTEN_A_INDEXED_A:
	    case COMBINE_INTEN_A_CHANNEL_MASK:
	    case COMBINE_INTEN_A_CHANNEL_SELECTION:
	      combine = type;
	      break;

	    case COMBINE_INDEXED_INDEXED:
	    case COMBINE_INDEXED_INDEXED_A:
	    case COMBINE_INDEXED_A_INDEXED_A:
	      /*  Now, apply the paint mode--for indexed images  */
	      combine = apply_indexed_layer_mode (s1, s2, &s, mode, ha1, ha2);
	      break;

	    case COMBINE_INTEN_INTEN_A:
	    case COMBINE_INTEN_A_INTEN:
	    case COMBINE_INTEN_INTEN:
	    case COMBINE_INTEN_A_INTEN_A:
	      /*  Now, apply the paint mode  */
	      combine = apply_layer_mode (s1, s2, &s, src1->x, src1->y + h, opacity, src1->w, mode,
					  src1->bytes, src2->bytes, ha1, ha2, &mode_affect);
	      break;

	    default:
	      break;
	    }

	  /*  based on the type of the initial image...  */
	  switch (combine)
	    {
	    case COMBINE_INDEXED_INDEXED:
	      combine_indexed_and_indexed_pixels (s1, s2, d, m, opacity,
						  affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INDEXED_INDEXED_A:
	      combine_indexed_and_indexed_a_pixels (s1, s2, d, m, opacity,
						    affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INDEXED_A_INDEXED_A:
	      combine_indexed_a_and_indexed_a_pixels (s1, s2, d, m, opacity,
						      affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INTEN_A_INDEXED_A:
	      /*  assume the data passed to this procedure is the
	       *  indexed layer's colormap
	       */
	      combine_inten_a_and_indexed_a_pixels (s1, s2, d, m, data, opacity,
						    src1->w, dest->bytes);
	      break;

	    case COMBINE_INTEN_A_CHANNEL_MASK:
	      /*  assume the data passed to this procedure is the
	       *  indexed layer's colormap
	       */
	      combine_inten_a_and_channel_mask_pixels (s1, s2, d, data, opacity,
						       src1->w, dest->bytes);
	      break;

	    case COMBINE_INTEN_A_CHANNEL_SELECTION:
	      combine_inten_a_and_channel_selection_pixels (s1, s2, d, data, opacity,
							    src1->w, src1->bytes);
	      break;

	    case COMBINE_INTEN_INTEN:
	      combine_inten_and_inten_pixels (s1, s, d, m, opacity,
					      affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INTEN_INTEN_A:
	      combine_inten_and_inten_a_pixels (s1, s, d, m, opacity,
						affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INTEN_A_INTEN:
	      combine_inten_a_and_inten_pixels (s1, s, d, m, opacity,
						affect, mode_affect, src1->w, src1->bytes);
	      break;

	    case COMBINE_INTEN_A_INTEN_A:
	      combine_inten_a_and_inten_a_pixels (s1, s, d, m, opacity,
						  affect, mode_affect, src1->w, src1->bytes);
	      break;

	    case BEHIND_INTEN:
	      behind_inten_pixels (s1, s, d, m, opacity,
				   affect, src1->w, src1->bytes,
				   src2->bytes, ha1, ha2);
	      break;

	    case BEHIND_INDEXED:
	      behind_indexed_pixels (s1, s, d, m, opacity,
				     affect, src1->w, src1->bytes,
				     src2->bytes, ha1, ha2);
	      break;

	    case REPLACE_INTEN:
	      replace_inten_pixels (s1, s, d, m, opacity,
				    affect, src1->w, src1->bytes,
				    src2->bytes, ha1, ha2);
	      break;

	    case REPLACE_INDEXED:
	      replace_indexed_pixels (s1, s, d, m, opacity,
				      affect, src1->w, src1->bytes,
				      src2->bytes, ha1, ha2);
	      break;

	    case ERASE_INTEN:
	      erase_inten_pixels (s1, s, d, m, opacity,
				  affect, src1->w, src1->bytes);
	      break;

	    case ERASE_INDEXED:
	      erase_indexed_pixels (s1, s, d, m, opacity,
				    affect, src1->w, src1->bytes);
	      break;

	    case NO_COMBINATION:
	      break;

	    default:
	      break;
	    }

	  s1 += src1->rowstride;
	  s2 += src2->rowstride;
	  d += dest->rowstride;
	  if (mask)
	    m += mask->rowstride;
	}
    }
}

void
combine_regions_replace (PixelRegion   *src1,
			 PixelRegion   *src2,
			 PixelRegion   *dest,
			 PixelRegion   *mask,
			 unsigned char *data,
			 int            opacity,
			 int           *affect,
			 int            type)
{
  int h;
  unsigned char * s1, * s2;
  unsigned char * d, * m;
  void * pr;

  for (pr = pixel_regions_register (4, src1, src2, dest, mask); pr != NULL; pr = pixel_regions_process (pr))
    {
      s1 = src1->data;
      s2 = src2->data;
      d = dest->data;
      m = mask->data;

      for (h = 0; h < src1->h; h++)
	{

	  /*  Now, apply the paint mode  */
	  apply_layer_mode_replace (s1, s2, d, m, src1->x, src1->y + h, opacity, src1->w,
					      src1->bytes, src2->bytes, affect);

	  s1 += src1->rowstride;
	  s2 += src2->rowstride;
	  d += dest->rowstride;
	  m += mask->rowstride;
	}
    }
}


/*********************************
 *   color conversion routines   *
 *********************************/

void
rgb_to_hsv (int *r,
	    int *g,
	    int *b)
{
  int red, green, blue;
  float h, s, v;
  int min, max;
  int delta;

  h = 0.0;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  v = max;

  if (max != 0)
    s = ((max - min) * 255) / (float) max;
  else
    s = 0;

  if (s == 0)
    h = 0;
  else
    {
      delta = max - min;
      if (red == max)
	h = (green - blue) / (float) delta;
      else if (green == max)
	h = 2 + (blue - red) / (float) delta;
      else if (blue == max)
	h = 4 + (red - green) / (float) delta;
      h *= 42.5;

      if (h < 0)
	h += 255;
      if (h > 255)
	h -= 255;
    }

  *r = h;
  *g = s;
  *b = v;
}


void
hsv_to_rgb (int *h,
	    int *s,
	    int *v)
{
  float hue, saturation, value;
  float f, p, q, t;

  if (*s == 0)
    {
      *h = *v;
      *s = *v;
      *v = *v;
    }
  else
    {
      hue = *h * 6.0 / 255.0;
      saturation = *s / 255.0;
      value = *v / 255.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - (saturation * f));
      t = value * (1.0 - (saturation * (1.0 - f)));

      switch ((int) hue)
	{
	case 0:
	  *h = value * 255;
	  *s = t * 255;
	  *v = p * 255;
	  break;
	case 1:
	  *h = q * 255;
	  *s = value * 255;
	  *v = p * 255;
	  break;
	case 2:
	  *h = p * 255;
	  *s = value * 255;
	  *v = t * 255;
	  break;
	case 3:
	  *h = p * 255;
	  *s = q * 255;
	  *v = value * 255;
	  break;
	case 4:
	  *h = t * 255;
	  *s = p * 255;
	  *v = value * 255;
	  break;
	case 5:
	  *h = value * 255;
	  *s = p * 255;
	  *v = q * 255;
	  break;
	}
    }
}


void
rgb_to_hls (int *r,
	    int *g,
	    int *b)
{
  int red, green, blue;
  float h, l, s;
  int min, max;
  int delta;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2.0;

  if (max == min)
    {
      s = 0.0;
      h = 0.0;
    }
  else
    {
      delta = (max - min);

      if (l < 128)
	s = 255 * (float) delta / (float) (max + min);
      else
	s = 255 * (float) delta / (float) (511 - max - min);

      if (red == max)
	h = (green - blue) / (float) delta;
      else if (green == max)
	h = 2 + (blue - red) / (float) delta;
      else
	h = 4 + (red - green) / (float) delta;

      h = h * 42.5;

      if (h < 0)
	h += 255;
      if (h > 255)
	h -= 255;
    }

  *r = h;
  *g = l;
  *b = s;
}


static int
hls_value (float n1,
	   float n2,
	   float hue)
{
  float value;

  if (hue > 255)
    hue -= 255;
  else if (hue < 0)
    hue += 255;
  if (hue < 42.5)
    value = n1 + (n2 - n1) * (hue / 42.5);
  else if (hue < 127.5)
    value = n2;
  else if (hue < 170)
    value = n1 + (n2 - n1) * ((170 - hue) / 42.5);
  else
    value = n1;

  return (int) (value * 255);
}


void
hls_to_rgb (int *h,
	    int *l,
	    int *s)
{
  float hue, lightness, saturation;
  float m1, m2;

  hue = *h;
  lightness = *l;
  saturation = *s;

  if (saturation == 0)
    {
      /*  achromatic case  */
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      if (lightness < 128)
	m2 = (lightness * (255 + saturation)) / 65025.0;
      else
	m2 = (lightness + saturation - (lightness * saturation)/255.0) / 255.0;

      m1 = (lightness / 127.5) - m2;

      /*  chromatic case  */
      *h = hls_value (m1, m2, hue + 85);
      *l = hls_value (m1, m2, hue);
      *s = hls_value (m1, m2, hue - 85);
    }
}


/************************************/
/*       apply layer modes          */
/************************************/

int
apply_layer_mode (unsigned char  *src1,
		  unsigned char  *src2,
		  unsigned char **dest,
		  int             x,
		  int             y,
		  int             opacity,
		  int             length,
		  int             mode,
		  int             b1,   /* bytes */
		  int             b2,   /* bytes */
		  int             ha1,  /* has alpha */
		  int             ha2,  /* has alpha */
		  int            *mode_affect)
{
  int combine;

  if (!ha1 && !ha2)
    combine = COMBINE_INTEN_INTEN;
  else if (!ha1 && ha2)
    combine = COMBINE_INTEN_INTEN_A;
  else if (ha1 && !ha2)
    combine = COMBINE_INTEN_A_INTEN;
  else
    combine = COMBINE_INTEN_A_INTEN_A;

  /*  assumes we're applying src2 TO src1  */
  switch (mode)
    {
    case NORMAL_MODE:
      *dest = src2;
      break;

    case DISSOLVE_MODE:
      /*  Since dissolve requires an alpha channels...  */
      if (! ha2)
	add_alpha_pixels (src2, *dest, length, b2);

      dissolve_pixels (src2, *dest, x, y, opacity, length, b2,
		       ((ha2) ? b2 : b2 + 1), ha2);
      combine = (ha1) ? COMBINE_INTEN_A_INTEN_A : COMBINE_INTEN_INTEN_A;
      break;

    case MULTIPLY_MODE:
      multiply_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case SCREEN_MODE:
      screen_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case OVERLAY_MODE:
      overlay_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case DIFFERENCE_MODE:
      difference_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case ADDITION_MODE:
      add_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case SUBTRACT_MODE:
      subtract_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case DARKEN_ONLY_MODE:
      darken_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case LIGHTEN_ONLY_MODE:
      lighten_pixels (src1, src2, *dest, length, b1, b2, ha1, ha2);
      break;

    case HUE_MODE: case SATURATION_MODE: case VALUE_MODE:
      /*  only works on RGB color images  */
      if (b1 > 2)
	hsv_only_pixels (src1, src2, *dest, mode, length, b1, b2, ha1, ha2);
      else
	*dest = src2;
      break;

    case COLOR_MODE:
      /*  only works on RGB color images  */
      if (b1 > 2)
	color_only_pixels (src1, src2, *dest, mode, length, b1, b2, ha1, ha2);
      else
	*dest = src2;
      break;

    case BEHIND_MODE:
      *dest = src2;
      if (ha1)
	combine = BEHIND_INTEN;
      else
	combine = NO_COMBINATION;
      break;

    case REPLACE_MODE:
      *dest = src2;
      combine = REPLACE_INTEN;
      break;

    case ERASE_MODE:
      *dest = src2;
      /*  If both sources have alpha channels, call erase function.
       *  Otherwise, just combine in the normal manner
       */
      combine = (ha1 && ha2) ? ERASE_INTEN : combine;
      break;

    default :
      break;
    }

  /*  Determine whether the alpha channel of the destination can be affected
   *  by the specified mode--This keeps consistency with varying opacities
   */
  *mode_affect = layer_modes[mode].affect_alpha;

  return combine;
}


int
apply_indexed_layer_mode (unsigned char  *src1,
			  unsigned char  *src2,
			  unsigned char **dest,
			  int             mode,
			  int             ha1, /* has alpha */
			  int             ha2) /* has alpha */
{
  int combine;

  if (!ha1 && !ha2)
    combine = COMBINE_INDEXED_INDEXED;
  else if (!ha1 && ha2)
    combine = COMBINE_INDEXED_INDEXED_A;
  else if (ha1 && ha2)
    combine = COMBINE_INDEXED_A_INDEXED_A;
  else
    combine = NO_COMBINATION;

  /*  assumes we're applying src2 TO src1  */
  switch (mode)
    {
    case REPLACE_MODE:
      *dest = src2;
      combine = REPLACE_INDEXED;
      break;

    case BEHIND_MODE:
      *dest = src2;
      if (ha1)
	combine = BEHIND_INDEXED;
      else
	combine = NO_COMBINATION;
      break;

    case ERASE_MODE:
      *dest = src2;
      /*  If both sources have alpha channels, call erase function.
       *  Otherwise, just combine in the normal manner
       */
      combine = (ha1 && ha2) ? ERASE_INDEXED : combine;
      break;

    default:
      break;
    }

  return combine;
}

static void
apply_layer_mode_replace (unsigned char  *src1,
			  unsigned char  *src2,
			  unsigned char  *dest,
			  unsigned char  *mask,
			  int             x,
			  int             y,
			  int             opacity,
			  int             length,
			  int             b1,   /* bytes */
			  int             b2,   /* bytes */
			  int            *affect)
{
  replace_pixels (src1, src2, dest, mask, length, opacity, affect, b1, b2);
}
