/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
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

#include <glib.h>  /* lcms.h uses the "inline" keyword */

#ifdef HAVE_LCMS_LCMS_H
#include <lcms/lcms.h>
#else
#include <lcms.h>
#endif

#include <gtk/gtk.h>

#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpmodule/gimpmodule.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "libgimp/libgimp-intl.h"


#define CDISPLAY_TYPE_LCMS            (cdisplay_lcms_type)
#define CDISPLAY_LCMS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CDISPLAY_TYPE_LCMS, CdisplayLcms))
#define CDISPLAY_LCMS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CDISPLAY_TYPE_LCMS, CdisplayLcmsClass))
#define CDISPLAY_IS_LCMS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CDISPLAY_TYPE_LCMS))
#define CDISPLAY_IS_LCMS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CDISPLAY_TYPE_LCMS))


typedef struct _CdisplayLcms CdisplayLcms;
typedef struct _CdisplayLcmsClass CdisplayLcmsClass;

struct _CdisplayLcms
{
  GimpColorDisplay  parent_instance;

  GimpColorConfig  *config;
  cmsHTRANSFORM     transform;
};

struct _CdisplayLcmsClass
{
  GimpColorDisplayClass parent_instance;
};


enum
{
  PROP_0,
  PROP_CONFIG
};


static GType     cdisplay_lcms_get_type     (GTypeModule       *module);
static void      cdisplay_lcms_class_init   (CdisplayLcmsClass *klass);
static void      cdisplay_lcms_init         (CdisplayLcms      *lcms);
static void      cdisplay_lcms_dispose      (GObject           *object);
static void      cdisplay_lcms_get_property (GObject           *object,
                                             guint              property_id,
                                             GValue            *value,
                                             GParamSpec        *pspec);
static void      cdisplay_lcms_set_property (GObject           *object,
                                             guint              property_id,
                                             const GValue      *value,
                                             GParamSpec        *pspec);

static GtkWidget * cdisplay_lcms_configure  (GimpColorDisplay  *display);
static void        cdisplay_lcms_convert    (GimpColorDisplay  *display,
                                             guchar            *buf,
                                             gint               width,
                                             gint               height,
                                             gint               bpp,
                                             gint               bpl);
static void        cdisplay_lcms_changed    (GimpColorDisplay  *display);
static void        cdisplay_lcms_set_config (CdisplayLcms      *lcms,
                                             GimpColorConfig   *config);

static cmsHPROFILE  cdisplay_lcms_get_rgb_profile     (CdisplayLcms *lcms);
static cmsHPROFILE  cdisplay_lcms_get_display_profile (CdisplayLcms *lcms);
static cmsHPROFILE  cdisplay_lcms_get_printer_profile (CdisplayLcms *lcms);

static void         cdisplay_lcms_attach_labelled (GtkTable    *table,
                                                   gint         row,
                                                   const gchar *text,
                                                   GtkWidget   *widget,
                                                   gboolean     tooltip);
static void         cdisplay_lcms_label_set_text  (GtkLabel    *label,
                                                   const gchar *text,
                                                   const gchar *tooltip);
static void         cdisplay_lcms_update_profile_label (CdisplayLcms *lcms,
                                                        const gchar  *name);
static void         cdisplay_lcms_notify_profile       (GObject      *config,
                                                        GParamSpec   *pspec,
                                                        CdisplayLcms *lcms);


static const GimpModuleInfo cdisplay_lcms_info =
{
  GIMP_MODULE_ABI_VERSION,
  N_("Color management display filter using ICC color profiles"),
  "Sven Neumann",
  "v0.1",
  "(c) 2005, released under the GPL",
  "2005"
};

static GType                  cdisplay_lcms_type = 0;
static GimpColorDisplayClass *parent_class       = NULL;


G_MODULE_EXPORT const GimpModuleInfo *
gimp_module_query (GTypeModule *module)
{
  return &cdisplay_lcms_info;
}

G_MODULE_EXPORT gboolean
gimp_module_register (GTypeModule *module)
{
  cdisplay_lcms_get_type (module);

  return TRUE;
}

