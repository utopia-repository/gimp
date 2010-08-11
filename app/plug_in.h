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
#ifndef __PLUG_IN_H__
#define __PLUG_IN_H__

#include "autodialog.h"
#include "linked.h"
#include "msg.h"
#include "progress.h"

typedef void (*PlugInCallback) (void *, void *);
typedef struct _PlugIn  PlugIn, *PlugInP;

struct _PlugIn {
  int open;                   /* Is the plug-in open */
  int destroy;                /* Should the plug-in by destroyed (by free_plug_in) */
  pid_t pid;                  /* Plug-ins process id */
  char **args;                /* Plug-ins command line arguments */

  int my_read, my_write;      /* Apps read and write file descriptors */
  int his_read, his_write;    /* Plug-ins read and write file descriptors */

  Msg send_msg, recv_msg;     /* Send and receive message structures */

  XtInputId input_id;         /* ID for input source */
  link_ptr dialogs;           /* list of dialogs this plug-in has created */
  link_ptr messages;          /* list of messages this plug-in has created */
  link_ptr images;            /* list of images the plug-in has created */
  ProgressP progress;         /* progress bar */

  PlugInCallback callback;    /* the procedure to be called on completion */
  void *callback_data;        /* data for the callback */

  void *image;                /* image this plug-in is attached to */
  void *display;              /* display this plug-in is attached to */
  int load_image;             /* is this plug-in loading an image */
};

void plug_in_init (void);
void plug_in_kill (void);

PlugInP make_plug_in (char *, void *, void *);
link_ptr plug_in_add_item (link_ptr, char *, char *, char *, char *);
void free_plug_in (PlugInP);
int plug_in_open (PlugInP);
void plug_in_close (PlugInP, int);
void plug_in_set_callback (PlugInP, PlugInCallback, void *);
void plug_in_send_message (PlugInP, long, long, void *);
void plug_in_recv_message (XtPointer, int *, XtInputId *);

#endif /* __PLUG_IN_H__ */
