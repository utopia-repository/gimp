/* -*- mode: c tab-width: 2; c-basic-indent: 2; indent-tabs-mode: nil -*-
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * -*- mode: c tab-width: 2; c-basic-indent: 2; indent-tabs-mode: nil -*-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Gimp image compositing
 * Copyright (C) 2003  Helvetix Victorinox, a pseudonym, <helvetix@gimp.org>
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

#include <stdio.h>

#include <glib-object.h>

#include "base/base-types.h"
#include "base/cpu-accel.h"

#include "gimp-composite.h"
#include "gimp-composite-sse2.h"

#ifdef COMPILE_SSE2_IS_OKAY

#include "gimp-composite-x86.h"

static const guint32 rgba8_alpha_mask_128[4] = { 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000 };
static const guint32 rgba8_b1_128[4] =         { 0x01010101, 0x01010101, 0x01010101, 0x01010101 };
static const guint32 rgba8_b255_128[4] =       { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
static const guint32 rgba8_w1_128[4] =         { 0x00010001, 0x00010001, 0x00010001, 0x00010001 };
static const guint32 rgba8_w2_128[4] =         { 0x00020002, 0x00020002, 0x00020002, 0x00020002 };
static const guint32 rgba8_w128_128[4] =       { 0x00800080, 0x00800080, 0x00800080, 0x00800080 };
static const guint32 rgba8_w256_128[4] =       { 0x01000100, 0x01000100, 0x01000100, 0x01000100 };
static const guint32 rgba8_w255_128[4] =       { 0X00FF00FF, 0X00FF00FF, 0X00FF00FF, 0X00FF00FF };

static const guint32 va8_alpha_mask_128[4] =   { 0xFF00FF00, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00 };
static const guint32 va8_b255_128[4] =         { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
static const guint32 va8_w1_128[4] =           { 0x00010001, 0x00010001, 0x00010001, 0x00010001 };
static const guint32 va8_w255_128[4] =         { 0x00FF00FF, 0x00FF00FF, 0x00FF00FF, 0x00FF00FF };

static const guint32 rgba8_alpha_mask_64[2] = { 0xFF000000, 0xFF000000 };
static const guint32 rgba8_b1_64[2] =         { 0x01010101, 0x01010101 };
static const guint32 rgba8_b255_64[2] =       { 0xFFFFFFFF, 0xFFFFFFFF };
static const guint32 rgba8_w1_64[2] =         { 0x00010001, 0x00010001 };
static const guint32 rgba8_w2_64[2] =         { 0x00020002, 0x00020002 };
static const guint32 rgba8_w128_64[2] =       { 0x00800080, 0x00800080 };
static const guint32 rgba8_w256_64[2] =       { 0x01000100, 0x01000100 };
static const guint32 rgba8_w255_64[2] =       { 0X00FF00FF, 0X00FF00FF };

static const guint32 va8_alpha_mask_64[2] =   { 0xFF00FF00, 0xFF00FF00 };
static const guint32 va8_b255_64[2] =         { 0xFFFFFFFF, 0xFFFFFFFF };
static const guint32 va8_w1_64[2] =           { 0x00010001, 0x00010001 };
static const guint32 va8_w255_64[2] =         { 0x00FF00FF, 0x00FF00FF };

void
debug_display_sse (void)
{
#define mask32(x) ((x)& (unsigned long long) 0xFFFFFFFF)
#define print128(reg) { \
  unsigned long long reg[2]; \
  asm("movdqu %%" #reg ",%0" : "=m" (reg)); \
  printf(#reg"=%08llx %08llx", mask32(reg[0]>>32), mask32(reg[0])); \
  printf(" %08llx %08llx", mask32(reg[1]>>32), mask32(reg[1])); \
 }
  printf("--------------------------------------------\n");
  print128(xmm0); printf("  "); print128(xmm1); printf("\n");
  print128(xmm2); printf("  "); print128(xmm3); printf("\n");
  print128(xmm4); printf("  "); print128(xmm5); printf("\n");
  print128(xmm6); printf("  "); print128(xmm7); printf("\n");
  printf("--------------------------------------------\n");
}

void
gimp_composite_addition_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  asm volatile ("  movdqu    %0,%%xmm0\n"
                "\tmovq      %1,%%mm0"
                : /* empty */
                : "m" (*rgba8_alpha_mask_128), "m" (*rgba8_alpha_mask_64)
