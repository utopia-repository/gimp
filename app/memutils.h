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
#ifndef __MEM_UTILS_H__
#define __MEM_UTILS_H__

#include <X11/Intrinsic.h>

typedef struct _MemChunk *MemChunk;

/* "xmalloc" and "xfree" are wrappers for "malloc" and "free".
 * "xmalloc" will die with an error message if the requested
 *  memory could not be allocated.
 * "xfree" makes sure not to free NULL pointers.
 */

void* xmalloc (unsigned long);
void* xrealloc (void *, unsigned long);
void xfree (void*);

/* "mem_chunk_create" creates a new memory chunk.
 * Memory chunks are used to allocate pieces of memory which are
 *  always the same size. Lists and segments (of gregions) are
 *  good examples of two such data types.
 * The memory chunk allocates and frees blocks of memory as needed.
 *  Just be sure to call "mem_chunk_free" and not "free" and data
 *  allocated in a mem chunk. ("free" will most likely cause a seg
 *  fault...somewhere).
 *
 * Oh yeah, MemChunk is an opaque data type. (You don't really
 *  want to know what's going on inside do you?)
 */

/* ALLOC_ONLY MemChunk's can only allocate memory. The free operation
 *  is interpreted as a no op. ALLOC_ONLY MemChunk's save 4 bytes per
 *  atom.
 * ALLOC_AND_FREE MemChunk's can allocate and free memory.
 */

#define ALLOC_ONLY      1
#define ALLOC_AND_FREE  2

MemChunk mem_chunk_create (int, int);
void  mem_chunk_destroy (MemChunk);
void* mem_chunk_alloc (MemChunk);
void  mem_chunk_free (MemChunk, void*);
void  mem_chunk_clean (XtPointer, XtIntervalId *);

/* Ah yes...we have a "blow_chunks" function.
 * "blow_chunks" simply "compresses" all the chunks. This operation
 *  consists of freeing every memory area that should be freed (but
 *  which we haven't gotten around to doing yet).
 * "free_chunks" frees up all the chunks.
 */
void blow_chunks (void);
void free_chunks (void);

#endif /* __MEM_UTILS_H__ */
