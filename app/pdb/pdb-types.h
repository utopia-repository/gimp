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

#ifndef __PDB_TYPES_H__
#define __PDB_TYPES_H__


#include "core/core-types.h"


typedef struct _Argument   Argument;
typedef struct _ProcArg    ProcArg;
typedef struct _ProcRecord ProcRecord;


typedef enum
{
  GIMP_PDB_COMPAT_OFF,
  GIMP_PDB_COMPAT_ON,
  GIMP_PDB_COMPAT_WARN
} GimpPDBCompatMode;


#endif /* __PDB_TYPES_H__ */