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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpimage.h"
#include "core/gimpselection.h"
#include "core/gimpstrokedesc.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpdialogfactory.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"

#include "dialogs/dialogs.h"
#include "dialogs/stroke-dialog.h"

#include "actions.h"
#include "select-commands.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   select_feather_callback (GtkWidget *widget,
                                       gdouble    size,
                                       GimpUnit   unit,
                                       gpointer   data);
static void   select_border_callback  (GtkWidget *widget,
                                       gdouble    size,
                                       GimpUnit   unit,
                                       gpointer   data);
static void   select_grow_callback    (GtkWidget *widget,
                                       gdouble    size,
                                       GimpUnit   unit,
                                       gpointer   data);
static void   select_shrink_callback  (GtkWidget *widget,
                                       gdouble    size,
                                       GimpUnit   unit,
                                       gpointer   data);


/*  private variables  */

static gdouble   select_feather_radius    = 5.0;
static gint      select_border_radius     = 5;
static gint      select_grow_pixels       = 1;
static gint      select_shrink_pixels     = 1;
static gboolean  select_shrink_edge_lock  = FALSE;


/*  public functions  */

void
select_invert_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpImage *gimage;
  return_if_no_image (gimage, data);

  gimp_channel_invert (gimp_image_get_mask (gimage), TRUE);
  gimp_image_flush (gimage);
}

void
select_all_cmd_callback (GtkAction *action,
                         gpointer   data)
{
  GimpImage *gimage;
  return_if_no_image (gimage, data);

  gimp_channel_all (gimp_image_get_mask (gimage), TRUE);
  gimp_image_flush (gimage);
}

void
select_none_cmd_callback (GtkAction *action,
                          gpointer   data)
{
  GimpImage *gimage;
  return_if_no_image (gimage, data);

  gimp_channel_clear (gimp_image_get_mask (gimage), NULL, TRUE);
  gimp_image_flush (gimage);
}

void
select_float_cmd_callback (GtkAction *action,
                           gpointer   data)
{
  GimpImage *gimage;
  return_if_no_image (gimage, data);

  gimp_selection_float (gimp_image_get_mask (gimage),
                        gimp_image_active_drawable (gimage),
                        action_data_get_context (data),
                        TRUE, 0, 0);
  gimp_image_flush (gimage);
}

void
select_feather_cmd_callback (GtkAction *action,
                             gpointer   data)
{
  GimpDisplay *gdisp;
  GtkWidget   *dialog;
  return_if_no_display (gdisp, data);

  dialog = gimp_query_size_box (_("Feather Selection"),
                                gdisp->shell,
                                gimp_standard_help_func,
                                GIMP_HELP_SELECTION_FEATHER,
                                _("Feather selection by"),
                                select_feather_radius, 0, 32767, 3,
                                GIMP_DISPLAY_SHELL (gdisp->shell)->unit,
                                MIN (gdisp->gimage->xresolution,
                                     gdisp->gimage->yresolution),
                                FALSE,
                                G_OBJECT (gdisp->gimage), "disconnect",
                                select_feather_callback, gdisp->gimage);
  gtk_widget_show (dialog);
}

void
select_sharpen_cmd_callback (GtkAction *action,
                             gpointer   data)
{
  GimpImage *gimage;
  return_if_no_image (gimage, data);

  gimp_channel_sharpen (gimp_image_get_mask (gimage), TRUE);
  gimp_image_flush (gimage);
}

void
select_shrink_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpDisplay *gdisp;
  GtkWidget   *dialog;
  GtkWidget   *edge_lock;
  return_if_no_display (gdisp, data);

  dialog = gimp_query_size_box (_("Shrink Selection"),
                                gdisp->shell,
                                gimp_standard_help_func,
                                GIMP_HELP_SELECTION_SHRINK,
                                _("Shrink selection by"),
                                select_shrink_pixels, 1, 32767, 0,
                                GIMP_DISPLAY_SHELL (gdisp->shell)->unit,
                                MIN (gdisp->gimage->xresolution,
                                     gdisp->gimage->yresolution),
                                FALSE,
                                G_OBJECT (gdisp->gimage), "disconnect",
                                select_shrink_callback, gdisp->gimage);

  edge_lock = gtk_check_button_new_with_label (_("Shrink from image border"));

  gtk_box_pack_start (GTK_BOX (GIMP_QUERY_BOX_VBOX (dialog)), edge_lock,
                      FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (dialog), "edge_lock_toggle", edge_lock);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edge_lock),
                                ! select_shrink_edge_lock);
  gtk_widget_show (edge_lock);

  gtk_widget_show (dialog);
}

void
select_grow_cmd_callback (GtkAction *action,
                          gpointer   data)
{
  GimpDisplay *gdisp;
  GtkWidget   *dialog;
  return_if_no_display (gdisp, data);

  dialog = gimp_query_size_box (_("Grow Selection"),
                                gdisp->shell,
                                gimp_standard_help_func,
                                GIMP_HELP_SELECTION_GROW,
                                _("Grow selection by"),
                                select_grow_pixels, 1, 32767, 0,
                                GIMP_DISPLAY_SHELL (gdisp->shell)->unit,
                                MIN (gdisp->gimage->xresolution,
                                     gdisp->gimage->yresolution),
                                FALSE,
                                G_OBJECT (gdisp->gimage), "disconnect",
                                select_grow_callback, gdisp->gimage);
  gtk_widget_show (dialog);
}