#ifdef __MMX__
                : "%mm0"
#ifdef __SSE__
                , "%xmm0"
#endif
#endif
                );

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu      %1,%%xmm2\n"
                    "\tmovdqu      %2,%%xmm3\n"
                    "\tmovdqu  %%xmm2,%%xmm4\n"
                    "\tpaddusb %%xmm3,%%xmm4\n"
                    
                    "\tmovdqu  %%xmm0,%%xmm1\n"
                    "\tpandn   %%xmm4,%%xmm1\n"
                    "\tpminub  %%xmm3,%%xmm2\n"
                    "\tpand    %%xmm0,%%xmm2\n"
                    "\tpor     %%xmm2,%%xmm1\n"
                    "\tmovdqu  %%xmm1,%0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq       %1,%%mm2\n"
                    "\tmovq       %2,%%mm3\n"
                    "\tmovq    %%mm2,%%mm4\n"
                    "\tpaddusb %%mm3,%%mm4\n"
                    "\tmovq    %%mm0,%%mm1\n"
                    "\tpandn   %%mm4,%%mm1\n"
                    "\tpminub  %%mm3,%%mm2\n"
                    "\tpand    %%mm0,%%mm2\n"
                    "\tpor     %%mm2,%%mm1\n"
                    "\tmovq    %%mm1,%0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd       %1,%%mm2\n"
                    "\tmovd       %2,%%mm3\n"
                    "\tmovq    %%mm2,%%mm4\n"
                    "\tpaddusb %%mm3,%%mm4\n"
                    "\tmovq    %%mm0,%%mm1\n"
                    "\tpandn   %%mm4,%%mm1\n"
                    "\tpminub  %%mm3,%%mm2\n"
                    "\tpand    %%mm0,%%mm2\n"
                    "\tpor     %%mm2,%%mm1\n"
                    "\tmovd    %%mm1,%0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7"
#endif
                    );
    }

  asm("emms");
}


#if 0
void
xxxgimp_composite_burn_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{

}
#endif

void
gimp_composite_darken_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu          %1,%%xmm2\n"
                    "\tmovdqu          %2,%%xmm3\n"
                    "\tpminub      %%xmm3,%%xmm2\n"
                    "\tmovdqu      %%xmm2,%0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm1", "%xmm2", "%xmm3", "%xmm4"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq       %1, %%mm2\n"
                    "\tpminub     %2, %%mm2\n"
                    "\tmovq    %%mm2, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd       %1, %%mm2\n"
                    "\tmovd       %2, %%mm3\n"
                    "\tpminub  %%mm3, %%mm2\n"
                    "\tmovd    %%mm2, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm2", "%mm3", "%mm4"
#endif
                    );
    }

  asm("emms");
}

void
gimp_composite_difference_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  asm volatile ("  movq   %0,%%mm0\n"
                "\tmovdqu %1,%%xmm0"
                :               /*  */
                : "m" (*rgba8_alpha_mask_64), "m" (*rgba8_alpha_mask_128)
#ifdef __MMX__
                : "%mm0"
#ifdef __SSE__
                , "%xmm0"
#endif
#endif
                );

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu        %1,%%xmm2\n"
                    "\tmovdqu        %2,%%xmm3\n"
                    "\tmovdqu    %%xmm2,%%xmm4\n"
                    "\tmovdqu    %%xmm3,%%xmm5\n"
                    "\tpsubusb   %%xmm3,%%xmm4\n"
                    "\tpsubusb   %%xmm2,%%xmm5\n"
                    "\tpaddb     %%xmm5,%%xmm4\n"
                    "\tmovdqu    %%xmm0,%%xmm1\n"
                    "\tpandn     %%xmm4,%%xmm1\n"
                    "\tpminub    %%xmm3,%%xmm2\n"
                    "\tpand      %%xmm0,%%xmm2\n"
                    "\tpor       %%xmm2,%%xmm1\n"
                    "\tmovdqu    %%xmm1,%0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq       %1, %%mm2\n"
                    "\tmovq       %2, %%mm3\n"
                    "\tmovq    %%mm2, %%mm4\n"
                    "\tmovq    %%mm3, %%mm5\n"
                    "\tpsubusb %%mm3, %%mm4\n"
                    "\tpsubusb %%mm2, %%mm5\n"
                    "\tpaddb   %%mm5, %%mm4\n"
                    "\tmovq    %%mm0, %%mm1\n"
                    "\tpandn   %%mm4, %%mm1\n"
                    "\tpminub  %%mm3, %%mm2\n"
                    "\tpand    %%mm0, %%mm2\n"
                    "\tpor     %%mm2, %%mm1\n"
                    "\tmovq    %%mm1, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4", "%mm5"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd       %1, %%mm2\n"
                    "\tmovd       %2, %%mm3\n"
                    "\tmovq    %%mm2, %%mm4\n"
                    "\tmovq    %%mm3, %%mm5\n"
                    "\tpsubusb %%mm3, %%mm4\n"
                    "\tpsubusb %%mm2, %%mm5\n"
                    "\tpaddb   %%mm5, %%mm4\n"
                    "\tmovq    %%mm0, %%mm1\n"
                    "\tpandn   %%mm4, %%mm1\n"
                    "\tpminub  %%mm3, %%mm2\n"
                    "\tpand    %%mm0, %%mm2\n"
                    "\tpor     %%mm2, %%mm1\n"
                    "\tmovd    %%mm1, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4", "%mm5"
