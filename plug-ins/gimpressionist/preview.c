#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gimpressionist.h"
#include "ppmtool.h"
#include "infile.h"

#include "libgimp/stdplugins-intl.h"


static GtkWidget *preview       = NULL;
static GtkWidget *previewbutton = NULL;

void preview_set_button_label (gchar * text)
{
  gtk_label_set_text (GTK_LABEL (GTK_BIN (previewbutton)->child), text);
}

static void
drawalpha (ppm_t *p, ppm_t *a)
{
  int    x, y, g;
  double v;
  int    gridsize = 16;
  int     rowstride = p->width * 3;

  for (y = 0; y < p->height; y++)
    {
      for (x = 0; x < p->width; x++)
        {
          int k = y * rowstride + x * 3;

          if (!a->col[k])
            continue;

          v = 1.0 - a->col[k] / 255.0;
          g = ((x / gridsize + y / gridsize) % 2) * 60 + 100;
          p->col[k+0] *= v;
          p->col[k+1] *= v;
          p->col[k+2] *= v;
          v = 1.0 - v;
          p->col[k+0] += g*v;
          p->col[k+1] += g*v;
          p->col[k+2] += g*v;
        }
    }
}

static ppm_t preview_ppm      = {0, 0, NULL};
static ppm_t alpha_ppm        = {0, 0, NULL};
static ppm_t backup_ppm       = {0, 0, NULL};
static ppm_t alpha_backup_ppm = {0, 0, NULL};

void
preview_free_resources (void)
{
  ppm_kill (&preview_ppm);
  ppm_kill (&alpha_ppm);
  ppm_kill (&backup_ppm);
  ppm_kill (&alpha_backup_ppm);
}

void
updatepreview (GtkWidget *wg, gpointer d)
{
  /* This portion is remmed out because of the remming out of the
   * code below.
   *            -- Shlomi Fish
   * */
#if 0
  guchar buf[PREVIEWSIZE*3];

  if (!PPM_IS_INITED (&infile) && d)
    grabarea();
#endif

  /* It seems that infile.col must be true here. (after grabarea() that is.)
   * Thus, I'm removing this entire portion of the code in hope that
   * it works OK afterwards.
   *            -- Shlomi Fish
   * */
#if 0
  if (!PPM_IS_INITED (&infile) && !d) {
    guchar *buffer;

    buffer = g_new0 (guchar, 3*PREVIEWSIZE*PREVIEWSIZE);
    gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview),
                            0, 0, PREVIEWSIZE, PREVIEWSIZE,
                            GIMP_RGB_IMAGE,
                            buffer,
                            PREVIEWSIZE * 3);
    
    g_free (buffer);
  }
  else 
#endif
  {
    if (!PPM_IS_INITED (&backup_ppm))
      {
        infile_copy_to_ppm (&backup_ppm);
        if ((backup_ppm.width != PREVIEWSIZE) ||
            (backup_ppm.height != PREVIEWSIZE))
          resize_fast (&backup_ppm, PREVIEWSIZE, PREVIEWSIZE);
        if (img_has_alpha)
          {
            infile_copy_alpha_to_ppm (&alpha_backup_ppm);
            if ((alpha_backup_ppm.width != PREVIEWSIZE) ||
                (alpha_backup_ppm.height != PREVIEWSIZE))
              resize_fast (&alpha_backup_ppm, PREVIEWSIZE, PREVIEWSIZE);
          }
      }
    if (!PPM_IS_INITED (&preview_ppm))
      {
        ppm_copy (&backup_ppm, &preview_ppm);

        if (img_has_alpha)
          ppm_copy (&alpha_backup_ppm, &alpha_ppm);
      }
    if (d)
      {
        store_values ();

        if (GPOINTER_TO_INT (d) != 2)
          repaint (&preview_ppm, &alpha_ppm);
      }
      if (img_has_alpha)
      drawalpha (&preview_ppm, &alpha_ppm);

      gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview),
                              0, 0, PREVIEWSIZE, PREVIEWSIZE,
                              GIMP_RGB_IMAGE,
                              preview_ppm.col,
                              PREVIEWSIZE * 3);

      ppm_kill (&preview_ppm);
      if (img_has_alpha)
        ppm_kill (&alpha_ppm);
    }
}

static void
preview_size_allocate (GtkWidget *preview)
{
  updatepreview (preview, GINT_TO_POINTER (0));
}

GtkWidget *
create_preview (void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *button;

  vbox = gtk_vbox_new (FALSE, 6);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 5);
  gtk_widget_show (frame);

  preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (preview, PREVIEWSIZE, PREVIEWSIZE);

  gtk_container_add (GTK_CONTAINER (frame), preview);
  gtk_widget_show (preview);
  /* This is so the preview will be displayed when the dialog is invoked. */
  g_signal_connect (preview, "size-allocate",
                    G_CALLBACK (preview_size_allocate), NULL);

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  previewbutton = button = gtk_button_new_with_mnemonic (_("_Update"));
  g_signal_connect (button, "clicked",
                    G_CALLBACK (updatepreview), (gpointer) 1);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  gimp_help_set_help_data (button,
                           _("Refresh the Preview window"), NULL);

  button = gtk_button_new_from_stock (GIMP_STOCK_RESET);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (updatepreview), (gpointer) 2);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  gimp_help_set_help_data (button,
                           _("Revert to the original image"), NULL);

  return vbox;
}