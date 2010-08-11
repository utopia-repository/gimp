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
#include <stdlib.h>
#include "appenv.h"
#include "actionarea.h"
#include "app_procs.h"
#include "buildmenu.h"
#include "colormaps.h"
#include "color_area.h"
#include "commands.h"
#include "disp_callbacks.h"
#include "errors.h"
#include "gdisplay.h"
#include "gdisplay_ops.h"
#include "gimage.h"
#include "gimprc.h"
#include "general.h"
#include "interface.h"
#include "menus.h"
#include "tools.h"

#include "pixmaps.h"


/*  local functions  */
static void  tools_select_update   (GtkWidget *widget,
				    gpointer   data);
static gint  tools_button_press    (GtkWidget *widget,
				    GdkEventButton *bevent,
				    gpointer   data);
static void  gdisplay_destroy      (GtkWidget *widget,
				    GDisplay  *display);

static gint  gdisplay_delete       (GtkWidget *widget,
				    GdkEvent *,
				    GDisplay  *display);

static void  toolbox_destroy       (void);
static gint  toolbox_delete        (GtkWidget *,
				    GdkEvent *,
				    gpointer);

static GdkPixmap *create_pixmap    (GdkWindow  *parent,
				    GdkBitmap **mask,
				    char      **data,
				    int         width,
				    int         height);

typedef struct _ToolButton ToolButton;

struct _ToolButton
{
  char **icon_data;
  char  *tool_desc;
  gpointer callback_data;
};

static ToolButton tool_data[] =
{
  { (char **) rect_bits,
    "Select rectangular regions",
    (gpointer) RECT_SELECT },
  { (char **) circ_bits,
    "Select elliptical regions",
    (gpointer) ELLIPSE_SELECT },
  { (char **) free_bits,
    "Select hand-drawn regions",
    (gpointer) FREE_SELECT },
  { (char **) fuzzy_bits,
    "Select contiguous regions",
    (gpointer) FUZZY_SELECT },
  { (char **) bezier_bits,
    "Select regions using Bezier curves",
    (gpointer) BEZIER_SELECT },
  { (char **) iscissors_bits,
    "Select shapes from image",
    (gpointer) ISCISSORS },
  { (char **) move_bits,
    "Move layers & selections",
    (gpointer) MOVE },
  { (char **) magnify_bits,
    "Zoom in & out",
    (gpointer) MAGNIFY },
  { (char **) crop_bits,
    "Crop the image",
    (gpointer) CROP },
  { (char **) scale_bits,
    "Transform the layer or selection",
    (gpointer) ROTATE },
  { (char **) horizflip_bits,
    "Flip the layer or selection",
    (gpointer) FLIP_HORZ },
  { (char **) text_bits,
    "Add text to the image",
    (gpointer) TEXT },
  { (char **) colorpicker_bits,
    "Pick colors from the image",
    (gpointer) COLOR_PICKER },
  { (char **) fill_bits,
    "Fill with a color or pattern",
    (gpointer) BUCKET_FILL },
  { (char **) gradient_bits,
    "Fill with a color gradient",
    (gpointer) BLEND },
  { (char **) pencil_bits,
    "Draw sharp pencil strokes",
    (gpointer) PENCIL },
  { (char **) paint_bits,
    "Paint fuzzy brush strokes",
    (gpointer) PAINTBRUSH },
  { (char **) erase_bits,
    "Erase to background or transparency",
    (gpointer) ERASER },
  { (char **) airbrush_bits,
    "Airbrush with variable pressure",
    (gpointer) AIRBRUSH },
  { (char **) clone_bits,
    "Paint using patterns or image regions",
    (gpointer) CLONE },
  { (char **) blur_bits,
    "Blur or sharpen",
    (gpointer) CONVOLVE },
  { NULL,
    NULL,
    (gpointer) BY_COLOR_SELECT },
  { NULL,
    NULL,
    (gpointer) COLOR_BALANCE },
  { NULL,
    NULL,
    (gpointer) BRIGHTNESS_CONTRAST },
  { NULL,
    NULL,
    (gpointer) HUE_SATURATION },
  { NULL,
    NULL,
    (gpointer) POSTERIZE },
  { NULL,
    NULL,
    (gpointer) THRESHOLD },
  { NULL,
    NULL,
    (gpointer) CURVES },
  { NULL,
    NULL,
    (gpointer) LEVELS },
  { NULL,
    NULL,
    (gpointer) HISTOGRAM }
};