#endif
                    );
    }

  asm("emms");
}


#if 0
void
xxxgimp_composite_dodge_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  for (; op.n_pixels >= 4; op.n_pixels -= 4)
    {
      asm volatile ("  movdqu        %1,%%xmm0\n"
                    "\tmovdqu        %2,%%xmm1\n"
                    "\tmovdqu    %%xmm1,%%xmm3\n"
                    "\tpxor      %%xmm2,%%xmm2\n"
                    "\tpunpcklbw %%xmm2,%%xmm3\n"
                    "\tpunpcklbw %%xmm0,%%xmm2\n"

                    "\tmovdqu        %3,%%xmm4\n"
                    "\tpsubw     %%xmm3,%%xmm4\n"

                    "\t" xmm_pdivwuqX(xmm2,xmm4,xmm5,xmm6) "\n"

                    "\tmovdqu    %%xmm1,%%xmm3\n"
                    "\tpxor      %%xmm2,%%xmm2\n"
                    "\tpunpckhbw %%xmm2,%%xmm3\n"
                    "\tpunpckhbw %%xmm0,%%xmm2\n"

                    "\tmovdqu        %3,%%xmm4\n"
                    "\tpsubw     %%xmm3,%%xmm4\n"

                    "\t" xmm_pdivwuqX(xmm2,xmm4,xmm6,xmm7) "\n"

                    "\tpackuswb  %%xmm6,%%xmm5\n"

                    "\tmovdqu        %4,%%xmm6\n"
                    "\tmovdqu    %%xmm1,%%xmm7\n"
                    "\tpminub    %%xmm0,%%xmm7\n"
                    "\tpand      %%xmm6,%%xmm7\n"
                    "\tpandn     %%xmm5,%%xmm6\n"

                    "\tpor       %%xmm6,%%xmm7\n"

                    "\tmovdqu    %%xmm7,%0\n"
                    : "=m" (*op.D)
                    : "m" (*op.A), "m" (*op.B), "m" (*rgba8_w256_128), "m" (*rgba8_alpha_mask_128)
                    : "%eax", "%ecx", "%edx"
#ifdef __SSE__
                    , "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );
      op.A += 16;
      op.B += 16;
      op.D += 16;
    }

  for (; op.n_pixels >= 2; op.n_pixels -= 2)
    {
      asm volatile ("  movq         %1,%%mm0\n"
                    "\tmovq         %2,%%mm1\n"
                    "\tmovq      %%mm1,%%mm3\n"
                    "\tpxor      %%mm2,%%mm2\n"
                    "\tpunpcklbw %%mm2,%%mm3\n"
                    "\tpunpcklbw %%mm0,%%mm2\n"

                    "\tmovq         %3,%%mm4\n"
                    "\tpsubw     %%mm3,%%mm4\n"

                    "\t" pdivwuqX(mm2,mm4,mm5) "\n"

                    "\tmovq      %%mm1,%%mm3\n"
                    "\tpxor      %%mm2,%%mm2\n"
                    "\tpunpckhbw %%mm2,%%mm3\n"
                    "\tpunpckhbw %%mm0,%%mm2\n"

                    "\tmovq         %3,%%mm4\n"
                    "\tpsubw     %%mm3,%%mm4\n"

                    "\t" pdivwuqX(mm2,mm4,mm6) "\n"

                    "\tpackuswb  %%mm6,%%mm5\n"

                    "\tmovq         %4,%%mm6\n"
                    "\tmovq      %%mm1,%%mm7\n"
                    "\tpminub    %%mm0,%%mm7\n"
                    "\tpand      %%mm6,%%mm7\n"
                    "\tpandn     %%mm5,%%mm6\n"

                    "\tpor       %%mm6,%%mm7\n"

                    "\tmovq      %%mm7,%0\n"
                    : (*op.D)
                    : "m" (*op.A), "m" (*op.B), "m" (*rgba8_w256_64), "m" (*rgba8_alpha_mask_64)
                    : "%eax", "%ecx", "%edx"
#ifdef __MMX__
                    , "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7"
#endif
                    );
      op.A += 8;
      op.B += 8;
      op.D += 8;
    }

  if (op.n_pixels)
    {
      asm volatile ("  movd         %1,%%mm0\n"
                    "\tmovq         %2,%%mm1\n"
                    "\tmovq      %%mm1,%%mm3\n"
                    "\tpxor      %%mm2,%%mm2\n"
                    "\tpunpcklbw %%mm2,%%mm3\n"
                    "\tpunpcklbw %%mm0,%%mm2\n"

                    "\tmovq         %3,%%mm4\n"
                    "\tpsubw     %%mm3,%%mm4\n"

                    "\t" pdivwuqX(mm2,mm4,mm5) "\n"

                    "\tmovq      %%mm1,%%mm3\n"
                    "\tpxor      %%mm2,%%mm2\n"
                    "\tpunpckhbw %%mm2,%%mm3\n"
                    "\tpunpckhbw %%mm0,%%mm2\n"

                    "\tmovq         %3,%%mm4\n"
                    "\tpsubw     %%mm3,%%mm4\n"

                    "\t" pdivwuqX(mm2,mm4,mm6) "\n"

                    "\tpackuswb  %%mm6,%%mm5\n"

                    "\tmovq         %4,%%mm6\n"
                    "\tmovq      %%mm1,%%mm7\n"
                    "\tpminub    %%mm0,%%mm7\n"
                    "\tpand      %%mm6,%%mm7\n"
                    "\tpandn     %%mm5,%%mm6\n"

                    "\tpor       %%mm6,%%mm7\n"

                    "\tmovd      %%mm7,%0\n"
                    : "=m" (*op.D)
                    : "m" (*op.A), "m" (*op.B), "m" (*rgba8_w256_64), "m" (*rgba8_alpha_mask_64)
                    : "%eax", "%ecx", "%edx"
#ifdef __MMX__
                    , "%mm1", "%mm2", "%mm3", "%mm4", "%mm5"
#endif
                    );
    }

  asm("emms");
}
#endif