static GType
cdisplay_lcms_get_type (GTypeModule *module)
{
  if (! cdisplay_lcms_type)
    {
      const GTypeInfo display_info =
      {
        sizeof (CdisplayLcmsClass),
        (GBaseInitFunc)     NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) cdisplay_lcms_class_init,
        NULL,                   /* class_finalize */
        NULL,                   /* class_data     */
        sizeof (CdisplayLcms),
        0,                      /* n_preallocs    */
        (GInstanceInitFunc) cdisplay_lcms_init,
      };

       cdisplay_lcms_type =
        g_type_module_register_type (module, GIMP_TYPE_COLOR_DISPLAY,
                                     "CdisplayLcms", &display_info, 0);
    }

  return cdisplay_lcms_type;
}

static void
cdisplay_lcms_class_init (CdisplayLcmsClass *klass)
{
  GObjectClass          *object_class  = G_OBJECT_CLASS (klass);
  GimpColorDisplayClass *display_class = GIMP_COLOR_DISPLAY_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose      = cdisplay_lcms_dispose;
  object_class->get_property = cdisplay_lcms_get_property;
  object_class->set_property = cdisplay_lcms_set_property;

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_CONFIG,
                                   "config", NULL,
                                   GIMP_TYPE_COLOR_CONFIG,
                                   0);

  display_class->name        = _("Color Management");
  display_class->help_id     = "gimp-colordisplay-lcms";
  display_class->stock_id    = GIMP_STOCK_DISPLAY_FILTER_LCMS;

  display_class->configure   = cdisplay_lcms_configure;
  display_class->convert     = cdisplay_lcms_convert;
  display_class->changed     = cdisplay_lcms_changed;

  cmsErrorAction (LCMS_ERROR_IGNORE);
}

static void
cdisplay_lcms_init (CdisplayLcms *lcms)
{
  lcms->config    = NULL;
  lcms->transform = NULL;
}