static int pixmap_colors[8][3] =
{
  { 0x00, 0x00, 0x00 }, /* a - 0   */
  { 0x24, 0x24, 0x24 }, /* b - 36  */
  { 0x49, 0x49, 0x49 }, /* c - 73  */
  { 0x6D, 0x6D, 0x6D }, /* d - 109 */
  { 0x92, 0x92, 0x92 }, /* e - 146 */
  { 0xB6, 0xB6, 0xB6 }, /* f - 182 */
  { 0xDB, 0xDB, 0xDB }, /* g - 219 */
  { 0xFF, 0xFF, 0xFF }, /* h - 255 */
};

#define NUM_TOOLS (sizeof (tool_data) / sizeof (ToolButton))
#define COLUMNS   3
#define ROWS      7
#define MARGIN    2

/*  Widgets for each tool button--these are used from command.c to activate on
 *  tool selection via both menus and keyboard accelerators.
 */
GtkWidget *tool_widgets[NUM_TOOLS];
GtkWidget *tool_label;
GtkTooltips *tool_tips;

/*  The popup shell is a pointer to the gdisplay shell that posted the latest
 *  popup menu.  When this is null, and a command is invoked, then the
 *  assumption is that the command was a result of a keyboard accelerator
 */
GtkWidget *popup_shell = NULL;


static GtkWidget *tool_label_area = NULL;
static GtkWidget *progress_area = NULL;
static GdkColor colors[12];
static GtkWidget *toolbox_shell = NULL;

static void
tools_select_update (GtkWidget *w,
		     gpointer   data)
{
  ToolType tool_type;

  tool_type = (ToolType) data;

  if ((tool_type != -1) && GTK_TOGGLE_BUTTON (w)->active)
    tools_select (tool_type);
}

static gint
tools_button_press (GtkWidget      *w,
		    GdkEventButton *event,
		    gpointer        data)
{
  if ((event->type == GDK_2BUTTON_PRESS) &&
      (event->button == 1))
    tools_options_dialog_show ();

  return FALSE;
}

static gint
toolbox_delete (GtkWidget *w, GdkEvent *e, gpointer data)
{
  app_exit (0);

  return TRUE;
}

static void
toolbox_destroy ()
{
  app_exit_finish ();
}

static void
gdisplay_destroy (GtkWidget *w,
		  GDisplay  *gdisp)
{
  gdisplay_remove_and_delete (gdisp);
}

static gint
gdisplay_delete (GtkWidget *w,
		 GdkEvent *e,
		 GDisplay *gdisp)
{
  gdisplay_close_window (gdisp, FALSE);

  return TRUE;
}

static void
allocate_colors (GtkWidget *parent)
{
  GdkColormap *colormap;
  int i;

  gtk_widget_realize (parent);
  colormap = gdk_window_get_colormap (parent->window);

  for (i = 0; i < 8; i++)
    {
      colors[i].red = pixmap_colors[i][0] << 8;
      colors[i].green = pixmap_colors[i][1] << 8;
      colors[i].blue = pixmap_colors[i][2] << 8;

      gdk_color_alloc (colormap, &colors[i]);
    }

  colors[8] = parent->style->bg[GTK_STATE_NORMAL];
  gdk_color_alloc (colormap, &colors[8]);

  colors[9] = parent->style->bg[GTK_STATE_ACTIVE];
  gdk_color_alloc (colormap, &colors[9]);

  colors[10] = parent->style->bg[GTK_STATE_PRELIGHT];
  gdk_color_alloc (colormap, &colors[10]);

  /* postit yellow (khaki) as background for tooltips */
  colors[11].red = 61669;
  colors[11].green = 59113;
  colors[11].blue = 35979;
  gdk_color_alloc (colormap, &colors[11]);
}


