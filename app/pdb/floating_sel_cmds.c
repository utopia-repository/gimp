/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2003 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "config.h"


#include <glib-object.h>

#include "pdb-types.h"
#include "procedural_db.h"

#include "core/gimpdrawable.h"
#include "core/gimplayer-floating-sel.h"
#include "core/gimplayer.h"

static ProcRecord floating_sel_remove_proc;
static ProcRecord floating_sel_anchor_proc;
static ProcRecord floating_sel_to_layer_proc;
static ProcRecord floating_sel_attach_proc;
static ProcRecord floating_sel_rigor_proc;
static ProcRecord floating_sel_relax_proc;

void
register_floating_sel_procs (Gimp *gimp)
{
  procedural_db_register (gimp, &floating_sel_remove_proc);
  procedural_db_register (gimp, &floating_sel_anchor_proc);
  procedural_db_register (gimp, &floating_sel_to_layer_proc);
  procedural_db_register (gimp, &floating_sel_attach_proc);
  procedural_db_register (gimp, &floating_sel_rigor_proc);
  procedural_db_register (gimp, &floating_sel_relax_proc);
}

static Argument *
floating_sel_remove_invoker (Gimp         *gimp,
                             GimpContext  *context,
                             GimpProgress *progress,
                             Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *floating_sel;

  floating_sel = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (floating_sel) && ! gimp_item_is_removed (GIMP_ITEM (floating_sel))))
    success = FALSE;

  if (success)
    {
      if (gimp_layer_is_floating_sel (floating_sel))
        floating_sel_remove (floating_sel);
      else
        success = FALSE;
    }

  return procedural_db_return_args (&floating_sel_remove_proc, success);
}

static ProcArg floating_sel_remove_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "floating_sel",
    "The floating selection"
  }
};

static ProcRecord floating_sel_remove_proc =
{
  "gimp_floating_sel_remove",
  "Remove the specified floating selection from its associated drawable.",
  "This procedure removes the floating selection completely, without any side effects. The associated drawable is then set to active.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  1,
  floating_sel_remove_inargs,
  0,
  NULL,
  { { floating_sel_remove_invoker } }
};

static Argument *
floating_sel_anchor_invoker (Gimp         *gimp,
                             GimpContext  *context,
                             GimpProgress *progress,
                             Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *floating_sel;

  floating_sel = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (floating_sel) && ! gimp_item_is_removed (GIMP_ITEM (floating_sel))))
    success = FALSE;

  if (success)
    {
      if (gimp_layer_is_floating_sel (floating_sel))
        floating_sel_anchor (floating_sel);
      else
        success = FALSE;
    }

  return procedural_db_return_args (&floating_sel_anchor_proc, success);
}

static ProcArg floating_sel_anchor_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "floating_sel",
    "The floating selection"
  }
};

static ProcRecord floating_sel_anchor_proc =
{
  "gimp_floating_sel_anchor",
  "Anchor the specified floating selection to its associated drawable.",
  "This procedure anchors the floating selection to its associated drawable. This is similar to merging with a merge type of ClipToBottomLayer. The floating selection layer is no longer valid after this operation.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  1,
  floating_sel_anchor_inargs,
  0,
  NULL,
  { { floating_sel_anchor_invoker } }
};

static Argument *
floating_sel_to_layer_invoker (Gimp         *gimp,
                               GimpContext  *context,
                               GimpProgress *progress,
                               Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *floating_sel;

  floating_sel = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (floating_sel) && ! gimp_item_is_removed (GIMP_ITEM (floating_sel))))
    success = FALSE;

  if (success)
    {
      if (gimp_layer_is_floating_sel (floating_sel))
        floating_sel_to_layer (floating_sel);
      else
        success = FALSE;
    }

  return procedural_db_return_args (&floating_sel_to_layer_proc, success);
}

static ProcArg floating_sel_to_layer_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "floating_sel",
    "The floating selection"
  }
};

static ProcRecord floating_sel_to_layer_proc =
{
  "gimp_floating_sel_to_layer",
  "Transforms the specified floating selection into a layer.",
  "This procedure transforms the specified floating selection into a layer with the same offsets and extents. The composited image will look precisely the same, but the floating selection layer will no longer be clipped to the extents of the drawable it was attached to. The floating selection will become the active layer. This procedure will not work if the floating selection has a different base type from the underlying image. This might be the case if the floating selection is above an auxillary channel or a layer mask.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  1,
  floating_sel_to_layer_inargs,
  0,
  NULL,
  { { floating_sel_to_layer_invoker } }
};

