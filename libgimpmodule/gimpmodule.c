/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpmodule.c
 * (C) 1999 Austin Donnelly <austin@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "gimpmodule.h"

#include "libgimp/libgimp-intl.h"


enum
{
  MODIFIED,
  LAST_SIGNAL
};


static void       gimp_module_class_init     (GimpModuleClass *klass);
static void       gimp_module_init           (GimpModule      *module);

static void       gimp_module_finalize       (GObject         *object);

static gboolean   gimp_module_load           (GTypeModule     *module);
static void       gimp_module_unload         (GTypeModule     *module);

static gboolean   gimp_module_open           (GimpModule      *module);
static gboolean   gimp_module_close          (GimpModule      *module);
static void       gimp_module_set_last_error (GimpModule      *module,
                                              const gchar     *error_str);


static guint module_signals[LAST_SIGNAL];

static GTypeModuleClass *parent_class = NULL;


GType
gimp_module_get_type (void)
{
  static GType module_type = 0;

  if (! module_type)
    {
      static const GTypeInfo module_info =
      {
        sizeof (GimpModuleClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_module_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpModule),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_module_init,
      };

      module_type = g_type_register_static (G_TYPE_TYPE_MODULE,
                                            "GimpModule",
                                            &module_info, 0);
    }

  return module_type;
}

static void
gimp_module_class_init (GimpModuleClass *klass)
{
  GObjectClass     *object_class;
  GTypeModuleClass *module_class;

  object_class = G_OBJECT_CLASS (klass);
  module_class = G_TYPE_MODULE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  module_signals[MODIFIED] =
    g_signal_new ("modified",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpModuleClass, modified),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  object_class->finalize = gimp_module_finalize;

  module_class->load     = gimp_module_load;
  module_class->unload   = gimp_module_unload;

  klass->modified        = NULL;
}

static void
gimp_module_init (GimpModule *module)
{
  module->filename          = NULL;
  module->verbose           = FALSE;
  module->state             = GIMP_MODULE_STATE_ERROR;
  module->on_disk           = FALSE;
  module->load_inhibit      = FALSE;

  module->module            = NULL;
  module->info              = NULL;
  module->last_module_error = NULL;

  module->query_module      = NULL;
  module->register_module   = NULL;
}

