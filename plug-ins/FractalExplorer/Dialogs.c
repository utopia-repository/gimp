#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "FractalExplorer.h"
#include "Dialogs.h"

#include "logo.h"

#include "libgimp/stdplugins-intl.h"


#define RESPONSE_ABOUT 1
#define ZOOM_UNDO_SIZE 100


static gint              n_gradient_samples = 0;
static gdouble          *gradient_samples = NULL;
static gchar            *gradient_name    = NULL;
static gboolean          ready_now = FALSE;
static gchar            *tpath = NULL;
static DialogElements   *elements = NULL;
static GtkWidget        *cmap_preview;
static GtkWidget        *maindlg;

static explorer_vals_t  zooms[ZOOM_UNDO_SIZE];
static gint             zoomindex = 0;
static gint             zoommax = 0;

static gint              oldxpos = -1;
static gint              oldypos = -1;
static gdouble           x_press = -1.0;
static gdouble           y_press = -1.0;

static explorer_vals_t standardvals =
{
  0,
  -2.0,
  2.0,
  -1.5,
  1.5,
  50.0,
  -0.75,
  -0.2,
  0,
  1.0,
  1.0,
  1.0,
  1,
  1,
  0,
  0,
  0,
  0,
  1,
  256,
  0
};

/**********************************************************************
 FORWARD DECLARATIONS
 *********************************************************************/

static void load_file_chooser_response (GtkFileChooser *chooser,
                                        gint            response_id,
                                        gpointer        data);
static void save_file_chooser_response (GtkFileChooser *chooser,
                                        gint            response_id,
                                        gpointer        data);
static void create_load_file_chooser   (GtkWidget      *widget,
                                        GtkWidget      *dialog);
static void create_save_file_chooser   (GtkWidget      *widget,
                                        GtkWidget      *dialog);

static void explorer_logo_dialog       (GtkWidget      *parent);

static void cmap_preview_size_allocate (GtkWidget      *widget,
                                        GtkAllocation  *allocation);

static void logo_preview_size_allocate (GtkWidget      *preview);

/**********************************************************************
 CALLBACKS
 *********************************************************************/

static void
dialog_response (GtkWidget *widget,
                 gint       response_id,
                 gpointer   data)
{
  switch (response_id)
    {
    case RESPONSE_ABOUT:
      explorer_logo_dialog (widget);
      break;

    case GTK_RESPONSE_OK:
      wint.run = TRUE;
      gtk_widget_destroy (widget);
      break;

    default:
      gtk_widget_destroy (widget);
      break;
    }
}

static void
dialog_reset_callback (GtkWidget *widget,
                       gpointer   data)
{
  wvals.xmin = standardvals.xmin;
  wvals.xmax = standardvals.xmax;
  wvals.ymin = standardvals.ymin;
  wvals.ymax = standardvals.ymax;
  wvals.iter = standardvals.iter;
  wvals.cx   = standardvals.cx;
  wvals.cy   = standardvals.cy;

  dialog_change_scale ();
  set_cmap_preview ();
  dialog_update_preview ();
}

static void
dialog_redraw_callback (GtkWidget *widget,
                        gpointer   data)
{
  gint alwaysprev = wvals.alwayspreview;

  wvals.alwayspreview = TRUE;
  set_cmap_preview ();
  dialog_update_preview ();
  wvals.alwayspreview = alwaysprev;
}

static void
dialog_undo_zoom_callback (GtkWidget *widget,
                           gpointer   data)
{
  if (zoomindex > 0)
    {
      zooms[zoomindex] = wvals;
      zoomindex--;
      wvals = zooms[zoomindex];
      dialog_change_scale ();
      set_cmap_preview ();
      dialog_update_preview ();
    }
}

static void
dialog_redo_zoom_callback (GtkWidget *widget,
                           gpointer   data)
{
  if (zoomindex < zoommax)
    {
      zoomindex++;
      wvals = zooms[zoomindex];
      dialog_change_scale ();
      set_cmap_preview ();
      dialog_update_preview ();
    }
}

static void
dialog_step_in_callback (GtkWidget *widget,
                         gpointer   data)
{
  double xdifferenz;
  double ydifferenz;

  if (zoomindex < ZOOM_UNDO_SIZE - 1)
    {
      zooms[zoomindex]=wvals;
      zoomindex++;
    }
  zoommax = zoomindex;

  xdifferenz =  wvals.xmax - wvals.xmin;
  ydifferenz =  wvals.ymax - wvals.ymin;
  wvals.xmin += 1.0 / 6.0 * xdifferenz;
  wvals.ymin += 1.0 / 6.0 * ydifferenz;
  wvals.xmax -= 1.0 / 6.0 * xdifferenz;
  wvals.ymax -= 1.0 / 6.0 * ydifferenz;
  zooms[zoomindex] = wvals;

  dialog_change_scale ();
  set_cmap_preview ();
  dialog_update_preview ();
}

static void
dialog_step_out_callback (GtkWidget *widget,
                          gpointer   data)
{
  gdouble xdifferenz;
  gdouble ydifferenz;

  if (zoomindex < ZOOM_UNDO_SIZE - 1)
    {
      zooms[zoomindex]=wvals;
      zoomindex++;
    }
  zoommax = zoomindex;

  xdifferenz =  wvals.xmax - wvals.xmin;
  ydifferenz =  wvals.ymax - wvals.ymin;
  wvals.xmin -= 1.0 / 4.0 * xdifferenz;
  wvals.ymin -= 1.0 / 4.0 * ydifferenz;
  wvals.xmax += 1.0 / 4.0 * xdifferenz;
  wvals.ymax += 1.0 / 4.0 * ydifferenz;
  zooms[zoomindex] = wvals;

  dialog_change_scale ();
  set_cmap_preview ();
  dialog_update_preview ();
}

static void
explorer_toggle_update (GtkWidget *widget,
                        gpointer   data)
{
  gimp_toggle_button_update (widget, data);

  set_cmap_preview ();
  dialog_update_preview ();
}

static void
explorer_radio_update  (GtkWidget *widget,
                        gpointer   data)
{
  gimp_radio_button_update (widget, data);

  set_cmap_preview ();
  dialog_update_preview ();
}

static void
explorer_double_adjustment_update (GtkAdjustment *adjustment,
                                   gpointer       data)
{
  gimp_double_adjustment_update (adjustment, data);

  set_cmap_preview ();
  dialog_update_preview ();
}

static void
explorer_number_of_colors_callback (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  gimp_int_adjustment_update (adjustment, data);

  g_free (gradient_samples);

  if (! gradient_name)
    gradient_name = gimp_context_get_gradient ();

  gimp_gradient_get_uniform_samples (gradient_name,
                                     wvals.ncolors,
                                     wvals.gradinvert,
                                     &n_gradient_samples,
                                     &gradient_samples);

  set_cmap_preview ();
  dialog_update_preview ();
}

