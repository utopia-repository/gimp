/*******************************************************************************

  papertile.c  -- This is a plug-in for the GIMP 1.0

  Copyright (C) 1997  Hirotsuna Mizuno
                      s1041150@u-aizu.ac.jp

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>

#define PLUG_IN_NAME    "plug_in_paper_tile"
#define PLUG_IN_VERSION "v0.7 (Dec. 25 1997)"
#define DIALOG_CAPTION  "Paper Tile"

/******************************************************************************/

static void query( void );
static void run( char *, int, GParam *, int *, GParam ** );
static void filter( GDrawable *drawable );
static int  dialog( void );

/******************************************************************************/

#define BG_TYPE_TRANSPARENT 0
#define BG_TYPE_BLACK       1
#define BG_TYPE_WHITE       2

typedef struct {
  gint32 tile_width;
  gint32 tile_height;
  gint32 slide_length;
  gint32 bg_type;
} parameter_t;

/******************************************************************************/

GPlugInInfo PLUG_IN_INFO = {
  NULL,    /* init_proc */
  NULL,    /* quit_proc */
  query,   /* query_proc */
  run,     /* run_proc */
};

static parameter_t parameters = {
  50,
  50,
  10,
  BG_TYPE_TRANSPARENT
};

static gint image_width;
static gint image_height;
static gint image_bpp;
static gint image_has_alpha;
static gint select_x1;
static gint select_y1;
static gint select_x2;
static gint select_y2;
static gint select_width;
static gint select_height;

/******************************************************************************/

MAIN()

/******************************************************************************/

static void query( void )
{
  static int nargs = 7;
  static GParamDef args[] = {
    { PARAM_INT32,    "run_mode",     "interactive / non-interactive" },
    { PARAM_IMAGE,    "image",        "input image" },
    { PARAM_DRAWABLE, "drawable",     "input drawable" },
    { PARAM_INT32,    "width",        "tile width" },
    { PARAM_INT32,    "height",       "tile height" },
    { PARAM_INT32,    "slide_length", "slide length" },
    { PARAM_INT32,    "bg_type",      "background color "
                                      "(0=transparent, 1=black, 2=white )" }
  };
  static int nreturn_vals = 0;
  static GParamDef *return_vals = NULL;

  gimp_install_procedure(
			 PLUG_IN_NAME,
			 "cut and slide image",
			 "cut and slide image",
			 "Hirotsuna Mizuno <s1041150@u-aizu.ac.jp>",
			 "Hirotsuna Mizuno",
			 PLUG_IN_VERSION,
			 "<Image>/Filters/Map/Paper Tile",
			 "RGB*, GRAY*",
			 PROC_PLUG_IN,
			 nargs,
			 nreturn_vals,
			 args,
			 return_vals 
			 );
}

/******************************************************************************/

static void run( char    *name, 
		 int      paramc, 
		 GParam  *params, 
		 int     *returnc,
		 GParam **returns )
{
  GDrawable     *drawable;
  GRunModeType   run_mode;
  static GParam  returnv[1];
  GStatusType    status = STATUS_SUCCESS;

  run_mode = params[0].data.d_int32;
  drawable = gimp_drawable_get( params[2].data.d_drawable );
  *returnc = 1;
  *returns = returnv;

  /* get the drawable info */
  image_width     = gimp_drawable_width( drawable->id );
  image_height    = gimp_drawable_height( drawable->id );
  image_bpp       = gimp_drawable_bpp( drawable->id );
  image_has_alpha = gimp_drawable_has_alpha( drawable->id );
  gimp_drawable_mask_bounds( drawable->id,
			     &select_x1, &select_y1, &select_x2, &select_y2 );
  select_width    = select_x2 - select_x1;
  select_height   = select_y2 - select_y1;

  /* switch the run mode */
  switch( run_mode ){

  case RUN_INTERACTIVE:
    gimp_get_data( PLUG_IN_NAME, &parameters );
    if( ! dialog() ) return;
    gimp_set_data( PLUG_IN_NAME, &parameters, sizeof( parameter_t ) );
    break;

  case RUN_NONINTERACTIVE:
    if( paramc != 7 ){
      status = STATUS_CALLING_ERROR;
    } else {
      parameters.tile_width   = params[3].data.d_int32;
      parameters.tile_height  = params[4].data.d_int32;
      parameters.slide_length = params[5].data.d_int32;
      parameters.bg_type      = params[6].data.d_int32;
    }
    break;

  case RUN_WITH_LAST_VALS:
    gimp_get_data( PLUG_IN_NAME, &parameters );
    break;
    
  }

  if( status == STATUS_SUCCESS ){
    if( gimp_drawable_color( drawable->id ) ||
	gimp_drawable_gray( drawable->id ) ){

      gimp_tile_cache_ntiles( 2 * ( drawable->width / gimp_tile_width() + 1 ) );
      filter( drawable );
      if( run_mode != RUN_NONINTERACTIVE ) gimp_displays_flush ();

    } else {

      status = STATUS_EXECUTION_ERROR;

    }
  }

  returnv[0].type          = PARAM_STATUS;
  returnv[0].data.d_status = status;

  gimp_drawable_detach( drawable );
}

