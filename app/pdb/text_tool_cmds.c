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

#include "libgimpbase/gimpbase.h"

#include "pdb-types.h"
#include "procedural_db.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "text/gimptext-compat.h"

static ProcRecord text_fontname_proc;
static ProcRecord text_get_extents_fontname_proc;
static ProcRecord text_proc;
static ProcRecord text_get_extents_proc;

void
register_text_tool_procs (Gimp *gimp)
{
  procedural_db_register (gimp, &text_fontname_proc);
  procedural_db_register (gimp, &text_get_extents_fontname_proc);
  procedural_db_register (gimp, &text_proc);
  procedural_db_register (gimp, &text_get_extents_proc);
}

static Argument *
text_fontname_invoker (Gimp         *gimp,
                       GimpContext  *context,
                       GimpProgress *progress,
                       Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  GimpDrawable *drawable;
  gdouble x;
  gdouble y;
  gchar *text;
  gint32 border;
  gboolean antialias;
  gdouble size;
  gint32 size_type;
  gchar *fontname;
  GimpLayer *text_layer = NULL;

  gimage = gimp_image_get_by_ID (gimp, args[0].value.pdb_int);
  if (! GIMP_IS_IMAGE (gimage))
    success = FALSE;

  drawable = (GimpDrawable *) gimp_item_get_by_ID (gimp, args[1].value.pdb_int);

  x = args[2].value.pdb_float;

  y = args[3].value.pdb_float;

  text = (gchar *) args[4].value.pdb_pointer;
  if (text == NULL || !g_utf8_validate (text, -1, NULL))
    success = FALSE;

  border = args[5].value.pdb_int;
  if (border < -1)
    success = FALSE;

  antialias = args[6].value.pdb_int ? TRUE : FALSE;

  size = args[7].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[8].value.pdb_int;
  if (size_type < GIMP_PIXELS || size_type > GIMP_POINTS)
    success = FALSE;

  fontname = (gchar *) args[9].value.pdb_pointer;
  if (fontname == NULL || !g_utf8_validate (fontname, -1, NULL))
    success = FALSE;

  if (success)
    {
      if (drawable && ! gimp_item_is_attached (GIMP_ITEM (drawable)))
        success = FALSE;

      if (success)
        {
          gchar *real_fontname = g_strdup_printf ("%s %d", fontname, (gint) size);

          text_layer = text_render (gimage, drawable, context,
                                    x, y, real_fontname, text,
                                    border, antialias);

          g_free (real_fontname);
        }
    }

  return_args = procedural_db_return_args (&text_fontname_proc, success);

  if (success)
    return_args[1].value.pdb_int = text_layer ? gimp_item_get_ID (GIMP_ITEM (text_layer)) : -1;

  return return_args;
}

static ProcArg text_fontname_inargs[] =
{
  {
    GIMP_PDB_IMAGE,
    "image",
    "The image"
  },
  {
    GIMP_PDB_DRAWABLE,
    "drawable",
    "The affected drawable: (-1 for a new text layer)"
  },
  {
    GIMP_PDB_FLOAT,
    "x",
    "The x coordinate for the left of the text bounding box"
  },
  {
    GIMP_PDB_FLOAT,
    "y",
    "The y coordinate for the top of the text bounding box"
  },
  {
    GIMP_PDB_STRING,
    "text",
    "The text to generate (in UTF-8 encoding)"
  },
  {
    GIMP_PDB_INT32,
    "border",
    "The size of the border: -1 <= border"
  },
  {
    GIMP_PDB_INT32,
    "antialias",
    "Antialiasing (TRUE or FALSE)"
  },
  {
    GIMP_PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    GIMP_PDB_INT32,
    "size_type",
    "The units of specified size: GIMP_PIXELS (0) or GIMP_POINTS (1)"
  },
  {
    GIMP_PDB_STRING,
    "fontname",
    "The name of the font"
  }
};

static ProcArg text_fontname_outargs[] =
{
  {
    GIMP_PDB_LAYER,
    "text_layer",
    "The new text layer or -1 if no layer was created."
  }
};