static void
gimp_module_finalize (GObject *object)
{
  GimpModule *module;

  module = GIMP_MODULE (object);

  if (module->info)
    {
      gimp_module_info_free (module->info);
      module->info = NULL;
    }

  if (module->last_module_error)
    {
      g_free (module->last_module_error);
      module->last_module_error = NULL;
    }

  if (module->filename)
    {
      g_free (module->filename);
      module->filename = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gimp_module_load (GTypeModule *module)
{
  GimpModule *gimp_module;
  gpointer    func;

  g_return_val_if_fail (GIMP_IS_MODULE (module), FALSE);

  gimp_module = GIMP_MODULE (module);

  g_return_val_if_fail (gimp_module->filename != NULL, FALSE);
  g_return_val_if_fail (gimp_module->module == NULL, FALSE);

  if (gimp_module->verbose)
    g_print (_("Loading module: '%s'\n"),
             gimp_filename_to_utf8 (gimp_module->filename));

  if (! gimp_module_open (gimp_module))
    return FALSE;

  if (! gimp_module_query_module (gimp_module))
    return FALSE;

  /* find the gimp_module_register symbol */
  if (! g_module_symbol (gimp_module->module, "gimp_module_register", &func))
    {
      gimp_module_set_last_error (gimp_module,
                                  "Missing gimp_module_register() symbol");

      if (gimp_module->verbose)
	g_message (_("Module '%s' load error: %s"),
		   gimp_filename_to_utf8 (gimp_module->filename),
                   gimp_module->last_module_error);

      gimp_module_close (gimp_module);

      gimp_module->state = GIMP_MODULE_STATE_ERROR;

      return FALSE;
    }

  gimp_module->register_module = func;

  if (! gimp_module->register_module (module))
    {
      gimp_module_set_last_error (gimp_module,
                                  "gimp_module_register() returned FALSE");

      if (gimp_module->verbose)
	g_message (_("Module '%s' load error: %s"),
		   gimp_filename_to_utf8 (gimp_module->filename),
                   gimp_module->last_module_error);

      gimp_module_close (gimp_module);

      gimp_module->state = GIMP_MODULE_STATE_LOAD_FAILED;

      return FALSE;
    }

  gimp_module->state = GIMP_MODULE_STATE_LOADED;

  return TRUE;
}

static void
gimp_module_unload (GTypeModule *module)
{
  GimpModule *gimp_module;

  g_return_if_fail (GIMP_IS_MODULE (module));

  gimp_module = GIMP_MODULE (module);

  g_return_if_fail (gimp_module->module != NULL);

  gimp_module_close (gimp_module);
}


/*  public functions  */

/**
 * gimp_module_new:
 * @filename:     The filename of a loadable module.
 * @load_inhibit: Pass %TRUE to exclude this module from auto-loading.
 * @verbose:      Pass %TRUE to enable debugging output.
 *
 * Creates a new #GimpModule instance.
 *
 * Return value: The new #GimpModule object.
 **/
GimpModule *
gimp_module_new (const gchar *filename,
                 gboolean     load_inhibit,
                 gboolean     verbose)
{
  GimpModule *module;

  g_return_val_if_fail (filename != NULL, NULL);

  module = g_object_new (GIMP_TYPE_MODULE, NULL);

  module->filename     = g_strdup (filename);
  module->load_inhibit = load_inhibit ? TRUE : FALSE;
  module->verbose      = verbose ? TRUE : FALSE;
  module->on_disk      = TRUE;

  if (! module->load_inhibit)
    {
      if (gimp_module_load (G_TYPE_MODULE (module)))
        gimp_module_unload (G_TYPE_MODULE (module));
    }
  else
    {
      if (verbose)
	{
	  gchar *filename_utf8 = g_filename_to_utf8 (filename,
						     -1, NULL, NULL, NULL);
	  g_print (_("Skipping module: '%s'\n"), filename_utf8);
	  g_free (filename_utf8);
	}

      module->state = GIMP_MODULE_STATE_NOT_LOADED;
    }

  return module;
}

/**
 * gimp_module_query_module:
 * @module: A #GimpModule.
 *
 * Queries the module without actually registering any of the types it
 * may implement. After successful query, the @info field of the
 * #GimpModule struct will be available for further inspection.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gimp_module_query_module (GimpModule *module)
{
  const GimpModuleInfo *info;
  gboolean              close_module = FALSE;
  gpointer              func;

  g_return_val_if_fail (GIMP_IS_MODULE (module), FALSE);

  if (! module->module)
    {
      if (! gimp_module_open (module))
        return FALSE;

      close_module = TRUE;
    }

  /* find the gimp_module_query symbol */
  if (! g_module_symbol (module->module, "gimp_module_query", &func))
    {
      gimp_module_set_last_error (module,
                                  "Missing gimp_module_query() symbol");

      if (module->verbose)
        g_message (_("Module '%s' load error: %s"),
                   gimp_filename_to_utf8 (module->filename),
                   module->last_module_error);

      gimp_module_close (module);

      module->state = GIMP_MODULE_STATE_ERROR;
      return FALSE;
    }

  module->query_module = func;

  if (module->info)
    {
      gimp_module_info_free (module->info);
      module->info = NULL;
    }

  info = module->query_module (G_TYPE_MODULE (module));

  if (! info || info->abi_version != GIMP_MODULE_ABI_VERSION)
    {
      gimp_module_set_last_error (module,
                                  info ?
                                  "module ABI version does not match" :
                                  "gimp_module_query() returned NULL");

      if (module->verbose)
        g_message (_("Module '%s' load error: %s"),
                   gimp_filename_to_utf8 (module->filename),
                   module->last_module_error);

      gimp_module_close (module);

      module->state = GIMP_MODULE_STATE_ERROR;
      return FALSE;
    }

  module->info = gimp_module_info_copy (info);

  if (close_module)
    return gimp_module_close (module);

  return TRUE;
}

/**
 * gimp_module_modified:
 * @module: A #GimpModule.
 *
 * Emits the "modified" signal. Call it whenever you have modified the module
 * manually (which you shouldn't do).
 **/
void
gimp_module_modified (GimpModule *module)
{
  g_return_if_fail (GIMP_IS_MODULE (module));

  g_signal_emit (module, module_signals[MODIFIED], 0);
}

/**
 * gimp_module_set_load_inhibit:
 * @module:       A #GimpModule.
 * @load_inhibit: Pass %TRUE to exclude this module from auto-loading.
 *
 * Sets the @load_inhibit property if the module. Emits "modified".
 **/
void
gimp_module_set_load_inhibit (GimpModule *module,
                              gboolean    load_inhibit)
{
  g_return_if_fail (GIMP_IS_MODULE (module));

  if (load_inhibit != module->load_inhibit)
    {
      module->load_inhibit = load_inhibit ? TRUE : FALSE;

      gimp_module_modified (module);
    }
}

/**
 * gimp_module_state_name:
 * @state: A #GimpModuleState.
 *
 * Returns the translated textual representation of a #GimpModuleState.
 * The returned string must not be freed.
 *
 * Return value: The @state's name.
 **/
const gchar *
gimp_module_state_name (GimpModuleState state)
{
  static const gchar * const statenames[] =
  {
    N_("Module error"),
    N_("Loaded"),
    N_("Load failed"),
    N_("Not loaded")
  };

  g_return_val_if_fail (state >= GIMP_MODULE_STATE_ERROR &&
                        state <= GIMP_MODULE_STATE_NOT_LOADED, NULL);

  return gettext (statenames[state]);
}

/**
 * gimp_module_register_enum:
 * @module:
 * @name:
 * @const_static_values:
 *
 * Registers an enum similar to g_enum_register_static() but for
 * modules. This function should actually live in GLib but since
 * there's no such API, it is provided here.
 *
 * Return value: a new enum #GType
 **/
GType
gimp_module_register_enum (GTypeModule      *module,
                           const gchar	    *name,
                           const GEnumValue *const_static_values)
{
  GTypeInfo enum_type_info = { 0, };

  g_return_val_if_fail (G_IS_TYPE_MODULE (module), 0);
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);

  g_enum_complete_type_info (G_TYPE_ENUM,
                             &enum_type_info, const_static_values);

  return g_type_module_register_type (G_TYPE_MODULE (module),
                                      G_TYPE_ENUM, name, &enum_type_info, 0);
}


