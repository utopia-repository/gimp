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

#include "core/gimpunit.h"

static ProcRecord unit_get_number_of_units_proc;
static ProcRecord unit_get_number_of_built_in_units_proc;
static ProcRecord unit_new_proc;
static ProcRecord unit_get_deletion_flag_proc;
static ProcRecord unit_set_deletion_flag_proc;
static ProcRecord unit_get_identifier_proc;
static ProcRecord unit_get_factor_proc;
static ProcRecord unit_get_digits_proc;
static ProcRecord unit_get_symbol_proc;
static ProcRecord unit_get_abbreviation_proc;
static ProcRecord unit_get_singular_proc;
static ProcRecord unit_get_plural_proc;

void
register_unit_procs (Gimp *gimp)
{
  procedural_db_register (gimp, &unit_get_number_of_units_proc);
  procedural_db_register (gimp, &unit_get_number_of_built_in_units_proc);
  procedural_db_register (gimp, &unit_new_proc);
  procedural_db_register (gimp, &unit_get_deletion_flag_proc);
  procedural_db_register (gimp, &unit_set_deletion_flag_proc);
  procedural_db_register (gimp, &unit_get_identifier_proc);
  procedural_db_register (gimp, &unit_get_factor_proc);
  procedural_db_register (gimp, &unit_get_digits_proc);
  procedural_db_register (gimp, &unit_get_symbol_proc);
  procedural_db_register (gimp, &unit_get_abbreviation_proc);
  procedural_db_register (gimp, &unit_get_singular_proc);
  procedural_db_register (gimp, &unit_get_plural_proc);
}

static Argument *
unit_get_number_of_units_invoker (Gimp         *gimp,
                                  GimpContext  *context,
                                  GimpProgress *progress,
                                  Argument     *args)
{
  Argument *return_args;

  return_args = procedural_db_return_args (&unit_get_number_of_units_proc, TRUE);
  return_args[1].value.pdb_int = _gimp_unit_get_number_of_units (gimp);

  return return_args;
}

static ProcArg unit_get_number_of_units_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "num_units",
    "The number of units"
  }
};

static ProcRecord unit_get_number_of_units_proc =
{
  "gimp_unit_get_number_of_units",
  "Returns the number of units.",
  "This procedure returns the number of defined units.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  0,
  NULL,
  1,
  unit_get_number_of_units_outargs,
  { { unit_get_number_of_units_invoker } }
};

static Argument *
unit_get_number_of_built_in_units_invoker (Gimp         *gimp,
                                           GimpContext  *context,
                                           GimpProgress *progress,
                                           Argument     *args)
{
  Argument *return_args;

  return_args = procedural_db_return_args (&unit_get_number_of_built_in_units_proc, TRUE);
  return_args[1].value.pdb_int = _gimp_unit_get_number_of_units (gimp);

  return return_args;
}

static ProcArg unit_get_number_of_built_in_units_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "num_units",
    "The number of built-in units"
  }
};

static ProcRecord unit_get_number_of_built_in_units_proc =
{
  "gimp_unit_get_number_of_built_in_units",
  "Returns the number of built-in units.",
  "This procedure returns the number of defined units built-in to the GIMP.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  0,
  NULL,
  1,
  unit_get_number_of_built_in_units_outargs,
  { { unit_get_number_of_built_in_units_invoker } }
};

static Argument *
unit_new_invoker (Gimp         *gimp,
                  GimpContext  *context,
                  GimpProgress *progress,
                  Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  gchar *identifier;
  gdouble factor;
  gint32 digits;
  gchar *symbol;
  gchar *abbreviation;
  gchar *singular;
  gchar *plural;
  GimpUnit unit = 0;

  identifier = (gchar *) args[0].value.pdb_pointer;
  if (identifier == NULL || !g_utf8_validate (identifier, -1, NULL))
    success = FALSE;

  factor = args[1].value.pdb_float;

  digits = args[2].value.pdb_int;

  symbol = (gchar *) args[3].value.pdb_pointer;
  if (symbol == NULL || !g_utf8_validate (symbol, -1, NULL))
    success = FALSE;

  abbreviation = (gchar *) args[4].value.pdb_pointer;
  if (abbreviation == NULL || !g_utf8_validate (abbreviation, -1, NULL))
    success = FALSE;

  singular = (gchar *) args[5].value.pdb_pointer;
  if (singular == NULL || !g_utf8_validate (singular, -1, NULL))
    success = FALSE;

  plural = (gchar *) args[6].value.pdb_pointer;
  if (plural == NULL || !g_utf8_validate (plural, -1, NULL))
    success = FALSE;

  if (success)
    unit = _gimp_unit_new (gimp, identifier, factor, digits, symbol, abbreviation,
                           singular, plural);

  return_args = procedural_db_return_args (&unit_new_proc, success);

  if (success)
    return_args[1].value.pdb_int = unit;

  return return_args;
}