static Argument *
floating_sel_attach_invoker (Gimp         *gimp,
                             GimpContext  *context,
                             GimpProgress *progress,
                             Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *layer;
  GimpDrawable *drawable;

  layer = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (layer) && ! gimp_item_is_removed (GIMP_ITEM (layer))))
    success = FALSE;

  drawable = (GimpDrawable *) gimp_item_get_by_ID (gimp, args[1].value.pdb_int);
  if (! (GIMP_IS_DRAWABLE (drawable) && ! gimp_item_is_removed (GIMP_ITEM (drawable))))
    success = FALSE;

  if (success)
    {
      success = gimp_item_is_attached (GIMP_ITEM (drawable));

      if (success)
        floating_sel_attach (layer, drawable);
    }

  return procedural_db_return_args (&floating_sel_attach_proc, success);
}

static ProcArg floating_sel_attach_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "layer",
    "The layer (is attached as floating selection)"
  },
  {
    GIMP_PDB_DRAWABLE,
    "drawable",
    "The drawable (where to attach the floating selection)"
  }
};

static ProcRecord floating_sel_attach_proc =
{
  "gimp_floating_sel_attach",
  "Attach the specified layer as floating to the specified drawable.",
  "This procedure attaches the layer as floating selection to the drawable.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  2,
  floating_sel_attach_inargs,
  0,
  NULL,
  { { floating_sel_attach_invoker } }
};

static Argument *
floating_sel_rigor_invoker (Gimp         *gimp,
                            GimpContext  *context,
                            GimpProgress *progress,
                            Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *floating_sel;
  gboolean undo;

  floating_sel = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (floating_sel) && ! gimp_item_is_removed (GIMP_ITEM (floating_sel))))
    success = FALSE;

  undo = args[1].value.pdb_int ? TRUE : FALSE;

  if (success)
    {
      if (gimp_layer_is_floating_sel (floating_sel))
        floating_sel_rigor (floating_sel, undo);
      else
        success = FALSE;
    }

  return procedural_db_return_args (&floating_sel_rigor_proc, success);
}

static ProcArg floating_sel_rigor_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "floating_sel",
    "The floating selection"
  },
  {
    GIMP_PDB_INT32,
    "undo",
    "TRUE or FALSE"
  }
};

static ProcRecord floating_sel_rigor_proc =
{
  "gimp_floating_sel_rigor",
  "Rigor the floating selection.",
  "This procedure rigors the floating selection.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  2,
  floating_sel_rigor_inargs,
  0,
  NULL,
  { { floating_sel_rigor_invoker } }
};

static Argument *
floating_sel_relax_invoker (Gimp         *gimp,
                            GimpContext  *context,
                            GimpProgress *progress,
                            Argument     *args)
{
  gboolean success = TRUE;
  GimpLayer *floating_sel;
  gboolean undo;

  floating_sel = (GimpLayer *) gimp_item_get_by_ID (gimp, args[0].value.pdb_int);
  if (! (GIMP_IS_LAYER (floating_sel) && ! gimp_item_is_removed (GIMP_ITEM (floating_sel))))
    success = FALSE;

  undo = args[1].value.pdb_int ? TRUE : FALSE;

  if (success)
    {
      if (gimp_layer_is_floating_sel (floating_sel))
        floating_sel_relax (floating_sel, undo);
      else
        success = FALSE;
    }

  return procedural_db_return_args (&floating_sel_relax_proc, success);
}

static ProcArg floating_sel_relax_inargs[] =
{
  {
    GIMP_PDB_LAYER,
    "floating_sel",
    "The floating selection"
  },
  {
    GIMP_PDB_INT32,
    "undo",
    "TRUE or FALSE"
  }
};

static ProcRecord floating_sel_relax_proc =
{
  "gimp_floating_sel_relax",
  "Relax the floating selection.",
  "This procedure relaxes the floating selection.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  NULL,
  GIMP_INTERNAL,
  2,
  floating_sel_relax_inargs,
  0,
  NULL,
  { { floating_sel_relax_invoker } }
};