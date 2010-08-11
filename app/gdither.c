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
#include <assert.h>
#include <stdlib.h>
#include "appenv.h"
#include "errors.h"
#include "gdisplay.h"
#include "scale.h"
#include "colormaps.h"

#define DITHER_LEVEL 8
#define NUM_DITHER_MATRICES DITHER_LEVEL*DITHER_LEVEL+1


/*
 *
 *  Callbacks
 *
 */

void 
dither_method_change (gdisp, dither_type)
     GDisplay *gdisp;
     int dither_type;
{
  GImage *gimage;

  app_data.dither_type = dither_type;	/*  change the application wide default  */

  if (gdisp)
    {
      gimage = gdisp->gimage;
      if (gdisp->dither_type != dither_type && !gimage->busy)
	{
	  gdisp->dither_type = dither_type;
	  gdisplay_remove_processes (gdisp);
	  gdisplay_dither (gdisp, 0, 0,
			   gdisp->disp_image->width,
			   gdisp->disp_image->height, 0);
	}
    }

}


static void free_floyd_steinberg ();
static void free_ordered_dither ();

/*

 *  Global variables
 *
 */

short *floyd_steinberg_error1;	        /* error for floyd-steinberg dithering */
short *floyd_steinberg_error2;	        /* error for floyd-steinberg dithering */
short *floyd_steinberg_error3;	        /* error for floyd-steinberg dithering */
short *floyd_steinberg_error4;	        /* error for floyd-steinberg dithering */
static long *red_ordered_dither;	/* array of information for ordered dithering */
static long *green_ordered_dither;	/* array of information for ordered dithering */
static long *blue_ordered_dither;	/* array of information for ordered dithering */

short *filter_array;	/* filter for floyd-steinberg (bounds checker) */
                        /* made globally available so that it can be used */
                        /* from "quantize.c" */

typedef unsigned char vector[DITHER_LEVEL];
typedef vector matrix[DITHER_LEVEL];

/*
static matrix DM =
{
      0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255,
    128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127,
     32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223,
    160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95,
      8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247,
    136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119,
     40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215,
    168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87,
      2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253,
    130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125,
     34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221,
    162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93,
     10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245,
    138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117,
     42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213,
    170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85

};
*/

static matrix DM =
{
  { 0,  32, 8,  40, 2,  34, 10, 42 },
  { 48, 16, 56, 24, 50, 18, 58, 26 },
  { 12, 44, 4,  36, 14, 46, 6,  38 },
  { 60, 28, 52, 20, 62, 30, 54, 22 },
  { 3,  35, 11, 43, 1,  33, 9,  41 },
  { 51, 19, 59, 27, 49, 17, 57, 25 },
  { 15, 47, 7,  39, 13, 45, 5,  37 },
  { 63, 31, 55, 23, 61, 29, 53, 21 }
};

/*
   matrix DM = {
   0,  8,  2,  10,
   12, 4,  14, 6,
   3,  11, 1,  9,
   15, 7,  13, 5
   };
 */

static matrix ordered_dither_matrices[NUM_DITHER_MATRICES];

void
init_floyd_steinberg ()
{
  int i;
  short err1, err2, err3, err4;

  /*  Initialize the filter array  */
  filter_array = (short *) xmalloc (sizeof (short) * 1025);

  floyd_steinberg_error1 = (short *) xmalloc (sizeof (short) * 1025);
  floyd_steinberg_error2 = (short *) xmalloc (sizeof (short) * 1025);
  floyd_steinberg_error3 = (short *) xmalloc (sizeof (short) * 1025);
  floyd_steinberg_error4 = (short *) xmalloc (sizeof (short) * 1025);

  /*  This array returns i for 0 <= i <= 255, 0 if i<0, and 255 if i>255  */
  for (i = 0; i < 1025; i++)
    {
      if (i < 256)
	filter_array[i] = 0;
      else if (i < 512)
	filter_array[i] = i - 256;
      else
	filter_array[i] = 255;
    }

  /*  Initialize the error values  */
  for (i = 0; i < 1025; i++)
    {
      err1 = (i - 511) * 7 / 16;
      err2 = (i - 511) * 3 / 16;
      err3 = (i - 511) * 5 / 16;
      err4 = (i - 511) - err1 - err2 - err3;
      floyd_steinberg_error1[i] = err1;
      floyd_steinberg_error2[i] = err2;
      floyd_steinberg_error3[i] = err3;
      floyd_steinberg_error4[i] = err4;
    }
}

