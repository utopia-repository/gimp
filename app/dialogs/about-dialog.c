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
#include <time.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "dialogs-types.h"

#include "core/gimpcontext.h"

#include "pdb/procedural_db.h"

#include "about.h"

#include "about-dialog.h"
#include "authors.h"

#include "gimp-intl.h"


#define PDB_URL_LOAD   "plug-in-web-browser"


typedef struct
{
  GtkWidget    *dialog;

  GtkWidget    *logo_area;
  GdkPixmap    *logo_pixmap;
  gint          logo_width;
  gint          logo_height;

  GdkRectangle  text_area;
  gdouble       text_size;
  gdouble       min_text_size;
  PangoLayout  *layout;
  PangoFontDescription *font_desc;

  gint          n_authors;
  gint          shuffle[G_N_ELEMENTS (authors) - 1];  /* NULL terminated */

  guint         timer;

  gint          index;
  gint          animstep;
  gint          textrange[2];
  gint          state;
  gboolean      visible;
  gboolean      pp;
} GimpAboutDialog;


static void      about_dialog_map         (GtkWidget       *widget,
                                           GimpAboutDialog *dialog);
static void      about_dialog_unmap       (GtkWidget       *widget,
                                           GimpAboutDialog *dialog);
static void      about_dialog_load_url    (GtkAboutDialog  *dialog,
                                           const gchar     *url,
                                           gpointer         data);
static void      about_dialog_add_logo    (GtkWidget       *vbox,
                                           GimpAboutDialog *dialog);
static gboolean  about_dialog_logo_expose (GtkWidget       *widget,
                                           GdkEventExpose  *event,
                                           GimpAboutDialog *dialog);
static gboolean  about_dialog_load_logo   (GtkWidget       *widget,
                                           GimpAboutDialog *dialog);
static void      about_dialog_reshuffle   (GimpAboutDialog *dialog);
static gboolean  about_dialog_timer       (gpointer         data);


GtkWidget *
about_dialog_create (GimpContext *context)
{
  static GimpAboutDialog *dialog = NULL;

  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);

  if (! dialog)
    {
      GtkWidget       *widget;
      GtkWidget       *container;
      GList           *children;
      GdkModifierType  mask;

      if (procedural_db_lookup (context->gimp, PDB_URL_LOAD))
        gtk_about_dialog_set_url_hook (about_dialog_load_url,
                                       g_object_ref (context),
                                       (GDestroyNotify) g_object_unref);

      dialog = g_new0 (GimpAboutDialog, 1);

      dialog->n_authors = G_N_ELEMENTS (authors) - 1;

      widget = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                             "role",               "about-dialog",
                             "window-position",    GTK_WIN_POS_CENTER,
                             "name",               GIMP_ACRONYM,
                             "version",            GIMP_VERSION,
                             "copyright",          GIMP_COPYRIGHT,
                             "comments",           GIMP_NAME,
                             "license",            GIMP_LICENSE,
                             "wrap-license",       TRUE,
                             "website",            "http://www.gimp.org/",
                             "website-label",      _("Visit the GIMP website"),
                             "authors",            authors,
                             "artists",            artists,
                             "documenters",        documenters,
                             /* Translators: insert your names here,
                              * separated by newline
                              */
                             "translator-credits", _("translator-credits"),
                             NULL);

      dialog->dialog = widget;

      g_object_add_weak_pointer (G_OBJECT (widget), (gpointer) &dialog);

      g_signal_connect (widget, "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      g_signal_connect (widget, "map",
                        G_CALLBACK (about_dialog_map),
                        dialog);
      g_signal_connect (widget, "unmap",
                        G_CALLBACK (about_dialog_unmap),
                        dialog);

      if (! about_dialog_load_logo (widget, dialog))
        {
          gtk_widget_destroy (widget);
          return NULL;
        }

      /*  kids, don't try this at home!  */
      container = GTK_DIALOG (widget)->vbox;
      children = gtk_container_get_children (GTK_CONTAINER (container));

      if (GTK_IS_VBOX (children->data))
        about_dialog_add_logo (children->data, dialog);
      else
        g_warning ("%s: ooops, no vbox in this container?", G_STRLOC);

      g_list_free (children);

      /* weird magic to determine the way the logo should be shown */
      gdk_window_get_pointer (NULL, NULL, NULL, &mask);

      mask &= ~GDK_BUTTON3_MASK;
      dialog->pp = (mask &= (GDK_SHIFT_MASK | GDK_CONTROL_MASK) &
                    (GDK_CONTROL_MASK | GDK_MOD1_MASK) &
                    (GDK_MOD1_MASK | ~GDK_SHIFT_MASK));

      dialog->text_area.height = dialog->pp ? 50 : 32;
      dialog->text_area.y = (dialog->logo_height - dialog->text_area.height);
    }

  gtk_window_present (GTK_WINDOW (dialog->dialog));

  return dialog->dialog;
}