static ProcRecord text_fontname_proc =
{
  "gimp_text_fontname",
  "Add text at the specified location as a floating selection or a new layer.",
  "This tool requires a fontname matching an installed PangoFT2 font. You can specify the fontsize in units of pixels or points, and the appropriate metric is specified using the size_type argument. The x and y parameters together control the placement of the new text by specifying the upper left corner of the text bounding box. If the specified drawable parameter is valid, the text will be created as a floating selection attached to the drawable. If the drawable parameter is not valid (-1), the text will appear as a new layer. Finally, a border can be specified around the final rendered text. The border is measured in pixels.",
  "Martin Edlman & Sven Neumann",
  "Spencer Kimball & Peter Mattis",
  "1998- 2001",
  NULL,
  GIMP_INTERNAL,
  10,
  text_fontname_inargs,
  1,
  text_fontname_outargs,
  { { text_fontname_invoker } }
};

static Argument *
text_get_extents_fontname_invoker (Gimp         *gimp,
                                   GimpContext  *context,
                                   GimpProgress *progress,
                                   Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *text;
  gdouble size;
  gint32 size_type;
  gchar *fontname;
  gint32 width;
  gint32 height;
  gint32 ascent;
  gint32 descent;
  gchar *real_fontname;

  text = (gchar *) args[0].value.pdb_pointer;
  if (text == NULL || !g_utf8_validate (text, -1, NULL))
    success = FALSE;

  size = args[1].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[2].value.pdb_int;
  if (size_type < GIMP_PIXELS || size_type > GIMP_POINTS)
    success = FALSE;

  fontname = (gchar *) args[3].value.pdb_pointer;
  if (fontname == NULL || !g_utf8_validate (fontname, -1, NULL))
    success = FALSE;

  if (success)
    {
      real_fontname = g_strdup_printf ("%s %d", fontname, (gint) size);

      success = text_get_extents (real_fontname, text,
                                  &width, &height,
                                  &ascent, &descent);

      g_free (real_fontname);
    }

  return_args = procedural_db_return_args (&text_get_extents_fontname_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = width;
      return_args[2].value.pdb_int = height;
      return_args[3].value.pdb_int = ascent;
      return_args[4].value.pdb_int = descent;
    }

  return return_args;
}

static ProcArg text_get_extents_fontname_inargs[] =
{
  {
    GIMP_PDB_STRING,
    "text",
    "The text to generate (in UTF-8 encoding)"
  },
  {
    GIMP_PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    GIMP_PDB_INT32,
    "size_type",
    "The units of specified size: GIMP_PIXELS (0) or GIMP_POINTS (1)"
  },
  {
    GIMP_PDB_STRING,
    "fontname",
    "The name of the font"
  }
};

static ProcArg text_get_extents_fontname_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "width",
    "The width of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "height",
    "The height of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "ascent",
    "The ascent of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "descent",
    "The descent of the specified font"
  }
};

static ProcRecord text_get_extents_fontname_proc =
{
  "gimp_text_get_extents_fontname",
  "Get extents of the bounding box for the specified text.",
  "This tool returns the width and height of a bounding box for the specified text string with the specified font information. Ascent and descent for the specified font are returned as well.",
  "Martin Edlman & Sven Neumann",
  "Spencer Kimball & Peter Mattis",
  "1998- 2001",
  NULL,
  GIMP_INTERNAL,
  4,
  text_get_extents_fontname_inargs,
  4,
  text_get_extents_fontname_outargs,
  { { text_get_extents_fontname_invoker } }
};