void
init_ordered_dither ()
{
  int i, j, k;
  unsigned char low_shade, high_shade;
  unsigned short index;
  long red_mult, green_mult;
  float red_colors_per_shade;
  float green_colors_per_shade;
  float blue_colors_per_shade;

  red_mult = shades_g * shades_b;
  green_mult = shades_b;

  red_colors_per_shade = 256.0 / (shades_r - 1);
  green_colors_per_shade = 256.0 / (shades_g - 1);
  blue_colors_per_shade = 256.0 / (shades_b - 1);

  /*  alloc the ordered dither arrays for accelerated dithering  */

  red_ordered_dither = (long *) xmalloc (sizeof (long) * 256);
  green_ordered_dither = (long *) xmalloc (sizeof (long) * 256);
  blue_ordered_dither = (long *) xmalloc (sizeof (long) * 256);


  /*  setup the ordered_dither_matrices  */

  for (i = 0; i < NUM_DITHER_MATRICES; i++)
    {
      for (j = 0; j < DITHER_LEVEL; j++)
	{
	  for (k = 0; k < DITHER_LEVEL; k++)
	    {
	      ordered_dither_matrices[i][j][k] = (DM[j][k] < i) ? 1 : 0;
	    }
	}
    }

  /*  setup arrays containing three bytes of information for red, green, & blue  */
  /*  the arrays contain :
   *    1st byte:    low end shade value
   *    2nd byte:    high end shade value
   *    3rd & 4th bytes:    ordered dither matrix index
   */

  for (i = 0; i < 256; i++)
    {

      /*  setup the red information  */
      {
	low_shade = (unsigned char) (i / red_colors_per_shade);
	high_shade = low_shade + 1;

	index = (unsigned short) 
	  (((i - low_shade * red_colors_per_shade) / red_colors_per_shade) *
	   NUM_DITHER_MATRICES);

	low_shade *= red_mult;
	high_shade *= red_mult;

	red_ordered_dither[i] = (index << 16) + (high_shade << 8) + (low_shade);
      }


      /*  setup the green information  */
      {
	low_shade = (unsigned char) (i / green_colors_per_shade);
	high_shade = low_shade + 1;

	index = (unsigned short) 
	  (((i - low_shade * green_colors_per_shade) / green_colors_per_shade) *
	   NUM_DITHER_MATRICES);

	low_shade *= green_mult;
	high_shade *= green_mult;

	green_ordered_dither[i] = (index << 16) + (high_shade << 8) + (low_shade);
      }


      /*  setup the blue information  */
      {
	low_shade = (unsigned char) (i / blue_colors_per_shade);
	high_shade = low_shade + 1;

	index = (unsigned short) 
	  (((i - low_shade * blue_colors_per_shade) / blue_colors_per_shade) *
	   NUM_DITHER_MATRICES);

	blue_ordered_dither[i] = (index << 16) + (high_shade << 8) + (low_shade);
      }
    }
}

static void
free_floyd_steinberg ()
{
  xfree (filter_array);
  xfree (floyd_steinberg_error1);
  xfree (floyd_steinberg_error2);
  xfree (floyd_steinberg_error3);
  xfree (floyd_steinberg_error4);
}

static void
free_ordered_dither ()
{
  xfree (red_ordered_dither);
  xfree (green_ordered_dither);
  xfree (blue_ordered_dither);
}

void
dither_start_engine ()
{
  /*  sets up data structures for accelerated dithering algorithms  */
  init_floyd_steinberg ();
  init_ordered_dither ();
}

void
dither_shutdown_engine ()
{
  free_floyd_steinberg ();
  free_ordered_dither ();
}


/*
 *
 *  The XtWorkProc stuff
 *
 */

void
dither_free (dither)
     GDither *dither;
{
  if (!dither) return;

  xfree (dither->scale_data);
  xfree (dither->dither_from);
  xfree (dither);
}


GDither *
dither_new ()
{
  GDither * dither;

  dither = (GDither *) xmalloc (sizeof (GDither));

  dither->scale_data = NULL;
  dither->dither_from = NULL;
  dither->running = 0;
  dither->interrupted = 0;

  return dither;
}


