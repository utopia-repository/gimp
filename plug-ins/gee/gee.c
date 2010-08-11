/*
 * Adam D. Moss : 1998 : adam@gimp.org : adam@foxbox.org
 *
 * This is part of the GIMP package and is released under the GNU
 * Public License.
 */

/*
 *     Version 1.01 : 98.04.19
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include "glib.h"
#include "libgimp/gimp.h"
#include "gtk/gtk.h"

#include "config.h"



/* Declare local functions. */
static void query(void);
static void run(char *name,
		int nparams,
		GParam * param,
		int *nreturn_vals,
		GParam ** return_vals);

static void do_playback (void);

static gint window_delete_callback (GtkWidget *widget,
				    GdkEvent  *event,
				    gpointer   data);
static void window_close_callback  (GtkWidget *widget,
				    gpointer   data);
static gint step_callback  (gpointer   data);
static void toggle_feedbacktype  (GtkWidget *widget,
				  gpointer   data);

static void         render_frame        (void);
static void         show_frame          (void);
static void         init_preview_misc   (void);



GPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};




static const guint  width = 256;
static const guint height = 256;


/* Global widgets'n'stuff */
static guchar*    seed_data;
static guchar*    preview_data1;
static guchar*    preview_data2;
static GtkPreview* preview = NULL;
static gint32     image_id;
static gint32     total_frames;
static gint32*    layers;
static GDrawable* drawable;
static GImageType imagetype;
static guchar*    palette;
static gint       ncolours;

static gint       idle_tag;
static GtkWidget* eventbox;
static gboolean   feedbacktype = FALSE;
static gboolean   rgb_mode;



MAIN()

static void query()
{
  static GParamDef args[] =
  {
    {PARAM_INT32, "run_mode", "Always interactive"},
    {PARAM_IMAGE, "image", "Input Image"},
    {PARAM_DRAWABLE, "drawable", "Input Drawable"},
  };
  static GParamDef *return_vals = NULL;
  static int nargs = sizeof(args) / sizeof(args[0]);
  static int nreturn_vals = 0;

  gimp_install_procedure("plug_in_the_egg",
			 "A big hello from the GIMP team!",
			 "",
			 "Adam D. Moss <adam@gimp.org>",
			 "Adam D. Moss <adam@gimp.org>",
			 "1998",
			 /*"<Image>/Filters/Animation/The Egg",*/
			 NULL,
			 "RGB*, INDEXED*, GRAY*",
			 PROC_PLUG_IN,
			 nargs, nreturn_vals,
			 args, return_vals);
}

static void run(char *name, int n_params, GParam * param, int *nreturn_vals,
		GParam ** return_vals)
{
  static GParam values[1];
  GRunModeType run_mode;
  GStatusType status = STATUS_SUCCESS;

  *nreturn_vals = 1;
  *return_vals = values;

  SRAND_FUNC (time(0));

  run_mode = param[0].data.d_int32;

  /*  if (run_mode == RUN_NONINTERACTIVE) {*/
    if (n_params != 3)
      {
	status = STATUS_CALLING_ERROR;
      }
    /*  }*/

  if (status == STATUS_SUCCESS)
    {
      drawable = gimp_drawable_get (param[2].data.d_drawable);
      image_id = param[1].data.d_image;
      
      do_playback();
      /*    if (run_mode != RUN_NONINTERACTIVE)
	    gimp_displays_flush();*/
    }

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = status;
}



