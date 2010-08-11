#ifndef MAPOBJECTMAINH
#define MAPOBJECTMAINH

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gck/gck.h>
#include <libgimp/gimp.h>

#include "arcball.h"
#include "mapobject_ui.h"
#include "mapobject_image.h"
#include "mapobject_apply.h"
#include "mapobject_preview.h"

/* Defines and stuff */
/* ================= */

#define TILE_CACHE_SIZE 16

/* Typedefs */
/* ======== */

typedef enum {
  POINT_LIGHT,
  DIRECTIONAL_LIGHT,
  NO_LIGHT
} LightType;

typedef enum {
  MAP_PLANE,
  MAP_SPHERE
} MapType;

/* Typedefs */
/* ======== */

typedef struct
{
  gdouble ambient_int;
  gdouble diffuse_int;
  gdouble diffuse_ref;
  gdouble specular_ref;
  gdouble highlight;
  GckRGB  color;
} MaterialSettings;

typedef struct
{
  LightType  type;
  GckVector3 position;
  GckVector3 direction;
  GckRGB     color;
  gdouble    intensity;
} LightSettings;

typedef struct {
  GckVector3    viewpoint,firstaxis,secondaxis,normal,position;
  LightSettings lightsource;

  MaterialSettings material;
  MaterialSettings refmaterial;

  MapType maptype;

  gint antialiasing;
  gint create_new_image;
  gint transparent_background;
  gint tiled;
  gint showgrid;
  gint tooltips_enabled;
  
  glong preview_zoom_factor;
  
  gdouble alpha,beta,gamma;
  gdouble maxdepth;
  gdouble pixeltreshold;
  gdouble radius;

} MapObjectValues;

/* Externally visible variables */
/* ============================ */

extern MapObjectValues mapvals;
extern GckRGB background;

#endif