static void
explorer_gradient_select_callback (const gchar   *name,
                                   gint           width,
                                   const gdouble *gradient_data,
                                   gboolean       dialog_closing,
                                   gpointer       data)
{
  g_free (gradient_name);
  g_free (gradient_samples);

  gradient_name = g_strdup (name);

  gimp_gradient_get_uniform_samples (gradient_name,
                                     wvals.ncolors,
                                     wvals.gradinvert,
                                     &n_gradient_samples,
                                     &gradient_samples);

  if (wvals.colormode == 1)
    {
      set_cmap_preview ();
      dialog_update_preview ();
    }
}

static void
preview_draw_crosshair (gint px,
                        gint py)
{
  gint     x, y;
  guchar  *p_ul;

  p_ul = wint.wimage + 3 * (preview_width * py + 0);

  for (x = 0; x < preview_width; x++)
    {
      p_ul[0] ^= 254;
      p_ul[1] ^= 254;
      p_ul[2] ^= 254;
      p_ul += 3;
    }

  p_ul = wint.wimage + 3 * (preview_width * 0 + px);

  for (y = 0; y < preview_height; y++)
    {
      p_ul[0] ^= 254;
      p_ul[1] ^= 254;
      p_ul[2] ^= 254;
      p_ul += 3 * preview_width;
    }
}

static void
preview_redraw (void)
{
  gimp_preview_area_draw (GIMP_PREVIEW_AREA (wint.preview),
                          0, 0, preview_width, preview_height,
                          GIMP_RGB_IMAGE,
                          wint.wimage, preview_width * 3);

  gtk_widget_queue_draw (wint.preview);
}

static gboolean
preview_button_press_event (GtkWidget      *widget,
                            GdkEventButton *event)
{
  if (event->button == 1)
    {
      x_press = event->x;
      y_press = event->y;
      xbild = preview_width;
      ybild = preview_height;
      xdiff = (xmax - xmin) / xbild;
      ydiff = (ymax - ymin) / ybild;

      preview_draw_crosshair (x_press, y_press);
      preview_redraw ();
    }
  return TRUE;
}

static gboolean
preview_motion_notify_event (GtkWidget      *widget,
                             GdkEventButton *event)
{
  if (oldypos != -1)
    {
      preview_draw_crosshair (oldxpos, oldypos);
    }

  oldxpos = event->x;
  oldypos = event->y;

  if ((oldxpos >= 0.0) &&
      (oldypos >= 0.0) &&
      (oldxpos < preview_width) &&
      (oldypos < preview_height))
    {
      preview_draw_crosshair (oldxpos, oldypos);
    }
  else
    {
      oldypos = -1;
      oldxpos = -1;
    }

  preview_redraw ();

  return TRUE;
}

static gboolean
preview_leave_notify_event (GtkWidget      *widget,
                            GdkEventButton *event)
{
  if (oldypos != -1)
    {
      preview_draw_crosshair (oldxpos, oldypos);
    }
  oldxpos = -1;
  oldypos = -1;

  preview_redraw ();

  gdk_window_set_cursor (maindlg->window, NULL);

  return TRUE;
}

static gboolean
preview_enter_notify_event (GtkWidget      *widget,
                            GdkEventButton *event)
{
  static GdkCursor *cursor = NULL;

  if (! cursor)
    {
      GdkDisplay *display = gtk_widget_get_display (maindlg);

      cursor = gdk_cursor_new_for_display (display, GDK_TCROSS);

    }

  gdk_window_set_cursor (maindlg->window, cursor);

  return TRUE;
}

static gboolean
preview_button_release_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
  gdouble l_xmin;
  gdouble l_xmax;
  gdouble l_ymin;
  gdouble l_ymax;

  if (event->button == 1)
    {
      gdouble x_release, y_release;

      x_release = event->x;
      y_release = event->y;

      if ((x_press >= 0.0) && (y_press >= 0.0) &&
          (x_release >= 0.0) && (y_release >= 0.0) &&
          (x_press < preview_width) && (y_press < preview_height) &&
          (x_release < preview_width) && (y_release < preview_height))
        {
          l_xmin = (wvals.xmin +
                    (wvals.xmax - wvals.xmin) * (x_press / preview_width));
          l_xmax = (wvals.xmin +
                    (wvals.xmax - wvals.xmin) * (x_release / preview_width));
          l_ymin = (wvals.ymin +
                    (wvals.ymax - wvals.ymin) * (y_press / preview_height));
          l_ymax = (wvals.ymin +
                    (wvals.ymax - wvals.ymin) * (y_release / preview_height));

          if (zoomindex < ZOOM_UNDO_SIZE - 1)
            {
              zooms[zoomindex] = wvals;
              zoomindex++;
            }
          zoommax = zoomindex;
          wvals.xmin = l_xmin;
          wvals.xmax = l_xmax;
          wvals.ymin = l_ymin;
          wvals.ymax = l_ymax;
          dialog_change_scale ();
          dialog_update_preview ();
          oldypos = oldxpos = -1;
        }
    }

  return TRUE;
}

/**********************************************************************
 FUNCTION: explorer_dialog
 *********************************************************************/

