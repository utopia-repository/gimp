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
#include <malloc.h>
#include <stdlib.h>
#include "appenv.h"
#include "errors.h"
#include "linked.h"
#include "memutils.h"

#define MEM_AREA_SIZE 65512L

typedef struct _MemArea MemArea;

struct _MemChunk {
  int type;                 /* the type of MemChunk: ALLOC_ONLY or ALLOC_AND_FREE */
  int num_mem_areas;        /* the number of memory areas */
  int atom_size;            /* the size of an atom */
  int marked_areas;         /* the number of marked areas */
  XtIntervalId timer;       /* used for garbage collection */
  MemArea *mem_area;        /* the current memory area */
  MemArea *mem_areas;       /* a list of all the mem areas owned by this chunk */
  MemArea *free_mem_area;   /* the free area...which is about to be destroyed */
  link_ptr free_atoms;      /* the free atoms list */
  MemChunk next;            /* pointer to the next chunk */
  MemChunk prev;            /* pointer to the previous chunk */
};

struct _MemArea {
  MemArea *next;            /* the next mem area */
  MemArea *prev;            /* the previous mem area */
  long index;               /* the current index into the "mem" array */
  long free;                /* the number of free bytes in this mem area */
  long allocated;           /* the number of atoms allocated from this area */
  long mark;                /* is this mem area marked for destruction */
  char mem[MEM_AREA_SIZE];  /* the mem array from which atoms get allocated */
};

static MemChunk mem_chunks = NULL;

void*
xmalloc (size)
     unsigned long size;
{
  void *p;

  p = malloc (size);
  if (!p)
    fatal_error ("could not allocate %d bytes", size);

  return p;
}

void*
xrealloc (ptr, size)
     void * ptr;
     unsigned long size;
{
  void *p;

  if (!ptr)
    p = malloc (size);
  else
    p = realloc (ptr, size);

  if (!p)
    fatal_error ("could not reallocate %d bytes", size);

  return p;
}

void
xfree (p)
     void *p;
{
  if (p)
    free (p);
}

MemChunk
mem_chunk_create (atom_size, type)
     int atom_size, type;
{
  MemChunk mem_chunk;

  mem_chunk = xmalloc (sizeof (struct _MemChunk));
  mem_chunk->type = type;
  mem_chunk->num_mem_areas = 0;
  mem_chunk->marked_areas = 0;
  mem_chunk->timer = 0;
  mem_chunk->mem_area = NULL;
  mem_chunk->free_mem_area = NULL;
  mem_chunk->free_atoms = NULL;
  mem_chunk->mem_areas = NULL;

  switch (mem_chunk->type)
    {
    case ALLOC_ONLY:
      mem_chunk->atom_size = atom_size;
      break;
    case ALLOC_AND_FREE:
      mem_chunk->atom_size = atom_size + 4;
      break;
    default:
      fatal_error ("unknown MemChunk type: %d", type);
      break;
    }
  
  if (mem_chunk->atom_size % 4)
    mem_chunk->atom_size += 4 - (mem_chunk->atom_size % 4);

  mem_chunk->next = mem_chunks;
  mem_chunk->prev = NULL;
  if (mem_chunks)
    mem_chunks->prev = mem_chunk;
  mem_chunks = mem_chunk;

  return mem_chunk;
}

void
mem_chunk_destroy (mem_chunk)
     MemChunk mem_chunk;
{
  MemArea *mem_areas;
  MemArea *temp_area;

  mem_areas = mem_chunk->mem_areas;
  while (mem_areas)
    {
/*      message ("mem area freed"); */

      temp_area = mem_areas;
      mem_areas = mem_areas->next;
      free (temp_area);
    }
  
  free_list (mem_chunk->free_atoms);

  if (mem_chunk->next)
    mem_chunk->next->prev = mem_chunk->prev;
  if (mem_chunk->prev)
    mem_chunk->prev->next = mem_chunk->next;

  if (mem_chunk == mem_chunks)
    mem_chunks = mem_chunks->next;

  free (mem_chunk);
}

