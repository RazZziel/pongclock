/* mismunch.c
 * Munch Errors
 * Copyright (c) 2004 Steven Hazel <sah@thalassocracy.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Based on munch.c
 * A munching squares implementation for X
 * Copyright 1997, Tim Showalter <tjs@andrew.cmu.edu>
 *
 * Some code stolen from / This is meant to work with
 * xscreensaver, Copyright (c) 1992, 1995, 1996 Jamie Zawinski <jwz@jwz.org>
 *
 */

#include <math.h>
#include "screenhack.h"

typedef struct _muncher {
  int width;
  int atX, atY;
  int kX, kT, kY;
  int grav;
  XColor fgc;
  int yshadow, xshadow;
  int x, y, t;
  int doom;
  int done;
} muncher;


struct state {
  Display *dpy;
  Window window;

  GC gc;
  int delay, simul, clear, xor;
  int logminwidth, logmaxwidth;
  int restart, window_width, window_height;

  int draw_n;  /* number of squares before we have to clear */
  int draw_i;

  muncher **munchers;
};


/*
 * dumb way to get # of digits in number.  Probably faster than actually
 * doing a log and a division, maybe.
 */
static int dumb_log_2(int k) 
{
  int r = -1;
  while (k > 0) {
    k >>= 1; r++;
  }
  return r;
}


static void calc_logwidths (struct state *st) 
{
  /* Choose a range of square sizes based on the window size.  We want
     a power of 2 for the square width or the munch doesn't fill up.
     Also, if a square doesn't fit inside an area 20% smaller than the
     window, it's too big.  Mismunched squares that big make things
     look too noisy. */

  if (st->window_height < st->window_width) {
    st->logmaxwidth = (int)dumb_log_2(st->window_height * 0.8);
  } else {
    st->logmaxwidth = (int)dumb_log_2(st->window_width * 0.8);
  }

  if (st->logmaxwidth < 2) {
    st->logmaxwidth = 2;
  }

  /* we always want three sizes of squares */
  st->logminwidth = st->logmaxwidth - 2;

  if (st->logminwidth < 2) {
    st->logminwidth = 2;
  }
}



static muncher *make_muncher (struct state *st) 
{
  int logwidth;
  XWindowAttributes xgwa;
  muncher *m = (muncher *) malloc(sizeof(muncher));

  XGetWindowAttributes(st->dpy, st->window, &xgwa);

  /* choose size -- power of two */
  logwidth = (st->logminwidth +
              (random() % (1 + st->logmaxwidth - st->logminwidth)));

  m->width = 1 << logwidth;

  /* draw at this location */
  m->atX = (random() % (xgwa.width <= m->width ? 1
                        : xgwa.width - m->width));
  m->atY = (random() % (xgwa.height <= m->width ? 1
                        : xgwa.width - m->width));

  /* wrap-around by these values; no need to % as we end up doing that
     later anyway */
  m->kX = ((random() % 2)
           ? (random() % m->width) : 0);
  m->kT = ((random() % 2)
           ? (random() % m->width) : 0);
  m->kY = ((random() % 2)
           ? (random() % m->width) : 0);

  /* set the gravity of the munch, or rather, which direction we draw
     stuff in. */
  m->grav = random() % 2;

  /* I like this color scheme better than random colors. */
  switch (random() % 4) {
    case 0:
      m->fgc.red = random() % 65536;
      m->fgc.blue = random() % 32768;
      m->fgc.green = random() % 16384;
      break;

    case 1:
      m->fgc.red = 0;
      m->fgc.blue = random() % 65536;
      m->fgc.green = random() % 16384;
      break;

    case 2:
      m->fgc.red = random() % 8192;
      m->fgc.blue = random() % 8192;
      m->fgc.green = random() % 49152;
      break;

    case 3:
      m->fgc.red = random() % 65536;
      m->fgc.green = m->fgc.red;
      m->fgc.blue = m->fgc.red;
      break;
  }

  /* Sometimes draw a mostly-overlapping copy of the square.  This
     generates all kinds of neat blocky graphics when drawing in xor
     mode. */
  if (random() % 4) {
    m->xshadow = 0;
    m->yshadow = 0;
  } else {
    m->xshadow = (random() % (m->width/3)) - (m->width/6);
    m->yshadow = (random() % (m->width/3)) - (m->width/6);
  }

  /* Start with a random y value -- this sort of controls the type of
     deformities seen in the squares. */
  m->y = random() % 256;

  m->t = 0;

  /*
    Doom each square to be aborted at some random point.
    (When doom == (width - 1), the entire square will be drawn.)
  */
  m->doom = random() % m->width;

  m->done = 0;

  return m;
}