GDither *
dither_create (func, gdisp, x, y, width, height, delay)
     GDither_Func func;
     GDisplay *gdisp;
     long x, y;
     long width, height;
     long delay;
{
  GDither *obj;
  long sx, sy;

  obj = gdisp->gdither;

  assert (obj != NULL);

  if (obj->scale_data)
    xfree (obj->scale_data);
  if (obj->dither_from)
    xfree (obj->dither_from);

  /*  If the last dither was interrupted, combine the area still
   *  undithered with the new request's area                   
   */
  if (obj->interrupted)
    {
      long x1, x2, y1, y2;

      x1 = MINIMUM (obj->x, x);
      y1 = MINIMUM (obj->y, y);
      x2 = MAXIMUM ((obj->x + obj->width), (x + width));
      y2 = MAXIMUM ((obj->y + obj->height), (y + height));

      x = MAXIMUM (x1, 0);
      y = MAXIMUM (y1, 0);
      width = MINIMUM ((x2 - x1), gdisp->disp_image->width);
      height = MINIMUM ((y2 - y1), gdisp->disp_image->height);
    }

  /*  initialize the dither object's attributes  */

  obj->done          = 0;
  obj->interrupted   = 0;
  obj->running       = 1;
  obj->scanline      = 0;
  obj->gimage        = gdisp->gimage;
  obj->engine        = func;
  obj->data          = NULL;
  obj->x             = x;
  obj->y             = y;
  obj->width         = width;
  obj->height        = height;
  obj->delay         = delay;

  obj->dest  = gdisp->disp_image->data;
  obj->dest_width = gdisp->disp_image->bytes_per_line;
  obj->dest += gdisp->disp_image->bytes_per_line * obj->y + obj->x;

  sy = UNSCALE(gdisp, (gdisp->offset_y + y));
  sx = UNSCALE(gdisp, (gdisp->offset_x + x));

  obj->src  = gdisp->gimage->raw_image;
  obj->src += (sy * gdisp->gimage->width + sx) * 3;
  obj->src_width = gdisp->gimage->width * 3;

  obj->dither_from = (unsigned char *) xmalloc (sizeof (unsigned char) * width * 3);

  obj->scale_data = accelerate_scaling (width, (x + gdisp->offset_x), 3,
					SCALESRC (gdisp),
					SCALEDEST (gdisp));

  return obj;
}


/*
 *  The Algorithms
 *
 */

typedef struct _GDither_FS GDither_FS;
struct _GDither_FS {
  short *red_n_row;
  short *grn_n_row;
  short *blu_n_row;
  short *red_p_row;
  short *grn_p_row;
  short *blu_p_row;
};


void
dither_segment_floyd_steinberg (src, dest, x, y, width)
     unsigned char * src;
     unsigned char * dest;
     int x, y;
     int width;
{
  fatal_error ("\"dither_segment_floyd_steinberg\" has not yet been implemented.");
}