void*
mem_chunk_alloc (mem_chunk)
     MemChunk mem_chunk;
{
  MemArea *temp_area;
  link_ptr temp_list;
  long *mem;

  if (!mem_chunk)
    return NULL;

  while (mem_chunk->free_atoms)
    {
      /* Get the first piece of memory on the "free_atoms" list.
       * We can go ahead and destroy the list node we used to keep
       *  track of it with and to update the "free_atoms" list to
       *  point to its next element.
       */
      mem = mem_chunk->free_atoms->data;
      temp_list = mem_chunk->free_atoms;
      mem_chunk->free_atoms = mem_chunk->free_atoms->next;
      temp_list->next = NULL;
      free_list (temp_list);

      /* Determine which area this piece of memory is allocated from */
      temp_area = (MemArea*) *((long*) mem - 1);
      
      /* If the area has been marked, then it is being destroyed.
       *  (ie marked to be destroyed).
       * We check to see if all of the segments on the free list that
       *  reference this area have been removed. This occurs when
       *  the ammount of free memory is less than the allocatable size.
       * If the chunk should be freed, then we place it in the "free_mem_area".
       * This is so we make sure not to free the mem area here and then
       *  allocate it again a few lines down.
       * If we don't allocate a chunk a few lines down then the "free_mem_area"
       *  will be freed.
       * If there is already a "free_mem_area" then we'll just free this mem area.
       */
      if (temp_area->mark)
	{
	  /* Update the "free" memory available in that area */
	  temp_area->free += mem_chunk->atom_size;

	  if (temp_area->free == MEM_AREA_SIZE)
	    {
	      if (temp_area == mem_chunk->mem_area)
		mem_chunk->mem_area = NULL;

	      if (mem_chunk->free_mem_area)
		{
/*		  message ("mem area freed"); */

		  mem_chunk->num_mem_areas -= 1;
		  mem_chunk->marked_areas -= 1;
		  if (temp_area->next)
		    temp_area->next->prev = temp_area->prev;
		  if (temp_area->prev)
		    temp_area->prev->next = temp_area->next;
		  if (temp_area == mem_chunk->mem_areas)
		    mem_chunk->mem_areas = mem_chunk->mem_areas->next;
		  if (temp_area == mem_chunk->mem_area)
		    mem_chunk->mem_area = NULL;

		  free (temp_area);
		}
	      else
		mem_chunk->free_mem_area = temp_area;
	    }
	}
      else
	{
	  /* Update the "free" memory available in that area */
	  temp_area->free -= mem_chunk->atom_size;
	  temp_area->allocated += 1;

          /* The area wasn't marked...return the memory */
          return mem;
	}
    }

  /* If there isn't a current mem area or the current mem area is out of space
   *  then allocate a new mem area. We'll first check and see if we can use
   *  the "free_mem_area". Otherwise we'll just malloc the mem area.
   */
  if ((!mem_chunk->mem_area) || 
      ((mem_chunk->mem_area->index + mem_chunk->atom_size) > MEM_AREA_SIZE))
    {
/*      message ("mem area allocated"); */
      
      if (mem_chunk->free_mem_area)
        {
          mem_chunk->mem_area = mem_chunk->free_mem_area;
	  mem_chunk->free_mem_area = NULL;
        }
      else
        {
	  mem_chunk->mem_area = xmalloc (sizeof (MemArea));
	  mem_chunk->num_mem_areas += 1;
	  mem_chunk->mem_area->next = mem_chunk->mem_areas;
	  mem_chunk->mem_area->prev = NULL;
	  if (mem_chunk->mem_areas)
	    mem_chunk->mem_areas->prev = mem_chunk->mem_area;
	  mem_chunk->mem_areas = mem_chunk->mem_area;
        }

      mem_chunk->mem_area->index = 0;
      mem_chunk->mem_area->free = MEM_AREA_SIZE;
      mem_chunk->mem_area->allocated = 0;
      mem_chunk->mem_area->mark = 0;
    }
  else if (mem_chunk->free_mem_area)
    {
/*      message ("mem area freed"); */
      
      mem_chunk->num_mem_areas -= 1;
      mem_chunk->marked_areas -= 1;
      if (mem_chunk->free_mem_area->next)
	mem_chunk->free_mem_area->next->prev = mem_chunk->free_mem_area->prev;
      if (mem_chunk->free_mem_area->prev)
	mem_chunk->free_mem_area->prev->next = mem_chunk->free_mem_area->next;
      if (mem_chunk->free_mem_area == mem_chunk->mem_areas)
	mem_chunk->mem_areas = mem_chunk->mem_areas->next;

      free (mem_chunk->free_mem_area);
      mem_chunk->free_mem_area = NULL;
    }

  /* Get the memory and modify the state variables appropriately */
  mem = (long*) &mem_chunk->mem_area->mem[mem_chunk->mem_area->index];
  mem_chunk->mem_area->index += mem_chunk->atom_size;
  mem_chunk->mem_area->free -= mem_chunk->atom_size;
  mem_chunk->mem_area->allocated += 1;

  /* If this is an ALLOC_AND_FREE chunk we calculated the atom_size with 4 extra bytes
   *  so that we can use that space to keep track of which mem area this piece 
   *  of memory came from.
   */
  if (mem_chunk->type == ALLOC_AND_FREE)
    {
      *mem = (long) mem_chunk->mem_area;
      mem += 1;
    }

  return mem;
}

