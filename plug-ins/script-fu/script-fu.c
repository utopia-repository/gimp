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

#include "config.h"

#include <string.h>

#include <libgimp/gimp.h>

#include "siod/siod.h"

#include "siod-wrapper.h"
#include "script-fu-console.h"
#include "script-fu-scripts.h"
#include "script-fu-server.h"
#include "script-fu-text-console.h"

#include "script-fu-intl.h"


/* Declare local functions. */

static void  script_fu_query          (void);
static void  script_fu_run            (const gchar      *name,
                                       gint              nparams,
                                       const GimpParam  *params,
                                       gint             *nreturn_vals,
                                       GimpParam       **return_vals);
static void  script_fu_extension_init (void);
static void  script_fu_refresh_proc   (const gchar      *name,
                                       gint              nparams,
                                       const GimpParam  *params,
                                       gint             *nreturn_vals,
                                       GimpParam       **return_vals);


const GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,             /* init_proc  */
  NULL,             /* quit_proc  */
  script_fu_query,  /* query_proc */
  script_fu_run     /* run_proc   */
};


MAIN ()


static void
script_fu_query (void)
{
  static const GimpParamDef console_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "Interactive, [non-interactive]" }
  };

  static const GimpParamDef textconsole_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "Interactive, [non-interactive]" }
  };

  static const GimpParamDef eval_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "[Interactive], non-interactive" },
    { GIMP_PDB_STRING, "code",     "The code to evaluate" }
  };

  static const GimpParamDef server_args[] =
  {
    { GIMP_PDB_INT32,  "run_mode", "[Interactive], non-interactive" },
    { GIMP_PDB_INT32,  "port",     "The port on which to listen for requests" },
    { GIMP_PDB_STRING, "logfile",  "The file to log server activity to" }
  };

  gimp_plugin_domain_register (GETTEXT_PACKAGE "-script-fu", NULL);

  gimp_install_procedure ("extension-script-fu",
			  "A scheme interpreter for scripting GIMP operations",
			  "More help here later",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  NULL,
			  NULL,
			  GIMP_EXTENSION,
			  0, 0, NULL, NULL);

  gimp_install_procedure ("plug-in-script-fu-console",
			  "Provides a console mode for script-fu development",
			  "Provides an interface which allows interactive "
                          "scheme development.",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("Script-Fu _Console"),
			  NULL,
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (console_args), 0,
			  console_args, NULL);

  gimp_plugin_menu_register ("plug-in-script-fu-console",
                             N_("<Toolbox>/Xtns/Languages/Script-Fu"));

  gimp_install_procedure ("plug-in-script-fu-text-console",
                          "Provides a text console mode for script-fu "
                          "development",
                          "Provides an interface which allows interactive "
                          "scheme development.",
                          "Spencer Kimball & Peter Mattis",
                          "Spencer Kimball & Peter Mattis",
                          "1997",
			  NULL,
                          NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (textconsole_args), 0,
                          textconsole_args, NULL);

  gimp_install_procedure ("plug-in-script-fu-server",
			  "Provides a server for remote script-fu operation",
			  "Provides a server for remote script-fu operation",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("_Start Server..."),
			  NULL,
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (server_args), 0,
			  server_args, NULL);

  gimp_plugin_menu_register ("plug-in-script-fu-server",
                             N_("<Toolbox>/Xtns/Languages/Script-Fu"));

  gimp_install_procedure ("plug-in-script-fu-eval",
			  "Evaluate scheme code",
			  "Evaluate the code under the scheme interpreter "
                          "(primarily for batch mode)",
			  "Manish Singh",
			  "Manish Singh",
			  "1998",
			  NULL,
			  NULL,
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (eval_args), 0,
			  eval_args, NULL);
}

