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
#include <string.h>
#include <sys/stat.h>
#include "general.h"
#include "memutils.h"


/* prune filename removes all of the leading path information to a filename */

char *
prune_filename (filename)
     char *filename;
{
  char *last_slash = filename;

  while (*filename)
    if (*filename++ == '/')
      last_slash = filename;

  return last_slash;
}


char*
search_in_path (search_path, filename)
     char *search_path, *filename;
{
  static char path[256];
  static char *home = NULL;

  char *local_path, *token;
  struct stat buf;
  int err;

  if (!home)
    home = getenv ("HOME");

  local_path = xstrdup (search_path);
  token = strtok (local_path, ":");

  while (token)
    {
      if (*token == '~')
	sprintf (path, "%s%s", home, token + 1);
      else
	sprintf (path, "%s", token);

      if (token[strlen (token) - 1] != '/')
	strcat (path, "/");
      strcat (path, filename);
      
      err = stat (path, &buf);
      if (!err && S_ISREG (buf.st_mode))
	{
	  token = path;
	  break;
	}

      token = strtok (NULL, ":");
    }

  xfree (local_path);
  return token;
}

char *
xstrdup (str)
     char *str;
{
  char *new_str;

  new_str = NULL;
  if (str)
    {
      new_str = xmalloc ((strlen (str) + 1) * sizeof (char));
      strcpy (new_str, str);
    }

  return new_str;
}