gint
explorer_dialog (void)
{
  GtkWidget *dialog;
  GtkWidget *top_hbox;
  GtkWidget *left_vbox;
  GtkWidget *abox;
  GtkWidget *vbox;
  GtkWidget *hbbox;
  GtkWidget *frame;
  GtkWidget *toggle;
  GtkWidget *toggle_vbox;
  GtkWidget *toggle_vbox2;
  GtkWidget *toggle_vbox3;
  GtkWidget *notebook;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *gradient;
  gchar     *gradient_name;
  GSList    *group = NULL;
  gint       i;

  gimp_ui_init ("fractalexplorer", TRUE);

  fractalexplorer_path = gimp_gimprc_query ("fractalexplorer-path");

  if (! fractalexplorer_path)
    {
      gchar *gimprc = gimp_personal_rc_file ("gimprc");
      gchar *full_path;
      gchar *esc_path;

      full_path =
        g_strconcat ("${gimp_dir}", G_DIR_SEPARATOR_S,
                     "fractalexplorer",
                     G_SEARCHPATH_SEPARATOR_S,
                     "${gimp_data_dir}", G_DIR_SEPARATOR_S,
                     "fractalexplorer",
                     NULL);
      esc_path = g_strescape (full_path, NULL);
      g_free (full_path);

      g_message (_("No %s in gimprc:\n"
                   "You need to add an entry like\n"
                   "(%s \"%s\")\n"
                   "to your %s file."),
                 "fractalexplorer-path",
                 "fractalexplorer-path", esc_path, gimprc);

      g_free (gimprc);
      g_free (esc_path);
    }


  wint.wimage = g_new (guchar, preview_width * preview_height * 3);
  elements    = g_new (DialogElements, 1);

  dialog = maindlg =
    gimp_dialog_new ("Fractal Explorer", "fractalexplorer",
                     NULL, 0,
                     gimp_standard_help_func, HELP_ID,

                     _("About"),       RESPONSE_ABOUT,
                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                     GTK_STOCK_OK,     GTK_RESPONSE_OK,

                     NULL);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_response),
                    NULL);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  top_hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (top_hbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), top_hbox,
                      FALSE, FALSE, 0);
  gtk_widget_show (top_hbox);

  left_vbox = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (top_hbox), left_vbox, FALSE, FALSE, 0);
  gtk_widget_show (left_vbox);

  /*  Preview  */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (left_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  abox = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), abox, FALSE, FALSE, 0);
  gtk_widget_show (abox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (abox), frame);
  gtk_widget_show (frame);

  wint.preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (wint.preview, preview_width, preview_height);
  gtk_container_add (GTK_CONTAINER (frame), wint.preview);

  g_signal_connect (wint.preview, "button_press_event",
                    G_CALLBACK (preview_button_press_event),
                    NULL);
  g_signal_connect (wint.preview, "button_release_event",
                    G_CALLBACK (preview_button_release_event),
                    NULL);
  g_signal_connect (wint.preview, "motion_notify_event",
                    G_CALLBACK (preview_motion_notify_event),
                    NULL);
  g_signal_connect (wint.preview, "leave_notify_event",
                    G_CALLBACK (preview_leave_notify_event),
                    NULL);
  g_signal_connect (wint.preview, "enter_notify_event",
                    G_CALLBACK (preview_enter_notify_event),
                    NULL);

  gtk_widget_set_events (wint.preview, (GDK_BUTTON_PRESS_MASK |
                                        GDK_BUTTON_RELEASE_MASK |
                                        GDK_POINTER_MOTION_MASK |
                                        GDK_LEAVE_NOTIFY_MASK |
                                        GDK_ENTER_NOTIFY_MASK));
  gtk_widget_show (wint.preview);

  toggle = gtk_check_button_new_with_label (_("Realtime Preview"));
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_toggle_update),
                    &wvals.alwayspreview);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), wvals.alwayspreview);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle, _("If you enable this option the preview "
                                     "will be redrawn automatically"), NULL);

  button = gtk_button_new_with_label (_("Redraw"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_redraw_callback),
                    dialog);
  gtk_widget_show (button);
  gimp_help_set_help_data (button, _("Redraw preview"), NULL);

  /*  Zoom Options  */
  frame = gimp_frame_new (_("Zoom"));
  gtk_box_pack_start (GTK_BOX (left_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  button = gtk_button_new_from_stock (GTK_STOCK_ZOOM_IN);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_step_in_callback),
                    dialog);

  button = gtk_button_new_from_stock (GTK_STOCK_ZOOM_OUT);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_step_out_callback),
                    dialog);

  button = gtk_button_new_from_stock (GTK_STOCK_UNDO);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  gimp_help_set_help_data (button, _("Undo last zoom"), NULL);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_undo_zoom_callback),
                    dialog);

  button = gtk_button_new_from_stock (GTK_STOCK_REDO);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  gimp_help_set_help_data (button, _("Redo last zoom"), NULL);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_redo_zoom_callback),
                    dialog);

  /*  Create notebook  */
  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (top_hbox), notebook, FALSE, FALSE, 0);
  gtk_widget_show (notebook);

  /*  "Parameters" page  */
  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox,
                            gtk_label_new_with_mnemonic (_("_Parameters")));
  gtk_widget_show (vbox);

  frame = gimp_frame_new (_("Fractal Parameters"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (8, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacing (GTK_TABLE (table), 6, 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  elements->xmin =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                          _("XMIN:"), SCALE_WIDTH, 10,
                          wvals.xmin, -3, 3, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the first (minimal) x-coordinate "
                            "delimitation"), NULL);
  g_signal_connect (elements->xmin, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.xmin);

  elements->xmax =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                          _("XMAX:"), SCALE_WIDTH, 10,
                          wvals.xmax, -3, 3, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the second (maximal) x-coordinate "
                            "delimitation"), NULL);
  g_signal_connect (elements->xmax, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.xmax);

  elements->ymin =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                          _("YMIN:"), SCALE_WIDTH, 10,
                          wvals.ymin, -3, 3, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the first (minimal) y-coordinate "
                            "delimitation"), NULL);
  g_signal_connect (elements->ymin, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.ymin);

  elements->ymax =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 3,
                          _("YMAX:"), SCALE_WIDTH, 10,
                          wvals.ymax, -3, 3, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the second (maximal) y-coordinate "
                            "delimitation"), NULL);
  g_signal_connect (elements->ymax, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.ymax);

  elements->iter =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 4,
                          _("ITER:"), SCALE_WIDTH, 10,
                          wvals.iter, 0, 1000, 1, 10, 5,
                          TRUE, 0, 0,
                          _("Change the iteration value. The higher it "
                            "is, the more details will be calculated, "
                            "which will take more time"), NULL);
  g_signal_connect (elements->iter, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.iter);

  elements->cx =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 5,
                          _("CX:"), SCALE_WIDTH, 10,
                          wvals.cx, -2.5, 2.5, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the CX value (changes aspect of "
                            "fractal, active with every fractal but "
                            "Mandelbrot and Sierpinski)"), NULL);
  g_signal_connect (elements->cx, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.cx);

  elements->cy =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 6,
                          _("CY:"), SCALE_WIDTH, 10,
                          wvals.cy, -2.5, 2.5, 0.001, 0.01, 5,
                          TRUE, 0, 0,
                          _("Change the CY value (changes aspect of "
                            "fractal, active with every fractal but "
                            "Mandelbrot and Sierpinski)"), NULL);
  g_signal_connect (elements->cy, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.cy);

  hbbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach_defaults (GTK_TABLE (table), hbbox, 0, 3, 7, 8);
  gtk_widget_show (hbbox);

  button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (create_load_file_chooser),
                    dialog);
  gtk_widget_show (button);
  gimp_help_set_help_data (button, _("Load a fractal from file"), NULL);

  button = gtk_button_new_from_stock (GIMP_STOCK_RESET);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (dialog_reset_callback),
                    dialog);
  gtk_widget_show (button);
  gimp_help_set_help_data (button, _("Reset parameters to default values"),
                           NULL);

  button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (create_save_file_chooser),
                    dialog);
  gtk_widget_show (button);
  gimp_help_set_help_data (button, _("Save active fractal to file"), NULL);

  /*  Fractal type toggle box  */
  frame = gimp_frame_new (_("Fractal Type"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  toggle_vbox =
    gimp_int_radio_group_new (FALSE, NULL,
                              G_CALLBACK (explorer_radio_update),
                              &wvals.fractaltype, wvals.fractaltype,

                              _("Mandelbrot"), TYPE_MANDELBROT,
                              &(elements->type[TYPE_MANDELBROT]),
                              _("Julia"),      TYPE_JULIA,
                              &(elements->type[TYPE_JULIA]),
                              _("Barnsley 1"), TYPE_BARNSLEY_1,
                              &(elements->type[TYPE_BARNSLEY_1]),
                              _("Barnsley 2"), TYPE_BARNSLEY_2,
                              &(elements->type[TYPE_BARNSLEY_2]),
                              _("Barnsley 3"), TYPE_BARNSLEY_3,
                              &(elements->type[TYPE_BARNSLEY_3]),
                              _("Spider"),     TYPE_SPIDER,
                              &(elements->type[TYPE_SPIDER]),
                              _("Man'o'war"),  TYPE_MAN_O_WAR,
                              &(elements->type[TYPE_MAN_O_WAR]),
                              _("Lambda"),     TYPE_LAMBDA,
                              &(elements->type[TYPE_LAMBDA]),
                              _("Sierpinski"), TYPE_SIERPINSKI,
                              &(elements->type[TYPE_SIERPINSKI]),

                              NULL);

  toggle_vbox2 = gtk_vbox_new (FALSE, 2);
  for (i = TYPE_BARNSLEY_2; i <= TYPE_SPIDER; i++)
    {
      g_object_ref (elements->type[i]);

      gtk_widget_hide (elements->type[i]);
      gtk_container_remove (GTK_CONTAINER (toggle_vbox), elements->type[i]);
      gtk_box_pack_start (GTK_BOX (toggle_vbox2), elements->type[i],
                          FALSE, FALSE, 0);
      gtk_widget_show (elements->type[i]);

      g_object_unref (elements->type[i]);
    }

  toggle_vbox3 = gtk_vbox_new (FALSE, 2);
  for (i = TYPE_MAN_O_WAR; i <= TYPE_SIERPINSKI; i++)
    {
      g_object_ref (elements->type[i]);

      gtk_widget_hide (elements->type[i]);
      gtk_container_remove (GTK_CONTAINER (toggle_vbox), elements->type[i]);
      gtk_box_pack_start (GTK_BOX (toggle_vbox3), elements->type[i],
                          FALSE, FALSE, 0);
      gtk_widget_show (elements->type[i]);

      g_object_unref (elements->type[i]);
    }

  gtk_box_pack_start (GTK_BOX (hbox), toggle_vbox, FALSE, FALSE, 0);
  gtk_widget_show (toggle_vbox);

  gtk_box_pack_start (GTK_BOX (hbox), toggle_vbox2, FALSE, FALSE, 0);
  gtk_widget_show (toggle_vbox2);

  gtk_box_pack_start (GTK_BOX (hbox), toggle_vbox3, FALSE, FALSE, 0);
  gtk_widget_show (toggle_vbox3);

  /*  Color page  */
  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox,
                            gtk_label_new_with_mnemonic (_("Co_lors")));
  gtk_widget_show (vbox);

  /*  Number of Colors frame  */
  frame = gimp_frame_new (_("Number of Colors"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  elements->ncol =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                          _("Number of colors:"), SCALE_WIDTH, 0,
                          wvals.ncolors, 2, MAXNCOLORS, 1, 10, 0,
                          TRUE, 0, 0,
                          _("Change the number of colors in the mapping"),
                          NULL);
  g_signal_connect (elements->ncol, "value_changed",
                    G_CALLBACK (explorer_number_of_colors_callback),
                    &wvals.ncolors);

  elements->useloglog = toggle =
    gtk_check_button_new_with_label (_("Use loglog smoothing"));
  gtk_table_attach_defaults (GTK_TABLE (table), toggle, 0, 3, 1, 2);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_toggle_update),
                    &wvals.useloglog);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), wvals.useloglog);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle, _("Use log log smoothing to eliminate "
                                     "\"banding\" in the result"), NULL);

  /*  Color Density frame  */
  frame = gimp_frame_new (_("Color Density"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  elements->red =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                          _("Red:"), SCALE_WIDTH, 0,
                          wvals.redstretch, 0, 1, 0.01, 0.1, 2,
                          TRUE, 0, 0,
                          _("Change the intensity of the red channel"), NULL);
  g_signal_connect (elements->red, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.redstretch);

  elements->green =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
                          _("Green:"), SCALE_WIDTH, 0,
                          wvals.greenstretch, 0, 1, 0.01, 0.1, 2,
                          TRUE, 0, 0,
                          _("Change the intensity of the green channel"), NULL);
  g_signal_connect (elements->green, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.greenstretch);

  elements->blue =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
                          _("Blue:"), SCALE_WIDTH, 0,
                          wvals.bluestretch, 0, 1, 0.01, 0.1, 2,
                          TRUE, 0, 0,
                          _("Change the intensity of the blue channel"), NULL);
  g_signal_connect (elements->blue, "value_changed",
                    G_CALLBACK (explorer_double_adjustment_update),
                    &wvals.bluestretch);

  /*  Color Function frame  */
  frame = gimp_frame_new (_("Color Function"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  /*  Redmode radio frame  */
  frame = gimp_int_radio_group_new (TRUE, _("Red"),
                                    G_CALLBACK (explorer_radio_update),
                                    &wvals.redmode, wvals.redmode,

                                    _("Sine"),         SINUS,
                                    &elements->redmode[SINUS],
                                    _("Cosine"),       COSINUS,
                                    &elements->redmode[COSINUS],
                                    _("None"),         NONE,
                                    &elements->redmode[NONE],

                                    NULL);
  gimp_help_set_help_data (elements->redmode[SINUS],
                           _("Use sine-function for this color component"),
                           NULL);
  gimp_help_set_help_data (elements->redmode[COSINUS],
                           _("Use cosine-function for this color "
                             "component"), NULL);
  gimp_help_set_help_data (elements->redmode[NONE],
                           _("Use linear mapping instead of any "
                             "trigonometrical function for this color "
                             "channel"), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  toggle_vbox = GTK_BIN (frame)->child;

  elements->redinvert = toggle =
    gtk_check_button_new_with_label (_("Inversion"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), wvals.redinvert);
  gtk_box_pack_start (GTK_BOX (toggle_vbox), toggle, FALSE, FALSE, 0);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_toggle_update),
                    &wvals.redinvert);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle,
                           _("If you enable this option higher color values "
                             "will be swapped with lower ones and vice "
                             "versa"), NULL);

  /*  Greenmode radio frame  */
  frame = gimp_int_radio_group_new (TRUE, _("Green"),
                                    G_CALLBACK (explorer_radio_update),
                                    &wvals.greenmode, wvals.greenmode,

                                    _("Sine"),           SINUS,
                                    &elements->greenmode[SINUS],
                                    _("Cosine"),         COSINUS,
                                    &elements->greenmode[COSINUS],
                                    _("None"),           NONE,
                                    &elements->greenmode[NONE],

                                    NULL);
  gimp_help_set_help_data (elements->greenmode[SINUS],
                           _("Use sine-function for this color component"),
                           NULL);
  gimp_help_set_help_data (elements->greenmode[COSINUS],
                           _("Use cosine-function for this color "
                             "component"), NULL);
  gimp_help_set_help_data (elements->greenmode[NONE],
                           _("Use linear mapping instead of any "
                             "trigonometrical function for this color "
                             "channel"), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  toggle_vbox = GTK_BIN (frame)->child;

  elements->greeninvert = toggle =
    gtk_check_button_new_with_label (_("Inversion"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), wvals.greeninvert);
  gtk_box_pack_start (GTK_BOX (toggle_vbox), toggle, FALSE, FALSE, 0);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_toggle_update),
                    &wvals.greeninvert);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle,
                           _("If you enable this option higher color values "
                             "will be swapped with lower ones and vice "
                             "versa"), NULL);

  /*  Bluemode radio frame  */
  frame = gimp_int_radio_group_new (TRUE, _("Blue"),
                                    G_CALLBACK (explorer_radio_update),
                                    &wvals.bluemode, wvals.bluemode,

                                    _("Sine"),          SINUS,
                                    &elements->bluemode[SINUS],
                                    _("Cosine"),        COSINUS,
                                    &elements->bluemode[COSINUS],
                                    _("None"),          NONE,
                                    &elements->bluemode[NONE],

                                    NULL);
  gimp_help_set_help_data (elements->bluemode[SINUS],
                           _("Use sine-function for this color component"),
                           NULL);
  gimp_help_set_help_data (elements->bluemode[COSINUS],
                           _("Use cosine-function for this color "
                             "component"), NULL);
  gimp_help_set_help_data (elements->bluemode[NONE],
                           _("Use linear mapping instead of any "
                             "trigonometrical function for this color "
                             "channel"), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  toggle_vbox = GTK_BIN (frame)->child;

  elements->blueinvert = toggle =
    gtk_check_button_new_with_label (_("Inversion"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON( toggle), wvals.blueinvert);
  gtk_box_pack_start (GTK_BOX (toggle_vbox), toggle, FALSE, FALSE, 0);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_toggle_update),
                    &wvals.blueinvert);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle,
                           _("If you enable this option higher color values "
                             "will be swapped with lower ones and vice "
                             "versa"), NULL);

  /*  Colormode toggle box  */
  frame = gimp_frame_new (_("Color Mode"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  toggle_vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), toggle_vbox);
  gtk_widget_show (toggle_vbox);

  toggle = elements->colormode[0] =
    gtk_radio_button_new_with_label (group, _("As specified above"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (toggle_vbox), toggle, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (0));
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_radio_update),
                    &wvals.colormode);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                wvals.colormode == 0);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle,
                           _("Create a color-map with the options you "
                             "specified above (color density/function). The "
                             "result is visible in the preview image"), NULL);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (toggle_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  toggle = elements->colormode[1] =
    gtk_radio_button_new_with_label (group,
                                     _("Apply active gradient to final image"));
  group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (toggle));
  gtk_box_pack_start (GTK_BOX (hbox), toggle, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (toggle), "gimp-item-data",
                     GINT_TO_POINTER (1));
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (explorer_radio_update),
                    &wvals.colormode);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                wvals.colormode == 1);
  gtk_widget_show (toggle);
  gimp_help_set_help_data (toggle,
                           _("Create a color-map using a gradient from "
                             "the gradient editor"), NULL);

  gradient_name = gimp_context_get_gradient ();

  gimp_gradient_get_uniform_samples (gradient_name,
                                     wvals.ncolors,
                                     wvals.gradinvert,
                                     &n_gradient_samples,
                                     &gradient_samples);

  gradient = gimp_gradient_select_widget_new (_("FractalExplorer Gradient"),
                                              gradient_name,
                                              explorer_gradient_select_callback,
                                              NULL);
  g_free (gradient_name);
  gtk_box_pack_start (GTK_BOX (hbox), gradient, FALSE, FALSE, 0);
  gtk_widget_show (gradient);

  abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  {
    gint xsize, ysize;

    for (ysize = 1; ysize * ysize * ysize < 8192; ysize++) /**/;
    xsize = wvals.ncolors / ysize;
    while (xsize * ysize < 8192) xsize++;

    gtk_widget_set_size_request (abox, xsize, ysize * 4);
  }
  gtk_box_pack_start (GTK_BOX (toggle_vbox), abox, FALSE, FALSE, 0);
  gtk_widget_show (abox);

  cmap_preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (cmap_preview, 32, 32);
  gtk_container_add (GTK_CONTAINER (abox), cmap_preview);
  g_signal_connect (cmap_preview, "size_allocate",
                    G_CALLBACK (cmap_preview_size_allocate), NULL);
  gtk_widget_show (cmap_preview);

  frame = add_objects_list ();
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame,
                            gtk_label_new_with_mnemonic (_("_Fractals")));
  gtk_widget_show (frame);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);

  gtk_widget_show (dialog);
  ready_now = TRUE;

  set_cmap_preview ();
  dialog_update_preview ();

  gtk_main ();
  gdk_flush ();

  g_free (wint.wimage);

  return wint.run;
}