static void
build_dialog(GImageType basetype,
	     char*      imagename)
{
  gchar** argv;
  gint argc;

  GtkWidget* dlg;
  GtkWidget* button;
  GtkWidget* frame;
  GtkWidget* frame2;
  GtkWidget* vbox;
  GtkWidget* hbox;
  GtkWidget* hbox2;
  guchar* color_cube;

  argc = 1;
  argv = g_new (gchar *, 1);
  argv[0] = g_strdup ("gee");
  gtk_init (&argc, &argv);
  gtk_rc_parse (gimp_gtkrc ());
  gdk_set_use_xshm (gimp_use_xshm ());
  gtk_preview_set_gamma (gimp_gamma ());
  gtk_preview_set_install_cmap (gimp_install_cmap ());
  color_cube = gimp_color_cube ();
  gtk_preview_set_color_cube (color_cube[0], color_cube[1],
                              color_cube[2], color_cube[3]);
  gtk_widget_set_default_visual (gtk_preview_get_visual ());
  gtk_widget_set_default_colormap (gtk_preview_get_cmap ());


  dlg = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dlg), "GEE!  The GIMP E'er Egg!");

  gtk_window_position (GTK_WINDOW (dlg), GTK_WIN_POS_MOUSE);
  gtk_signal_connect (GTK_OBJECT (dlg), "delete_event",
		      (GtkSignalFunc) window_delete_callback,
		      NULL);

  
  /* Action area - 'close' button only. */

  button = gtk_button_new_with_label ("** Thank you for choosing GIMP **");
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     (GtkSignalFunc) window_close_callback,
			     GTK_OBJECT (dlg));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->action_area),
		      button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);
  

  {
    /* The 'playback' half of the dialog */
    
    frame = gtk_frame_new (NULL);

    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_border_width (GTK_CONTAINER (frame), 3);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox),
			frame, TRUE, TRUE, 0);
    
    {
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (hbox), 3);
      gtk_container_add (GTK_CONTAINER (frame), hbox);
      
      {
	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_border_width (GTK_CONTAINER (vbox), 3);
	gtk_container_add (GTK_CONTAINER (hbox), vbox);
	
	{
	  hbox2 = gtk_hbox_new (TRUE, 0);
	  gtk_container_border_width (GTK_CONTAINER (hbox2), 0);
	  gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
	  {
	    frame2 = gtk_frame_new (NULL);
	    gtk_frame_set_shadow_type (GTK_FRAME (frame2), GTK_SHADOW_ETCHED_IN);
	    gtk_box_pack_start (GTK_BOX (hbox2), frame2, FALSE, FALSE, 0);
	    
	    {
	      eventbox = gtk_event_box_new();
	      gtk_container_add (GTK_CONTAINER (frame2), GTK_WIDGET (eventbox));
	      
	      {
		preview =
		  GTK_PREVIEW (gtk_preview_new (rgb_mode?
						GTK_PREVIEW_COLOR:
						GTK_PREVIEW_GRAYSCALE));
		gtk_preview_size (preview, width, height);
		gtk_container_add (GTK_CONTAINER (eventbox),
				   GTK_WIDGET (preview));
		gtk_widget_show(GTK_WIDGET (preview));
	      }
	      gtk_widget_show(eventbox);
	      gtk_widget_set_events (eventbox,
				     gtk_widget_get_events (eventbox)
				     | GDK_BUTTON_PRESS_MASK);
	    }
	    gtk_widget_show(frame2);
	  }
	  gtk_widget_show(hbox2);
	}
	gtk_widget_show(vbox);
	
      }
      gtk_widget_show(hbox);
      
    }
    gtk_widget_show(frame);
    
  }
  gtk_widget_show(dlg);

	    
  idle_tag = gtk_idle_add_priority (
				    GTK_PRIORITY_LOW,
				    (GtkFunction) step_callback,
				    NULL);
  
  gtk_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      GTK_SIGNAL_FUNC (toggle_feedbacktype), NULL);
}



static void do_playback()
{
  layers    = gimp_image_get_layers (image_id, &total_frames);
  imagetype = gimp_image_base_type(image_id);

  if (imagetype == INDEXED)
    palette = gimp_image_get_cmap(image_id, &ncolours);

  /* cache hint */
  gimp_tile_cache_ntiles (MAX(drawable->width,drawable->height)/
			  MIN(gimp_tile_width(),gimp_tile_height())
			  +1);

  init_preview_misc();
  build_dialog(gimp_image_base_type(image_id),
               gimp_image_get_filename(image_id));

  
  render_frame();
  show_frame();

  gtk_main ();
  gdk_flush ();
}


/* Rendering Functions */

