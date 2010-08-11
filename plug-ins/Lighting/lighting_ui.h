#ifndef __LIGHTING_UI_H__
#define __LIGHTING_UI_H__

/* Externally visible variables */
/* ============================ */

extern GdkGC     *gc;
extern GtkWidget *previewarea;

/* Externally visible functions */
/* ============================ */

gboolean main_dialog (GimpDrawable *drawable);

#endif  /* __LIGHTING_UI_H__ */