/**********************************************************************
 FUNCTION: dialog_update_preview
 *********************************************************************/

void
dialog_update_preview (void)
{
  gdouble  left;
  gdouble  right;
  gdouble  bottom;
  gdouble  top;
  gdouble  dx;
  gdouble  dy;
  gdouble  cx;
  gdouble  cy;
  gint     px;
  gint     py;
  gint     xcoord;
  gint     ycoord;
  gint     iteration;
  guchar  *p_ul;
  gdouble  a;
  gdouble  b;
  gdouble  x;
  gdouble  y;
  gdouble  oldx;
  gdouble  oldy;
  gdouble  foldxinitx;
  gdouble  foldxinity;
  gdouble  tempsqrx;
  gdouble  tempsqry;
  gdouble  tmpx = 0;
  gdouble  tmpy = 0;
  gdouble  foldyinitx;
  gdouble  foldyinity;
  gdouble  adjust;
  gdouble  xx = 0;
  gint     zaehler;
  gint     color;
  gint     useloglog;

  if (NULL == wint.preview)
    return;

  if (ready_now && wvals.alwayspreview)
    {
      left = sel_x1;
      right = sel_x2 - 1;
      bottom = sel_y2 - 1;
      top = sel_y1;
      dx = (right - left) / (preview_width - 1);
      dy = (bottom - top) / (preview_height - 1);

      xmin = wvals.xmin;
      xmax = wvals.xmax;
      ymin = wvals.ymin;
      ymax = wvals.ymax;
      cx = wvals.cx;
      cy = wvals.cy;
      xbild = preview_width;
      ybild = preview_height;
      xdiff = (xmax - xmin) / xbild;
      ydiff = (ymax - ymin) / ybild;

      py = 0;

      p_ul = wint.wimage;
      iteration = (int) wvals.iter;
      useloglog = wvals.useloglog;
      for (ycoord = 0; ycoord < preview_height; ycoord++)
        {
          px = 0;

          for (xcoord = 0; xcoord < preview_width; xcoord++)
            {
              a = (double) xmin + (double) xcoord *xdiff;
              b = (double) ymin + (double) ycoord *ydiff;

              if (wvals.fractaltype != TYPE_MANDELBROT)
                {
                  tmpx = x = a;
                  tmpy = y = b;
                }
              else
                {
                  x = 0;
                  y = 0;
                }
              for (zaehler = 0;
                   (zaehler < iteration) && ((x * x + y * y) < 4);
                   zaehler++)
                {
                  oldx = x;
                  oldy = y;

                  switch (wvals.fractaltype)
                    {
                    case TYPE_MANDELBROT:
                      xx = x * x - y * y + a;
                      y = 2.0 * x * y + b;
                      break;

                    case TYPE_JULIA:
                      xx = x * x - y * y + cx;
                      y = 2.0 * x * y + cy;
                      break;

                    case TYPE_BARNSLEY_1:
                      foldxinitx = oldx * cx;
                      foldyinity = oldy * cy;
                      foldxinity = oldx * cy;
                      foldyinitx = oldy * cx;
                      /* orbit calculation */
                      if (oldx >= 0)
                        {
                          xx = (foldxinitx - cx - foldyinity);
                          y  = (foldyinitx - cy + foldxinity);
                        }
                      else
                        {
                          xx = (foldxinitx + cx - foldyinity);
                          y  = (foldyinitx + cy + foldxinity);
                        }
                      break;

                    case TYPE_BARNSLEY_2:
                      foldxinitx = oldx * cx;
                      foldyinity = oldy * cy;
                      foldxinity = oldx * cy;
                      foldyinitx = oldy * cx;
                      /* orbit calculation */
                      if (foldxinity + foldyinitx >= 0)
                        {
                          xx = foldxinitx - cx - foldyinity;
                          y  = foldyinitx - cy + foldxinity;
                        }
                      else
                        {
                          xx = foldxinitx + cx - foldyinity;
                          y  = foldyinitx + cy + foldxinity;
                        }
                      break;

                    case TYPE_BARNSLEY_3:
                      foldxinitx  = oldx * oldx;
                      foldyinity  = oldy * oldy;
                      foldxinity  = oldx * oldy;
                      /* orbit calculation */
                      if(oldx > 0)
                        {
                          xx = foldxinitx - foldyinity - 1.0;
                          y  = foldxinity * 2;
                        }
                      else
                        {
                          xx = foldxinitx - foldyinity -1.0 + cx * oldx;
                          y  = foldxinity * 2;
                          y += cy * oldx;
                        }
                      break;

                    case TYPE_SPIDER:
                      /* { c=z=pixel: z=z*z+c; c=c/2+z, |z|<=4 } */
                      xx = x*x - y*y + tmpx + cx;
                      y = 2 * oldx * oldy + tmpy +cy;
                      tmpx = tmpx/2 + xx;
                      tmpy = tmpy/2 + y;
                      break;

                    case TYPE_MAN_O_WAR:
                      xx = x*x - y*y + tmpx + cx;
                      y = 2.0 * x * y + tmpy + cy;
                      tmpx = oldx;
                      tmpy = oldy;
                      break;

                    case TYPE_LAMBDA:
                      tempsqrx = x * x;
                      tempsqry = y * y;
                      tempsqrx = oldx - tempsqrx + tempsqry;
                      tempsqry = -(oldy * oldx);
                      tempsqry += tempsqry + oldy;
                      xx = cx * tempsqrx - cy * tempsqry;
                      y = cx * tempsqry + cy * tempsqrx;
                      break;

                    case TYPE_SIERPINSKI:
                      xx = oldx + oldx;
                      y = oldy + oldy;
                      if (oldy > .5)
                        y = y - 1;
                      else if (oldx > .5)
                        xx = xx - 1;
                      break;

                    default:
                      break;
                    }

                  x = xx;
                }

              if (useloglog)
                {
                  adjust = log (log (x * x + y * y) / 2) / log (2);
                }
              else
                {
                  adjust = 0.0;
                }
              color = (int) (((zaehler - adjust) *
                              (wvals.ncolors - 1)) / iteration);
              p_ul[0] = colormap[color][0];
              p_ul[1] = colormap[color][1];
              p_ul[2] = colormap[color][2];
              p_ul += 3;
              px += 1;
            } /* for */
          py += 1;
        } /* for */

      preview_redraw ();
      gdk_flush ();
    }
}