static void
about_dialog_map (GtkWidget       *widget,
                  GimpAboutDialog *dialog)
{
  if (dialog->layout && dialog->timer == 0)
    {
      dialog->state    = 0;
      dialog->index    = 0;
      dialog->animstep = 0;
      dialog->visible  = FALSE;

      about_dialog_reshuffle (dialog);

      dialog->timer = g_timeout_add (800, about_dialog_timer, dialog);
    }
}

static void
about_dialog_unmap (GtkWidget       *widget,
                    GimpAboutDialog *dialog)
{
  if (dialog->timer)
    {
      g_source_remove (dialog->timer);
      dialog->timer = 0;
    }
}

static void
about_dialog_load_url (GtkAboutDialog *dialog,
                       const gchar    *url,
                       gpointer        data)
{
  GimpContext *context = GIMP_CONTEXT (data);
  Argument    *return_vals;
  gint         nreturn_vals;

  return_vals = procedural_db_run_proc (context->gimp, context, NULL,
                                        PDB_URL_LOAD,
                                        &nreturn_vals,
                                        GIMP_PDB_STRING, url,
                                        GIMP_PDB_END);
  procedural_db_destroy_args (return_vals, nreturn_vals);
}


static void
about_dialog_add_logo (GtkWidget       *vbox,
                       GimpAboutDialog *dialog)
{
  GtkWidget *align;
  GList     *children;

  children = gtk_container_get_children (GTK_CONTAINER (vbox));

  for (children = gtk_container_get_children (GTK_CONTAINER (vbox));
       children;
       children = g_list_next (children))
    {
      if (GTK_IS_IMAGE (children->data))
        {
          gtk_widget_hide (children->data);
          break;
        }
    }

  g_list_free (children);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (vbox), align, 0);
  gtk_widget_show (align);

  dialog->logo_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (dialog->logo_area,
                               dialog->logo_width, dialog->logo_height);
  gtk_widget_set_events (dialog->logo_area, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (align), dialog->logo_area);
  gtk_widget_show (dialog->logo_area);

  g_signal_connect (dialog->logo_area, "expose-event",
                    G_CALLBACK (about_dialog_logo_expose),
                    dialog);

  /* place the scrolltext at the bottom of the image */
  dialog->text_area.width = dialog->logo_width;
  dialog->text_area.height = 32; /* gets changed in map() as well */
  dialog->text_area.x = 0;
  dialog->text_area.y = (dialog->logo_height - dialog->text_area.height);

  dialog->layout = gtk_widget_create_pango_layout (dialog->logo_area, NULL);
  g_object_weak_ref (G_OBJECT (dialog->logo_area),
                     (GWeakNotify) g_object_unref, dialog->layout);

  dialog->font_desc = pango_font_description_from_string ("Sans, 11");

  pango_layout_set_font_description (dialog->layout, dialog->font_desc);
  pango_layout_set_justify (dialog->layout, PANGO_ALIGN_CENTER);
}

static void
about_dialog_reshuffle (GimpAboutDialog *dialog)
{
  GRand *gr = g_rand_new ();
  gint   i;

  for (i = 0; i < dialog->n_authors; i++)
    dialog->shuffle[i] = i;

  /* here we rely on the authors array having Peter and Spencer first */
#define START_INDEX 2

  for (i = START_INDEX; i < dialog->n_authors; i++)
    {
      gint j = g_rand_int_range (gr, START_INDEX, dialog->n_authors);

      if (i != j)
        {
          gint t;

          t = dialog->shuffle[j];
          dialog->shuffle[j] = dialog->shuffle[i];
          dialog->shuffle[i] = t;
        }
    }

#undef START_INDEX

  g_rand_free (gr);
}

static gboolean
about_dialog_logo_expose (GtkWidget       *widget,
                          GdkEventExpose  *event,
                          GimpAboutDialog *dialog)
{
  gdk_gc_set_clip_rectangle (widget->style->black_gc, &event->area);

  gdk_draw_drawable (widget->window,
                     widget->style->black_gc,
                     dialog->logo_pixmap,
                     event->area.x, event->area.y +
                     (dialog->pp ? dialog->logo_height : 0),
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);

  gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);

  if (dialog->visible)
    {
      GdkGC *text_gc;
      gint   layout_x, layout_y;
      gint   width, height;

      text_gc = dialog->logo_area->style->text_gc[GTK_STATE_NORMAL];

      pango_layout_get_pixel_size (dialog->layout, &width, &height);

      layout_x = dialog->text_area.x + (dialog->text_area.width - width) / 2;
      layout_y = dialog->text_area.y + (dialog->text_area.height - height) / 2;

      if (dialog->textrange[1] > 0)
        {
          GdkRegion *covered_region = NULL;
          GdkRegion *rect_region;

          covered_region =
            gdk_pango_layout_get_clip_region (dialog->layout,
                                              layout_x, layout_y,
                                              dialog->textrange, 1);

          rect_region = gdk_region_rectangle (&event->area);

          gdk_region_intersect (covered_region, rect_region);
          gdk_region_destroy (rect_region);

          gdk_gc_set_clip_region (text_gc, covered_region);
          gdk_region_destroy (covered_region);
        }

      gdk_draw_layout (widget->window,
                       widget->style->text_gc[GTK_STATE_NORMAL],
                       layout_x, layout_y,
                       dialog->layout);

      gdk_gc_set_clip_region (text_gc, NULL);
    }

  return FALSE;
}

