/*************************************************/
/* Compute a preview image and preview wireframe */
/*************************************************/

#include "mapobject_preview.h"

line    linetab[WIRESIZE*2+8];
gdouble mat[3][4];

gint lightx,lighty;
BackBuffer backbuf={0,0,0,0,NULL};

/* Protos */
/* ====== */

void update_light       (gint xpos,gint ypos);
void draw_light_marker  (gint xpos,gint ypos);
void clear_light_marker (void);
void draw_wireframe_plane(gint startx,gint starty,gint pw,gint ph);
void draw_wireframe_sphere(gint startx,gint starty,gint pw,gint ph);
void clear_wireframe(void);

/**************************************************************/
/* Computes a preview of the rectangle starting at (x,y) with */
/* dimensions (w,h), placing the result in preview_RGB_data.  */ 
/**************************************************************/

void compute_preview(gint x,gint y,gint w,gint h,gint pw,gint ph)
{
  gdouble xpostab[PREVIEW_WIDTH],ypostab[PREVIEW_HEIGHT],realw,realh;
  GckVector3 p1,p2;
  GckRGB color,lightcheck,darkcheck,temp;
  guchar r,g,b;
  gint xcnt,ycnt,f1,f2;
  glong index=0;

  init_compute();
  
  p1=int_to_pos(x,y);
  p2=int_to_pos(x+w,y+h);

  /* First, compute the linear mapping (x,y,x+w,y+h) to (0,0,pw,ph) */
  /* ============================================================== */

  realw=(p2.x-p1.x);
  realh=(p2.y-p1.y);

  for (xcnt=0;xcnt<pw;xcnt++)
    xpostab[xcnt]=p1.x+realw*((double)xcnt/(double)pw);
 
  for (ycnt=0;ycnt<ph;ycnt++)
    ypostab[ycnt]=p1.y+realh*((double)ycnt/(double)ph);
  
  /* Compute preview using the offset tables */
  /* ======================================= */

  if (mapvals.transparent_background==TRUE)
    gck_rgba_set(&background,0.0,0.0,0.0,0.0);
  else
    {
      gimp_palette_get_background(&r,&g,&b);
      background.r=(gdouble)r/255.0;
      background.g=(gdouble)g/255.0;
      background.b=(gdouble)b/255.0;
      background.a=1.0;
    }

  gck_rgb_set(&lightcheck,0.75,0.75,0.75);
  gck_rgb_set(&darkcheck, 0.50,0.50,0.50);
  gck_vector3_set(&p2,-1.0,-1.0,0.0);

  for (ycnt=0;ycnt<ph;ycnt++)
    {
      for (xcnt=0;xcnt<pw;xcnt++)
        {
          p1.x=xpostab[xcnt];
          p1.y=ypostab[ycnt];
          
          p2=p1;
          color=(*get_ray_color)(&p1);

          if (color.a<1.0)
            {
              f1=((xcnt % 32)<16);
              f2=((ycnt % 32)<16);
              f1=f1^f2;

              if (f1)
                {
                  if (color.a==0.0)
                    color=lightcheck;
                  else
                    {
                      gck_rgb_mul(&color,color.a);
                      temp=lightcheck;
                      gck_rgb_mul(&temp,1.0-color.a);
                      gck_rgb_add(&color,&temp);
                    }
                }
              else
                {
                  if (color.a==0.0)
                    color=darkcheck;
                  else
                    {
                      gck_rgb_mul(&color,color.a);
                      temp=darkcheck;
                      gck_rgb_mul(&temp,1.0-color.a);
                      gck_rgb_add(&color,&temp);
                    }
                }
            }

          preview_rgb_data[index++]=(guchar)(color.r*255.0);
          preview_rgb_data[index++]=(guchar)(color.g*255.0);
          preview_rgb_data[index++]=(guchar)(color.b*255.0);
        }
    }

  /* Convert to visual type */
  /* ====================== */

  gck_rgb_to_gdkimage(appwin->visinfo,preview_rgb_data,image,pw,ph);
}

/*************************************************/
/* Check if the given position is within the     */
/* light marker. Return TRUE if so, FALSE if not */
/*************************************************/