/**********************************************************************
 FUNCTION: cmap_preview_size_allocate()
 *********************************************************************/

static void
cmap_preview_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  gint             i;
  gint             x;
  gint             y;
  gint             j;
  guchar          *b;
  GimpPreviewArea *preview = GIMP_PREVIEW_AREA (widget);

  b = g_new (guchar, allocation->width * allocation->height * 3);

  for (y = 0; y < allocation->height; y++)
    {
      for (x = 0; x < allocation->width; x++)
        {
          i = x + (y / 4) * allocation->width;
          if (i > wvals.ncolors)
            {
              for (j = 0; j < 3; j++)
                b[(y*allocation->width + x) * 3 + j] = 0;
            }
          else
            {
              for (j = 0; j < 3; j++)
                b[(y*allocation->width + x) * 3 + j] = colormap[i][j];
            }
        }
    }
  gimp_preview_area_draw (preview,
                          0, 0, allocation->width, allocation->height,
                          GIMP_RGB_IMAGE, b, allocation->width*3);
  gtk_widget_queue_draw (cmap_preview);

  g_free (b);

}

/**********************************************************************
 FUNCTION: set_cmap_preview()
 *********************************************************************/

void
set_cmap_preview (void)
{
  gint    xsize, ysize;

  if (NULL == cmap_preview)
    return;

  make_color_map ();

  for (ysize = 1; ysize * ysize * ysize < wvals.ncolors; ysize++)
    /**/;
  xsize = wvals.ncolors / ysize;
  while (xsize * ysize < wvals.ncolors)
    xsize++;

  gtk_widget_set_size_request (cmap_preview, xsize, ysize * 4);
}