static void
create_color_area (GtkWidget *parent)
{
  GtkWidget *frame;
  GtkWidget *alignment;
  GtkWidget *col_area;
  GdkPixmap *default_pixmap;
  GdkPixmap *swap_pixmap;

  gtk_widget_realize (parent);

  default_pixmap = create_pixmap (parent->window, NULL, default_bits,
				  default_width, default_height);
  swap_pixmap    = create_pixmap (parent->window, NULL, swap_bits,
				  swap_width, swap_height);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_box_pack_start (GTK_BOX (parent), frame, FALSE, FALSE, 0);
  gtk_widget_realize (frame);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_border_width (GTK_CONTAINER (alignment), 3);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  col_area = color_area_create (54, 42, default_pixmap, swap_pixmap);
  gtk_container_add (GTK_CONTAINER (alignment), col_area);
  gtk_tooltips_set_tip (tool_tips, col_area, "Foreground & background colors.  The small black "
			"and white squares reset colors.  The small arrows swap colors.  Double "
			"click to change colors.",
			NULL);
  gtk_widget_show (col_area);
  gtk_widget_show (alignment);
  gtk_widget_show (frame);
}


static void
create_tools (GtkWidget *parent)
{
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *alignment;
  GtkWidget *pixmap;
  GSList *group;
  gint i;

  /*create_logo (parent);*/
  table = gtk_table_new (ROWS, COLUMNS, TRUE);
  gtk_box_pack_start (GTK_BOX (parent), table, TRUE, TRUE, 0);
  gtk_widget_realize (table);

  group = NULL;

  for (i = 0; i < 21; i++)
    {
      tool_widgets[i] = button = gtk_radio_button_new (group);
      gtk_container_border_width (GTK_CONTAINER (button), 0);
      group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));

      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

      gtk_table_attach (GTK_TABLE (table), button,
			(i % 3), (i % 3) + 1,
			(i / 3), (i / 3) + 1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			0, 0);

      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      gtk_container_border_width (GTK_CONTAINER (alignment), 0);
      gtk_container_add (GTK_CONTAINER (button), alignment);

      pixmap = create_pixmap_widget (table->window, tool_data[i].icon_data, 22, 22);
      gtk_container_add (GTK_CONTAINER (alignment), pixmap);

      gtk_signal_connect (GTK_OBJECT (button), "toggled",
			  (GtkSignalFunc) tools_select_update,
			  tool_data[i].callback_data);

      gtk_signal_connect (GTK_OBJECT (button), "button_press_event",
			  (GtkSignalFunc) tools_button_press,
			  tool_data[i].callback_data);

      gtk_tooltips_set_tip (tool_tips, button, tool_data[i].tool_desc, NULL);

      gtk_widget_show (pixmap);
      gtk_widget_show (alignment);
      gtk_widget_show (button);
    }

  /*  The non-visible tool buttons  */
  for (i = 21; i < NUM_TOOLS; i++)
    {
      tool_widgets[i] = button = gtk_radio_button_new (group);
      group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));

      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  (GtkSignalFunc) tools_select_update,
			  tool_data[i].callback_data);
    }

  gtk_widget_show (table);
}


static GdkPixmap *
create_pixmap (GdkWindow *parent, GdkBitmap **mask,
	       char **data, int width, int height)
{
  GdkPixmap *pixmap;
  GdkImage *image;
  GdkGC *gc;
  GdkVisual *visual;
  GdkColormap *cmap;
  gint r, s, t, cnt;
  guchar *mem;
  guchar value;
  guint32 pixel;

  visual = gdk_window_get_visual (parent);
  cmap = gdk_window_get_colormap (parent);
  image = gdk_image_new (GDK_IMAGE_NORMAL, visual, width, height);
  pixmap = gdk_pixmap_new (parent, width, height, -1);
  gc = NULL;

  if (mask)
    {
      GdkColor tmp_color;

      *mask = gdk_pixmap_new (parent, width, height, 1);
      gc = gdk_gc_new (*mask);
      gdk_draw_rectangle (*mask, gc, TRUE, 0, 0, -1, -1);

      tmp_color.pixel = 1;
      gdk_gc_set_foreground (gc, &tmp_color);
    }

  for (r = 0; r < height; r++)
    {
      mem = image->mem;
      mem += image->bpl * r;

      for (s = 0, cnt = 0; s < width; s++)
	{
	  value = data[r][s];

	  if (value == '.')
	    {
	      pixel = colors[8].pixel;

	      if (mask)
		{
		  if (cnt < s)
		    gdk_draw_line (*mask, gc, cnt, r, s - 1, r);
		  cnt = s + 1;
		}
	    }
	  else
	    {
	      pixel = colors[value - 'a'].pixel;
	    }

	  if (image->byte_order == GDK_LSB_FIRST)
	    {
	      for (t = 0; t < image->bpp; t++)
		*mem++ = (unsigned char) ((pixel >> (t * 8)) & 0xFF);
	    }
	  else
	    {
	      for (t = 0; t < image->bpp; t++)
		*mem++ = (unsigned char) ((pixel >> ((image->bpp - t - 1) * 8)) & 0xFF);
	    }
	}

      if (mask && (cnt < s))
	gdk_draw_line (*mask, gc, cnt, r, s - 1, r);
    }

  if (mask)
    gdk_gc_destroy (gc);

  gc = gdk_gc_new (parent);
  gdk_draw_image (pixmap, gc, image, 0, 0, 0, 0, width, height);
  gdk_gc_destroy (gc);
  gdk_image_destroy (image);

  return pixmap;
}

