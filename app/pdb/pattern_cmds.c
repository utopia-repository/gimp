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

#include <string.h>

#include <glib-object.h>

#include "pdb-types.h"
#include "procedural_db.h"

#include "base/temp-buf.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimplist.h"
#include "core/gimppattern.h"

static ProcRecord pattern_get_info_proc;
static ProcRecord pattern_get_pixels_proc;

void
register_pattern_procs (Gimp *gimp)
{
  procedural_db_register (gimp, &pattern_get_info_proc);
  procedural_db_register (gimp, &pattern_get_pixels_proc);
}

static Argument *
pattern_get_info_invoker (Gimp         *gimp,
                          GimpContext  *context,
                          GimpProgress *progress,
                          Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *name;
  GimpPattern *pattern = NULL;

  name = (gchar *) args[0].value.pdb_pointer;
  if (name == NULL || !g_utf8_validate (name, -1, NULL))
    success = FALSE;

  if (success)
    {
      pattern = (GimpPattern *)
        gimp_container_get_child_by_name (gimp->pattern_factory->container, name);

      success = (pattern != NULL);
    }

  return_args = procedural_db_return_args (&pattern_get_info_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = pattern->mask->width;
      return_args[2].value.pdb_int = pattern->mask->height;
      return_args[3].value.pdb_int = pattern->mask->bytes;
    }

  return return_args;
}

static ProcArg pattern_get_info_inargs[] =
{
  {
    GIMP_PDB_STRING,
    "name",
    "The pattern name."
  }
};

static ProcArg pattern_get_info_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "width",
    "The pattern width"
  },
  {
    GIMP_PDB_INT32,
    "height",
    "The pattern height"
  },
  {
    GIMP_PDB_INT32,
    "bpp",
    "The pattern bpp"
  }
};

static ProcRecord pattern_get_info_proc =
{
  "gimp_pattern_get_info",
  "Retrieve information about the specified pattern.",
  "This procedure retrieves information about the specified pattern. This includes the pattern extents (width and height).",
  "Michael Natterer <mitch@gimp.org>",
  "Michael Natterer",
  "2004",
  NULL,
  GIMP_INTERNAL,
  1,
  pattern_get_info_inargs,
  3,
  pattern_get_info_outargs,
  { { pattern_get_info_invoker } }
};

static Argument *
pattern_get_pixels_invoker (Gimp         *gimp,
                            GimpContext  *context,
                            GimpProgress *progress,
                            Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *name;
  gint32 num_color_bytes = 0;
  guint8 *color_bytes = NULL;
  GimpPattern *pattern = NULL;

  name = (gchar *) args[0].value.pdb_pointer;
  if (name == NULL || !g_utf8_validate (name, -1, NULL))
    success = FALSE;

  if (success)
    {
      pattern = (GimpPattern *)
        gimp_container_get_child_by_name (gimp->pattern_factory->container, name);

      if (pattern)
        {
          num_color_bytes = pattern->mask->height * pattern->mask->width *
                            pattern->mask->bytes;
          color_bytes     = g_memdup (temp_buf_data (pattern->mask),
                                      num_color_bytes);
        }
      else
        success = FALSE;
    }

  return_args = procedural_db_return_args (&pattern_get_pixels_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = pattern->mask->width;
      return_args[2].value.pdb_int = pattern->mask->height;
      return_args[3].value.pdb_int = pattern->mask->bytes;
      return_args[4].value.pdb_int = num_color_bytes;
      return_args[5].value.pdb_pointer = color_bytes;
    }

  return return_args;
}

static ProcArg pattern_get_pixels_inargs[] =
{
  {
    GIMP_PDB_STRING,
    "name",
    "The pattern name."
  }
};

static ProcArg pattern_get_pixels_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "width",
    "The pattern width"
  },
  {
    GIMP_PDB_INT32,
    "height",
    "The pattern height"
  },
  {
    GIMP_PDB_INT32,
    "bpp",
    "The pattern bpp"
  },
  {
    GIMP_PDB_INT32,
    "num_color_bytes",
    "Number of pattern bytes"
  },
  {
    GIMP_PDB_INT8ARRAY,
    "color_bytes",
    "The pattern data."
  }
};

static ProcRecord pattern_get_pixels_proc =
{
  "gimp_pattern_get_pixels",
  "Retrieve information about the specified pattern (including pixels).",
  "This procedure retrieves information about the specified. This includes the pattern extents (width and height), its bpp and its pixel data.",
  "Michael Natterer <mitch@gimp.org>",
  "Michael Natterer",
  "2004",
  NULL,
  GIMP_INTERNAL,
  1,
  pattern_get_pixels_inargs,
  5,
  pattern_get_pixels_outargs,
  { { pattern_get_pixels_invoker } }
};