/******************************************************************************/

static void filter( GDrawable *drawable )
{
  GPixelRgn srcPR, destPR;
  guchar  **pixels;
  guchar  **destpixels;
  gint      x, y, b;
  gint      vx, vy, xx, yy;
  
  gimp_pixel_rgn_init( &srcPR, drawable,
		       0, 0, image_width, image_height, FALSE, FALSE );
  gimp_pixel_rgn_init( &destPR, drawable,
		       0, 0, image_width, image_height, TRUE, TRUE );

  pixels = (guchar **)malloc( image_height * sizeof(guchar *) );
  destpixels = (guchar **)malloc( image_height * sizeof(guchar *) );
  for( y = 0; y < image_height; y++ ){
    pixels[y] = (guchar *)malloc( image_width * image_bpp );
    destpixels[y] = (guchar *)malloc( image_width * image_bpp );
    gimp_pixel_rgn_get_row( &srcPR, pixels[y], 0, y, image_width );
  }

  for( y = select_y1; y < select_y2; y++ ){
    for( x = select_x1; x < select_x2; x++ ){
      switch( parameters.bg_type ){

      case BG_TYPE_TRANSPARENT:
	for( b = 0; b < image_bpp; b++ ){
	  destpixels[y][x*image_bpp+b] = 0;
	}
	break;

      case BG_TYPE_BLACK:
	for( b = 0; b < image_bpp; b++ ){
	  if( b == image_bpp - 1 && image_has_alpha ){
	    destpixels[y][x*image_bpp+b] = 255;
	  } else {
	    destpixels[y][x*image_bpp+b] = 0;
	  }
	}
	break;
	
      case BG_TYPE_WHITE:
	for( b = 0; b < image_bpp; b++ ){
	  destpixels[y][x*image_bpp+b] = 255;
	}
	break;

      }
    }
  }
  
  gimp_progress_init( PLUG_IN_NAME );

  for( y = select_y1; y < select_y2; y+=parameters.tile_height )
    for( x = select_x1; x < select_x2; x+=parameters.tile_width ){
      vx = rand()%parameters.slide_length - parameters.slide_length/2;
      vy = rand()%parameters.slide_length - parameters.slide_length/2;
      for( xx = 0; xx < parameters.tile_width; xx++ )
	if( 0 <= x+xx+vx && x+xx+vx < image_width && x+xx < image_width )
	  for( yy = 0; yy < parameters.tile_height; yy++ )
	    if( 0 <= y+yy+vy && y+yy+vy < image_height && y+yy < image_height )
	      for( b = 0; b < image_bpp; b++ )
		destpixels[y+yy+vy][(x+xx+vx)*image_bpp+b] 
		  = pixels[y+yy][(x+xx)*image_bpp+b];
    }

  for( y = select_y1; y < select_y2; y++ ){
    gimp_pixel_rgn_set_row (&destPR, destpixels[y], 0, y, image_width );
    gimp_progress_update ( (double)( y - select_y1 ) / select_height );
  }
  
  gimp_drawable_flush( drawable );
  gimp_drawable_merge_shadow( drawable->id, TRUE );
  gimp_drawable_update( drawable->id,
			select_x1, select_y1, select_width, select_height );

  for( y = select_y1; y < select_y2; y++ ) free( pixels[y-select_y1] );
  free( pixels );
  for( y = select_y1; y < select_y2; y++ ) free( destpixels[y-select_y1] );
  free( destpixels );
}