GtkWidget*
create_pixmap_widget (GdkWindow *parent,
		      char **data, int width, int height)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  pixmap = create_pixmap (parent, &mask, data, width, height);

  return gtk_pixmap_new (pixmap, mask);
}

void
create_toolbox ()
{
  GtkWidget *window;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *menubar;
  GtkAccelGroup *table;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_wmclass (GTK_WINDOW (window), "toolbox", "Gimp");
  gtk_window_set_title (GTK_WINDOW (window), "The GIMP");
  gtk_widget_set_uposition (window, toolbox_x, toolbox_y);
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		      GTK_SIGNAL_FUNC (toolbox_delete),
		      NULL);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      (GtkSignalFunc) toolbox_destroy,
		      NULL);

  main_vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_border_width (GTK_CONTAINER (main_vbox), 1);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);
  gtk_widget_show (main_vbox);

  /*  allocate the colors for creating pixmaps  */
  allocate_colors (main_vbox);

  /*  tooltips  */
  tool_tips = gtk_tooltips_new ();
  gtk_tooltips_set_colors (tool_tips,
			   &colors[11],
			   &main_vbox->style->fg[GTK_STATE_NORMAL]);
  if (!show_tool_tips)
    gtk_tooltips_disable (tool_tips);

  /*  Build the menu bar with menus  */
  menus_get_toolbox_menubar (&menubar, &table);
  gtk_box_pack_start (GTK_BOX (main_vbox), menubar, FALSE, TRUE, 0);
  gtk_widget_show (menubar);

  /*  Install the accelerator table in the main window  */
  gtk_window_add_accel_group (GTK_WINDOW (window), table);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (vbox), 0);
  gtk_widget_show (vbox);

  create_tools (vbox);
  /*create_tool_label (vbox);*/
  /*create_progress_area (vbox);*/
  create_color_area (vbox);

  gtk_widget_show (window);
  toolbox_shell = window;
}

void
toolbox_free ()
{
  int i;

  gtk_widget_destroy (toolbox_shell);
  for (i = 21; i < NUM_TOOLS; i++)
    {
      gtk_object_sink    (GTK_OBJECT (tool_widgets[i]));
    }
  gtk_object_destroy (GTK_OBJECT (tool_tips));
  gtk_object_unref   (GTK_OBJECT (tool_tips));
}

void
toolbox_raise_callback (GtkWidget *widget,
			gpointer  client_data)
{
  gdk_window_raise(toolbox_shell->window);
}


