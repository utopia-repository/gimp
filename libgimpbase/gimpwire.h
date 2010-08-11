/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_WIRE_H__
#define __GIMP_WIRE_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


typedef struct _WireMessage  WireMessage;

typedef void     (* WireReadFunc)    (GIOChannel  *channel,
				      WireMessage *msg,
                                      gpointer     user_data);
typedef void     (* WireWriteFunc)   (GIOChannel  *channel,
				      WireMessage *msg,
                                      gpointer     user_data);
typedef void     (* WireDestroyFunc) (WireMessage *msg);
typedef gboolean (* WireIOFunc)      (GIOChannel  *channel,
				      guint8      *buf,
				      gulong       count,
                                      gpointer     user_data);
typedef gboolean (* WireFlushFunc)   (GIOChannel  *channel,
                                      gpointer     user_data);


struct _WireMessage
{
  guint32  type;
  gpointer data;
};


void      wire_register     (guint32          type,
			     WireReadFunc     read_func,
			     WireWriteFunc    write_func,
			     WireDestroyFunc  destroy_func);

void      wire_set_reader   (WireIOFunc       read_func);
void      wire_set_writer   (WireIOFunc       write_func);
void      wire_set_flusher  (WireFlushFunc    flush_func);

gboolean  wire_read         (GIOChannel	     *channel,
			     guint8          *buf,
			     gsize            count,
                             gpointer         user_data);
gboolean  wire_write        (GIOChannel      *channel,
			     guint8          *buf,
			     gsize            count,
                             gpointer         user_data);
gboolean  wire_flush        (GIOChannel	     *channel,
                             gpointer         user_data);

gboolean  wire_error        (void);
void      wire_clear_error  (void);

gboolean  wire_read_msg     (GIOChannel	     *channel,
			     WireMessage     *msg,
                             gpointer         user_data);
gboolean  wire_write_msg    (GIOChannel	     *channel,
			     WireMessage     *msg,
                             gpointer         user_data);

void      wire_destroy      (WireMessage     *msg);

gboolean  wire_read_int32   (GIOChannel      *channel,
			     guint32         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_read_int16   (GIOChannel	     *channel,
			     guint16         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_read_int8    (GIOChannel      *channel,
			     guint8          *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_read_double  (GIOChannel      *channel,
			     gdouble         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_read_string  (GIOChannel      *channel,
			     gchar          **data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_write_int32  (GIOChannel      *channel,
			     guint32         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_write_int16  (GIOChannel      *channel,
			     guint16         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_write_int8   (GIOChannel      *channel,
			     guint8          *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_write_double (GIOChannel      *channel,
			     gdouble         *data,
			     gint             count,
                             gpointer         user_data);
gboolean  wire_write_string (GIOChannel      *channel,
			     gchar          **data,
			     gint             count,
                             gpointer         user_data);


G_END_DECLS

#endif /* __GIMP_WIRE_H__ */
