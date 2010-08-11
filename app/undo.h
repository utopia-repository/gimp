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
#ifndef __UNDO_H__
#define __UNDO_H__

/*  UNDO types  */
#define  IMAGE			0
#define  SELECTION		1
#define  MOVE_SELECTION		2


typedef Boolean (* UndoPopFunc)  (int, int, void *);
typedef void    (* UndoFreeFunc) (int, void *);


typedef struct _undo Undo;

struct _undo
{
  int           ID;             /*  ID of gdisplay or gimage structure  */
  void *        data;           /*  data to implement the undo          */
  long          bytes;          /*  size of undo item                   */

  UndoPopFunc   pop_func;       /*  function pointer to undo pop proc   */
  UndoFreeFunc  free_func;      /*  function with specifics for freeing */
};


/*  Undo interface functions  */

Boolean      undo_push_image      (int, int, int, int, int);
Boolean      undo_push_buffer     (int, void *);
Boolean      undo_push_mod_sel    (int, void *);
Boolean      undo_push_selection  (int, void *);
Boolean      undo_push_region     (int, void *);
Boolean      undo_push_move_sel   (int, int, int, int);
Boolean      undo_push_change_opacity (int, double, double, int);
Boolean      undo_push_cut        (int, void *, void *, int);
Boolean      undo_push_copy       (int, void *, void *);
Boolean      undo_push_paste      (int, void *);
Boolean      undo_push_transform  (int, void *);
Boolean      undo_pop             (void);
Boolean      undo_redo            (void);
void         undo_clean_stack     (void);
void         undo_free            (void);


#endif  /* __UNDO_H__ */