gint check_light_hit(gint xpos,gint ypos)
{
  gdouble dx,dy,r;
  
  if (mapvals.lightsource.type==POINT_LIGHT)
    {
      dx=(gdouble)lightx-xpos;
      dy=(gdouble)lighty-ypos;
      r=sqrt(dx*dx+dy*dy)+0.5;
      if ((gint)r>7)
        return(FALSE);
      else
        return(TRUE);
    }
  
  return(FALSE);
}

/****************************************/
/* Draw a marker to show light position */
/****************************************/

void draw_light_marker(gint xpos,gint ypos)
{
  gck_gc_set_foreground(appwin->visinfo,gc,0,50,255);
  gck_gc_set_background(appwin->visinfo,gc,0,0,0);

  gdk_gc_set_function(gc,GDK_COPY);

  if (mapvals.lightsource.type==POINT_LIGHT)
    {
      lightx=xpos;
      lighty=ypos;
    
      /* Save background */
      /* =============== */
 
      backbuf.x=lightx-7;
      backbuf.y=lighty-7;
      backbuf.w=14;
      backbuf.h=14;
    
      /* X doesn't like images that's outside a window, make sure */
      /* we get the backbuffer image from within the boundaries   */
      /* ======================================================== */
 
      if (backbuf.x<0)
        backbuf.x=0;
      else if ((backbuf.x+backbuf.w)>PREVIEW_WIDTH)
        backbuf.w=(PREVIEW_WIDTH-backbuf.x);
      if (backbuf.y<0)
        backbuf.y=0;
      else if ((backbuf.y+backbuf.h)>PREVIEW_HEIGHT)
        backbuf.h=(PREVIEW_WIDTH-backbuf.y);
 
      backbuf.image=gdk_image_get(previewarea->window,backbuf.x,backbuf.y,backbuf.w,backbuf.h);
      gdk_draw_arc(previewarea->window,gc,TRUE,lightx-7,lighty-7,14,14,0,360*64);
    }
}

void clear_light_marker()
{
  /* Restore background if it has been saved */
  /* ======================================= */
  
  if (backbuf.image!=NULL)
    {
      gck_gc_set_foreground(appwin->visinfo,gc,255,255,255);
      gck_gc_set_background(appwin->visinfo,gc,0,0,0);

      gdk_gc_set_function(gc,GDK_COPY);
      gdk_draw_image(previewarea->window,gc,backbuf.image,0,0,backbuf.x,backbuf.y,
        backbuf.w,backbuf.h);
      gdk_image_destroy(backbuf.image);
      backbuf.image=NULL;
    }
}

void draw_lights(gint startx,gint starty,gint pw,gint ph)
{
  gdouble dxpos,dypos;
  gint xpos,ypos;

  clear_light_marker();
 
  gck_3d_to_2d(startx,starty,pw,ph,&dxpos,&dypos,&mapvals.viewpoint,
    &mapvals.lightsource.position);
  xpos=(gint)(dxpos+0.5);
  ypos=(gint)(dypos+0.5);

  if (xpos>=0 && xpos<=PREVIEW_WIDTH && ypos>=0 && ypos<=PREVIEW_HEIGHT)
    draw_light_marker(xpos,ypos);
}

/*************************************************/
/* Update light position given new screen coords */
/*************************************************/

void update_light(gint xpos,gint ypos)
{
  gint startx,starty,pw,ph;

  pw=PREVIEW_WIDTH >> mapvals.preview_zoom_factor;
  ph=PREVIEW_HEIGHT >> mapvals.preview_zoom_factor;
  startx=(PREVIEW_WIDTH-pw)>>1;
  starty=(PREVIEW_HEIGHT-ph)>>1;
  
  gck_2d_to_3d(startx,starty,pw,ph,xpos,ypos,&mapvals.viewpoint,&mapvals.lightsource.position);
  draw_lights(startx,starty,pw,ph);
}

/******************************************************************/
/* Draw preview image. if DoCompute is TRUE then recompute image. */
/******************************************************************/