static void
cdisplay_lcms_dispose (GObject *object)
{
  CdisplayLcms *lcms = CDISPLAY_LCMS (object);

  cdisplay_lcms_set_config (lcms, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cdisplay_lcms_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CdisplayLcms *lcms = CDISPLAY_LCMS (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      g_value_set_object (value, lcms->config);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cdisplay_lcms_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CdisplayLcms *lcms = CDISPLAY_LCMS (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      cdisplay_lcms_set_config (lcms, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cdisplay_lcms_profile_get_info (cmsHPROFILE   profile,
                                const gchar **name,
                                const gchar **info)
{
  if (profile)
    {
      *name = cmsTakeProductName (profile);
      if (! g_utf8_validate (*name, -1, NULL))
        *name = _("(invalid UTF-8 string)");

      *info = cmsTakeProductInfo (profile);
      if (! g_utf8_validate (*info, -1, NULL))
        *info = NULL;
    }
  else
    {
      *name = _("None");
      *info = NULL;
    }
}

static GtkWidget *
cdisplay_lcms_configure (GimpColorDisplay *display)
{
  CdisplayLcms *lcms   = CDISPLAY_LCMS (display);
  GObject      *config = G_OBJECT (lcms->config);
  GtkWidget    *vbox;
  GtkWidget    *hint;
  GtkWidget    *table;
  GtkWidget    *label;
  gint          row = 0;

  if (! config)
    return NULL;

  vbox = gtk_vbox_new (FALSE, 12);

  hint = gimp_hint_box_new (_("This filter takes its configuration "
                              "from the Color Management section "
                              "in the Preferences dialog."));
  gtk_box_pack_start (GTK_BOX (vbox), hint, FALSE, FALSE, 0);
  gtk_widget_show (hint);

  table = gtk_table_new (5, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  cdisplay_lcms_attach_labelled (GTK_TABLE (table), row++,
                                 _("Mode of operation:"),
                                 gimp_prop_enum_label_new (config, "mode"),
                                 FALSE);

  label = gtk_label_new (NULL);
  g_object_set_data (G_OBJECT (lcms), "rgb-profile", label);
  cdisplay_lcms_attach_labelled (GTK_TABLE (table), row++,
                                 _("RGB working space profile:"),
                                 label, TRUE);
  cdisplay_lcms_update_profile_label (lcms, "rgb-profile");

  label = gtk_label_new (NULL);
  g_object_set_data (G_OBJECT (lcms), "display-profile", label);
  cdisplay_lcms_attach_labelled (GTK_TABLE (table), row++,
                                 _("Monitor profile:"),
                                 label, TRUE);
  cdisplay_lcms_update_profile_label (lcms, "display-profile");

  label = gtk_label_new (NULL);
  g_object_set_data (G_OBJECT (lcms), "printer-profile", label);
  cdisplay_lcms_attach_labelled (GTK_TABLE (table), row++,
                                 _("Print simulation profile:"),
                                 label, TRUE);
  cdisplay_lcms_update_profile_label (lcms, "printer-profile");

  g_signal_connect_object (config, "notify",
                           G_CALLBACK (cdisplay_lcms_notify_profile),
                           lcms, 0);

  return vbox;
}

static void
cdisplay_lcms_convert (GimpColorDisplay *display,
                       guchar           *buf,
                       gint              width,
                       gint              height,
                       gint              bpp,
                       gint              bpl)
{
  CdisplayLcms *lcms = CDISPLAY_LCMS (display);
  gint          y;

  if (bpp != 3)
    return;

  if (! lcms->transform)
    return;

  for (y = 0; y < height; y++, buf += bpl)
    cmsDoTransform (lcms->transform, buf, buf, width);
}

static void
cdisplay_lcms_changed (GimpColorDisplay *display)
{
  CdisplayLcms    *lcms   = CDISPLAY_LCMS (display);
  GimpColorConfig *config = lcms->config;

  cmsHPROFILE      src_profile   = NULL;
  cmsHPROFILE      dest_profile  = NULL;
  cmsHPROFILE      proof_profile = NULL;

  if (lcms->transform)
    {
      cmsDeleteTransform (lcms->transform);
      lcms->transform = NULL;
    }

  if (! config)
    return;

  switch (config->mode)
    {
    case GIMP_COLOR_MANAGEMENT_OFF:
      return;

    case GIMP_COLOR_MANAGEMENT_SOFTPROOF:
      proof_profile = cdisplay_lcms_get_printer_profile (lcms);

      /*  fallthru  */

    case GIMP_COLOR_MANAGEMENT_DISPLAY:
      src_profile = cdisplay_lcms_get_rgb_profile (lcms);
      dest_profile = cdisplay_lcms_get_display_profile (lcms);
      break;
    }

  if (proof_profile)
    {
      lcms->transform = cmsCreateProofingTransform (src_profile, TYPE_RGB_8,
                                                    (dest_profile ?
                                                     dest_profile :
                                                     src_profile), TYPE_RGB_8,
                                                    proof_profile,
                                                    config->simulation_intent,
                                                    config->display_intent,
                                                    cmsFLAGS_SOFTPROOFING);
      cmsCloseProfile (proof_profile);
    }
  else if (dest_profile)
    {
      lcms->transform = cmsCreateTransform (src_profile,  TYPE_RGB_8,
                                            dest_profile, TYPE_RGB_8,
                                            config->display_intent,
                                            0);
    }

  if (dest_profile)
    cmsCloseProfile (dest_profile);

  cmsCloseProfile (src_profile);
}

static void
cdisplay_lcms_set_config (CdisplayLcms    *lcms,
                          GimpColorConfig *config)
{
  if (config == lcms->config)
    return;

  if (lcms->config)
    {
      g_signal_handlers_disconnect_by_func (lcms->config,
                                            G_CALLBACK (gimp_color_display_changed),
                                            lcms);
      g_object_unref (lcms->config);
    }

  lcms->config = config;

  if (lcms->config)
    {
      g_object_ref (lcms->config);
      g_signal_connect_swapped (lcms->config, "notify",
                                G_CALLBACK (gimp_color_display_changed),
                                lcms);
    }

  gimp_color_display_changed (GIMP_COLOR_DISPLAY (lcms));
}

static cmsHPROFILE
cdisplay_lcms_get_rgb_profile (CdisplayLcms *lcms)
{
  GimpColorConfig *config  = lcms->config;
  cmsHPROFILE      profile = NULL;

  /*  this should be taken from the image  */

  if (config->rgb_profile)
    profile = cmsOpenProfileFromFile (config->rgb_profile, "r");

  return profile ? profile : cmsCreate_sRGBProfile ();
}

static cmsHPROFILE
cdisplay_lcms_get_display_profile (CdisplayLcms *lcms)
{
  GimpColorConfig *config = lcms->config;

#if defined (GDK_WINDOWING_X11)
  if (config->display_profile_from_gdk)
    {
      /*  FIXME: need to access the display's screen here  */
      GdkScreen *screen = gdk_screen_get_default ();
      GdkAtom    type   = GDK_NONE;
      gint       format = 0;
      gint       nitems = 0;
      guchar    *data   = NULL;

      g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

      if (gdk_property_get (gdk_screen_get_root_window (screen),
                            gdk_atom_intern ("_ICC_PROFILE", FALSE),
                            GDK_NONE,
                            0, 64 * 1024 * 1024, FALSE,
                            &type, &format, &nitems, &data) && nitems > 0)
        {
          cmsHPROFILE  profile = cmsOpenProfileFromMem (data, nitems);

          g_free (data);

          return profile;
        }
    }
#endif

  if (config->display_profile)
    return cmsOpenProfileFromFile (config->display_profile, "r");

  return NULL;
}

static cmsHPROFILE
cdisplay_lcms_get_printer_profile (CdisplayLcms *lcms)
{
  GimpColorConfig *config = lcms->config;

  if (config->printer_profile)
    return cmsOpenProfileFromFile (config->printer_profile, "r");

  return NULL;
}

static void
cdisplay_lcms_attach_labelled (GtkTable    *table,
                               gint         row,
                               const gchar *text,
                               GtkWidget   *widget,
                               gboolean     tooltip)
{
  GtkWidget *label;
  GtkWidget *ebox = NULL;
  GtkWidget *hbox;

  label = g_object_new (GTK_TYPE_LABEL,
                        "label",  text,
                        "xalign", 1.0,
                        "yalign", 0.5,
                        NULL);

  gimp_label_set_attributes (GTK_LABEL (label),
                             PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD,
                             -1);
  gtk_table_attach (table, label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  if (tooltip)
    {
      ebox = gtk_event_box_new ();
      gtk_table_attach (table, ebox, 1, 2, row, row + 1,
                        GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
      gtk_widget_show (ebox);

      g_object_set_data (G_OBJECT (label), "tooltip-widget", ebox);
    }

  hbox = gtk_hbox_new (FALSE, 0);

  if (ebox)
    gtk_container_add (GTK_CONTAINER (ebox), hbox);
  else
    gtk_table_attach (table, hbox, 1, 2, row, row + 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

static void
cdisplay_lcms_label_set_text (GtkLabel    *label,
                              const gchar *text,
                              const gchar *tooltip)
{
  GtkWidget *tooltip_widget;

  gtk_label_set_text (label, text);

  tooltip_widget = g_object_get_data (G_OBJECT (label), "tooltip-widget");

  if (tooltip_widget)
    gimp_help_set_help_data (tooltip_widget, tooltip, NULL);
}

static void
cdisplay_lcms_update_profile_label (CdisplayLcms *lcms,
                                    const gchar  *name)
{
  GtkWidget   *label;
  cmsHPROFILE  profile = NULL;
  const gchar *text;
  const gchar *tooltip;

  label = g_object_get_data (G_OBJECT (lcms), name);

  if (! label)
    return;

  if (strcmp (name, "rgb-profile") == 0)
    {
      profile = cdisplay_lcms_get_rgb_profile (lcms);
    }
  else if (g_str_has_prefix (name, "display-profile"))
    {
      profile = cdisplay_lcms_get_display_profile (lcms);
    }
  else if (strcmp (name, "printer-profile") == 0)
    {
      profile = cdisplay_lcms_get_printer_profile (lcms);
    }
  else
    {
      g_return_if_reached ();
    }

  cdisplay_lcms_profile_get_info (profile, &text, &tooltip);
  cdisplay_lcms_label_set_text (GTK_LABEL (label), text, tooltip);

  if (profile)
    cmsCloseProfile (profile);
}

static void
cdisplay_lcms_notify_profile (GObject      *config,
                              GParamSpec   *pspec,
                              CdisplayLcms *lcms)
{
  cdisplay_lcms_update_profile_label (lcms, pspec->name);
}