void
mem_chunk_free (mem_chunk, mem)
     MemChunk mem_chunk;
     void *mem;
{
  MemArea *temp_area;
  link_ptr new_list;

  /* don't do anything if this is an ALLOC_ONLY chunk */
  if (mem_chunk->type == ALLOC_ONLY)
    return;

  /* Place the memory on the "free_atoms" list */
  new_list = alloc_list ();
  new_list->data = mem;
  new_list->next = mem_chunk->free_atoms;
  mem_chunk->free_atoms = new_list;

  temp_area = (MemArea*) *((long*) mem - 1);
  temp_area->allocated -= 1;

  if (temp_area->allocated == 0)
    {
/*      message ("mem area marked"); */
      
      temp_area->mark = 1;
      mem_chunk->marked_areas += 1;

      if (!mem_chunk->timer)
	mem_chunk->timer = XtAppAddTimeOut (app_context, 5000, 
					    mem_chunk_clean, 
					    mem_chunk);
    }
}

void
mem_chunk_clean (client_data, call_data)
     XtPointer client_data;
     XtIntervalId *call_data;
{
  MemChunk mem_chunk;
  MemArea *mem_area;
  link_ptr temp_list;
  link_ptr temp_list2;
  link_ptr prev_list;
  long *mem;

  mem_chunk = (MemChunk) client_data;
  mem_chunk->timer = 0;
  if ((mem_chunk->type == ALLOC_AND_FREE) && (mem_chunk->marked_areas > 0))
    {
      prev_list = NULL;
      temp_list = mem_chunk->free_atoms;
      
      while (temp_list)
	{
	  mem = temp_list->data;
	  temp_list2 = temp_list;
	  temp_list = temp_list->next;
	  
	  mem_area = (MemArea*) *((long*) mem - 1);
	  
	  /* If this mem area is marked for destruction then delete this 
	   *  list node and decrement the free mem.
	   */
	  if (mem_area->mark)
	    {
	      if (temp_list2 == mem_chunk->free_atoms)
		mem_chunk->free_atoms = temp_list2->next;
	      if (prev_list)
		prev_list->next = temp_list2->next;
	      temp_list2->next = NULL;
	      free_list (temp_list2);
	      
	      mem_area->free += mem_chunk->atom_size;
	      if (mem_area->free == MEM_AREA_SIZE)
		{
/*		  message ("mem area freed"); */
		  
		  mem_chunk->num_mem_areas -= 1;
		  mem_chunk->marked_areas -= 1;
		  if (mem_area->next)
		    mem_area->next->prev = mem_area->prev;
		  if (mem_area->prev)
		    mem_area->prev->next = mem_area->next;
		  if (mem_area == mem_chunk->mem_areas)
		    mem_chunk->mem_areas = mem_chunk->mem_areas->next;
		  if (mem_area == mem_chunk->mem_area)
		    mem_chunk->mem_area = NULL;
		  
		  free (mem_area);
		}
	    }
	  else
	    {
	      prev_list = temp_list2;
	    }
	}
    }
}

void
blow_chunks ()
{
  MemChunk mem_chunk;

/*  message ("blowing chunks"); */

  mem_chunk = mem_chunks;
  while (mem_chunk)
    {
      mem_chunk_clean (mem_chunk, NULL);
      mem_chunk = mem_chunk->next;
    }
}

void
free_chunks ()
{
  while (mem_chunks)
    mem_chunk_destroy (mem_chunks);
}