void draw_preview_image(gint docompute)
{
  gint startx,starty,pw,ph;
  
  gck_gc_set_foreground(appwin->visinfo,gc,255,255,255);
  gck_gc_set_background(appwin->visinfo,gc,0,0,0);

  gdk_gc_set_function(gc,GDK_COPY);
  linetab[0].x1=-1;

  pw=PREVIEW_WIDTH >> mapvals.preview_zoom_factor;
  ph=PREVIEW_HEIGHT >> mapvals.preview_zoom_factor;
  startx=(PREVIEW_WIDTH-pw)>>1;
  starty=(PREVIEW_HEIGHT-ph)>>1;

  if (docompute==TRUE)
    {
      gck_cursor_set(previewarea->window,GDK_WATCH);
      compute_preview(0,0,width-1,height-1,pw,ph);
      gck_cursor_set(previewarea->window,GDK_HAND2);
      clear_light_marker();
    }

  if (pw!=PREVIEW_WIDTH)
    gdk_window_clear(previewarea->window);
  
  gdk_draw_image(previewarea->window,gc,image,0,0,startx,starty,pw,ph);
  draw_lights(startx,starty,pw,ph);
}

/**************************/
/* Draw preview wireframe */
/**************************/

void draw_preview_wireframe(void)
{
  gint startx,starty,pw,ph;

  gck_gc_set_foreground(appwin->visinfo,gc,255,255,255);
  gck_gc_set_background(appwin->visinfo,gc,0,0,0);

  gdk_gc_set_function(gc,GDK_INVERT);

  pw=PREVIEW_WIDTH >> mapvals.preview_zoom_factor;
  ph=PREVIEW_HEIGHT >> mapvals.preview_zoom_factor;
  startx=(PREVIEW_WIDTH-pw)>>1;
  starty=(PREVIEW_HEIGHT-ph)>>1;

  clear_wireframe();
  draw_wireframe(startx,starty,pw,ph);
}

/****************************/
/* Draw a wireframe preview */
/****************************/

void draw_wireframe(gint startx,gint starty,gint pw,gint ph)
{
  switch (mapvals.maptype)
    {
      case MAP_PLANE:
        draw_wireframe_plane(startx,starty,pw,ph);
        break;
      case MAP_SPHERE:
        draw_wireframe_sphere(startx,starty,pw,ph);
        break;
    }
}