void
create_display_shell (int   gdisp_id,
		      int   width,
		      int   height,
		      char *title,
		      int   type)
{
  static GtkWidget *image_popup_menu = NULL;
  static GtkAccelGroup *image_accel_group = NULL;
  GDisplay *gdisp;
  GtkWidget *table;
  int n_width, n_height;
  int s_width, s_height;
  int scalesrc, scaledest;

  /*  Get the gdisplay  */
  if (! (gdisp = gdisplay_get_ID (gdisp_id)))
    return;

  /*  adjust the initial scale -- so that window fits on screen */
  {
    s_width = gdk_screen_width ();
    s_height = gdk_screen_height ();

    scalesrc = gdisp->scale & 0x00ff;
    scaledest = gdisp->scale >> 8;

    n_width = (width * scaledest) / scalesrc;
    n_height = (height * scaledest) / scalesrc;

    /*  Limit to the size of the screen...  */
    while (n_width > s_width || n_height > s_height)
      {
	if (scaledest > 1)
	  scaledest--;
	else
	  if (scalesrc < 0xff)
	    scalesrc++;

	n_width = (width * scaledest) / scalesrc;
	n_height = (height * scaledest) / scalesrc;
      }

    gdisp->scale = (scaledest << 8) + scalesrc;
  }

  /*  The adjustment datums  */
  gdisp->hsbdata = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, width, 1, 1, width));
  gdisp->vsbdata = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, height, 1, 1, height));

  /*  The toplevel shell */
  gdisp->shell = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_ref  (gdisp->shell);
  gtk_window_set_title (GTK_WINDOW (gdisp->shell), title);
  gtk_window_set_wmclass (GTK_WINDOW (gdisp->shell), "image_window", "Gimp");
  gtk_window_set_policy (GTK_WINDOW (gdisp->shell), TRUE, TRUE, TRUE);
  gtk_object_set_user_data (GTK_OBJECT (gdisp->shell), (gpointer) gdisp);
  gtk_widget_set_events (gdisp->shell, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  gtk_signal_connect (GTK_OBJECT (gdisp->shell), "delete_event",
		      GTK_SIGNAL_FUNC (gdisplay_delete),
		      gdisp);

  gtk_signal_connect (GTK_OBJECT (gdisp->shell), "destroy",
		      (GtkSignalFunc) gdisplay_destroy,
		      gdisp);

  /*  the table containing all widgets  */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 1);
  gtk_table_set_col_spacing (GTK_TABLE (table), 1, 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 1);
  gtk_table_set_row_spacing (GTK_TABLE (table), 1, 2);
  gtk_container_border_width (GTK_CONTAINER (table), 2);
  gtk_container_add (GTK_CONTAINER (gdisp->shell), table);

  /*  scrollbars, rulers, canvas, menu popup button  */
  gdisp->origin = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (gdisp->origin), GTK_SHADOW_OUT);

  gdisp->hrule = gtk_hruler_new ();
  gtk_widget_set_events (GTK_WIDGET (gdisp->hrule),
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_ruler_set_metric (GTK_RULER (gdisp->hrule), ruler_units);
  gtk_signal_connect_object (GTK_OBJECT (gdisp->shell), "motion_notify_event",
			     (GtkSignalFunc) GTK_WIDGET_CLASS (GTK_OBJECT (gdisp->hrule)->klass)->motion_notify_event,
			     GTK_OBJECT (gdisp->hrule));
  gtk_signal_connect (GTK_OBJECT (gdisp->hrule), "button_press_event",
		      (GtkSignalFunc) gdisplay_hruler_button_press,
		      gdisp);

  gdisp->vrule = gtk_vruler_new ();
  gtk_widget_set_events (GTK_WIDGET (gdisp->vrule),
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_ruler_set_metric (GTK_RULER (gdisp->vrule), ruler_units);
  gtk_signal_connect_object (GTK_OBJECT (gdisp->shell), "motion_notify_event",
			     (GtkSignalFunc) GTK_WIDGET_CLASS (GTK_OBJECT (gdisp->vrule)->klass)->motion_notify_event,
			     GTK_OBJECT (gdisp->vrule));
  gtk_signal_connect (GTK_OBJECT (gdisp->vrule), "button_press_event",
		      (GtkSignalFunc) gdisplay_vruler_button_press,
		      gdisp);

  gdisp->hsb = gtk_hscrollbar_new (gdisp->hsbdata);
  GTK_WIDGET_UNSET_FLAGS (gdisp->hsb, GTK_CAN_FOCUS);
  gdisp->vsb = gtk_vscrollbar_new (gdisp->vsbdata);
  GTK_WIDGET_UNSET_FLAGS (gdisp->vsb, GTK_CAN_FOCUS);

  gdisp->canvas = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (gdisp->canvas), n_width, n_height);
  gtk_widget_set_events (gdisp->canvas, CANVAS_EVENT_MASK);
  GTK_WIDGET_SET_FLAGS (gdisp->canvas, GTK_CAN_FOCUS);
  gtk_signal_connect (GTK_OBJECT (gdisp->canvas), "event",
		      (GtkSignalFunc) gdisplay_canvas_events,
		      gdisp);
  gtk_object_set_user_data (GTK_OBJECT (gdisp->canvas), (gpointer) gdisp);


  /*  pack all the widgets  */
  gtk_table_attach (GTK_TABLE (table), gdisp->origin, 0, 1, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), gdisp->hrule, 1, 2, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), gdisp->vrule, 0, 1, 1, 2,
		    GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), gdisp->canvas, 1, 2, 1, 2,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), gdisp->hsb, 0, 2, 2, 3,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), gdisp->vsb, 2, 3, 0, 2,
		    GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

  if (! image_popup_menu)
    menus_get_image_menu (&image_popup_menu, &image_accel_group);

  /*  the popup menu  */
  gdisp->popup = image_popup_menu;

  /*  the accelerator table for images  */
  gtk_window_add_accel_group (GTK_WINDOW (gdisp->shell), image_accel_group);

  gtk_widget_show (gdisp->hsb);
  gtk_widget_show (gdisp->vsb);

  if (show_rulers)
    {
      gtk_widget_show (gdisp->origin);
      gtk_widget_show (gdisp->hrule);
      gtk_widget_show (gdisp->vrule);
    }

  gtk_widget_show (gdisp->canvas);
  gtk_widget_show (table);
  gtk_widget_show (gdisp->shell);

  /*  set the focus to the canvas area  */
  gtk_widget_grab_focus (gdisp->canvas);
}