static Argument *
text_invoker (Gimp         *gimp,
              GimpContext  *context,
              GimpProgress *progress,
              Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  GimpDrawable *drawable;
  gdouble x;
  gdouble y;
  gchar *text;
  gint32 border;
  gboolean antialias;
  gdouble size;
  gint32 size_type;
  gchar *foundry;
  gchar *family;
  gchar *weight;
  gchar *slant;
  gchar *set_width;
  gchar *spacing;
  gchar *registry;
  gchar *encoding;
  GimpLayer *text_layer = NULL;

  gimage = gimp_image_get_by_ID (gimp, args[0].value.pdb_int);
  if (! GIMP_IS_IMAGE (gimage))
    success = FALSE;

  drawable = (GimpDrawable *) gimp_item_get_by_ID (gimp, args[1].value.pdb_int);

  x = args[2].value.pdb_float;

  y = args[3].value.pdb_float;

  text = (gchar *) args[4].value.pdb_pointer;
  if (text == NULL || !g_utf8_validate (text, -1, NULL))
    success = FALSE;

  border = args[5].value.pdb_int;
  if (border < -1)
    success = FALSE;

  antialias = args[6].value.pdb_int ? TRUE : FALSE;

  size = args[7].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[8].value.pdb_int;
  if (size_type < GIMP_PIXELS || size_type > GIMP_POINTS)
    success = FALSE;

  foundry = (gchar *) args[9].value.pdb_pointer;
  if (foundry == NULL)
    success = FALSE;

  family = (gchar *) args[10].value.pdb_pointer;
  if (family == NULL)
    success = FALSE;

  weight = (gchar *) args[11].value.pdb_pointer;
  if (weight == NULL)
    success = FALSE;

  slant = (gchar *) args[12].value.pdb_pointer;
  if (slant == NULL)
    success = FALSE;

  set_width = (gchar *) args[13].value.pdb_pointer;
  if (set_width == NULL)
    success = FALSE;

  spacing = (gchar *) args[14].value.pdb_pointer;
  if (spacing == NULL)
    success = FALSE;

  registry = (gchar *) args[15].value.pdb_pointer;
  if (registry == NULL)
    success = FALSE;

  encoding = (gchar *) args[16].value.pdb_pointer;
  if (encoding == NULL)
    success = FALSE;

  if (success)
    {
      if (drawable && ! gimp_item_is_attached (GIMP_ITEM (drawable)))
        success = FALSE;

      if (success)
        {
          gchar *real_fontname = g_strdup_printf ("%s %d", family, (gint) size);

          text_layer = text_render (gimage, drawable, context,
                                    x, y, real_fontname, text,
                                    border, antialias);

          g_free (real_fontname);
        }
    }

  return_args = procedural_db_return_args (&text_proc, success);

  if (success)
    return_args[1].value.pdb_int = text_layer ? gimp_item_get_ID (GIMP_ITEM (text_layer)) : -1;

  return return_args;
}

static ProcArg text_inargs[] =
{
  {
    GIMP_PDB_IMAGE,
    "image",
    "The image"
  },
  {
    GIMP_PDB_DRAWABLE,
    "drawable",
    "The affected drawable: (-1 for a new text layer)"
  },
  {
    GIMP_PDB_FLOAT,
    "x",
    "The x coordinate for the left of the text bounding box"
  },
  {
    GIMP_PDB_FLOAT,
    "y",
    "The y coordinate for the top of the text bounding box"
  },
  {
    GIMP_PDB_STRING,
    "text",
    "The text to generate (in UTF-8 encoding)"
  },
  {
    GIMP_PDB_INT32,
    "border",
    "The size of the border: -1 <= border"
  },
  {
    GIMP_PDB_INT32,
    "antialias",
    "Antialiasing (TRUE or FALSE)"
  },
  {
    GIMP_PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    GIMP_PDB_INT32,
    "size_type",
    "The units of specified size: GIMP_PIXELS (0) or GIMP_POINTS (1)"
  },
  {
    GIMP_PDB_STRING,
    "foundry",
    "The font foundry"
  },
  {
    GIMP_PDB_STRING,
    "family",
    "The font family"
  },
  {
    GIMP_PDB_STRING,
    "weight",
    "The font weight"
  },
  {
    GIMP_PDB_STRING,
    "slant",
    "The font slant"
  },
  {
    GIMP_PDB_STRING,
    "set_width",
    "The font set-width"
  },
  {
    GIMP_PDB_STRING,
    "spacing",
    "The font spacing"
  },
  {
    GIMP_PDB_STRING,
    "registry",
    "The font registry"
  },
  {
    GIMP_PDB_STRING,
    "encoding",
    "The font encoding"
  }
};

static ProcArg text_outargs[] =
{
  {
    GIMP_PDB_LAYER,
    "text_layer",
    "The new text layer or -1 if no layer was created."
  }
};

static ProcRecord text_proc =
{
  "gimp_text",
  "This procedure is deprecated! Use 'gimp_text_fontname' instead.",
  "This procedure is deprecated! Use 'gimp_text_fontname' instead.",
  "",
  "",
  "",
  "gimp_text_fontname",
  GIMP_INTERNAL,
  17,
  text_inargs,
  1,
  text_outargs,
  { { text_invoker } }
};