void draw_wireframe_plane(gint startx,gint starty,gint pw,gint ph)
{
  GckVector3 v1,v2,a,b,c,d,dir1,dir2;
  gint cnt,n=0;
  gdouble x1,y1,x2,y2,cx1,cy1,cx2,cy2,fac;
  
  /* Find rotated box corners */
  /* ======================== */

  gck_vector3_set(&v1,0.5,0.0,0.0);
  gck_vector3_set(&v2,0.0,0.5,0.0);

  gck_vector3_rotate(&v1,gck_deg_to_rad(mapvals.alpha),
    gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));
  gck_vector3_rotate(&v2,gck_deg_to_rad(mapvals.alpha),
    gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));

  dir1=v1; gck_vector3_normalize(&dir1);
  dir2=v2; gck_vector3_normalize(&dir2);

  fac=1.0/(gdouble)WIRESIZE;
  
  gck_vector3_mul(&dir1,fac);
  gck_vector3_mul(&dir2,fac);
  
  gck_vector3_add(&a,&mapvals.position,&v1);
  gck_vector3_sub(&b,&a,&v2);
  gck_vector3_add(&a,&a,&v2);
  gck_vector3_sub(&d,&mapvals.position,&v1);
  gck_vector3_sub(&d,&d,&v2);

  c=b;

  cx1=(gdouble)startx;
  cy1=(gdouble)starty;
  cx2=cx1+(gdouble)pw;
  cy2=cy1+(gdouble)ph;

  for (cnt=0;cnt<=WIRESIZE;cnt++)
    {
      gck_3d_to_2d(startx,starty,pw,ph,&x1,&y1,&mapvals.viewpoint,&a);
      gck_3d_to_2d(startx,starty,pw,ph,&x2,&y2,&mapvals.viewpoint,&b);

      if (gck_clip_line(&x1,&y1,&x2,&y2,cx1,cy1,cx2,cy2)==TRUE)
        {
          linetab[n].x1=(gint)(x1+0.5);
          linetab[n].y1=(gint)(y1+0.5);
          linetab[n].x2=(gint)(x2+0.5);
          linetab[n].y2=(gint)(y2+0.5);
          linetab[n].linewidth=1;
          linetab[n].linestyle=GDK_LINE_SOLID;
          gdk_gc_set_line_attributes(gc,linetab[n].linewidth,linetab[n].linestyle,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
          gdk_draw_line(previewarea->window,gc,linetab[n].x1,linetab[n].y1,linetab[n].x2,linetab[n].y2);
          n++;
        }

      gck_3d_to_2d(startx,starty,pw,ph,&x1,&y1,&mapvals.viewpoint,&c);
      gck_3d_to_2d(startx,starty,pw,ph,&x2,&y2,&mapvals.viewpoint,&d);

      if (gck_clip_line(&x1,&y1,&x2,&y2,cx1,cy1,cx2,cy2)==TRUE)
        {
          linetab[n].x1=(gint)(x1+0.5);
          linetab[n].y1=(gint)(y1+0.5);
          linetab[n].x2=(gint)(x2+0.5);
          linetab[n].y2=(gint)(y2+0.5);
          linetab[n].linewidth=1;
          linetab[n].linestyle=GDK_LINE_SOLID;
          gdk_gc_set_line_attributes(gc,linetab[n].linewidth,linetab[n].linestyle,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
          gdk_draw_line(previewarea->window,gc,linetab[n].x1,linetab[n].y1,linetab[n].x2,linetab[n].y2);
          n++;
        }
        
      gck_vector3_sub(&a,&a,&dir1);
      gck_vector3_sub(&b,&b,&dir1);
      gck_vector3_add(&c,&c,&dir2);
      gck_vector3_add(&d,&d,&dir2);
    }

  /* Mark end of lines */
  /* ================= */

  linetab[n].x1=-1;
}

void draw_wireframe_sphere(gint startx,gint starty,gint pw,gint ph)
{
  GckVector3 p[2*(WIRESIZE+5)];
  gint cnt,cnt2,n=0;
  gdouble x1,y1,x2,y2,twopifac,cx1,cy1,cx2,cy2;
  
  /* Compute wireframe points */
  /* ======================== */

  twopifac=(2.0*M_PI)/WIRESIZE;
  
  for (cnt=0;cnt<WIRESIZE;cnt++)
    {
      p[cnt].x=mapvals.radius*cos((gdouble)cnt*twopifac);
      p[cnt].y=0.0;
      p[cnt].z=mapvals.radius*sin((gdouble)cnt*twopifac);
      gck_vector3_rotate(&p[cnt],gck_deg_to_rad(mapvals.alpha),
        gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));
      gck_vector3_add(&p[cnt],&p[cnt],&mapvals.position);
    }
  p[cnt]=p[0];
  for (cnt=WIRESIZE+1;cnt<2*WIRESIZE+1;cnt++)
    {
      p[cnt].x=mapvals.radius*cos((gdouble)(cnt-(WIRESIZE+1))*twopifac);
      p[cnt].y=mapvals.radius*sin((gdouble)(cnt-(WIRESIZE+1))*twopifac);
      p[cnt].z=0.0;
      gck_vector3_rotate(&p[cnt],gck_deg_to_rad(mapvals.alpha),
        gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));
      gck_vector3_add(&p[cnt],&p[cnt],&mapvals.position);
    }
  p[cnt]=p[WIRESIZE+1];
  cnt++;
  cnt2=cnt;
  
  /* Find rotated axis */
  /* ================= */

  gck_vector3_set(&p[cnt],0.0,-0.35,0.0);
  gck_vector3_rotate(&p[cnt],gck_deg_to_rad(mapvals.alpha),
    gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));
  p[cnt+1]=mapvals.position;

  gck_vector3_set(&p[cnt+2],0.0,0.0,-0.35);
  gck_vector3_rotate(&p[cnt+2],gck_deg_to_rad(mapvals.alpha),
    gck_deg_to_rad(mapvals.beta),gck_deg_to_rad(mapvals.gamma));
  p[cnt+3]=mapvals.position;

  p[cnt+4]=p[cnt];
  gck_vector3_mul(&p[cnt+4],-1.0);
  p[cnt+5]=p[cnt+1];

  gck_vector3_add(&p[cnt],&p[cnt],&mapvals.position);
  gck_vector3_add(&p[cnt+2],&p[cnt+2],&mapvals.position);
  gck_vector3_add(&p[cnt+4],&p[cnt+4],&mapvals.position);

  /* Draw the circles (equator and zero meridian) */
  /* ============================================ */

  cx1=(gdouble)startx;
  cy1=(gdouble)starty;
  cx2=cx1+(gdouble)pw;
  cy2=cy1+(gdouble)ph;

  for (cnt=0;cnt<cnt2-1;cnt++)
    {
      if (p[cnt].z>mapvals.position.z && p[cnt+1].z>mapvals.position.z)
        {
          gck_3d_to_2d(startx,starty,pw,ph,&x1,&y1,&mapvals.viewpoint,&p[cnt]);
          gck_3d_to_2d(startx,starty,pw,ph,&x2,&y2,&mapvals.viewpoint,&p[cnt+1]);
 
          if (gck_clip_line(&x1,&y1,&x2,&y2,cx1,cy1,cx2,cy2)==TRUE)
            {
              linetab[n].x1=(gint)(x1+0.5);
              linetab[n].y1=(gint)(y1+0.5);
              linetab[n].x2=(gint)(x2+0.5);
              linetab[n].y2=(gint)(y2+0.5);
              linetab[n].linewidth=3;
              linetab[n].linestyle=GDK_LINE_SOLID;
              gdk_gc_set_line_attributes(gc,linetab[n].linewidth,linetab[n].linestyle,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
              gdk_draw_line(previewarea->window,gc,linetab[n].x1,linetab[n].y1,linetab[n].x2,linetab[n].y2);
              n++;
            }
        }
    }

  /* Draw the axis (pole to pole and center to zero meridian) */
  /* ======================================================== */

  for (cnt=0;cnt<3;cnt++)
    {
      gck_3d_to_2d(startx,starty,pw,ph,&x1,&y1,&mapvals.viewpoint,&p[cnt2]);
      gck_3d_to_2d(startx,starty,pw,ph,&x2,&y2,&mapvals.viewpoint,&p[cnt2+1]);

      if (gck_clip_line(&x1,&y1,&x2,&y2,cx1,cy1,cx2,cy2)==TRUE)
        {
          linetab[n].x1=(gint)(x1+0.5);
          linetab[n].y1=(gint)(y1+0.5);
          linetab[n].x2=(gint)(x2+0.5);
          linetab[n].y2=(gint)(y2+0.5);

          if (p[cnt2].z<mapvals.position.z || p[cnt2+1].z<mapvals.position.z)
            {
              linetab[n].linewidth=1;
              linetab[n].linestyle=GDK_LINE_DOUBLE_DASH;
            }
          else
            {
              linetab[n].linewidth=3;
              linetab[n].linestyle=GDK_LINE_SOLID;
            }
          gdk_gc_set_line_attributes(gc,linetab[n].linewidth,linetab[n].linestyle,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
          gdk_draw_line(previewarea->window,gc,linetab[n].x1,linetab[n].y1,linetab[n].x2,linetab[n].y2);
          n++;
        }
      cnt2+=2;
    }

  /* Mark end of lines */
  /* ================= */

  linetab[n].x1=-1;
}

void clear_wireframe(void)
{
  gint n=0;
  
  while (linetab[n].x1!=-1)
    {
      gdk_gc_set_line_attributes(gc,linetab[n].linewidth,linetab[n].linestyle,GDK_CAP_NOT_LAST,GDK_JOIN_MITER);
      gdk_draw_line(previewarea->window,gc,linetab[n].x1,linetab[n].y1,
        linetab[n].x2,linetab[n].y2);
      n++;
    }
}