/*
 *  A text string query box
 */

typedef struct _QueryBox QueryBox;

struct _QueryBox
{
  GtkWidget *qbox;
  GtkWidget *entry;
  QueryFunc callback;
  gpointer data;
};

static void query_box_cancel_callback (GtkWidget *, gpointer);
static void query_box_ok_callback (GtkWidget *, gpointer);
static gint query_box_delete_callback (GtkWidget *, GdkEvent *, gpointer);

GtkWidget *
query_string_box (char        *title,
		  char        *message,
		  char        *initial,
		  QueryFunc    callback,
		  gpointer     data)
{
  QueryBox  *query_box;
  GtkWidget *qbox;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;

  query_box = (QueryBox *) g_malloc (sizeof (QueryBox));

  qbox = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (qbox), title);
  gtk_window_set_wmclass (GTK_WINDOW (qbox), "query_box", "Gimp");
  gtk_window_position (GTK_WINDOW (qbox), GTK_WIN_POS_MOUSE);
  gtk_signal_connect (GTK_OBJECT (qbox), "delete_event",
		      (GtkSignalFunc) query_box_delete_callback,
		      query_box);

  gtk_container_border_width (GTK_CONTAINER (GTK_DIALOG (qbox)->action_area), 2);

  button = gtk_button_new_with_label ("OK");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) query_box_ok_callback,
                      query_box);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (qbox)->action_area), button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Cancel");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) query_box_cancel_callback,
                      query_box);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (qbox)->action_area), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (qbox)->vbox), vbox);
  gtk_widget_show (vbox);

  label = gtk_label_new (message);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 0);
  if (initial)
    gtk_entry_set_text (GTK_ENTRY (entry), initial);
  gtk_widget_show (entry);

  query_box->qbox = qbox;
  query_box->entry = entry;
  query_box->callback = callback;
  query_box->data = data;

  gtk_widget_show (qbox);

  return qbox;
}

static gint
query_box_delete_callback (GtkWidget *w,
			   GdkEvent  *e,
			   gpointer   client_data)
{
  query_box_cancel_callback (w, client_data);

  return TRUE;
}

static void
query_box_cancel_callback (GtkWidget *w,
			   gpointer   client_data)
{
  QueryBox *query_box;

  query_box = (QueryBox *) client_data;

  /*  Destroy the box  */
  gtk_widget_destroy (query_box->qbox);

  g_free (query_box);
}

static void
query_box_ok_callback (GtkWidget *w,
		       gpointer   client_data)
{
  QueryBox *query_box;
  char *string;

  query_box = (QueryBox *) client_data;

  /*  Get the entry data  */
  string = g_strdup (gtk_entry_get_text (GTK_ENTRY (query_box->entry)));

  /*  Call the user defined callback  */
  (* query_box->callback) (w, query_box->data, (gpointer) string);

  /*  Destroy the box  */
  gtk_widget_destroy (query_box->qbox);

  g_free (query_box);
}


/*
 *  Message Boxes...
 */

typedef struct _MessageBox MessageBox;

struct _MessageBox
{
  GtkWidget  *mbox;
  GtkCallback callback;
  gpointer    data;
};

static void message_box_close_callback (GtkWidget *, gpointer);
static gint message_box_delete_callback (GtkWidget *, GdkEvent *, gpointer);