/******************************************************************************/

static int dialog_status;

static GtkWidget *entry_width;
static GtkWidget *entry_height;
static GtkWidget *entry_slide;
static GtkWidget *button_transparent;
static GtkWidget *button_black;
static GtkWidget *button_white;

static void dialog_destroy_handler( GtkWidget *widget, 
				    gpointer  *data )
{
  gtk_main_quit();
}

static void dialog_ok_handler( GtkWidget *widget,
			       gpointer  *data )
{
  dialog_status = TRUE;

  if( GTK_TOGGLE_BUTTON( button_white )->active ) parameters.bg_type = BG_TYPE_WHITE;
  if( GTK_TOGGLE_BUTTON( button_black )->active ) parameters.bg_type = BG_TYPE_BLACK;
  if( image_has_alpha ){
    if( GTK_TOGGLE_BUTTON( button_transparent )->active ){
      parameters.bg_type = BG_TYPE_TRANSPARENT;
    }
  }

  parameters.tile_width =
    (gint32)atof(gtk_entry_get_text( GTK_ENTRY( entry_width ) ) );
  parameters.tile_height =
    (gint32)atof(gtk_entry_get_text( GTK_ENTRY( entry_height ) ) );
  parameters.slide_length =
    (gint32)atof(gtk_entry_get_text( GTK_ENTRY( entry_slide ) ) );

  gtk_widget_destroy( GTK_WIDGET( data ) );
}

static void dialog_cancel_handler( GtkWidget *widget,
				   gpointer  *data )
{
  dialog_status = FALSE;
  gtk_widget_destroy( GTK_WIDGET( data ) );
}

/******************************************************************************/