static Argument *
text_get_extents_invoker (Gimp         *gimp,
                          GimpContext  *context,
                          GimpProgress *progress,
                          Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *text;
  gdouble size;
  gint32 size_type;
  gchar *foundry;
  gchar *family;
  gchar *weight;
  gchar *slant;
  gchar *set_width;
  gchar *spacing;
  gchar *registry;
  gchar *encoding;
  gint32 width;
  gint32 height;
  gint32 ascent;
  gint32 descent;
  gchar *real_fontname;

  text = (gchar *) args[0].value.pdb_pointer;
  if (text == NULL || !g_utf8_validate (text, -1, NULL))
    success = FALSE;

  size = args[1].value.pdb_float;
  if (size <= 0.0)
    success = FALSE;

  size_type = args[2].value.pdb_int;
  if (size_type < GIMP_PIXELS || size_type > GIMP_POINTS)
    success = FALSE;

  foundry = (gchar *) args[3].value.pdb_pointer;
  if (foundry == NULL)
    success = FALSE;

  family = (gchar *) args[4].value.pdb_pointer;
  if (family == NULL)
    success = FALSE;

  weight = (gchar *) args[5].value.pdb_pointer;
  if (weight == NULL)
    success = FALSE;

  slant = (gchar *) args[6].value.pdb_pointer;
  if (slant == NULL)
    success = FALSE;

  set_width = (gchar *) args[7].value.pdb_pointer;
  if (set_width == NULL)
    success = FALSE;

  spacing = (gchar *) args[8].value.pdb_pointer;
  if (spacing == NULL)
    success = FALSE;

  registry = (gchar *) args[9].value.pdb_pointer;
  if (registry == NULL)
    success = FALSE;

  encoding = (gchar *) args[10].value.pdb_pointer;
  if (encoding == NULL)
    success = FALSE;

  if (success)
    {
      real_fontname = g_strdup_printf ("%s %d", family, (gint) size);

      success = text_get_extents (real_fontname, text,
                                  &width, &height,
                                  &ascent, &descent);

      g_free (real_fontname);
    }

  return_args = procedural_db_return_args (&text_get_extents_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = width;
      return_args[2].value.pdb_int = height;
      return_args[3].value.pdb_int = ascent;
      return_args[4].value.pdb_int = descent;
    }

  return return_args;
}

static ProcArg text_get_extents_inargs[] =
{
  {
    GIMP_PDB_STRING,
    "text",
    "The text to generate (in UTF-8 encoding)"
  },
  {
    GIMP_PDB_FLOAT,
    "size",
    "The size of text in either pixels or points"
  },
  {
    GIMP_PDB_INT32,
    "size_type",
    "The units of specified size: GIMP_PIXELS (0) or GIMP_POINTS (1)"
  },
  {
    GIMP_PDB_STRING,
    "foundry",
    "The font foundry"
  },
  {
    GIMP_PDB_STRING,
    "family",
    "The font family"
  },
  {
    GIMP_PDB_STRING,
    "weight",
    "The font weight"
  },
  {
    GIMP_PDB_STRING,
    "slant",
    "The font slant"
  },
  {
    GIMP_PDB_STRING,
    "set_width",
    "The font set-width"
  },
  {
    GIMP_PDB_STRING,
    "spacing",
    "The font spacing"
  },
  {
    GIMP_PDB_STRING,
    "registry",
    "The font registry"
  },
  {
    GIMP_PDB_STRING,
    "encoding",
    "The font encoding"
  }
};

static ProcArg text_get_extents_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "width",
    "The width of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "height",
    "The height of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "ascent",
    "The ascent of the specified font"
  },
  {
    GIMP_PDB_INT32,
    "descent",
    "The descent of the specified font"
  }
};

static ProcRecord text_get_extents_proc =
{
  "gimp_text_get_extents",
  "This procedure is deprecated! Use 'gimp_text_get_extents_fontname' instead.",
  "This procedure is deprecated! Use 'gimp_text_get_extents_fontname' instead.",
  "",
  "",
  "",
  "gimp_text_get_extents_fontname",
  GIMP_INTERNAL,
  11,
  text_get_extents_inargs,
  4,
  text_get_extents_outargs,
  { { text_get_extents_invoker } }
};