static ProcArg unit_new_inargs[] =
{
  {
    GIMP_PDB_STRING,
    "identifier",
    "The new unit's identifier"
  },
  {
    GIMP_PDB_FLOAT,
    "factor",
    "The new unit's factor"
  },
  {
    GIMP_PDB_INT32,
    "digits",
    "The new unit's digits"
  },
  {
    GIMP_PDB_STRING,
    "symbol",
    "The new unit's symbol"
  },
  {
    GIMP_PDB_STRING,
    "abbreviation",
    "The new unit's abbreviation"
  },
  {
    GIMP_PDB_STRING,
    "singular",
    "The new unit's singular form"
  },
  {
    GIMP_PDB_STRING,
    "plural",
    "The new unit's plural form"
  }
};

static ProcArg unit_new_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The new unit's ID"
  }
};

static ProcRecord unit_new_proc =
{
  "gimp_unit_new",
  "Creates a new unit and returns it's integer ID.",
  "This procedure creates a new unit and returns it's integer ID. Note that the new unit will have it's deletion flag set to TRUE, so you will have to set it to FALSE with gimp_unit_set_deletion_flag to make it persistent.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  7,
  unit_new_inargs,
  1,
  unit_new_outargs,
  { { unit_new_invoker } }
};

static Argument *
unit_get_deletion_flag_invoker (Gimp         *gimp,
                                GimpContext  *context,
                                GimpProgress *progress,
                                Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_deletion_flag_proc, success);

  if (success)
    return_args[1].value.pdb_int = _gimp_unit_get_deletion_flag (gimp, unit);

  return return_args;
}

static ProcArg unit_get_deletion_flag_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_deletion_flag_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "deletion_flag",
    "The unit's deletion flag"
  }
};

static ProcRecord unit_get_deletion_flag_proc =
{
  "gimp_unit_get_deletion_flag",
  "Returns the deletion flag of the unit.",
  "This procedure returns the deletion flag of the unit. If this value is TRUE the unit's definition will not be saved in the user's unitrc file on gimp exit.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_deletion_flag_inargs,
  1,
  unit_get_deletion_flag_outargs,
  { { unit_get_deletion_flag_invoker } }
};

static Argument *
unit_set_deletion_flag_invoker (Gimp         *gimp,
                                GimpContext  *context,
                                GimpProgress *progress,
                                Argument     *args)
{
  gboolean success = TRUE;
  GimpUnit unit;
  gboolean deletion_flag;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  deletion_flag = args[1].value.pdb_int ? TRUE : FALSE;

  if (success)
    _gimp_unit_set_deletion_flag (gimp, unit, deletion_flag);

  return procedural_db_return_args (&unit_set_deletion_flag_proc, success);
}

static ProcArg unit_set_deletion_flag_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  },
  {
    GIMP_PDB_INT32,
    "deletion_flag",
    "The new deletion flag of the unit"
  }
};

static ProcRecord unit_set_deletion_flag_proc =
{
  "gimp_unit_set_deletion_flag",
  "Sets the deletion flag of a unit.",
  "This procedure sets the unit's deletion flag. If the deletion flag of a unit is TRUE on gimp exit, this unit's definition will not be saved in the user's unitrc.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  2,
  unit_set_deletion_flag_inargs,
  0,
  NULL,
  { { unit_set_deletion_flag_invoker } }
};

static Argument *
unit_get_identifier_invoker (Gimp         *gimp,
                             GimpContext  *context,
                             GimpProgress *progress,
                             Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_identifier_proc, success);

  if (success)
    return_args[1].value.pdb_pointer = g_strdup (_gimp_unit_get_identifier (gimp, unit));

  return return_args;
}

static ProcArg unit_get_identifier_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_identifier_outargs[] =
{
  {
    GIMP_PDB_STRING,
    "identifier",
    "The unit's textual identifier"
  }
};

static ProcRecord unit_get_identifier_proc =
{
  "gimp_unit_get_identifier",
  "Returns the textual identifier of the unit.",
  "This procedure returns the textual identifier of the unit. For built-in units it will be the english singular form of the unit's name. For user-defined units this should equal to the singular form.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_identifier_inargs,
  1,
  unit_get_identifier_outargs,
  { { unit_get_identifier_invoker } }
};

static Argument *
unit_get_factor_invoker (Gimp         *gimp,
                         GimpContext  *context,
                         GimpProgress *progress,
                         Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_factor_proc, success);

  if (success)
    return_args[1].value.pdb_float = _gimp_unit_get_factor (gimp, unit);

  return return_args;
}

static ProcArg unit_get_factor_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_factor_outargs[] =
{
  {
    GIMP_PDB_FLOAT,
    "factor",
    "The unit's factor"
  }
};

static ProcRecord unit_get_factor_proc =
{
  "gimp_unit_get_factor",
  "Returns the factor of the unit.",
  "This procedure returns the unit's factor which indicates how many units make up an inch. Note that asking for the factor of \"pixels\" will produce an error.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_factor_inargs,
  1,
  unit_get_factor_outargs,
  { { unit_get_factor_invoker } }
};