static int dialog( void )
{
  GtkWidget *window;
  dialog_status = FALSE;
  {
    gint    argc = 1;
    gchar **argv = g_new( gchar *, 1 );
    argv[0] = g_strdup( DIALOG_CAPTION );
    gtk_init( &argc, &argv );
    gtk_rc_parse(gimp_gtkrc());
  }

  /* dialog window */
  window = gtk_dialog_new();
  gtk_signal_connect( GTK_OBJECT( window ), "destroy",
		      GTK_SIGNAL_FUNC( dialog_destroy_handler ), NULL );
  gtk_container_border_width( GTK_CONTAINER( window ), 0 );
  gtk_container_border_width( GTK_CONTAINER( GTK_DIALOG( window )->vbox ), 5 );

  {
    /* buttons */
    GtkWidget *button;

    /* ok button */
    button = gtk_button_new_with_label( "OK" );
    gtk_signal_connect_object( GTK_OBJECT( button ), "clicked",
			       GTK_SIGNAL_FUNC( dialog_ok_handler ),
			       GTK_OBJECT( window ) );
    GTK_WIDGET_SET_FLAGS( button, GTK_CAN_DEFAULT );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( window )->action_area ),
			button, TRUE, TRUE, 0 );
    gtk_widget_grab_default( button );
    gtk_widget_show( button );
    
    /* cancel button */
    button = gtk_button_new_with_label( "Cancel" );
    gtk_signal_connect_object( GTK_OBJECT( button ), "clicked",
			       GTK_SIGNAL_FUNC( dialog_cancel_handler ),
			       GTK_OBJECT( window ));
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( window )->action_area ),
			button, TRUE, TRUE, 0 );
    gtk_widget_show( button );
  }
  
  {
    /* text boxes */
    GtkWidget *table;
    GtkWidget *label;
    char       buffer[32];
    
    /* table */
    table = gtk_table_new( 3, 2, FALSE );
    gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
    gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( window )->vbox ),
			table, TRUE, TRUE, 0 );
    gtk_widget_show( table );
    
    /* tile width */
    label = gtk_label_new( "width: " );
    entry_width = gtk_entry_new();
    sprintf( buffer, "%d", parameters.tile_width );
    gtk_entry_set_text( GTK_ENTRY( entry_width ), buffer );
    gtk_table_attach_defaults( GTK_TABLE( table ), label, 0, 1, 0, 1 );
    gtk_table_attach_defaults( GTK_TABLE( table ), entry_width, 1, 2, 0, 1 );
    gtk_widget_show( label );
    gtk_widget_show( entry_width );

    /* tile height */
    label = gtk_label_new( "height: " );
    entry_height = gtk_entry_new();
    sprintf( buffer, "%d", parameters.tile_height );
    gtk_entry_set_text( GTK_ENTRY( entry_height ), buffer );
    gtk_table_attach_defaults( GTK_TABLE( table ), label, 0, 1, 1, 2 );
    gtk_table_attach_defaults( GTK_TABLE( table ), entry_height, 1, 2, 1, 2 );
    gtk_widget_show( label );
    gtk_widget_show( entry_height );

    /* slide length */
    label = gtk_label_new( "slide: " );
    entry_slide = gtk_entry_new();
    sprintf( buffer, "%d", parameters.slide_length );
    gtk_entry_set_text( GTK_ENTRY( entry_slide ), buffer );
    gtk_table_attach_defaults( GTK_TABLE( table ), label, 0, 1, 2, 3 );
    gtk_table_attach_defaults( GTK_TABLE( table ), entry_slide, 1, 2, 2, 3 );
    gtk_widget_show( label );
    gtk_widget_show( entry_slide );
  }

  {
    /* radio buttons */
    GtkWidget *frame;
    GtkWidget *vbox;
    GSList    *group;
    
    frame = gtk_frame_new( "Background" );
    gtk_container_border_width( GTK_CONTAINER( frame ), 0 );
    gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_ETCHED_IN );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( window )->vbox ),
			frame, TRUE, TRUE, 0 );
    gtk_widget_show( frame );

    vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_border_width( GTK_CONTAINER( vbox ), 0 );
    gtk_container_add( GTK_CONTAINER( frame ), vbox );
    gtk_widget_show( vbox );
  
    group = NULL;
    
    if( image_has_alpha ){
      /* transparent */
      button_transparent =
	gtk_radio_button_new_with_label( NULL, "Transparent" );
      gtk_box_pack_start( GTK_BOX( vbox ),
			  button_transparent, TRUE, TRUE, 0 );
      if( parameters.bg_type == BG_TYPE_TRANSPARENT )
	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( button_transparent ),
				     TRUE );
      gtk_widget_show( button_transparent );
      group = gtk_radio_button_group( GTK_RADIO_BUTTON( button_transparent ) );
    }
    
    /* black */
    button_black = gtk_radio_button_new_with_label( group, "Black" );
    gtk_box_pack_start( GTK_BOX( vbox ), button_black, TRUE, TRUE, 0 );
    if( parameters.bg_type == BG_TYPE_BLACK )
      gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( button_black ), TRUE );
    gtk_widget_show( button_black );
    group = gtk_radio_button_group( GTK_RADIO_BUTTON( button_black ) );

    /* white */
    button_white = gtk_radio_button_new_with_label( group, "White" );
    gtk_box_pack_start( GTK_BOX( vbox ), button_white, TRUE, TRUE, 0 );
    if( parameters.bg_type == BG_TYPE_WHITE )
      gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( button_white ), TRUE );
    gtk_widget_show( button_white );
  }

  gtk_widget_show( window );
  gtk_main();

  return dialog_status;
}

/******************************************************************************/
