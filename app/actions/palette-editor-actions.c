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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimppaletteeditor.h"

#include "palette-editor-actions.h"
#include "palette-editor-commands.h"

#include "gimp-intl.h"


static GimpActionEntry palette_editor_actions[] =
{
  { "palette-editor-popup", GIMP_STOCK_PALETTE,
    N_("Palette Editor Menu"), NULL, NULL, NULL,
    GIMP_HELP_PALETTE_EDITOR_DIALOG },

  { "palette-editor-edit-color", GIMP_STOCK_EDIT,
    N_("_Edit Color..."), "",
    N_("Edit color"),
    G_CALLBACK (palette_editor_edit_color_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_EDIT },

  { "palette-editor-delete-color", GTK_STOCK_DELETE,
    N_("_Delete Color"), "",
    N_("Delete color"),
    G_CALLBACK (palette_editor_delete_color_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_DELETE }
};

static GimpEnumActionEntry palette_editor_new_actions[] =
{
  { "palette-editor-new-color-fg", GTK_STOCK_NEW,
    N_("New Color from _FG"), "",
    N_("New color from FG"),
    FALSE, FALSE,
    GIMP_HELP_PALETTE_EDITOR_NEW },

  { "palette-editor-new-color-bg", GTK_STOCK_NEW,
    N_("New Color from _BG"), "",
    N_("New color from BG"),
    TRUE, FALSE,
    GIMP_HELP_PALETTE_EDITOR_NEW }
};

static GimpEnumActionEntry palette_editor_zoom_actions[] =
{
  { "palette-editor-zoom-in", GTK_STOCK_ZOOM_IN,
    N_("Zoom _In"), "",
    N_("Zoom in"),
    GIMP_ZOOM_IN, FALSE,
    GIMP_HELP_PALETTE_EDITOR_ZOOM_IN },

  { "palette-editor-zoom-out", GTK_STOCK_ZOOM_OUT,
    N_("Zoom _Out"), "",
    N_("Zoom out"),
    GIMP_ZOOM_OUT, FALSE,
    GIMP_HELP_PALETTE_EDITOR_ZOOM_OUT },

  { "palette-editor-zoom-all", GTK_STOCK_ZOOM_FIT,
    N_("Zoom _All"), "",
    N_("Zoom all"),
    GIMP_ZOOM_TO /* abused */, FALSE,
    GIMP_HELP_PALETTE_EDITOR_ZOOM_ALL }
};


void
palette_editor_actions_setup (GimpActionGroup *group)
{
  gimp_action_group_add_actions (group,
                                 palette_editor_actions,
                                 G_N_ELEMENTS (palette_editor_actions));

  gimp_action_group_add_enum_actions (group,
                                      palette_editor_new_actions,
                                      G_N_ELEMENTS (palette_editor_new_actions),
                                      G_CALLBACK (palette_editor_new_color_cmd_callback));

  gimp_action_group_add_enum_actions (group,
                                      palette_editor_zoom_actions,
                                      G_N_ELEMENTS (palette_editor_zoom_actions),
                                      G_CALLBACK (palette_editor_zoom_cmd_callback));
}

void
palette_editor_actions_update (GimpActionGroup *group,
                               gpointer         user_data)
{
  GimpPaletteEditor *editor      = GIMP_PALETTE_EDITOR (user_data);
  GimpDataEditor    *data_editor = GIMP_DATA_EDITOR (user_data);
  GimpContext       *context;
  GimpData          *data;
  gboolean           editable    = FALSE;
  GimpRGB            fg;
  GimpRGB            bg;

  context = gimp_get_user_context (group->gimp);

  data = data_editor->data;

  if (data)
    {
      if (data_editor->data_editable)
        editable = TRUE;
    }

  if (context)
    {
      gimp_context_get_foreground (context, &fg);
      gimp_context_get_background (context, &bg);
    }

#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)
#define SET_COLOR(action,color) \
        gimp_action_group_set_action_color (group, action, color, FALSE);

  SET_SENSITIVE ("palette-editor-edit-color",   editable && editor->color);
  SET_SENSITIVE ("palette-editor-delete-color", editable && editor->color);

  SET_SENSITIVE ("palette-editor-new-color-fg", editable);
  SET_SENSITIVE ("palette-editor-new-color-bg", editable);

  SET_COLOR ("palette-editor-new-color-fg", context ? &fg : NULL);
  SET_COLOR ("palette-editor-new-color-bg", context ? &bg : NULL);

  SET_SENSITIVE ("palette-editor-zoom-out", data);
  SET_SENSITIVE ("palette-editor-zoom-in",  data);
  SET_SENSITIVE ("palette-editor-zoom-all", data);

#undef SET_SENSITIVE
#undef SET_COLOR
}