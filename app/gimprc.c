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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <glob.h>

#include "appenv.h"
#include "errors.h"
#include "fileops.h"
#include "general.h"
#include "gimprc.h"
#include "plug_in.h"

/*  global gimprc variables  */
char *    plug_in_path = NULL;
link_ptr  plug_ins = NULL;
char *    swap_path = NULL;
char *    brush_path = NULL;
char *    default_brush = NULL;

/*  static function prototypes  */
static char* get_token (char *, int);

static void parse_a_gimprc (FILE *fp);

void
parse_gimprc () {

  FILE *fp = NULL;
  char *path;
  
  path = search_in_path (app_data.gimprc_search_path, ".gimprc");
  if (path) {
    fp = fopen (path, "rt");
    if (!fp) {
      fatal_error ("Unable to open \".gimprc\"");
    }
  } else {
    path = search_in_path ("/etc/gimp", "gimprc");
    if (path) {
      fp = fopen (path, "rt");
      if (!fp) {
        fatal_error ("Unable to open \"gimprc\"");
      }
    } else {
      fatal_error ("\".gimprc\" file not found");
    }   
  }
  plug_ins = NULL;
  parse_a_gimprc(fp);
}

void
parse_a_gimprc (FILE *fp) {
  
  char str[200];
  char *token;
  struct stat stat_buf;

  /* Parse the file.
     This is a very basic parsing mechanism.
     It parses one line at a time. */
  
  while (!feof (fp)) {
    if (!fgets (str, 200, fp)) continue;
    
    if (str[0] != '#') {
      token = strtok (str, " \t\n");
      if (!token) continue;
      
      if (strcmp (token, "include") == 0) {
        char *include_file = get_token (strtok (NULL, "\n"), '\"');
        if (include_file) {
          FILE* newfp = fopen (include_file, "rt");
          if (!newfp) {
            fatal_error ("Unable to open included resource file.");
          }
          parse_a_gimprc(newfp);
        } else {
          warning ("no include file specified.");
        }
      } else if (strcmp (token, "swap-path") == 0) {
        Boolean path_error = True;
        
        swap_path = get_token (strtok (NULL, "\n"), '\"');
        if (swap_path) {
          if (!(stat (swap_path, &stat_buf))) {
            /*  do some consistency checks...  */
            if (! S_ISDIR (stat_buf.st_mode))
              warning ("\"%s\" is not a directory.", swap_path);
            else {
              swap_path = xstrdup (swap_path);
              path_error = False;
            }
          } else
            warning ("disk swap directory \"%s\" could not be found.",
                     swap_path);
        } else
          warning ("no swap directory specified.");
        
        if (path_error) {
          warning ("Using /tmp for disk swapping.");
          swap_path = xstrdup ("/tmp");
        }
      } else if (strcmp (token, "brush-path") == 0) {
        brush_path = get_token (strtok (NULL, "\n"), '\"');
        if (brush_path)
          brush_path = xstrdup (brush_path);
      } else if (strcmp (token, "default-brush") == 0) {
        default_brush = get_token (strtok (NULL, "\n"), '\"');
        if (default_brush)
          default_brush = xstrdup (default_brush);
      } else if (strcmp (token, "plug-in-path") == 0) {
        plug_in_path = get_token (strtok (NULL, "\n"), '\"');
        if (plug_in_path)
          plug_in_path = xstrdup (plug_in_path);
      } else if (strcmp (token, "file-plug-in") == 0) {
        char *prog, *types, *ext, *title;
        
        prog = strtok (NULL, " \t\n");
        types = strtok (NULL, " ");
        title = get_token (strtok (NULL, "\n"), '\"');
        ext = get_token (NULL, '\"');
        
        add_file_filter (ext, prog, types, title);
      } else if (strcmp (token, "plug-in") == 0) {
        char *prog, *title;
        char *accel, *accel_text;
        
        prog = strtok (NULL, " \t\n");
        title = get_token (strtok (NULL, "\n"), '\"');
        accel = get_token (NULL, '\"');
        accel_text = get_token (NULL, '\"');
        
        if (prog && title)
          plug_ins = 
            plug_in_add_item (plug_ins, prog, title, accel, accel_text);
      } else {
        warning ("unknown token: %s", token);
      }
    }
  }
  
  fclose (fp);
  
}


static char*
get_token (str, delim)
     char *str;
     int delim;
{
  static char *the_str = NULL;
  char *val;

  if (str)
    the_str = str;

  while (*the_str && (*the_str++ != delim))
    ;

  if (*the_str)
    val = the_str;
  else
    return NULL;

  while (*the_str && (*the_str++ != delim))
    ;

  if (*(the_str - 1) == delim)
    *(the_str - 1) = 0;
  
  return val;
}