static gchar *
insert_spacers (const gchar *string)
{
  GString  *str = g_string_new (NULL);
  gchar    *normalized;
  gchar    *ptr;
  gunichar  unichr;

  normalized = g_utf8_normalize (string, -1, G_NORMALIZE_DEFAULT_COMPOSE);
  ptr = normalized;

  while ((unichr = g_utf8_get_char (ptr)))
    {
      g_string_append_unichar (str, unichr);
      g_string_append_unichar (str, 0x200b);  /* ZERO WIDTH SPACE */
      ptr = g_utf8_next_char (ptr);
    }

  g_free (normalized);

  return g_string_free (str, FALSE);
}


/* color constants */

static const PangoColor foreground = { 5000,      5000,      5000      };
static const PangoColor background = { 139 * 257, 137 * 257, 124 * 257 };

static const PangoColor gradient[] =
{
  { 139 * 257,  137 * 257,  124 * 257  },
  { 65535,      65535,      65535      },
  { 5000,       5000,       5000       }
};

/* backup values */

static const PangoColor grad1ent[] =
{
  { 0xff * 257, 0xba * 257, 0x00 * 257 },
  { 37522,      51914,      57568      }
};

static const PangoColor foregr0und = { 37522, 51914, 57568 };
static const PangoColor backgr0und = { 0,     0,     0     };

static void
mix_gradient (const PangoColor *gradient,
              guint             ncolors,
              PangoColor       *target,
              gdouble           pos)
{
  gint index;

  if (pos == 1.0)
    {
      target->red   = gradient[ncolors - 1].red;
      target->green = gradient[ncolors - 1].green;
      target->blue  = gradient[ncolors - 1].blue;
      return;
    }

  index = (gint) floor (pos * (ncolors - 1));
  pos = pos * (ncolors - 1) - index;

  target->red   = (gradient[index].red *
                   (1.0 - pos) + gradient[index + 1].red   * pos);
  target->green = (gradient[index].green *
                   (1.0 - pos) + gradient[index + 1].green * pos);
  target->blue  = (gradient[index].blue *
                   (1.0 - pos) + gradient[index + 1].blue  * pos);
}

static void inline
mix_colors (const PangoColor *start,
            const PangoColor *end,
            PangoColor       *target,
            gdouble           pos)
{
  target->red   = start->red   * (1.0 - pos) + end->red   * pos;
  target->green = start->green * (1.0 - pos) + end->green * pos;
  target->blue  = start->blue  * (1.0 - pos) + end->blue  * pos;
}