void
gimp_composite_grain_extract_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  asm volatile ("  movq       %0,%%mm0\n"
                "\tpxor    %%mm6,%%mm6\n"
                "\tmovq       %1,%%mm7\n"
								"\tmovdqu     %2,%%xmm0\n"
                "\tpxor   %%xmm6,%%xmm6\n"
                "\tmovdqu     %3,%%xmm7\n"
                : /* empty */
                : "m" (*rgba8_alpha_mask_64), "m" (*rgba8_w128_64), "m" (*rgba8_alpha_mask_128), "m" (*rgba8_w128_128)
#ifdef __MMX__
                : "%mm0", "%mm6", "%mm7"
#ifdef __SSE__
                , "%xmm0", "%xmm6", "%xmm7"
#endif
#endif
                );

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu       %1,%%xmm2\n"
                    "\tmovdqu       %2,%%xmm3\n"
                    xmm_low_bytes_to_words(xmm2,xmm4,xmm6)
                    xmm_low_bytes_to_words(xmm3,xmm5,xmm6)
                    "\tpsubw     %%xmm5,%%xmm4\n"
                    "\tpaddw     %%xmm7,%%xmm4\n"
                    "\tmovdqu    %%xmm4,%%xmm1\n"

                    xmm_high_bytes_to_words(xmm2,xmm4,xmm6)
                    xmm_high_bytes_to_words(xmm3,xmm5,xmm6)

                    "\tpsubw     %%xmm5,%%xmm4\n"
                    "\tpaddw     %%xmm7,%%xmm4\n"

                    "\tpackuswb  %%xmm4,%%xmm1\n"
                    "\tmovdqu    %%xmm1,%%xmm4\n"

                    "\tmovdqu    %%xmm0,%%xmm1\n"
                    "\tpandn     %%xmm4,%%xmm1\n"

                    "\tpminub    %%xmm3,%%xmm2\n"
                    "\tpand      %%xmm0,%%xmm2\n"

                    "\tpor       %%xmm2,%%xmm1\n"
                    "\tmovdqu    %%xmm1,%0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm1", "%xmm2", "%xmm3", "%xmm4"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq         %1,%%mm2\n"
                    "\tmovq         %2,%%mm3\n"
                    mmx_low_bytes_to_words(mm2,mm4,mm6)
                    mmx_low_bytes_to_words(mm3,mm5,mm6)
                    "\tpsubw     %%mm5,%%mm4\n"
                    "\tpaddw     %%mm7,%%mm4\n"
                    "\tmovq      %%mm4,%%mm1\n"

                    mmx_high_bytes_to_words(mm2,mm4,mm6)
                    mmx_high_bytes_to_words(mm3,mm5,mm6)

                    "\tpsubw     %%mm5,%%mm4\n"
                    "\tpaddw     %%mm7,%%mm4\n"

                    "\tpackuswb  %%mm4,%%mm1\n"
                    "\tmovq      %%mm1,%%mm4\n"

                    "\tmovq      %%mm0,%%mm1\n"
                    "\tpandn     %%mm4,%%mm1\n"

                    "\tpminub    %%mm3,%%mm2\n"
                    "\tpand      %%mm0,%%mm2\n"

                    "\tpor       %%mm2,%%mm1\n"
                    "\tmovq      %%mm1,%0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd         %1, %%mm2\n"
                    "\tmovd         %2, %%mm3\n"
                    mmx_low_bytes_to_words(mm2,mm4,mm6)
                    mmx_low_bytes_to_words(mm3,mm5,mm6)
                    "\tpsubw     %%mm5, %%mm4\n"
                    "\tpaddw     %%mm7, %%mm4\n"
                    "\tmovq      %%mm4, %%mm1\n"
                    "\tpackuswb  %%mm6, %%mm1\n"
                    "\tmovq      %%mm1, %%mm4\n"
                    "\tmovq      %%mm0, %%mm1\n"
                    "\tpandn     %%mm4, %%mm1\n"
                    "\tpminub    %%mm3, %%mm2\n"
                    "\tpand      %%mm0, %%mm2\n"
                    "\tpor       %%mm2, %%mm1\n"
                    "\tmovd      %%mm1, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
    }

  asm("emms");
}

