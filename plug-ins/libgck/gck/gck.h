/***************************************************************************/
/* GCK - The General Convenience Kit. Generally useful conveniece routines */
/* for GIMP plug-in writers and users of the GDK/GTK libraries.            */
/* Copyright (C) 1996 Tom Bech                                             */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU General Public License as published by    */
/* the Free Software Foundation; either version 2 of the License, or       */
/* (at your option) any later version.                                     */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU General Public License for more details.                            */
/*                                                                         */
/* You should have received a copy of the GNU General Public License       */
/* along with this program; if not, write to the Free Software             */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,   */
/* USA.                                                                    */
/***************************************************************************/

#ifndef __GCK_H__
#define __GCK_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define g_function_enter(fname)
#define g_function_leave(fname)

#include <gck/gckcommon.h>
#include <gck/gcktypes.h>
#include <gck/gckcolor.h>
#include <gck/gckmath.h>
#include <gck/gckvector.h>
#include <gck/gckui.h>
#include <gck/gcklistbox.h>

#ifdef __cplusplus
}
#endif

#endif