/**********************************************************************
 FUNCTION: make_color_map()
 *********************************************************************/

void
make_color_map (void)
{
  gint     i;
  gint     j;
  gint     r;
  gint     gr;
  gint     bl;
  gdouble  redstretch;
  gdouble  greenstretch;
  gdouble  bluestretch;
  gdouble  pi = atan (1) * 4;

  /*  get gradient samples if they don't exist -- fixes gradient color
   *  mode for noninteractive use (bug #103470).
   */
  if (gradient_samples == NULL)
    {
      gchar *gradient_name = gimp_context_get_gradient ();

      gimp_gradient_get_uniform_samples (gradient_name,
                                         wvals.ncolors,
                                         wvals.gradinvert,
                                         &n_gradient_samples,
                                         &gradient_samples);

      g_free (gradient_name);
    }

  redstretch   = wvals.redstretch * 127.5;
  greenstretch = wvals.greenstretch * 127.5;
  bluestretch  = wvals.bluestretch * 127.5;

  for (i = 0; i < wvals.ncolors; i++)
    if (wvals.colormode == 1)
      {
        for (j = 0; j < 3; j++)
          colormap[i][j] = (int) (gradient_samples[i * 4 + j] * 255.0);
      }
    else
      {
        double x = (i*2.0) / wvals.ncolors;
        r = gr = bl = 0;

        switch (wvals.redmode)
          {
          case SINUS:
            r = (int) redstretch *(1.0 + sin((x - 1) * pi));
            break;
          case COSINUS:
            r = (int) redstretch *(1.0 + cos((x - 1) * pi));
            break;
          case NONE:
            r = (int)(redstretch *(x));
            break;
          default:
            break;
          }

        switch (wvals.greenmode)
          {
          case SINUS:
            gr = (int) greenstretch *(1.0 + sin((x - 1) * pi));
            break;
          case COSINUS:
            gr = (int) greenstretch *(1.0 + cos((x - 1) * pi));
            break;
          case NONE:
            gr = (int)(greenstretch *(x));
            break;
          default:
            break;
          }

        switch (wvals.bluemode)
          {
          case SINUS:
            bl = (int) bluestretch * (1.0 + sin ((x - 1) * pi));
            break;
          case COSINUS:
            bl = (int) bluestretch * (1.0 + cos ((x - 1) * pi));
            break;
          case NONE:
            bl = (int) (bluestretch * x);
            break;
          default:
            break;
          }

        r  = MIN (r,  255);
        gr = MIN (gr, 255);
        bl = MIN (bl, 255);

        if (wvals.redinvert)
          r = 255 - r;

        if (wvals.greeninvert)
          gr = 255 - gr;

        if (wvals.blueinvert)
          bl = 255 - bl;

        colormap[i][0] = r;
        colormap[i][1] = gr;
        colormap[i][2] = bl;
      }
}

