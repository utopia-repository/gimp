
#ifndef ___gimp_widgets_marshal_MARSHAL_H__
#define ___gimp_widgets_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:INT (./gimpwidgetsmarshal.list:25) */
#define _gimp_widgets_marshal_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:INT,INT (./gimpwidgetsmarshal.list:26) */
extern void _gimp_widgets_marshal_VOID__INT_INT (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* VOID:OBJECT (./gimpwidgetsmarshal.list:27) */
#define _gimp_widgets_marshal_VOID__OBJECT	g_cclosure_marshal_VOID__OBJECT

/* VOID:OBJECT,INT (./gimpwidgetsmarshal.list:28) */
extern void _gimp_widgets_marshal_VOID__OBJECT_INT (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:POINTER,POINTER (./gimpwidgetsmarshal.list:29) */
extern void _gimp_widgets_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* VOID:STRING,FLAGS (./gimpwidgetsmarshal.list:30) */
extern void _gimp_widgets_marshal_VOID__STRING_FLAGS (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* BOOLEAN:POINTER (./gimpwidgetsmarshal.list:32) */
extern void _gimp_widgets_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

G_END_DECLS

#endif /* ___gimp_widgets_marshal_MARSHAL_H__ */