void
gimp_composite_lighten_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  asm volatile ("movdqu    %0,%%xmm0"
          :
          : "m" (*rgba8_alpha_mask_64)
#ifdef __SSE__
          : "%xmm0"
#endif
          );

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu      %1, %%xmm2\n"
                    "\tmovdqu      %2, %%xmm3\n"
                    "\tmovdqu  %%xmm2, %%xmm4\n"
                    "\tpmaxub  %%xmm3, %%xmm4\n"
                    "\tmovdqu  %%xmm0, %%xmm1\n"
                    "\tpandn   %%xmm4, %%xmm1\n"
                    "\tpminub  %%xmm2, %%xmm3\n"
                    "\tpand    %%xmm0, %%xmm3\n"
                    "\tpor     %%xmm3, %%xmm1\n"
                    "\tmovdqu  %%xmm1, %0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm1", "%xmm2", "%xmm3", "%xmm4"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq       %1, %%mm2\n"
                    "\tmovq       %2, %%mm3\n"
                    "\tmovq    %%mm2, %%mm4\n"
                    "\tpmaxub  %%mm3, %%mm4\n"
                    "\tmovq    %%mm0, %%mm1\n"
                    "\tpandn   %%mm4, %%mm1\n"
                    "\tpminub  %%mm2, %%mm3\n"
                    "\tpand    %%mm0, %%mm3\n"
                    "\tpor     %%mm3, %%mm1\n"
                    "\tmovq    %%mm1, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd       %1, %%mm2\n"
                    "\tmovd       %2, %%mm3\n"
                    "\tmovq    %%mm2, %%mm4\n"
                    "\tpmaxub  %%mm3, %%mm4\n"
                    "\tmovq    %%mm0, %%mm1\n"
                    "\tpandn   %%mm4, %%mm1\n"
                    "\tpminub  %%mm2,%%mm3\n"
                    "\tpand    %%mm0, %%mm3\n"
                    "\tpor     %%mm3, %%mm1\n"
                    "\tmovd    %%mm1, %0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
    }

  asm("emms");
}