GtkWidget *
message_box (char        *message,
	     GtkCallback  callback,
	     gpointer     data)
{
  MessageBox *msg_box;
  GtkWidget *mbox;
  GtkWidget *vbox;
  GtkWidget *label_vbox;
  GtkWidget *label;
  GtkWidget *button;
  char *str, *orig;

  if (message)
    message = orig = g_strdup (message);
  else
    return NULL;

  msg_box = (MessageBox *) g_malloc (sizeof (MessageBox));

  mbox = gtk_dialog_new ();
  gtk_window_set_wmclass (GTK_WINDOW (mbox), "gimp_message", "Gimp");
  gtk_window_set_title (GTK_WINDOW (mbox), "GIMP Message");
  gtk_window_position (GTK_WINDOW (mbox), GTK_WIN_POS_MOUSE);
  gtk_container_border_width (GTK_CONTAINER (GTK_DIALOG (mbox)->action_area), 2);
  gtk_signal_connect (GTK_OBJECT (mbox), "delete_event",
		      GTK_SIGNAL_FUNC (message_box_delete_callback),
		      msg_box);

  button = gtk_button_new_with_label ("OK");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) message_box_close_callback,
                      msg_box);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mbox)->action_area), button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_border_width (GTK_CONTAINER (vbox), 1);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (mbox)->vbox), vbox);
  gtk_widget_show (vbox);

  label_vbox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), label_vbox, TRUE, FALSE, 0);
  gtk_widget_show (label_vbox);

  str = message;
  while (*str != '\0')
    {
      if (*str == '\n')
	{
	  *str = '\0';
	  label = gtk_label_new (message);
	  gtk_box_pack_start (GTK_BOX (label_vbox), label, TRUE, FALSE, 0);
	  gtk_widget_show (label);
	  message = str + 1;
	}
      str++;
    }

  if (*message != '\0')
    {
      label = gtk_label_new (message);
      gtk_box_pack_start (GTK_BOX (label_vbox), label, TRUE, FALSE, 0);
      gtk_widget_show (label);
    }

  g_free (orig);

  msg_box->mbox = mbox;
  msg_box->callback = callback;
  msg_box->data = data;

  gtk_widget_show (mbox);

  return mbox;
}

static gint
message_box_delete_callback (GtkWidget *w, GdkEvent *e, gpointer client_data)
{
  message_box_close_callback (w, client_data);

  return TRUE;
}


static void
message_box_close_callback (GtkWidget *w,
			    gpointer   client_data)
{
  MessageBox *msg_box;

  msg_box = (MessageBox *) client_data;

  /*  If there is a valid callback, invoke it  */
  if (msg_box->callback)
    (* msg_box->callback) (w, msg_box->data);

  /*  Destroy the box  */
  gtk_widget_destroy (msg_box->mbox);

  g_free (msg_box);
}


void
progress_start ()
{
  if (!GTK_WIDGET_VISIBLE (progress_area))
    {
      gtk_widget_set_usize (progress_area,
			    tool_label_area->allocation.width,
			    tool_label_area->allocation.height);

      /*
      gtk_container_disable_resize (GTK_CONTAINER (progress_area->parent));
      */

      gtk_widget_hide (tool_label_area);
      gtk_widget_show (progress_area);

      /*
      gtk_container_enable_resize (GTK_CONTAINER (progress_area->parent));
      */
    }
}

void
progress_update (float percentage)
{
  if (!(percentage >= 0.0 && percentage <= 1.0))
    return;

  gtk_progress_bar_update (GTK_PROGRESS_BAR (progress_area), percentage);

  if (GTK_WIDGET_VISIBLE (progress_area))
    gdk_flush ();
}

void
progress_step ()
{
  float val;

  if (GTK_WIDGET_VISIBLE (progress_area))
    {
      val = gtk_progress_get_current_percentage(&(GTK_PROGRESS_BAR(progress_area)->progress))+
						  0.01;
      if (val > 1.0)
	val = 0.0;

      progress_update (val);
    }
}

void
progress_end ()
{
  if (GTK_WIDGET_VISIBLE (progress_area))
    {
      /*
      tk_container_disable_resize (GTK_CONTAINER (progress_area->parent));
      */

      gtk_widget_hide (progress_area);
      gtk_widget_show (tool_label_area);

      /*
      gtk_container_enable_resize (GTK_CONTAINER (progress_area->parent));
      */

      gdk_flush ();

      gtk_progress_bar_update (GTK_PROGRESS_BAR (progress_area), 0.0);
    }
}
