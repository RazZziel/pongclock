/*
 * spotlight - an xscreensaver module
 * Copyright (c) 1999, 2001 Rick Schultz <rick@skapunx.net>
 *
 * loosely based on the BackSpace module "StefView" by Darcy Brockbank
 */

/* modified from a module from the xscreensaver distribution */

/*
 * xscreensaver, Copyright (c) 1992-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


/* #define DEBUG */
#include <math.h>
#include "screenhack.h"

#define MINX 0.0
#define MINY 0.0
#define X_PERIOD 15000.0
#define Y_PERIOD 12000.0

struct state {
  Display *dpy;
  Window window;

  int sizex, sizey; /* screen size */
  int delay;        /* in case it's too fast... */
  GC window_gc;
#ifdef DEBUG
  GC white_gc;
#endif
  GC buffer_gc;     /* draw in buffer, then flush to screen
                       to avoid flicker */
  int radius;       /* radius of spotlight in pixels */

  Pixmap pm;        /* pixmap grabbed from screen */
  Pixmap buffer;    /* pixmap for the buffer */

  int x, y, s;      /* x & y coords of buffer (upper left corner) */
  /* s is the width of the buffer */

  int off;	/* random offset from currentTimeInMs(), so that
                   two concurrent copies of spotlight have different
                   behavior. */

  int oldx, oldy, max_x_speed, max_y_speed;
  /* used to keep the new buffer position
     over the old spotlight image to make sure 
     the old image is completely erased */

  Bool first_p;
  async_load_state *img_loader;
};


/* The path the spotlight follows around the screen is sinusoidal.
   This function is fed to sin() to get the x & y coords */
static long
currentTimeInMs(struct state *st)
{
  struct timeval curTime;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tz = {0,0};
  gettimeofday(&curTime, &tz);
#else
  gettimeofday(&curTime);
#endif
  return curTime.tv_sec*1000 + curTime.tv_usec/1000.0;
}


static void *
spotlight_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  long gcflags;
  Colormap cmap;
  unsigned long fg, bg;
  GC clip_gc;
  Pixmap clip_pm;

  st->dpy = dpy;
  st->window = window;
  st->first_p = True;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->sizex = xgwa.width;
  st->sizey = xgwa.height;
  cmap = xgwa.colormap;
  fg = get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
  bg = get_pixel_resource (st->dpy, cmap, "background", "Background");

  /* read parameters, keep em sane */
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 1) st->delay = 1;
  st->radius = get_integer_resource (st->dpy, "radius", "Integer");
  if (st->radius < 0) st->radius = 125;

  /* Don't let the spotlight be bigger than the window */
  while (st->radius > xgwa.width * 0.45)
    st->radius /= 2;
  while (st->radius > xgwa.height * 0.45)
    st->radius /= 2;

  if (st->radius < 4)
    st->radius = 4;

  /* do the dance */
  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground | GCFunction;
  gcv.foreground = bg;

#ifdef NOPE
  if (use_subwindow_mode_p(xgwa.screen, st->window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
#endif
  st->window_gc = XCreateGC(st->dpy, st->window, gcflags, &gcv);

  /* grab screen to pixmap */
  st->pm = XCreatePixmap(st->dpy, st->window, st->sizex, st->sizey, xgwa.depth);
  XClearWindow(st->dpy, st->window);

  /* create buffer to reduce flicker */
#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  st->buffer = 0;
#else
  st->buffer = XCreatePixmap(st->dpy, st->window, st->sizex, st->sizey, xgwa.depth);
#endif

  st->buffer_gc = XCreateGC(st->dpy, (st->buffer ? st->buffer : window), gcflags, &gcv);
  if (st->buffer)
    XFillRectangle(st->dpy, st->buffer, st->buffer_gc, 0, 0, st->sizex, st->sizey);

  /* blank out screen */
  XFillRectangle(st->dpy, st->window, st->window_gc, 0, 0, st->sizex, st->sizey);
  XSetWindowBackground (st->dpy, st->window, bg);

  /* create clip mask (so it's a circle, not a square) */
  clip_pm = XCreatePixmap(st->dpy, st->window, st->radius*4, st->radius*4, 1);
  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window, st->pm,
                                            0, 0);

  gcv.foreground = 0L;
  clip_gc = XCreateGC(st->dpy, clip_pm, gcflags, &gcv);
  XFillRectangle(st->dpy, clip_pm, clip_gc, 0, 0, st->radius*4, st->radius*4);

  XSetForeground(st->dpy, clip_gc, 1L);
  XFillArc(st->dpy, clip_pm, clip_gc, st->radius , st->radius,
	   st->radius*2, st->radius*2, 0, 360*64);

  /* set buffer's clip mask to the one we just made */
  XSetClipMask(st->dpy, st->buffer_gc, clip_pm);

  /* free everything */
  XFreeGC(st->dpy, clip_gc);
  XFreePixmap(st->dpy, clip_pm);

  /* avoid remants */
  st->max_x_speed = st->max_y_speed = st->radius;
  
  st->off = random();

#ifdef DEBUG
  /* create GC with white fg */
  gcv.foreground = fg;
  st->white_gc = XCreateGC(st->dpy, st->window, gcflags, &gcv);
#endif
  return st;
}