static Argument *
unit_get_digits_invoker (Gimp         *gimp,
                         GimpContext  *context,
                         GimpProgress *progress,
                         Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_digits_proc, success);

  if (success)
    return_args[1].value.pdb_int = _gimp_unit_get_digits (gimp, unit);

  return return_args;
}

static ProcArg unit_get_digits_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_digits_outargs[] =
{
  {
    GIMP_PDB_INT32,
    "digits",
    "The unit's number of digits"
  }
};

static ProcRecord unit_get_digits_proc =
{
  "gimp_unit_get_digits",
  "Returns the number of digits of the unit.",
  "This procedure returns the number of digits you should provide in input or output functions to get approximately the same accuracy as with two digits and inches. Note that asking for the digits of \"pixels\" will produce an error.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_digits_inargs,
  1,
  unit_get_digits_outargs,
  { { unit_get_digits_invoker } }
};

static Argument *
unit_get_symbol_invoker (Gimp         *gimp,
                         GimpContext  *context,
                         GimpProgress *progress,
                         Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_symbol_proc, success);

  if (success)
    return_args[1].value.pdb_pointer = g_strdup (_gimp_unit_get_symbol (gimp, unit));

  return return_args;
}

static ProcArg unit_get_symbol_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_symbol_outargs[] =
{
  {
    GIMP_PDB_STRING,
    "symbol",
    "The unit's symbol"
  }
};

static ProcRecord unit_get_symbol_proc =
{
  "gimp_unit_get_symbol",
  "Returns the symbol of the unit.",
  "This procedure returns the symbol of the unit (\"''\" for inches).",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_symbol_inargs,
  1,
  unit_get_symbol_outargs,
  { { unit_get_symbol_invoker } }
};

static Argument *
unit_get_abbreviation_invoker (Gimp         *gimp,
                               GimpContext  *context,
                               GimpProgress *progress,
                               Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_abbreviation_proc, success);

  if (success)
    return_args[1].value.pdb_pointer = g_strdup (_gimp_unit_get_abbreviation (gimp, unit));

  return return_args;
}

static ProcArg unit_get_abbreviation_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_abbreviation_outargs[] =
{
  {
    GIMP_PDB_STRING,
    "abbreviation",
    "The unit's abbreviation"
  }
};

static ProcRecord unit_get_abbreviation_proc =
{
  "gimp_unit_get_abbreviation",
  "Returns the abbreviation of the unit.",
  "This procedure returns the abbreviation of the unit (\"in\" for inches).",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_abbreviation_inargs,
  1,
  unit_get_abbreviation_outargs,
  { { unit_get_abbreviation_invoker } }
};

static Argument *
unit_get_singular_invoker (Gimp         *gimp,
                           GimpContext  *context,
                           GimpProgress *progress,
                           Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_singular_proc, success);

  if (success)
    return_args[1].value.pdb_pointer = g_strdup (_gimp_unit_get_singular (gimp, unit));

  return return_args;
}

static ProcArg unit_get_singular_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_singular_outargs[] =
{
  {
    GIMP_PDB_STRING,
    "singular",
    "The unit's singular form"
  }
};

static ProcRecord unit_get_singular_proc =
{
  "gimp_unit_get_singular",
  "Returns the singular form of the unit.",
  "This procedure returns the singular form of the unit.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_singular_inargs,
  1,
  unit_get_singular_outargs,
  { { unit_get_singular_invoker } }
};

static Argument *
unit_get_plural_invoker (Gimp         *gimp,
                         GimpContext  *context,
                         GimpProgress *progress,
                         Argument     *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpUnit unit;

  unit = args[0].value.pdb_int;
  if (unit < GIMP_UNIT_PIXEL || unit >= _gimp_unit_get_number_of_units (gimp))
    success = FALSE;

  return_args = procedural_db_return_args (&unit_get_plural_proc, success);

  if (success)
    return_args[1].value.pdb_pointer = g_strdup (_gimp_unit_get_plural (gimp, unit));

  return return_args;
}

static ProcArg unit_get_plural_inargs[] =
{
  {
    GIMP_PDB_INT32,
    "unit_id",
    "The unit's integer ID"
  }
};

static ProcArg unit_get_plural_outargs[] =
{
  {
    GIMP_PDB_STRING,
    "plural",
    "The unit's plural form"
  }
};

static ProcRecord unit_get_plural_proc =
{
  "gimp_unit_get_plural",
  "Returns the plural form of the unit.",
  "This procedure returns the plural form of the unit.",
  "Michael Natterer",
  "Michael Natterer",
  "1999",
  NULL,
  GIMP_INTERNAL,
  1,
  unit_get_plural_inargs,
  1,
  unit_get_plural_outargs,
  { { unit_get_plural_invoker } }
};