/**********************************************************************
 FUNCTION: explorer_logo_dialog
 *********************************************************************/

static void
logo_preview_size_allocate (GtkWidget *preview)
{
  guchar *temp;
  guchar *temp2;
  guchar *datapointer;
  gint    x, y;

  temp2 = temp = g_new (guchar, logo_height*(logo_width + 10) * 3);
  datapointer = header_data + logo_width * logo_height - 1;

  for (y = 0; y < logo_height; y++)
  {
    for (x = 0; x < logo_width; x++)
    {
      HEADER_PIXEL (datapointer, temp2);
      temp2 += 3;
    }
  }
  gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview), 0, 0, 
      logo_width, logo_height, GIMP_RGB_IMAGE,
      temp, logo_width * 3);
  g_free (temp);
}

/**********************************************************************
 FUNCTION: explorer_logo_dialog
 *********************************************************************/

static void
explorer_logo_dialog (GtkWidget *parent)
{
  static GtkWidget *dialog = NULL;

  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *abox;
  GtkWidget *preview;
  GtkWidget *frame;

  if (dialog)
    {
      gtk_window_present (GTK_WINDOW (dialog));
      return;
    }

  dialog = gimp_dialog_new (_("About"), "fractalexplorer",
                            parent, 0,
                            gimp_standard_help_func, HELP_ID,

                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,

                            NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &dialog);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /*  The logo frame & drawing area  */
  abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), abox, FALSE, FALSE, 0);
  gtk_widget_show (abox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (abox), frame);
  gtk_widget_show (frame);

  preview = gimp_preview_area_new ();
  gtk_widget_set_size_request (preview, logo_width, logo_height);
  gtk_container_add (GTK_CONTAINER (frame), preview);
  gtk_widget_show (preview);
  g_signal_connect (preview, "size_allocate",
                    G_CALLBACK (logo_preview_size_allocate), NULL);


  label = gtk_label_new ("Fractal Chaos Explorer\n"
                         "Plug-In for the GIMP\n"
                         "Version 2.00 (Multilingual)");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("\nContains code from:\n\n"
                         "Daniel Cotting  <cotting@mygale.org>\n"
                         "Peter Kirchgessner  <Pkirchg@aol.com>\n"
                         "Scott Draves  <spot@cs.cmu.edu>\n"
                         "Andy Thomas  <alt@picnic.demon.co.uk>\n"
                         "and the GIMP distribution.");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_widget_show (dialog);
}

/**********************************************************************
 FUNCTION: dialog_change_scale
 *********************************************************************/

void
dialog_change_scale (void)
{
  ready_now = FALSE;

  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->xmin), wvals.xmin);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->xmax), wvals.xmax);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->ymin), wvals.ymin);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->ymax), wvals.ymax);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->iter), wvals.iter);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->cx),   wvals.cx);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->cy),   wvals.cy);

  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->red),  wvals.redstretch);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->green),wvals.greenstretch);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (elements->blue), wvals.bluestretch);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (elements->type[wvals.fractaltype]), TRUE);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (elements->redmode[wvals.redmode]), TRUE);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (elements->greenmode[wvals.greenmode]), TRUE);
  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (elements->bluemode[wvals.bluemode]), TRUE);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (elements->redinvert),
                                wvals.redinvert);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (elements->greeninvert),
                                wvals.greeninvert);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (elements->blueinvert),
                                wvals.blueinvert);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (elements->colormode[wvals.colormode]), TRUE);

  ready_now = TRUE;
}


/**********************************************************************
 FUNCTION: save_options
 *********************************************************************/

static void
save_options (FILE * fp)
{
  /* Save options */

  fprintf (fp, "fractaltype: %i\n", wvals.fractaltype);
  fprintf (fp, "xmin: %0.15f\n", wvals.xmin);
  fprintf (fp, "xmax: %0.15f\n", wvals.xmax);
  fprintf (fp, "ymin: %0.15f\n", wvals.ymin);
  fprintf (fp, "ymax: %0.15f\n", wvals.ymax);
  fprintf (fp, "iter: %0.15f\n", wvals.iter);
  fprintf (fp, "cx: %0.15f\n", wvals.cx);
  fprintf (fp, "cy: %0.15f\n", wvals.cy);
  fprintf (fp, "redstretch: %0.15f\n", wvals.redstretch * 128.0);
  fprintf (fp, "greenstretch: %0.15f\n", wvals.greenstretch * 128.0);
  fprintf (fp, "bluestretch: %0.15f\n", wvals.bluestretch * 128.0);
  fprintf (fp, "redmode: %i\n", wvals.redmode);
  fprintf (fp, "greenmode: %i\n", wvals.greenmode);
  fprintf (fp, "bluemode: %i\n", wvals.bluemode);
  fprintf (fp, "redinvert: %i\n", wvals.redinvert);
  fprintf (fp, "greeninvert: %i\n", wvals.greeninvert);
  fprintf (fp, "blueinvert: %i\n", wvals.blueinvert);
  fprintf (fp, "colormode: %i\n", wvals.colormode);
  fputs ("#**********************************************************************\n", fp);
  fprintf(fp, "<EOF>\n");
  fputs ("#**********************************************************************\n", fp);
}

static void
save_callback (void)
{
  FILE  *fp;
  gchar *savename;

  savename = filename;

  fp = fopen (savename, "wt+");

  if (!fp)
    {
      g_message (_("Could not open '%s' for writing: %s"),
                 gimp_filename_to_utf8 (savename), g_strerror (errno));
      return;
    }

  /* Write header out */
  fputs (FRACTAL_HEADER, fp);
  fputs ("#**********************************************************************\n", fp);
  fputs ("# This is a data file for the Fractal Explorer plug-in for the GIMP   *\n", fp);
  fputs ("#**********************************************************************\n", fp);

  save_options (fp);

  if (ferror (fp))
    g_message (_("Could not write '%s': %s"),
               gimp_filename_to_utf8 (savename), g_strerror (ferror (fp)));

  fclose (fp);
}

