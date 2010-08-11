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
#ifndef __PROGRESS_H__
#define __PROGRESS_H__

typedef void (*CancelCallback) (void *, void *);
typedef struct _Progress _Progress, *ProgressP;

struct _Progress {
  long           amount;
  CancelCallback callback;
  void         * callback_data;
  Widget         pbar;
  Pixmap         pixmap;
  Widget         cancel_cur;
  Widget         cancel_all;
  Widget         shell;
  Widget         dialog;
  long           width;
  long           height;
};

ProgressP progress_new (char *, char *, CancelCallback, void *);
void progress_free (ProgressP);
void progress_update (ProgressP, long);

#endif /* __PROGRESS_H__ */