static void
script_fu_run (const gchar      *name,
	       gint              nparams,
	       const GimpParam  *param,
	       gint             *nreturn_vals,
	       GimpParam       **return_vals)
{
  INIT_I18N();

  siod_set_console_mode (0);
  siod_set_output_file (stdout);

  /*  Determine before we allow scripts to register themselves
   *   whether this is the base, automatically installed script-fu extension
   */
  if (strcmp (name, "extension-script-fu") == 0)
    {
      /*  Setup auxillary temporary procedures for the base extension  */
      script_fu_extension_init ();

      /*  Init the interpreter  */
      siod_init (TRUE);
    }
  else
    {
      /*  Init the interpreter  */
      siod_init (FALSE);
    }

  /*  Load all of the available scripts  */
  script_fu_find_scripts ();

  if (strcmp (name, "extension-script-fu") == 0)
    {
      /*
       *  The main script-fu extension.
       */

      static GimpParam  values[1];

      /*  Acknowledge that the extension is properly initialized  */
      gimp_extension_ack ();

      /*  Go into an endless loop  */
      while (TRUE)
	gimp_extension_process (0);

      /*  Set return values; pointless because we never get out of the loop  */
      *nreturn_vals = 1;
      *return_vals  = values;

      values[0].type          = GIMP_PDB_STATUS;
      values[0].data.d_status = GIMP_PDB_SUCCESS;
    }
  else if (strcmp (name, "plug-in-script-fu-text-console") == 0)
    {
      /*
       *  The script-fu text console for interactive SIOD development
       */

      script_fu_text_console_run (name, nparams, param,
				  nreturn_vals, return_vals);
    }
  else if (strcmp (name, "plug-in-script-fu-console") == 0)
    {
      /*
       *  The script-fu console for interactive SIOD development
       */

      script_fu_console_run (name, nparams, param,
			     nreturn_vals, return_vals);
    }
  else if (strcmp (name, "plug-in-script-fu-server") == 0)
    {
      /*
       *  The script-fu server for remote operation
       */

      script_fu_server_run (name, nparams, param,
			    nreturn_vals, return_vals);
    }
  else if (strcmp (name, "plug-in-script-fu-eval") == 0)
    {
      /*
       *  A non-interactive "console" (for batch mode)
       */

      script_fu_eval_run (name, nparams, param,
			  nreturn_vals, return_vals);
    }
}

static void
script_fu_extension_init (void)
{
  static const GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "[Interactive], non-interactive" }
  };

  gimp_plugin_menu_branch_register ("<Toolbox>/Help", N_("_GIMP Online"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Help", N_("_User Manual"));

  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns/Languages",
                                    N_("_Script-Fu"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Buttons"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Logos"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Misc"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Patterns"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns/Languages/Script-Fu",
                                    N_("_Test"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Utils"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns",
                                    N_("_Web Page Themes"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns/Web Page Themes",
                                    N_("_Alien Glow"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns/Web Page Themes",
                                    N_("_Beveled Pattern"));
  gimp_plugin_menu_branch_register ("<Toolbox>/Xtns/Web Page Themes",
                                    N_("_Classic.Gimp.Org"));

  gimp_plugin_menu_branch_register ("<Image>/Filters",
                                    N_("Alpha to _Logo"));
  gimp_plugin_menu_branch_register ("<Image>/Filters",
                                    N_("_Decor"));
  gimp_plugin_menu_branch_register ("<Image>/Filters",
                                    N_("_Render"));

  gimp_install_temp_proc ("script-fu-refresh",
			  "Re-read all available scripts",
			  "Re-read all available scripts",
			  "Spencer Kimball & Peter Mattis",
			  "Spencer Kimball & Peter Mattis",
			  "1997",
			  N_("_Refresh Scripts"),
			  NULL,
			  GIMP_TEMPORARY,
			  G_N_ELEMENTS (args), 0,
			  args, NULL,
			  script_fu_refresh_proc);

  gimp_plugin_menu_register ("script_fu_refresh",
                             N_("<Toolbox>/Xtns/Languages/Script-Fu"));
}

static void
script_fu_refresh_proc (const gchar      *name,
			gint              nparams,
			const GimpParam  *params,
			gint             *nreturn_vals,
			GimpParam       **return_vals)
{
  static GimpParam  values[1];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /*  Reload all of the available scripts  */
  script_fu_find_scripts ();

  *nreturn_vals = 1;
  *return_vals  = values;

  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}