static void
decorate_text (GimpAboutDialog *dialog,
               gint             anim_type,
               gdouble          time)
{
  const gchar    *text;
  const gchar    *ptr;
  gint            letter_count = 0;
  gint            text_length  = 0;
  gint            text_bytelen = 0;
  gint            cluster_start, cluster_end;
  gunichar        unichr;
  PangoAttrList  *attrlist = NULL;
  PangoAttribute *attr;
  PangoRectangle  irect = {0, 0, 0, 0};
  PangoRectangle  lrect = {0, 0, 0, 0};
  PangoColor      mix;

  mix_colors ((dialog->pp ? &backgr0und : &background),
              (dialog->pp ? &foregr0und : &foreground), &mix, time);

  text = pango_layout_get_text (dialog->layout);
  g_return_if_fail (text != NULL);

  text_length = g_utf8_strlen (text, -1);
  text_bytelen = strlen (text);

  attrlist = pango_attr_list_new ();

  dialog->textrange[0] = 0;
  dialog->textrange[1] = text_bytelen;

  switch (anim_type)
    {
    case 0: /* Fade in */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_insert (attrlist, attr);
      break;

    case 1: /* Fade in, spread */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_change (attrlist, attr);

      ptr = text;

      cluster_start = 0;
      while ((unichr = g_utf8_get_char (ptr)))
        {
          ptr = g_utf8_next_char (ptr);
          cluster_end = (ptr - text);

          if (unichr == 0x200b)
            {
              lrect.width = (1.0 - time) * 15.0 * PANGO_SCALE + 0.5;
              attr = pango_attr_shape_new (&irect, &lrect);
              attr->start_index = cluster_start;
              attr->end_index = cluster_end;
              pango_attr_list_change (attrlist, attr);
            }
          cluster_start = cluster_end;
        }
      break;

    case 2: /* Fade in, sinewave */
      attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
      attr->start_index = 0;
      attr->end_index = text_bytelen;
      pango_attr_list_change (attrlist, attr);

      ptr = text;

      cluster_start = 0;

      while ((unichr = g_utf8_get_char (ptr)))
        {
          if (unichr == 0x200b)
            {
              cluster_end = ptr - text;
              attr = pango_attr_rise_new ((1.0 -time) * 18000 *
                                          sin (4.0 * time +
                                               (float) letter_count * 0.7));
              attr->start_index = cluster_start;
              attr->end_index = cluster_end;
              pango_attr_list_change (attrlist, attr);

              letter_count++;
              cluster_start = cluster_end;
            }

          ptr = g_utf8_next_char (ptr);
        }
      break;

    case 3: /* letterwise Fade in */
      ptr = text;

      letter_count  = 0;
      cluster_start = 0;

      while ((unichr = g_utf8_get_char (ptr)))
        {
          gint    border = (text_length + 15) * time - 15;
          gdouble pos;

          if (letter_count < border)
            pos = 0;
          else if (letter_count > border + 15)
            pos = 1;
          else
            pos = ((gdouble) (letter_count - border)) / 15;

          mix_colors ((dialog->pp ? &foregr0und : &foreground),
                      (dialog->pp ? &backgr0und : &background),
                      &mix, pos);

          ptr = g_utf8_next_char (ptr);

          cluster_end = ptr - text;

          attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
          attr->start_index = cluster_start;
          attr->end_index = cluster_end;
          pango_attr_list_change (attrlist, attr);

          if (pos < 1.0)
            dialog->textrange[1] = cluster_end;

          letter_count++;
          cluster_start = cluster_end;
        }

      break;

    case 4: /* letterwise Fade in, triangular */
      ptr = text;

      letter_count  = 0;
      cluster_start = 0;

      while ((unichr = g_utf8_get_char (ptr)))
        {
          gint    border = (text_length + 15) * time - 15;
          gdouble pos;

          if (letter_count < border)
            pos = 1.0;
          else if (letter_count > border + 15)
            pos = 0.0;
          else
            pos = 1.0 - ((gdouble) (letter_count - border)) / 15;

          mix_gradient (dialog->pp ? grad1ent : gradient,
                        dialog->pp ?
                        G_N_ELEMENTS (grad1ent) : G_N_ELEMENTS (gradient),
                        &mix, pos);

          ptr = g_utf8_next_char (ptr);

          cluster_end = ptr - text;

          attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
          attr->start_index = cluster_start;
          attr->end_index = cluster_end;
          pango_attr_list_change (attrlist, attr);

          if (pos > 0.0)
            dialog->textrange[1] = cluster_end;

          letter_count++;
          cluster_start = cluster_end;
        }
      break;

    default:
      g_printerr ("Unknown animation type %d\n", anim_type);
    }

  pango_layout_set_attributes (dialog->layout, attrlist);
  pango_attr_list_unref (attrlist);
}

static gboolean
about_dialog_timer (gpointer data)
{
  GimpAboutDialog *dialog  = data;
  gint             timeout = 0;

  if (dialog->animstep == 0)
    {
      gchar   *text = NULL;
      gdouble  size = 11.0;
      gint     width;
      gint     height;

      dialog->visible = TRUE;

      switch (dialog->state)
        {
        case 0:
          dialog->timer = g_timeout_add (30, about_dialog_timer, dialog);
          dialog->state += 1;
          return FALSE;

        case 1:
          text = insert_spacers (_("GIMP is brought to you by"));
          dialog->state += 1;
          break;

        case 2:
          if (! (dialog->index < dialog->n_authors))
            dialog->index = 0;

          text = insert_spacers (authors[dialog->shuffle[dialog->index]]);
          dialog->index += 1;
          break;

        default:
          g_return_val_if_reached (TRUE);
          break;
        }

      g_return_val_if_fail (text != NULL, TRUE);

      pango_layout_set_text (dialog->layout, text, -1);
      pango_layout_set_attributes (dialog->layout, NULL);

      pango_font_description_set_size (dialog->font_desc, size * PANGO_SCALE);
      pango_layout_set_font_description (dialog->layout, dialog->font_desc);

      pango_layout_get_pixel_size (dialog->layout, &width, &height);

      while (width >= dialog->text_area.width && size >= 6.0)
        {
          size -= 0.5;
          pango_font_description_set_size (dialog->font_desc,
                                           size * PANGO_SCALE);
          pango_layout_set_font_description (dialog->layout, dialog->font_desc);
          pango_layout_get_pixel_size (dialog->layout, &width, &height);
        }
    }

  if (dialog->animstep < 16)
    {
      decorate_text (dialog, 4, ((gfloat) dialog->animstep) / 15.0);
    }
  else if (dialog->animstep == 16)
    {
      timeout = 800;
    }
  else if (dialog->animstep == 17)
    {
      timeout = 30;
    }
  else if (dialog->animstep < 33)
    {
      decorate_text (dialog, 1,
                     1.0 - ((gfloat) (dialog->animstep - 17)) / 15.0);
    }
  else if (dialog->animstep == 33)
    {
      dialog->visible = FALSE;
      timeout = 300;
    }
  else
    {
      dialog->visible  = FALSE;
      dialog->animstep = -1;
      timeout = 30;
    }

  dialog->animstep++;

  gtk_widget_queue_draw_area (dialog->logo_area,
                              dialog->text_area.x,
                              dialog->text_area.y,
                              dialog->text_area.width,
                              dialog->text_area.height);

  if (timeout > 0)
    {
      dialog->timer = g_timeout_add (timeout, about_dialog_timer, dialog);
      return FALSE;
    }

  /* else keep the current timeout */
  return TRUE;
}