void
select_border_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpDisplay *gdisp;
  GtkWidget   *dialog;
  return_if_no_display (gdisp, data);

  dialog = gimp_query_size_box (_("Border Selection"),
                                gdisp->shell,
                                gimp_standard_help_func,
                                GIMP_HELP_SELECTION_BORDER,
                                _("Border selection by"),
                                select_border_radius, 1, 32767, 0,
                                GIMP_DISPLAY_SHELL (gdisp->shell)->unit,
                                MIN (gdisp->gimage->xresolution,
                                     gdisp->gimage->yresolution),
                                FALSE,
                                G_OBJECT (gdisp->gimage), "disconnect",
                                select_border_callback, gdisp->gimage);
  gtk_widget_show (dialog);
}

void
select_save_cmd_callback (GtkAction *action,
                          gpointer   data)
{
  GimpImage *gimage;
  GtkWidget *widget;
  return_if_no_image (gimage, data);
  return_if_no_widget (widget, data);

  gimp_selection_save (gimp_image_get_mask (gimage));
  gimp_image_flush (gimage);

  gimp_dialog_factory_dialog_raise (global_dock_factory,
                                    gtk_widget_get_screen (widget),
                                    "gimp-channel-list", -1);
}

void
select_stroke_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpImage    *gimage;
  GimpDrawable *drawable;
  GtkWidget    *widget;
  GtkWidget    *dialog;
  return_if_no_image (gimage, data);
  return_if_no_widget (widget, data);

  drawable = gimp_image_active_drawable (gimage);

  if (! drawable)
    {
      g_message (_("There is no active layer or channel to stroke to."));
      return;
    }

  dialog = stroke_dialog_new (GIMP_ITEM (gimp_image_get_mask (gimage)),
                              _("Stroke Selection"),
                              GIMP_STOCK_SELECTION_STROKE,
                              GIMP_HELP_SELECTION_STROKE,
                              widget);
  gtk_widget_show (dialog);
}

void
select_stroke_last_vals_cmd_callback (GtkAction *action,
                                      gpointer   data)
{
  GimpImage      *image;
  GimpDrawable   *drawable;
  GimpContext    *context;
  GimpStrokeDesc *desc;
  return_if_no_image (image, data);

  drawable = gimp_image_active_drawable (image);

  if (! drawable)
    {
      g_message (_("There is no active layer or channel to stroke to."));
      return;
    }

  context = gimp_get_user_context (image->gimp);

  desc = g_object_get_data (G_OBJECT (context), "saved-stroke-desc");

  if (desc)
    g_object_ref (desc);
  else
    desc = gimp_stroke_desc_new (image->gimp, context);

  gimp_item_stroke (GIMP_ITEM (gimp_image_get_mask (image)),
                    drawable, context, desc, FALSE);

  g_object_unref (desc);

  gimp_image_flush (image);
}


/*  private functions  */

static void
select_feather_callback (GtkWidget *widget,
                         gdouble    size,
                         GimpUnit   unit,
                         gpointer   data)
{
  GimpImage *gimage = GIMP_IMAGE (data);
  gdouble    radius_x;
  gdouble    radius_y;

  select_feather_radius = size;

  radius_x = radius_y = select_feather_radius;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
                MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
        radius_y *= factor;
      else
        radius_x *= factor;
    }

  gimp_channel_feather (gimp_image_get_mask (gimage), radius_x, radius_y, TRUE);
  gimp_image_flush (gimage);
}

static void
select_border_callback (GtkWidget *widget,
                        gdouble    size,
                        GimpUnit   unit,
                        gpointer   data)
{
  GimpImage *gimage = GIMP_IMAGE (data);
  gdouble    radius_x;
  gdouble    radius_y;

  select_border_radius = ROUND (size);

  radius_x = radius_y = select_border_radius;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
                MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
        radius_y *= factor;
      else
        radius_x *= factor;
    }

  gimp_channel_border (gimp_image_get_mask (gimage), radius_x, radius_y, TRUE);
  gimp_image_flush (gimage);
}

static void
select_grow_callback (GtkWidget *widget,
                      gdouble    size,
                      GimpUnit   unit,
                      gpointer   data)
{
  GimpImage *gimage = GIMP_IMAGE (data);
  gdouble    radius_x;
  gdouble    radius_y;

  select_grow_pixels = ROUND (size);

  radius_x = radius_y = select_grow_pixels;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
                MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
        radius_y *= factor;
      else
        radius_x *= factor;
    }

  gimp_channel_grow (gimp_image_get_mask (gimage), radius_x, radius_y, TRUE);
  gimp_image_flush (gimage);
}

static void
select_shrink_callback (GtkWidget *widget,
                        gdouble    size,
                        GimpUnit   unit,
                        gpointer   data)
{
  GimpImage *gimage = GIMP_IMAGE (data);
  gint       radius_x;
  gint       radius_y;

  select_shrink_pixels = ROUND (size);

  radius_x = radius_y = select_shrink_pixels;

  select_shrink_edge_lock =
    ! GTK_TOGGLE_BUTTON (g_object_get_data (G_OBJECT (widget),
                                            "edge_lock_toggle"))->active;

  if (unit != GIMP_UNIT_PIXEL)
    {
      gdouble factor;

      factor = (MAX (gimage->xresolution, gimage->yresolution) /
                MIN (gimage->xresolution, gimage->yresolution));

      if (gimage->xresolution == MIN (gimage->xresolution, gimage->yresolution))
        radius_y *= factor;
      else
        radius_x *= factor;
    }

  gimp_channel_shrink (gimp_image_get_mask (gimage), radius_x, radius_y,
                       select_shrink_edge_lock, TRUE);
  gimp_image_flush (gimage);
}