static void munch (struct state *st, muncher *m) 
{
  int drawX, drawY;
  XWindowAttributes xgwa;

  if (m->done) {
    return;
  }

  XGetWindowAttributes(st->dpy, st->window, &xgwa);

  if (!mono_p) {
    /* XXX there are probably bugs with this. */
    if (XAllocColor(st->dpy, xgwa.colormap, &m->fgc)) {
      XSetForeground(st->dpy, st->gc, m->fgc.pixel);
    }
  }

  /* Finally draw this pass of the munching error. */

  for(m->x = 0; m->x < m->width; m->x++) {
    /* figure out the next point */

    /*
      The ordinary Munching Squares calculation is:
      m->y = ((m->x ^ ((m->t + m->kT) % m->width)) + m->kY) % m->width;

      We create some feedback by plugging in y in place of x, and
      make a couple of values negative so that some parts of some
      squares get drawn in the wrong place.
    */
    m->y = ((-m->y ^ ((-m->t + m->kT) % m->width)) + m->kY) % m->width;

    drawX = ((m->x + m->kX) % m->width) + m->atX;
    drawY = (m->grav ? m->y + m->atY : m->atY + m->width - 1 - m->y);

    XDrawPoint(st->dpy, st->window, st->gc, drawX, drawY);
    if ((m->xshadow != 0) || (m->yshadow != 0)) {
      /* draw the corresponding shadow point */
      XDrawPoint(st->dpy, st->window, st->gc, drawX + m->xshadow, drawY + m->yshadow);
    }
    /* XXX may want to change this to XDrawPoints,
       but it's fast enough without it for the moment. */

  }

  m->t++;
  if (m->t > m->doom) {
    m->done = 1;
  }
}


static void *
mismunch_init (Display *dpy, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes xgwa;
  XGCValues gcv;
  int i;

  st->dpy = dpy;
  st->window = w;
  st->restart = 0;

  /* get the dimensions of the window */
  XGetWindowAttributes(st->dpy, w, &xgwa);

  /* create the gc */
  gcv.foreground= get_pixel_resource(st->dpy, xgwa.colormap,
                                     "foreground","Foreground");
  gcv.background= get_pixel_resource(st->dpy, xgwa.colormap,
                                     "background","Background");

  st->gc = XCreateGC(st->dpy, w, GCForeground|GCBackground, &gcv);

  st->delay = get_integer_resource(st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;

  st->simul = get_integer_resource(st->dpy, "simul", "Integer");
  if (st->simul < 1) st->simul = 1;

  st->clear = get_integer_resource(st->dpy, "clear", "Integer");
  if (st->clear < 0) st->clear = 0;

  st->xor = get_boolean_resource(st->dpy, "xor", "Boolean");

  st->window_width = xgwa.width;
  st->window_height = xgwa.height;

  calc_logwidths(st);

  /* always draw xor on mono. */
  if (mono_p || st->xor) {
    XSetFunction(st->dpy, st->gc, GXxor);
  }

  st->munchers = (muncher **) calloc(st->simul, sizeof(muncher *));
  for (i = 0; i < st->simul; i++) {
    st->munchers[i] = make_muncher(st);
  }

  return st;
}

static unsigned long
mismunch_draw (Display *dpy, Window w, void *closure)
{
  struct state *st = (struct state *) closure;

  /* for (draw_i = 0; draw_i < simul; draw_i++)  */
  {
    munch(st, st->munchers[st->draw_i]);

    if (st->munchers[st->draw_i]->done) {
      st->draw_n++;

      free(st->munchers[st->draw_i]);
      st->munchers[st->draw_i] = make_muncher(st);
    }
  }

  st->draw_i++;
  if (st->draw_i >= st->simul) {
    int i = 0;
    st->draw_i = 0;
    if (st->restart || (st->clear && st->draw_n >= st->clear)) {
      for (i = 0; i < st->simul; i++) {
        free(st->munchers[i]);
        st->munchers[i] = make_muncher(st);
      }

      XClearWindow(st->dpy, w);
      st->draw_n = 0;
      st->restart = 0;
    }
  }

  return st->delay;
}


static void
mismunch_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  if (w != st->window_width ||
      h != st->window_height) {
    st->window_width = w;
    st->window_height = h;
    calc_logwidths(st);
    st->restart = 1;
    st->draw_i = 0;
  }
}

static Bool
mismunch_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
mismunch_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *mismunch_defaults [] = {
  ".background:       black",
  ".foreground:       white",
  "*delay:            2500",
  "*simul:            5",
  "*clear:            65",
  "*xor:              True",
  0
};

static XrmOptionDescRec mismunch_options [] = {
  { "-delay",         ".delay",       XrmoptionSepArg,  0 },
  { "-simul",         ".simul",       XrmoptionSepArg,  0 },
  { "-clear",         ".clear",       XrmoptionSepArg, "true" },
  { "-xor",           ".xor",         XrmoptionNoArg,  "true" },
  { "-no-xor",        ".xor",         XrmoptionNoArg,  "false" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Mismunch", mismunch)