/* Adam's silly algorithm. */
void domap1(unsigned char *src, unsigned char *dest,
	    int bx, int by, int cx, int cy)
{
#ifdef __AAARGH_GNUC__
  unsigned int dy __attribute__ ((aligned));
  signed int bycxmcybx __attribute__ ((aligned));
  signed int bx2,by2 __attribute__ ((aligned));
  signed int cx2,cy2 __attribute__ ((aligned));
  unsigned int __attribute__ ((aligned)) basesx;
  unsigned int __attribute__ ((aligned)) basesy;
#else
  unsigned int dy;
  signed int bycxmcybx;
  signed int bx2,by2;
  signed int cx2,cy2;
  unsigned int basesx;
  unsigned int basesy;
#endif

  bycxmcybx = (by*cx-cy*bx);

  /* A little sub-pixel jitter to liven things up. */
  basesx = (((RAND_FUNC ()%89)<<19)/bycxmcybx) + ((-128-((128*256)/(cx+bx)))<<11);
  basesy = (((RAND_FUNC ()%89)<<19)/bycxmcybx) + ((-128-((128*256)/(cy+by)))<<11);

  bx2 = ((bx)<<19)/bycxmcybx;
  cx2 = ((cx)<<19)/bycxmcybx;
  by2 = ((by)<<19)/bycxmcybx;
  cy2 = ((cy)<<19)/bycxmcybx;

  for (dy=0;dy<256;dy++)
    {
#ifdef __AAARGH_GNUC__
      unsigned int __attribute__ ((aligned)) sx;
      unsigned int __attribute__ ((aligned)) sy;
      unsigned int __attribute__ ((aligned)) dx;
#else
      unsigned int sx;
      unsigned int sy;
      unsigned int dx;
#endif

      sx = (basesx-=bx2);
      sy = (basesy+=cx2);

      dx = 256;
      do
	{
	  *dest++ = (*(src +
		   (
		    (
		     ((255&(
			    (sx>>11)
			    )))
		     |
		     ((((255&(
			      (sy>>11)
			      ))<<8)))
		     )
		    )));
	  ;
	  sx += by2;
	  sy -= cy2;
	}
      while (--dx);
    }
}

/* 3bypp variant */
void domap3(unsigned char *src, unsigned char *dest,
	    int bx, int by, int cx, int cy)
{
#ifdef __AAARGH_GNUC__
  unsigned int dy __attribute__ ((aligned));
  signed int bycxmcybx __attribute__ ((aligned));
  signed int bx2,by2 __attribute__ ((aligned));
  signed int cx2,cy2 __attribute__ ((aligned));
  unsigned int __attribute__ ((aligned)) basesx;
  unsigned int __attribute__ ((aligned)) basesy;
#else
  unsigned int dy;
  signed int bycxmcybx;
  signed int bx2,by2;
  signed int cx2,cy2;
  unsigned int basesx;
  unsigned int basesy;
#endif

  bycxmcybx = (by*cx-cy*bx);

  /* A little sub-pixel jitter to liven things up. */
  basesx = (((RAND_FUNC ()%89)<<19)/bycxmcybx) + ((-128-((128*256)/(cx+bx)))<<11);
  basesy = (((RAND_FUNC ()%89)<<19)/bycxmcybx) + ((-128-((128*256)/(cy+by)))<<11);

  bx2 = ((bx)<<19)/bycxmcybx;
  cx2 = ((cx)<<19)/bycxmcybx;
  by2 = ((by)<<19)/bycxmcybx;
  cy2 = ((cy)<<19)/bycxmcybx;

  for (dy=0;dy<256;dy++)
    {
#ifdef __AAARGH_GNUC__
      unsigned int __attribute__ ((aligned)) sx;
      unsigned int __attribute__ ((aligned)) sy;
      unsigned int __attribute__ ((aligned)) dx;
#else
      unsigned int sx;
      unsigned int sy;
      unsigned int dx;      
#endif

      sx = (basesx-=bx2);
      sy = (basesy+=cx2);

      dx = 256;
      do
	{
	  unsigned char* addr;

	  addr = src + 3*
	    (
	     (
	      ((255&(
		     (sx>>11)
		     )))
	      |
	      ((((255&(
		       (sy>>11)
		       ))<<8)))
	      )
	     );

	  *dest++ = *(addr);
	  *dest++ = *(addr+1);
	  *dest++ = *(addr+2);

	  sx += by2;
	  sy -= cy2;
	}
      while (--dx);
    }
}