/* some handy shortcuts */

#define random() gdk_pixbuf_loader_new_with_type ("\160\x6e\147", NULL)
#define pink(a) gdk_pixbuf_loader_close ((a), NULL)
#define line gdk_pixbuf_loader_write
#define white gdk_pixbuf_loader_get_pixbuf
#define level(a) gdk_pixbuf_get_width ((a))
#define variance(a) gdk_pixbuf_get_height ((a))
#define GPL GdkPixbufLoader

static gboolean
about_dialog_load_logo (GtkWidget       *widget,
                         GimpAboutDialog *dialog)
{
  GdkPixbuf *pixbuf;
  GdkGC     *gc;
  GPL       *noise;
  gchar     *filename;

  g_return_val_if_fail (dialog->logo_pixmap == NULL, FALSE);

  filename = g_build_filename (gimp_data_directory (), "images",
                               "gimp-logo.png",
                               NULL);

  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  if (! pixbuf)
    return FALSE;

  dialog->logo_width = gdk_pixbuf_get_width (pixbuf);
  dialog->logo_height = gdk_pixbuf_get_height (pixbuf);

  gtk_widget_realize (widget);

  dialog->logo_pixmap = gdk_pixmap_new (widget->window,
                                        dialog->logo_width,
                                        dialog->logo_height * 2,
                                        -1);

  gc = gdk_gc_new (dialog->logo_pixmap);

  /* draw a defined content to the pixmap */
  gdk_draw_rectangle (GDK_DRAWABLE (dialog->logo_pixmap),
                      gc, TRUE,
                      0, 0,
                      dialog->logo_width, dialog->logo_height * 2);

  gdk_draw_pixbuf (GDK_DRAWABLE (dialog->logo_pixmap),
                   gc, pixbuf,
                   0, 0, 0, 0,
                   dialog->logo_width, dialog->logo_height,
                   GDK_RGB_DITHER_NORMAL, 0, 0);

  g_object_unref (pixbuf);

  if ((noise = random ()) && line (noise, (const guchar *)
        "\211\120\116\107\r\n\032\n\0\0\0\r\111\110D\122\0\0\001+\0\0\001"
        "\r\004\003\0\0\0\245\177^\254\0\0\0000\120\114TE\0\0\0\023\026\026"
        " \'(2=ANXYSr\177surg\216\234\226\230\225z\247\272\261\263\257\222�"
        "�������������;\014�\016\000\000\017\210I\104A\124x��\235[l\033�\025"
        "@gw)�a�b�\246\255��\242\037)j+\251hII\2122)i�v�\270\211\234��\010"
        "��&F`\004\251\251�\r\202B?jR\030A\220\226I\213�0�\243�\201a\030\t"
        "X;\026\004\201?�\003E\020\250\241�\257\242?\222Q#\010\002W\264e\201"
        "\020\004\205�y-\271\217;\273\\\212\273b\212\275\200%q�\261gg��\271"
        "sg\226F(\220@\002\t$\220@\276B\"\037\2760>}�D\254\265\250\036-p9"
        "�BPR\272P\225�h�`�\2500W\253P%\n\006�SkP}\243`\222\221\226\030\203"
        "�f\254�X�u!\221k\033O\245\024\000����\205\260\256n\270�\032\207\260"
        "\2467\016h�\000\221Gk,�_\256!\236\035X\237�7h\224�3\267��\230~\016"
        "Z\27744\213�g\256�iv�\201�q�\261\t\266�--\245\275y\\�u\017\277I�"
        "�\270.y�5�\270���\232�\\n\255_\233�s\035k\032�\233��C���\276a\275"
        "8j2\236\006##5\r�\245Q\226�\n\177\257�)\\\227\254O\265\210\\\270"
        "p��\223\'O\014\r���\273AX�\013>I#\256�{n�\270\023.��c\r`\271w\215"
        "\245\267\032\032�\256\212\204��\r�\256z\217��~�\216�\200\225pm��"
        "\013>`\215\273\236\031�>`�5E�\225\235�\036\253�\265M9�\007V\257�"
        "H\203R�\003+m]r�\265\245\001\231\036M\2461�\007\226\004\030���\205"
        "�/\235\034��)\\�z\216�\006x\216�\216;9\\\236cm\242\005\246\214&#"
        "�4\223{\216u�jL\225i;\'�\037,�\230vN:�J\274�R\200y:q��\033�\032\013"
        "\232\247G\247�\026k\276`\261y�\272�\265�\271S�\n\033\201���\257&"
        "\203P[���\033\201�\027=���4�\260��\030\213\033�\250e6�s�\274o�\'"
        "�v\2531�\226\253\'�w\037\256���C\232��\017\254\2645\222�f\037\004"
        "j�\003\013\230\2477\001T\223�W,h�\003EhF��\202\214)`\021\246\220"
        "\277X�\242\'\0044�q\237\261\240y�Ju\035�\213\005-z\022\016�E\037"
        "\260\240E\217U\265R�g,`�c�\256\236F~c\001\213\036�\200�\240\272�"
        "\203\005-zX�)\211�1\205\227b�w,h�3\252�F���\223C1Q!/\2616\t��\021"
        "\247g�\022\013X�\2649\206�\274�\002\214�v�\020\234�X\2201M;\206�"
        "<�J\000-�\2749�5\226\014\234Uis\016:{\215�\013x�\235�;\270\036ci"
        "+\255�\240\0138\031� ,mY\032\205���\003\033\202\245\235w\230\022"
        "��g7\002k\233y\257��\002�\035�-�8t��\024\234\001;�S\254v�\254\221"
        "���\257|�\032\025l\266\207�\235N\224y\211�.��\226\237\261�\266�\032"
        "+ms\220�Q�C\002\036b\265�\236\0000\234\002\232�\021+]\260�~RFmN�"
        "x\207\025r<Q�cq/z\207\225\020,j@Kq�/,\245\236�\267�\216\274�\027"
        "V\242\256S\256�3�\244�!\226\\\200�\'\260\024�>a�\002\215�vP8o�\204"
        "\245\271\177�F/g�\240`��S\'B\036\r=�c\001S�\201�\r\226���\';mh\232"
        "��|\034\211Pc�\202\270S?\2658\205�|�\002�?C ^\007\266�\272\035�\025"
        "\026��\231vW\247~f\260o#~`\001��6Q\0107\rN�^`\001�\037\2603w]�\010"
        "1\037\260\000�O�2\001|��\003,��S\204�Vex\212�\000\013p���\035\236"
        "\220_�iH\264�\005\006��a\277\272�X\200�\247\215��\207��\273\026\235"
        "\240�j>\026��m\253\235\217\227j`�%\266�+\037\253�W5\016L\261\037"
        "\036�ky\002v\254\233\215\005\270\177\t�VE\233�\232�\275�\262\272"
        "\177!��\\\273�l\215[�_\036`\001�_�\274\026�O�\222 \024�d,\253G�n"
        "y\247\244W�\234\212��o.\226��\223F-�\017��\014;\201��\2626�6k\260"
        "h�b\266\256y\214eq�L��29�N`\223\261\254�_�\272\217)[�\226�a7\213"
        "�\027\002\266��\234\235��bY�\2774\020(2\230\255Q�\033C��\262\270"
        "\177��\013g\006u\022\235tn\"\226��\253\276\034i\210\215��\226,�X"
        "l\"\226���\013F\222\217�\206@H\024kj\036\226��\223��\250^\235�E�"
        "��aY�\277\004|�A\257N\235\242\2737\rK67M\010~��`\266zE�\260M��f�"
        "\2434\034E6\250SB\024]m\032V�4�\264\013�F5\250SZ\264\235�,,��X\260"
        "q0\233-\201\023�<\254\220���+�f�\233-Ix\036\242YX�F\217F\021n\233"
        "��\226\"\214�6\013\253��c\211\202hgE\257N\"\'\260yX\233\014��&~\021"
        "u�b\266\256y\216U�WO\214��8\211\214\013\234��\275�l��\202�5s\273"
        "(�\210M c\275X�\013��Q�\252\273 <\200\'\212<5\202\005�3b\263\276"
        "\255\t�m\tMz\201Y\262?:m��S\266�W�2�\2403�\266\253R�[�\275\rc]\265"
        "o\005�\261\261�\272�\261o|(\036\007\035\033��\245��<,tKN�b�\211\275"
        "��\001\212\205\277�\260?\263�/80}��\244�\206\220mc5�\022\277�\266"
        "\206\023\004S1�\034\241\273�\264�\220\247��`\234\002\236R\032<\251"
        "I\277����o��s��{�\016\277�VN\240@\002\t$\220@\002\t$\220@\002\201"
        "$��\257[\220\212\2548�k=\254���NN0\030�f�a\215:\204�\244��1W\025"
        "~\272\270\270\030oP\241v\r��t\221 \033�\222Tu\214\227�=\260\253\216"
        "\257�,\251\252�\020V�]\225��\251j\014�\031�\205\"-S\271\034�\014"
        "k\263�%SO\'2\254\222Vf5�9�Z\224\007#G��\242\205\274�\272���7p�\031"
        "\026\240\232\216\272�R��\014k\215��\247\252+�B�\022\271�Z�\026\013"
        "\r\253*\031\216\017�Gub�\267\246\222\036buhc\277N\254\030\222\237"
        "\"X\013�b�\032\026�`\221\217y��\256�Xy�\216[\254\035*�H/\261r�\222"
        "(�\236�\247�\020V\0071]�\233���-\273vq�\277��\225s\251\032V��s5\266"
        "�k\237\2604$���\227z�\242\035V\226��fr\233\244�%+VRo\220�\277 \026"
        "=\003`\205�PT��\254���\233t0\\�\260\276E�k\206\227\245�l\021\242"
        "\242JI]vl\255n\202\245i\177\227\252\222�~�,A\254n\2540\033\243\025"
        "\236s\220~�\235����\034+\216-�\262\275n��Xh\236���\272B\270�-��\206"
        "\260\023�Xy\265\nBrf�Z\003iit\244��V\207T\007,j�\031V\222>\251�F"
        "�0\261MJ\236f3a��nn��\215E�\2463\274KZ\277\261�\2122\254\'\034\260"
        ":\250edXaZ}\027\035DRI�g5�\204\225eu\232\260zt\034:\254U\246)\270"
        "��L\247N0\254\274\003\026n\224\250\206\205\212\244\221\207�2m�\n"
        "o�\0053\026�\223�\026,��\"�sf\241\212�� \\\275��\276�\240\273t�*"
        "��\003\204@p\254><\026q3��?Y\251$�\255�:\270�\210\2462&\254\034C"
        "@q\236\023\017�a\222%�@&\250�\222���\177�&\210U\031\212n=��s,�\213"
        "\231\0166I��9�\264b\005\232\252\t\200\t+�\2614\273\205\037G�\017"
        "�\247\262J\263t\234R\254J���\242#\230c�\252\227\222\264�p�\031\256"
        "{k\020�2\002\261��a\215Q\255\240\217\235U\265� �\024�\037\016n`J"
        "\217\265C]-��\223\264�#L\236�\212\225B\240n\251\037�\260R<i\202\022"
        "\257\2209!I\215?�\212�bq�W�\n�1L�\270�\032\2217�\255-X\037 +V\037"
        "M�,\212\252\276\006\035PDSu\235\217i;l�5\254[\247\257\\>�?iX�\271"
        "\226Q�bs52c\275\215\000\254\260N)\214X\222\036+C\261V\234\246j#V"
        "7}:#\226\241\265�\277\177=\213 ,�\222O�\260\222\024K�c\215Q\254\262"
        ";\254,o-\\�\212&F7\260�%�\2029q�\036+�\000\226\254�V\230�\021�N\r"
        "X1\275\007���p0bIV�r\207\025Q�X/\204\014\000b\254\270\036\213\267"
        "�\214\031\213�\\�\276�/�\000VN\275\221\247\275\250p�a\207\245Y\201"
        "|\025\013\205��\274d�*\252|*\253:9\256\2600L\246\207�\"k4G\254\031"
        "���e�f6�\231\260r\252�)]c��\027�\014(\257�\245a,�\005��\261$�B\002"
        "XI�\232�5V\216\024.�^L��a,j�\177�\2106s�\275�[��\211��\"���\250{"
        "\254\020\255>I{1\242�E\030+i\260F\230�\213s�\227@\225\0171�q��t\214"
        "\270��A\025\263\203\002�����B\254\210\016k\242�\027[\r\004S\256�"
        "j�-\026\033\204�\245\037&\201\254�_+\222�a,\205��s\206URE�T�\004"
        "\023\256\261�|@%\251�\024�=\037\214��\240�\035z7Y\037�1c�\232k�5"
        "V\0373\r\274\027I0�\001\213\271\231�\207\030\026o\2552\202\260\250"
        "A��\266\035V����Oo�ro�\235�\017�\\\202\253\252�\233�\233��\014\275"
        ">;k\210\202�+\021\017K�\2271\265L\226\020\225KQ�s\022\254��o6rB\277"
        "\'P_\220Y>\214/~\264\236\010��\203N9\006�\255\'�\261;\256\263ep\250"
        "u�s\274@\002\t�+(�\200��\204h@@�y�e\ry��\227�__\207y���g�\235):\033"
        "t\254�_�\2207\253��\201\021�\'�<��-�3�����+W�\002U���|�\214\025\276"
        "Y\037\026)k�k�:\003E=\036#�ld�E�0�s�1\224[\\\274\001U\235B;�\230"
        "\261�micY3\026(�f1V.\205�\251\216e�\265\214�\021V�qW�\217�.�8\035"
        "\212\206��\020�\227\235h\013\032$\213\254C�-�\'Y\252��\023�\001\234"
        "D\263nAC�\262;qn|\001)C�\n\256\204N\222\007�\226\250\024#\027\021"
        "�3�\222\261�z0�\227�M\231�\236C�*��\006+\214�\275\"\222��\205p9\217"
        "\035%�9�\030�q\236RW\263\230.\227!��8qkX���+\265\262�\032\222*Q\245"
        "\244�\007_!\225�\244\207\260\203\233Mu�\245q\237\007qA\232L\004c"
        "a\032�\275�\207S*\250,�\210��^]�r*\\ye�*\271w�\213\201�\230R\211"
        "�#^w�\014\212,)\225\003,\211`�>\247+\233�t�E}\037(\213�l\212TB\034"
        "��\036\234\034\256\\��\n��^�\023\247�:\254�\267\tVI^\273\014�{6�"
        "\265�\030\273W7~�0.Q\242X\023\250g!B�T�e�Z�\265E�jh>�\261��e#K�\031"
        "\224\217\242�L6��v\216*\224\032�&U\210\006!\226�\261H\235��\233\227"
        "\260\273\255|R\254��A^�M\020�\035C���h\013\240�\235\236\031�\t[\263"
        "2\276\007�\220�O\220����k\267\020Z%w�\246\"�^\024�(\026�\242b\264"
        "\207<�*\003b?\263\237\035--\204+\247�@By\2719\010��\251(�tk%Z�J\241"
        "\256;I2j��,��$\276��\004�J\031�f\227\230\013?Atk%Ju�\267\004\213"
        "�\026MN�%w\025+\\Z\033\236@g�\217�\202&r\027�D\2048\026\n\025�\254"
        "Xy\032��\277\002aie�e\244\254\036\032\030\210��\270\022\222�\223"
        "/\rX,Y�BR\224\224\275\227\232\243\216e!\026N.\221}\205e#V\017V\224"
        "\"\251\254�\034�\2569\222\233e5b\205��1iM\263[]�6�\224\206�G\"��"
        "\205\014�J\231\206HH\n\036�\",<�Cd�\025.\033\261\272��:��\2725Cn"
        "\'\225b,\253\021\253o.9\203\212q\216�-r6\243aE�\205b�\200�\275e\272"
        "\005\227��@,Tz;W\214v=��\222\021K.}\234\247\255\025��1Y}��2�j�*�"
        ";�\250\257<p\024\227\"\225�\253\227\006K1\rKQ�>\222\242�\032V�\005"
        "\262�>\222�v��#\245\224\020�!�f6z\277Z\241\025�\252Xx\215x\221�\026"
        "\"1�\007�\262\237f5`\205W�\220W\212xy\231M�O7\000\244y�fU\267�<\250"
        "fh\262\206�]y\205��y\214Dv>\264\231\252v�\245\022dA\212��\241�~w"
        "\254\232\025ts\242\265J$C\256\255�\253�,<@�\244\207\010\266�7���"
        "TWw�h)\'��{\237\244\241��T+Q�|\253\250\257�R\215\205\224#t\\\207"
        "c��\"\220@\002\t$\220@\002\t$\220@\002\t$\220@\002\t$\220@\002\t"
        "$\220@���\177t�\007�uUy$\0\0\0\0IE\116\104\256B`\202", 4093, NULL)
      && pink (noise) && white (noise))
    {
      gdk_draw_pixbuf (GDK_DRAWABLE (dialog->logo_pixmap),
                       gc, white (noise), 0, 0,
                       (dialog->logo_width - level (white (noise))) / 2,
                       dialog->logo_height + (dialog->logo_height -
                                              variance (white (noise))) / 2,
                       level (white (noise)),
                       variance (white (noise)),
                       GDK_RGB_DITHER_NORMAL, 0, 0);
    }

  g_object_unref (noise);
  g_object_unref (gc);

  return TRUE;
}
