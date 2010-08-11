#include <stdlib.h>
#include <stdio.h>
#include "libgimp/gimp.h"

/* Declare some local functions.
 */
static void   query      (void);
static void   run        (char    *name,
                          int      nparams,
                          GParam  *param,
                          int     *nreturn_vals,
                          GParam **return_vals);
static int    save_image (char   *filename,
			  gint32  image_ID,
			  gint32  drawable_ID);


GPlugInInfo PLUG_IN_INFO =
{
  NULL,    /* init_proc */
  NULL,    /* quit_proc */
  query,   /* query_proc */
  run,     /* run_proc */
};


MAIN ()

static void
query ()
{
  static GParamDef save_args[] =
  {
    { PARAM_INT32, "run_mode", "Interactive, non-interactive" },
    { PARAM_IMAGE, "image", "Input image" },
    { PARAM_DRAWABLE, "drawable", "Drawable to save" },
    { PARAM_STRING, "filename", "The name of the file to save the image in" },
    { PARAM_STRING, "raw_filename", "The name of the file to save the image in" },
  };
  static int nsave_args = sizeof (save_args) / sizeof (save_args[0]);

  gimp_install_procedure ("file_header_save",
                          "saves files as C unsigned character array",
                          "FIXME: write help",
                          "Spencer Kimball & Peter Mattis",
                          "Spencer Kimball & Peter Mattis",
                          "1997",
                          "<Save>/Header",
			  "RGB*",
                          PROC_PLUG_IN,
                          nsave_args, 0,
                          save_args, NULL);

  gimp_register_save_handler ("file_header_save", "h", "");
}

static void
run (char    *name,
     int      nparams,
     GParam  *param,
     int     *nreturn_vals,
     GParam **return_vals)
{
  static GParam values[2];
  GRunModeType run_mode;

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = PARAM_STATUS;
  values[0].data.d_status = STATUS_CALLING_ERROR;

  if (strcmp (name, "file_header_save") == 0)
    {
      *nreturn_vals = 1;
      if (save_image (param[3].data.d_string, param[1].data.d_int32, param[2].data.d_int32))
	values[0].data.d_status = STATUS_SUCCESS;
      else
	values[0].data.d_status = STATUS_EXECUTION_ERROR;
    }
}

static int
save_image (char   *filename,
	    gint32  image_ID,
	    gint32  drawable_ID)
{
  GPixelRgn pixel_rgn;
  GDrawable *drawable;
  GDrawableType drawable_type;
  FILE *fp;
  int x, y, b, c;
  gchar *backslash = "\\\\";
  gchar *quote = "\\\"";
  gchar *newline = "\"\n\t\"";
  gchar buf[4];
  guchar *d;
  guchar *data;

  if ((fp = fopen (filename, "w")) == NULL)
    return FALSE;

  drawable = gimp_drawable_get (drawable_ID);
  drawable_type = gimp_drawable_type (drawable_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, FALSE, FALSE);

  fprintf (fp, "/*  GIMP header image file format (RGB-only): %s  */\n\n", filename);
  fprintf (fp, "static unsigned int width = %d;\n", drawable->width);
  fprintf (fp, "static unsigned int height = %d;\n\n", drawable->height);
  fprintf (fp, "/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */\n\n");
  fprintf (fp, "#define HEADER_PIXEL(data,pixel) \\\n  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \\\n  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \\\n  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \\\n  data += 4;\n\n");
  fprintf (fp, "static char *header_data =\n\t\"");

  data = g_new (guchar, drawable->width * drawable->bpp);

  c = 0;
  for (y = 0; y < drawable->height; y++)
    {
      gimp_pixel_rgn_get_row (&pixel_rgn, data, 0, y, drawable->width);
      for (x = 0; x < drawable->width; x++)
	{
	  d = data + x * drawable->bpp;

	  buf[0] = ((d[0] >> 2) & 0x3F) + 33;
	  buf[1] = ((((d[0] & 0x3) << 4) | (d[1] >> 4)) & 0x3F) + 33;
	  buf[2] = ((((d[1] & 0xF) << 2) | (d[2] >> 6)) & 0x3F) + 33;
	  buf[3] = (d[2] & 0x3F) + 33;

	  for (b = 0; b < 4; b++)
	    if (buf[b] == 34)
	      fwrite (quote, 1, 2, fp);
	    else if (buf[b] == 92)
	      fwrite (backslash, 1, 2, fp);
	    else
	      fwrite (buf + b, 1, 1, fp);

	  c++;
	  if (c >= 16)
	    {
	      fwrite (newline, 1, 4, fp);
	      c = 0;
	    }
	}
    }

  fprintf (fp, "\";\n");
  fclose (fp);

  g_free (data);
  gimp_drawable_detach (drawable);

  return TRUE;
}