/*
 * perform one iteration
 */
static void
onestep (struct state *st, Bool first_p)
{
  long now;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      return;
    }

#define nrnd(x) (random() % (x))

  st->oldx = st->x;
  st->oldy = st->y;

  st->s = st->radius *4 ;   /* s = width of buffer */

  now = currentTimeInMs(st) + st->off;

  /* find new x,y */
  st->x = ((1 + sin(((double)now) / X_PERIOD * 2. * M_PI))/2.0) 
    * (st->sizex - st->s/2) -st->s/4  + MINX;
  st->y = ((1 + sin(((double)now) / Y_PERIOD * 2. * M_PI))/2.0) 
    * (st->sizey - st->s/2) -st->s/4  + MINY;
    
  if (!st->first_p)
    {
      /* limit change in x and y to buffer width */
      if ( st->x < (st->oldx - st->max_x_speed) ) st->x = st->oldx - st->max_x_speed;
      if ( st->x > (st->oldx + st->max_x_speed) ) st->x = st->oldx + st->max_x_speed;
      if ( st->y < (st->oldy - st->max_y_speed) ) st->y = st->oldy - st->max_y_speed;
      if ( st->y > (st->oldy + st->max_y_speed) ) st->y = st->oldy + st->max_y_speed;
    }

  if (! st->buffer)
    {
      XClearWindow (st->dpy, st->window);
      XSetClipOrigin(st->dpy, st->buffer_gc, st->x,st->y);
      XCopyArea(st->dpy, st->pm, st->window, st->buffer_gc, st->x, st->y, st->s, st->s, st->x, st->y);
    }
  else
    {
      /* clear buffer */
      XFillRectangle(st->dpy, st->buffer, st->buffer_gc, st->x, st->y, st->s, st->s);

      /* copy area of screen image (pm) to buffer
         Clip to a circle */
      XSetClipOrigin(st->dpy, st->buffer_gc, st->x,st->y);
      XCopyArea(st->dpy, st->pm, st->buffer, st->buffer_gc, st->x, st->y, st->s, st->s, st->x, st->y);

      /* copy buffer to screen (window) */
      XCopyArea(st->dpy, st->buffer, st->window, st->window_gc, st->x , st->y, st->s, st->s, st->x, st->y);
    }

#ifdef DEBUG
  /* draw a box around the buffer */
  XDrawRectangle(st->dpy, st->window, st->white_gc, st->x , st->y, st->s, st->s);
#endif

}


static unsigned long
spotlight_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  onestep(st, st->first_p);
  st->first_p = False;
  return st->delay;
}
  
static void
spotlight_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
spotlight_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
spotlight_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->window_gc);
  XFreeGC (dpy, st->buffer_gc);
  if (st->pm) XFreePixmap (dpy, st->pm);
  if (st->buffer) XFreePixmap (dpy, st->buffer);
  free (st);
}




static const char *spotlight_defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*dontClearRoot:		True",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*radius:			125",
  0
};

static XrmOptionDescRec spotlight_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-radius",		".radius",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Spotlight", spotlight)