void
gimp_composite_subtract_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  uint64 *d;
  uint64 *a;
  uint64 *b;
  uint128 *D = (uint128 *) _op->D;
  uint128 *A = (uint128 *) _op->A;
  uint128 *B = (uint128 *) _op->B;
  gulong n_pixels = _op->n_pixels;

  asm volatile ("  movq    %0,%%mm0\n"
                "\tmovdqu  %1,%%xmm0\n"
                : /* empty */
                : "m" (*rgba8_alpha_mask_64), "m" (*rgba8_alpha_mask_128)
#ifdef __MMX__
                : "%mm0"
#ifdef __SSE__
                , "%xmm0"
#endif
#endif
                );

  for (; n_pixels >= 4; n_pixels -= 4)
    {
      asm volatile ("  movdqu       %1,%%xmm2\n"
                    "\tmovdqu       %2,%%xmm3\n"
                    "\tmovdqu   %%xmm2,%%xmm4\n"
                    "\tpsubusb  %%xmm3,%%xmm4\n"

                    "\tmovdqu   %%xmm0,%%xmm1\n"
                    "\tpandn    %%xmm4,%%xmm1\n"
                    "\tpminub   %%xmm3,%%xmm2\n"
                    "\tpand     %%xmm0,%%xmm2\n"
                    "\tpor      %%xmm2,%%xmm1\n"
                    "\tmovdqu   %%xmm1,%0\n"
                    : "=m" (*D)
                    : "m" (*A), "m" (*B)
#ifdef __SSE__
                    : "%xmm1", "%xmm2", "%xmm3", "%xmm4"
#endif
                    );
      A++;
      B++;
      D++;
    }

  a = (uint64 *) A;
  b = (uint64 *) B;
  d = (uint64 *) D;

  for (; n_pixels >= 2; n_pixels -= 2)
    {
      asm volatile ("  movq       %1,%%mm2\n"
                    "\tmovq       %2,%%mm3\n"
                    "\tmovq    %%mm2,%%mm4\n"
                    "\tpsubusb %%mm3,%%mm4\n"
                    "\tmovq    %%mm0,%%mm1\n"
                    "\tpandn   %%mm4,%%mm1\n"
                    "\tpminub  %%mm3,%%mm2\n"
                    "\tpand    %%mm0,%%mm2\n"
                    "\tpor     %%mm2,%%mm1\n"
                    "\tmovq    %%mm1,%0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
      a++;
      b++;
      d++;
    }

  if (n_pixels > 0)
    {
      asm volatile ("  movd       %1,%%mm2\n"
                    "\tmovd       %2,%%mm3\n"
                    "\tmovq    %%mm2,%%mm4\n"
                    "\tpsubusb %%mm3,%%mm4\n"
                    "\tmovq    %%mm0,%%mm1\n"
                    "\tpandn   %%mm4,%%mm1\n"
                    "\tpminub  %%mm3,%%mm2\n"
                    "\tpand    %%mm0,%%mm2\n"
                    "\tpor     %%mm2,%%mm1\n"
                    "\tmovd    %%mm1,%0\n"
                    : "=m" (*d)
                    : "m" (*a), "m" (*b)
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
    }

  asm("emms");
}

void
gimp_composite_swap_rgba8_rgba8_rgba8_sse2 (GimpCompositeContext *_op)
{
  GimpCompositeContext op = *_op;

  /*
   * Inhale one whole i686 cache line at once. 64 bytes, 16 rgba8
   * pixels, 4 128 bit xmm registers.
   */
  for (; op.n_pixels >= 16; op.n_pixels -= 16)
    {
#ifdef __OPTIMIZE__
      asm volatile ("  movdqu      %0,%%xmm0\n"
                    "\tmovdqu      %1,%%xmm1\n"
                    "\tmovdqu      %2,%%xmm2\n"
                    "\tmovdqu      %3,%%xmm3\n"
                    "\tmovdqu      %4,%%xmm4\n"
                    "\tmovdqu      %5,%%xmm5\n"
                    "\tmovdqu      %6,%%xmm6\n"
                    "\tmovdqu      %7,%%xmm7\n"

                    "\tmovdqu      %%xmm0,%1\n"
                    "\tmovdqu      %%xmm1,%0\n"
                    "\tmovdqu      %%xmm2,%3\n"
                    "\tmovdqu      %%xmm3,%2\n"
                    "\tmovdqu      %%xmm4,%5\n"
                    "\tmovdqu      %%xmm5,%4\n"
                    "\tmovdqu      %%xmm6,%7\n"
                    "\tmovdqu      %%xmm7,%6\n"
                    : "+m" (op.A[0]), "+m" (op.B[0]),
                      "+m" (op.A[1]), "+m" (op.B[1]),
                      "+m" (op.A[2]), "+m" (op.B[2]),
                      "+m" (op.A[3]), "+m" (op.B[3])
                    : /* empty */
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );
#else
      asm volatile ("  movdqu      %0,%%xmm0\n"
                    "\tmovdqu      %1,%%xmm1\n"
                    "\tmovdqu      %2,%%xmm2\n"
                    "\tmovdqu      %3,%%xmm3\n"
                    : "+m" (op.A[0]), "+m" (op.B[0]),
                      "+m" (op.A[1]), "+m" (op.B[1])
                    : /* empty */
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );

      asm volatile ("\tmovdqu      %4,%%xmm4\n"
                    "\tmovdqu      %5,%%xmm5\n"
                    "\tmovdqu      %6,%%xmm6\n"
                    "\tmovdqu      %7,%%xmm7\n"
                    : "+m" (op.A[2]), "+m" (op.B[2]),
                      "+m" (op.A[3]), "+m" (op.B[3])
                    : /* empty */
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );

      asm volatile ("\tmovdqu      %%xmm0,%1\n"
                    "\tmovdqu      %%xmm1,%0\n"
                    "\tmovdqu      %%xmm2,%3\n"
                    "\tmovdqu      %%xmm3,%2\n"
                    : "+m" (op.A[0]), "+m" (op.B[0]),
                      "+m" (op.A[1]), "+m" (op.B[1])
                    : /* empty */
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );

      asm volatile ("\tmovdqu      %%xmm4,%5\n"
                    "\tmovdqu      %%xmm5,%4\n"
                    "\tmovdqu      %%xmm6,%7\n"
                    "\tmovdqu      %%xmm7,%6\n"
                    : "+m" (op.A[2]), "+m" (op.B[2]),
                      "+m" (op.A[3]), "+m" (op.B[3])
                    : /* empty */
#ifdef __SSE__
                    : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                    );
#endif
      op.A += 64;
      op.B += 64;
    }


  for (; op.n_pixels >= 4; op.n_pixels -= 4)
    {
      asm volatile ("  movdqu      %0,%%xmm2\n"
                    "\tmovdqu      %1,%%xmm3\n"
                    "\tmovdqu  %%xmm3,%0\n"
                    "\tmovdqu  %%xmm2,%1\n"
                    : "+m" (*op.A), "+m" (*op.B)
                    : /* empty */
#ifdef __SSE__
                    : "%xmm2", "%xmm3"
#endif
                    );
      op.A += 16;
      op.B += 16;
    }

  for (; op.n_pixels >= 2; op.n_pixels -= 2)
    {
      asm volatile ("  movq      %0,%%mm2\n"
                    "\tmovq      %1,%%mm3\n"
                    "\tmovq   %%mm3,%0\n"
                    "\tmovq   %%mm2,%1\n"
                    : "+m" (*op.A), "+m" (*op.B)
                    : /* empty */
#ifdef __MMX__
                    : "%mm2", "%mm3"
#endif
                    );
      op.A += 8;
      op.B += 8;
    }

  if (op.n_pixels)
    {
      asm volatile ("  movd      %0,%%mm2\n"
                    "\tmovd      %1,%%mm3\n"
                    "\tmovd   %%mm3,%0\n"
                    "\tmovd   %%mm2,%1\n"
                    : "+m" (*op.A), "+m" (*op.B)
                    : /* empty */
#ifdef __MMX__
                    : "%mm1", "%mm2", "%mm3", "%mm4"
#endif
                    );
    }

  asm("emms");
}

#endif /* COMPILE_SSE2_IS_OKAY */

gboolean
gimp_composite_sse2_init (void)
{
#ifdef COMPILE_SSE2_IS_OKAY
  guint32 cpu = cpu_accel ();

  if (cpu & CPU_ACCEL_X86_SSE2)
    {
      return (TRUE);
    }
#endif
  return (FALSE);
}