static void
render_frame(void)
{
  int i;
  static int frame = 0;
  unsigned char* tmp;
  static gint xp=128, yp=128;
  gint rxp, ryp;
  GdkModifierType mask;
  gint pixels;

  pixels = width*height*(rgb_mode?3:1);

  gdk_flush();

  tmp = preview_data2;
  preview_data2 = preview_data1;
  preview_data1 = tmp;

  if (frame==0)
    {
      for (i=0;i<pixels;i++)
	{
	  preview_data2[i] =
	    preview_data1[i] =
	    seed_data[i];
	}
    }

  gdk_window_get_pointer (eventbox->window, &rxp, &ryp, &mask);

  if ((abs(rxp)>60)||(abs(ryp)>60))
    {
      xp = rxp;
      yp = ryp;
    }

  if (rgb_mode)
    {
      domap3(preview_data2, preview_data1,
	     -(yp-xp)/2, xp+yp
	     ,
	     xp+yp, (yp-xp)/2
	     );

      for (i=0;i<height;i++)
	{
	  gtk_preview_draw_row (preview,
				&preview_data1[i*width*3],
				0, i, width);
	}

      if (frame != 0)
	{
	  if (feedbacktype)
	    {
	      for (i=0;i<pixels;i++)
		{
		  int t;
		  t = preview_data1[i] + seed_data[i] - 128;
		  preview_data1[i] = CLAMP(t,0,255);
		}
	    }
	  else
	    {
	      for (i=0;i<pixels;i++)
		{
		  preview_data1[i] = (preview_data1[i]*2 + seed_data[i]) /3;
		}
	    }	
	}
    }
  else /* GRAYSCALE */
    {
      domap1(preview_data2, preview_data1,
	     -(yp-xp)/2, xp+yp
	     ,
	     xp+yp, (yp-xp)/2
	     );

      for (i=0;i<height;i++)
	{
	  gtk_preview_draw_row (preview,
				&preview_data1[i*width],
				0, i, width);
	}

      if (frame != 0)
	{
	  if (feedbacktype)
	    {
	      for (i=0;i<pixels;i++)
		{
		  int t;
		  t = preview_data1[i] + seed_data[i] - 128;
		  preview_data1[i] = CLAMP(t,0,255);
		}
	    }
	  else
	    {
	      for (i=0;i<pixels;i++)
		{
		  preview_data1[i] = (preview_data1[i]*2 + seed_data[i]) /3;
		}
	    }	
	}
    }

  frame++;
}


static void
show_frame(void)
{
  /* Tell GTK to physically draw the preview */
  gtk_widget_draw (GTK_WIDGET (preview), NULL);
}


