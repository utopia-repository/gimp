/*
 * The GIMP -- an image manipulation program
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

#define MAXCOLORS   256

#define BitSet(byte, bit)        (((byte) & (bit)) == (bit))

#define ReadOK(file,buffer,len)  (fread(buffer, len, 1, file) != 0)
#define Write(file,buffer,len)   fwrite(buffer, len, 1, file)
#define WriteOK(file,buffer,len) (Write(buffer, len, file) != 0)

gint32             ReadBMP   (const gchar *filename);
GimpPDBStatusType  WriteBMP  (const gchar *filename,
                              gint32       image,
                              gint32       drawable_ID);

extern       gboolean  interactive_bmp;
extern const gchar    *filename;

extern struct Bitmap_File_Head_Struct
{
  gchar    zzMagic[2];	/* 00 "BM" */
  gulong   bfSize;      /* 02 */
  gushort  zzHotX;	/* 06 */
  gushort  zzHotY;	/* 08 */
  gulong   bfOffs;      /* 0A */
  gulong   biSize;      /* 0E */
} Bitmap_File_Head;

extern struct Bitmap_Head_Struct
{
  gulong   biWidth;     /* 12 */
  glong    biHeight;    /* 16 */
  gushort  biPlanes;    /* 1A */
  gushort  biBitCnt;    /* 1C */
  gulong   biCompr;     /* 1E */
  gulong   biSizeIm;    /* 22 */
  gulong   biXPels;     /* 26 */
  gulong   biYPels;     /* 2A */
  gulong   biClrUsed;   /* 2E */
  gulong   biClrImp;    /* 32 */
                        /* 36 */
} Bitmap_Head;

typedef struct _Bitmap_Channel
{
  guint32 mask;
  guint32 shiftin;
  guint32 shiftout;
} Bitmap_Channel;