static void
save_file_chooser_response (GtkFileChooser *chooser,
                            gint            response_id,
                            gpointer        data)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      filename = gtk_file_chooser_get_filename (chooser);

      save_callback ();
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
load_file_chooser_response (GtkFileChooser *chooser,
                            gint            response_id,
                            gpointer        data)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      filename = gtk_file_chooser_get_filename (chooser);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        {
          explorer_load ();
        }

      gtk_widget_show (maindlg);
      dialog_change_scale ();
      set_cmap_preview ();
      dialog_update_preview ();
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
create_load_file_chooser (GtkWidget *widget,
                          GtkWidget *dialog)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window =
        gtk_file_chooser_dialog_new (_("Load Fractal Parameters"),
                                     GTK_WINDOW (dialog),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,

                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_OPEN,   GTK_RESPONSE_OK,

                                     NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_OK);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);
      g_signal_connect (window, "response",
                        G_CALLBACK (load_file_chooser_response),
                        window);
    }

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_save_file_chooser (GtkWidget *widget,
                          GtkWidget *dialog)
{
  static GtkWidget *window = NULL;

  if (! window)
    {
      window =
        gtk_file_chooser_dialog_new (_("Save Fractal Parameters"),
                                     GTK_WINDOW (dialog),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,

                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_SAVE,   GTK_RESPONSE_OK,

                                     NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_OK);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);
      g_signal_connect (window, "response",
                        G_CALLBACK (save_file_chooser_response),
                        window);
    }

  if (tpath)
    {
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (window), tpath);
    }
  else if (fractalexplorer_path)
    {
      GList *path_list;
      gchar *dir;

      path_list = gimp_path_parse (fractalexplorer_path, 16, FALSE, NULL);

      dir = gimp_path_get_user_writable_dir (path_list);

      if (!dir)
        dir = g_strdup (gimp_directory ());

      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (window), dir);

      g_free (dir);
      gimp_path_free (path_list);
    }
  else
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (window), "/tmp");
    }

  gtk_window_present (GTK_WINDOW (window));
}

gchar*
get_line (gchar *buf,
          gint   s,
          FILE  *from,
          gint   init)
{
  gint   slen;
  gchar *ret;

  if (init)
    line_no = 1;
  else
    line_no++;

  do
    {
      ret = fgets (buf, s, from);
    }
  while (!ferror (from) && buf[0] == '#');

  slen = strlen (buf);

  /* The last newline is a pain */
  if (slen > 0)
    buf[slen - 1] = '\0';

  if (ferror (from))
    {
      g_warning ("Error reading file");
      return NULL;
    }

  return ret;
}

gint
load_options (fractalexplorerOBJ *xxx,
              FILE               *fp)
{
  gchar load_buf[MAX_LOAD_LINE];
  gchar str_buf[MAX_LOAD_LINE];
  gchar opt_buf[MAX_LOAD_LINE];

  /*  default values  */
  xxx->opts = standardvals;
  xxx->opts.gradinvert    = FALSE;

  get_line (load_buf, MAX_LOAD_LINE, fp, 0);

  while (!feof (fp) && strcmp (load_buf, "<EOF>"))
    {
      /* Get option name */
      sscanf (load_buf, "%s %s", str_buf, opt_buf);

      if (!strcmp (str_buf, "fractaltype:"))
        {
          gint sp = 0;

          sp = atoi (opt_buf);
          if (sp < 0)
            return -1;
          xxx->opts.fractaltype = sp;
        }
      else if (!strcmp (str_buf, "xmin:"))
        {
          xxx->opts.xmin = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp (str_buf, "xmax:"))
        {
          xxx->opts.xmax = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp(str_buf, "ymin:"))
        {
          xxx->opts.ymin = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp (str_buf, "ymax:"))
        {
          xxx->opts.ymax = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp(str_buf, "redstretch:"))
        {
          gdouble sp = g_ascii_strtod (opt_buf, NULL);
          xxx->opts.redstretch = sp / 128.0;
        }
      else if (!strcmp(str_buf, "greenstretch:"))
        {
          gdouble sp = g_ascii_strtod (opt_buf, NULL);
          xxx->opts.greenstretch = sp / 128.0;
        }
      else if (!strcmp (str_buf, "bluestretch:"))
        {
          gdouble sp = g_ascii_strtod (opt_buf, NULL);
          xxx->opts.bluestretch = sp / 128.0;
        }
      else if (!strcmp (str_buf, "iter:"))
        {
          xxx->opts.iter = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp(str_buf, "cx:"))
        {
          xxx->opts.cx = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp (str_buf, "cy:"))
        {
          xxx->opts.cy = g_ascii_strtod (opt_buf, NULL);
        }
      else if (!strcmp(str_buf, "redmode:"))
        {
          xxx->opts.redmode = atoi (opt_buf);
        }
      else if (!strcmp(str_buf, "greenmode:"))
        {
          xxx->opts.greenmode = atoi (opt_buf);
        }
      else if (!strcmp(str_buf, "bluemode:"))
        {
          xxx->opts.bluemode = atoi (opt_buf);
        }
      else if (!strcmp (str_buf, "redinvert:"))
        {
          xxx->opts.redinvert = atoi (opt_buf);
        }
      else if (!strcmp (str_buf, "greeninvert:"))
        {
          xxx->opts.greeninvert = atoi (opt_buf);
        }
      else if (!strcmp(str_buf, "blueinvert:"))
        {
          xxx->opts.blueinvert = atoi (opt_buf);
        }
      else if (!strcmp (str_buf, "colormode:"))
        {
          xxx->opts.colormode = atoi (opt_buf);
        }

      get_line (load_buf, MAX_LOAD_LINE, fp, 0);
    }

  return 0;
}

void
explorer_load (void)
{
  FILE  *fp;
  gchar  load_buf[MAX_LOAD_LINE];

  g_assert (filename != NULL);
  fp = fopen (filename, "rt");

  if (!fp)
    {
      g_message (_("Could not open '%s' for reading: %s"),
                 gimp_filename_to_utf8 (filename), g_strerror (errno));
      return;
    }
  get_line (load_buf, MAX_LOAD_LINE, fp, 1);

  if (strncmp (FRACTAL_HEADER, load_buf, strlen (load_buf)))
    {
      g_message (_("'%s' is not a FractalExplorer file"),
                 gimp_filename_to_utf8 (filename));
      return;
    }
  if (load_options (current_obj,fp))
    {
      g_message (_("'%s' is corrupt. Line %d Option section incorrect"),
                 gimp_filename_to_utf8 (filename), line_no);
      return;
    }

  wvals = current_obj->opts;

  fclose (fp);
}