static void
init_preview_misc(void)
{
  GPixelRgn pixel_rgn;
  int i;
  gboolean has_alpha;

  if ((imagetype == RGB)||(imagetype == INDEXED))
    rgb_mode = TRUE;
  else
    rgb_mode = FALSE;

  has_alpha = gimp_drawable_has_alpha(drawable->id);

  seed_data = g_malloc(width*height*4);
  preview_data1 = g_malloc(width*height*(rgb_mode?3:1));
  preview_data2 = g_malloc(width*height*(rgb_mode?3:1));

  if ((drawable->width<256) || (drawable->height<256))
    {
      for (i=0;i<256;i++)
	{
	  if (i < drawable->height)
	    {
	      gimp_pixel_rgn_init (&pixel_rgn,
				   drawable,
				   drawable->width>256?
				   (drawable->width/2-128):0,
				   (drawable->height>256?
				   (drawable->height/2-128):0)+i,
				   MIN(256,drawable->width),
				   1,
				   FALSE,
				   FALSE);
	      gimp_pixel_rgn_get_rect (&pixel_rgn,
				       &seed_data[(256*i +
						 (
						  (
						   drawable->width<256 ?
						   (256-drawable->width)/2 :
						   0
						   )
						  +
						  (
						   drawable->height<256 ?
						   (256-drawable->height)/2 :
						   0
						   ) * 256
						  )) *
						 gimp_drawable_bpp
						 (drawable->id)
				       ],
				       drawable->width>256?
				       (drawable->width/2-128):0,
				       (drawable->height>256?
				       (drawable->height/2-128):0)+i,
				       MIN(256,drawable->width),
				       1);
	    }
	}
    }
  else
    {
      gimp_pixel_rgn_init (&pixel_rgn,
			   drawable,
			   drawable->width>256?(drawable->width/2-128):0,
			   drawable->height>256?(drawable->height/2-128):0,
			   MIN(256,drawable->width),
			   MIN(256,drawable->height),
			   FALSE,
			   FALSE);
      gimp_pixel_rgn_get_rect (&pixel_rgn,
			       seed_data,
			       drawable->width>256?(drawable->width/2-128):0,
			       drawable->height>256?(drawable->height/2-128):0,
			       MIN(256,drawable->width),
			       MIN(256,drawable->height));
    }

  gimp_drawable_detach(drawable);


  /* convert the image data of varying types into flat grey or rgb. */
  switch (imagetype)
    {
    case INDEXED:
      if (has_alpha)
	{
	  for (i=width*height;i>0;i--)
	    {
	      seed_data[3*(i-1)+2] =
		((palette[3*(seed_data[(i-1)*2])+2]*seed_data[(i-1)*2+1])/255)
		+ ((255-seed_data[(i-1)*2+1])*(RAND_FUNC ()%256))/255;
	      seed_data[3*(i-1)+1] =
		((palette[3*(seed_data[(i-1)*2])+1]*seed_data[(i-1)*2+1])/255)
		+ ((255-seed_data[(i-1)*2+1])*(RAND_FUNC ()%256))/255;
	      seed_data[3*(i-1)+0] =
		((palette[3*(seed_data[(i-1)*2])+0]*seed_data[(i-1)*2+1])/255)
		+ ((255-seed_data[(i-1)*2+1])*(RAND_FUNC ()%256))/255;
	    }
	}
      else
	{
	  for (i=width*height;i>0;i--)
	    {
	      seed_data[3*(i-1)+2] = palette[3*(seed_data[i-1])+2];
	      seed_data[3*(i-1)+1] = palette[3*(seed_data[i-1])+1];
	      seed_data[3*(i-1)+0] = palette[3*(seed_data[i-1])+0];
	    }
	}
      break;
    case GRAY:
      if (has_alpha)
	{
	  for (i=0;i<width*height;i++)
	    {
	      seed_data[i] =
		(seed_data[i*2]*seed_data[i*2+1])/255
		+ ((255-seed_data[i*2+1])*(RAND_FUNC ()%256))/255;
	    }
	}
      break;
    case RGB:
      if (has_alpha)
	{
	  for (i=0;i<width*height;i++)
	    {
	      seed_data[i*3+2] =
		(seed_data[i*4+2]*seed_data[i*4+3])/255
		+ ((255-seed_data[i*4+3])*(RAND_FUNC ()%256))/255;
	      seed_data[i*3+1] =
		(seed_data[i*4+1]*seed_data[i*4+3])/255
		+ ((255-seed_data[i*4+3])*(RAND_FUNC ()%256))/255;
	      seed_data[i*3+0] =
		(seed_data[i*4+0]*seed_data[i*4+3])/255
		+ ((255-seed_data[i*4+3])*(RAND_FUNC ()%256))/255;
	    }
	}
      break;
    default:
      break;
    }
}



/* Util. */

static int
do_step(void)
{
  render_frame();

  return(1);
}



/*  Callbacks  */

static gint
window_delete_callback (GtkWidget *widget,
		        GdkEvent  *event,
		        gpointer   data)
{
  gtk_idle_remove (idle_tag);

  gdk_flush();
  gtk_main_quit();

  return FALSE;
}

static void
window_close_callback (GtkWidget *widget,
                       gpointer   data)
{
  if (data)
    gtk_widget_destroy(GTK_WIDGET(data));

  window_delete_callback (NULL, NULL, NULL);
}

static void
toggle_feedbacktype (GtkWidget *widget,
		     gpointer   data)
{
  feedbacktype = !feedbacktype;
}


static gint
step_callback (gpointer   data)
{
  do_step();
  show_frame();

  return TRUE;
}