void
dither_floyd_steinberg (gdither)
     GDither *gdither;
{
  GDither_FS *dither_data;
  unsigned char *src;
  unsigned char *dest;

  short *filter;
  short *rnr, *gnr, *bnr, *rpr, *gpr, *bpr;
  short *fs_err1, *fs_err2, *fs_err3, *fs_err4;
  short re, ge, be;
  short error_size;

  short width, index;
  short r, g, b;
  XColor *col;

  if (gdither->done)
    return;

  if ( ! gdither->interrupted)
    {
      src = gdither->dither_from;
      dest = gdither->dest;
      gdither->dest += gdither->dest_width;
      
      error_size = sizeof (short) * (gdither->width + 2);
      
      if (!gdither->data)
	{
	  dither_data = (GDither_FS*) xmalloc (sizeof (GDither_FS));
	  gdither->data = dither_data;
	  
	  dither_data->red_n_row = (short *) xmalloc(error_size);
	  dither_data->grn_n_row = (short *) xmalloc(error_size);
	  dither_data->blu_n_row = (short *) xmalloc(error_size);
	  dither_data->red_p_row = (short *) xmalloc(error_size);
	  dither_data->grn_p_row = (short *) xmalloc(error_size);
	  dither_data->blu_p_row = (short *) xmalloc(error_size);
	  
	  memset(dither_data->red_p_row, 0, error_size);
	  memset(dither_data->grn_p_row, 0, error_size);
	  memset(dither_data->blu_p_row, 0, error_size);
	}
      else
	{
	  dither_data = gdither->data;
	}
      
      rnr = dither_data->red_n_row;
      gnr = dither_data->grn_n_row;
      bnr = dither_data->blu_n_row;
      
      rpr = dither_data->red_p_row + 1;
      gpr = dither_data->grn_p_row + 1;
      bpr = dither_data->blu_p_row + 1;
      
      *rnr = *gnr = *bnr = 0;
      *(rnr + 1) = *(gnr + 1) = *(bnr + 1) = 0;
      
      filter = filter_array + 256;
      
      fs_err1 = floyd_steinberg_error1 + 511;
      fs_err2 = floyd_steinberg_error2 + 511;
      fs_err3 = floyd_steinberg_error3 + 511;
      fs_err4 = floyd_steinberg_error4 + 511;
      
      width = gdither->width;
      if (width > 0)
	while (width--)
	  {
	    r = filter[*src++ + *rpr];
	    g = filter[*src++ + *gpr];
	    b = filter[*src++ + *bpr];
	    
	    index = red_shades[r] + green_shades[g] + blue_shades[b];
	    
	    col = rgbpal + index;

	    *dest++ = col->pixel;
	    
	    re = r - col->red;
	    ge = g - col->green;
	    be = b - col->blue;
	    
	    *(++rpr) += fs_err1[re];
	    *(++gpr) += fs_err1[ge];
	    *(++bpr) += fs_err1[be];
	    
	    *rnr++ += fs_err2[re];
	    *gnr++ += fs_err2[ge];
	    *bnr++ += fs_err2[be];
	    
	    *rnr += fs_err3[re];
	    *gnr += fs_err3[ge];
	    *bnr += fs_err3[be];
	    
	    *(rnr+1) = fs_err4[re];
	    *(gnr+1) = fs_err4[ge];
	    *(bnr+1) = fs_err4[be];
	  }
      
      {
	short *tmp;
	
	tmp = dither_data->red_n_row;
	dither_data->red_n_row = dither_data->red_p_row;
	dither_data->red_p_row = tmp;
	
	tmp = dither_data->grn_n_row;
	dither_data->grn_n_row = dither_data->grn_p_row;
	dither_data->grn_p_row = tmp;
	
	tmp = dither_data->blu_n_row;
	dither_data->blu_n_row = dither_data->blu_p_row;
	dither_data->blu_p_row = tmp;
      }
      
      gdither->scanline++;
    }

  if (gdither->scanline >= gdither->height || gdither->interrupted)
    {
      dither_data = (GDither_FS *) gdither->data;
      if (dither_data)
	{
	  xfree (dither_data->red_n_row);
	  xfree (dither_data->grn_n_row);
	  xfree (dither_data->blu_n_row);
	  xfree (dither_data->red_p_row);
	  xfree (dither_data->grn_p_row);
	  xfree (dither_data->blu_p_row);
	  xfree (dither_data);
	}

      gdither->done = 1;
    }
}

void
dither_segment_ordered (src, dest, x, y, width)
     unsigned char * src;
     unsigned char * dest;
     int x, y;
     int width;
{
  long r, g, b;
  long ra, ga, ba;
  short xmod, ymod;
  XColor *col;

  ymod = y & 0x7;
  xmod = x & 0x7;

  if (width > 0)
    while (width--)
      {
	r = red_ordered_dither[*src++];
	g = green_ordered_dither[*src++];
	b = blue_ordered_dither[*src++];
	
	ra = (ordered_dither_matrices[SECONDWORD(r)][ymod][xmod]) ?
	  SECONDBYTE(r) : FIRSTBYTE(r);
	ga = (ordered_dither_matrices[SECONDWORD(g)][ymod][xmod]) ?
	  SECONDBYTE(g) : FIRSTBYTE(g);
	ba = (ordered_dither_matrices[SECONDWORD(b)][ymod][xmod]) ?
	  SECONDBYTE(b) : FIRSTBYTE(b);
	
	col = rgbpal + ra + ga + ba;
	*dest++ = col->pixel;
	
	xmod++;
	xmod &= 0x7;
      }

}


void
dither_ordered (gdither)
     GDither *gdither;
{
  unsigned char *src;
  unsigned char *dest;

  if (gdither->done)
    return;

  if ( ! gdither->interrupted )
    {
      src = gdither->dither_from;
      dest = gdither->dest;
      gdither->dest += gdither->dest_width;

      dither_segment_ordered (src, dest, gdither->x, 
			      gdither->scanline + gdither->y, gdither->width);
      
      gdither->scanline++;
    }

  if (gdither->scanline >= gdither->height || gdither->interrupted)
    gdither->done = 1;
}