/*  private functions  */

static gboolean
gimp_module_open (GimpModule *module)
{
  module->module = g_module_open (module->filename, 0);

  if (! module->module)
    {
      module->state = GIMP_MODULE_STATE_ERROR;
      gimp_module_set_last_error (module, g_module_error ());

      if (module->verbose)
        g_message (_("Module '%s' load error: %s"),
                   gimp_filename_to_utf8 (module->filename),
                   module->last_module_error);

      return FALSE;
    }

  return TRUE;
}

static gboolean
gimp_module_close (GimpModule *module)
{
  g_module_close (module->module); /* FIXME: error handling */
  module->module          = NULL;
  module->query_module    = NULL;
  module->register_module = NULL;

  module->state = GIMP_MODULE_STATE_NOT_LOADED;

  return TRUE;
}

static void
gimp_module_set_last_error (GimpModule  *module,
                            const gchar *error_str)
{
  if (module->last_module_error)
    g_free (module->last_module_error);

  module->last_module_error = g_strdup (error_str);
}


/*  GimpModuleInfo functions  */

/**
 * gimp_module_info_new:
 * @abi_version: The #GIMP_MODULE_ABI_VERSION the module was compiled against.
 * @purpose:     The module's general purpose.
 * @author:      The module's author.
 * @version:     The module's version.
 * @copyright:   The module's copyright.
 * @date:        The module's release date.
 *
 * Creates a newly allocated #GimpModuleInfo struct.
 *
 * Return value: The new #GimpModuleInfo struct.
 **/
GimpModuleInfo *
gimp_module_info_new (guint32      abi_version,
                      const gchar *purpose,
                      const gchar *author,
                      const gchar *version,
                      const gchar *copyright,
                      const gchar *date)
{
  GimpModuleInfo *info;

  info = g_new0 (GimpModuleInfo, 1);

  info->abi_version = abi_version;
  info->purpose     = g_strdup (purpose);
  info->author      = g_strdup (author);
  info->version     = g_strdup (version);
  info->copyright   = g_strdup (copyright);
  info->date        = g_strdup (date);

  return info;
}

/**
 * gimp_module_info_copy:
 * @info: The #GimpModuleInfo struct to copy.
 *
 * Copies a #GimpModuleInfo struct.
 *
 * Return value: The new copy.
 **/
GimpModuleInfo *
gimp_module_info_copy (const GimpModuleInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);

  return gimp_module_info_new (info->abi_version,
                               info->purpose,
                               info->author,
                               info->version,
                               info->copyright,
                               info->date);
}

/**
 * gimp_module_info_free:
 * @info: The #GimpModuleInfo struct to free
 *
 * Frees the passed #GimpModuleInfo.
 **/
void
gimp_module_info_free (GimpModuleInfo *info)
{
  g_return_if_fail (info != NULL);

  g_free (info->purpose);
  g_free (info->author);
  g_free (info->version);
  g_free (info->copyright);
  g_free (info->date);

  g_free (info);
}