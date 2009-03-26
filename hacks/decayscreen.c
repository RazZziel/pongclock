/* xscreensaver, Copyright (c) 1992-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* decayscreen
 *
 * Based on slidescreen program from the xscreensaver application and the
 * decay program for Sun framebuffers.  This is the comment from the decay.c
 * file:

 * decay.c
 *   find the screen bitmap for the console and make it "decay" by
 *   randomly shifting random rectangles by one pixelwidth at a time.
 *
 *   by David Wald, 1988
 *        rewritten by Natuerlich!
 *   based on a similar "utility" on the Apollo ring at Yale.

 * X version by
 *
 *  Vivek Khera <khera@cs.duke.edu>
 *  5-AUG-1993
 *
 *  Hacked by jwz, 28-Nov-97 (sped up and added new motion directions)
 
 *  R. Schultz
 *  Added "melt" & "stretch" modes 28-Mar-1999
 *
 */

#include "screenhack.h"

struct state {
  Display *dpy;
  Window window;

  int sizex, sizey;
  int delay;
  GC gc;
  int mode;
  int iterations;

  int fuzz_toggle;
  const int *current_bias;

  async_load_state *img_loader;
};


#define SHUFFLE		0
#define UP		1
#define LEFT		2
#define RIGHT		3
#define DOWN		4
#define UPLEFT		5
#define DOWNLEFT	6
#define UPRIGHT		7
#define DOWNRIGHT	8
#define IN		9
#define OUT		10
#define MELT		11
#define STRETCH		12
#define FUZZ		13

static void *
decayscreen_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  long gcflags;
  unsigned long bg;
  char *s;

  st->dpy = dpy;
  st->window = window;

  s = get_string_resource(st->dpy, "mode", "Mode");
  if      (s && !strcmp(s, "shuffle")) st->mode = SHUFFLE;
  else if (s && !strcmp(s, "up")) st->mode = UP;
  else if (s && !strcmp(s, "left")) st->mode = LEFT;
  else if (s && !strcmp(s, "right")) st->mode = RIGHT;
  else if (s && !strcmp(s, "down")) st->mode = DOWN;
  else if (s && !strcmp(s, "upleft")) st->mode = UPLEFT;
  else if (s && !strcmp(s, "downleft")) st->mode = DOWNLEFT;
  else if (s && !strcmp(s, "upright")) st->mode = UPRIGHT;
  else if (s && !strcmp(s, "downright")) st->mode = DOWNRIGHT;
  else if (s && !strcmp(s, "in")) st->mode = IN;
  else if (s && !strcmp(s, "out")) st->mode = OUT;
  else if (s && !strcmp(s, "melt")) st->mode = MELT;
  else if (s && !strcmp(s, "stretch")) st->mode = STRETCH;
  else if (s && !strcmp(s, "fuzz")) st->mode = FUZZ;
  else {
    if (s && *s && !!strcmp(s, "random"))
      fprintf(stderr, "%s: unknown mode %s\n", progname, s);
    st->mode = random() % (FUZZ+1);
  }

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");

  if (st->delay < 0) st->delay = 0;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  bg = get_pixel_resource (st->dpy, xgwa.colormap, "background", "Background");
  gcv.foreground = bg;

  gcflags = GCForeground | GCFunction;
  if (use_subwindow_mode_p(xgwa.screen, st->window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  st->gc = XCreateGC (st->dpy, st->window, gcflags, &gcv);

  st->sizex = xgwa.width;
  st->sizey = xgwa.height;

  if (st->mode == MELT || st->mode == STRETCH)
    st->iterations = 1;    /* slow down for smoother melting */

  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->window, 0, 0);

  return st;
}


/*
 * perform one iteration of decay
 */
static unsigned long
decayscreen_draw (Display *dpy, Window window, void *closure)
{
    struct state *st = (struct state *) closure;
    int left, top, width, height, toleft, totop;

#define L 101
#define R 102
#define U 103
#define D 104
    static const int no_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, D,D,D,D };
    static const int up_bias[]        = { L,L,L,L, R,R,R,R, U,U,U,U, U,U,D,D };
    static const int down_bias[]      = { L,L,L,L, R,R,R,R, U,U,D,D, D,D,D,D };
    static const int left_bias[]      = { L,L,L,L, L,L,R,R, U,U,U,U, D,D,D,D };
    static const int right_bias[]     = { L,L,R,R, R,R,R,R, U,U,U,U, D,D,D,D };

    static const int upleft_bias[]    = { L,L,L,L, L,R,R,R, U,U,U,U, U,D,D,D };
    static const int downleft_bias[]  = { L,L,L,L, L,R,R,R, U,U,U,D, D,D,D,D };
    static const int upright_bias[]   = { L,L,L,R, R,R,R,R, U,U,U,U, U,D,D,D };
    static const int downright_bias[] = { L,L,L,R, R,R,R,R, U,U,U,D, D,D,D,D };

    if (st->img_loader)   /* still loading */
      {
        st->img_loader = load_image_async_simple (st->img_loader, 
                                                  0, 0, 0, 0, 0);
        if (! st->img_loader) {  /* just finished */
          if (st->mode == MELT || st->mode == STRETCH)
            /* make sure screen eventually turns background color */
            XDrawLine (st->dpy, st->window, st->gc, 0, 0, st->sizex, 0); 
        }
      return st->delay;
    }

    switch (st->mode) {
      case SHUFFLE:	st->current_bias = no_bias; break;
      case UP:		st->current_bias = up_bias; break;
      case LEFT:	st->current_bias = left_bias; break;
      case RIGHT:	st->current_bias = right_bias; break;
      case DOWN:	st->current_bias = down_bias; break;
      case UPLEFT:	st->current_bias = upleft_bias; break;
      case DOWNLEFT:	st->current_bias = downleft_bias; break;
      case UPRIGHT:	st->current_bias = upright_bias; break;
      case DOWNRIGHT:	st->current_bias = downright_bias; break;
      case IN:		st->current_bias = no_bias; break;
      case OUT:		st->current_bias = no_bias; break;
      case MELT:	st->current_bias = no_bias; break;
      case STRETCH:	st->current_bias = no_bias; break;
      case FUZZ:	st->current_bias = no_bias; break;
     default: abort();
    }

#define nrnd(x) ((x) ? random() % (x) : x)

    if (st->mode == MELT || st->mode == STRETCH) {
      left = nrnd(st->sizex/2);
      top = nrnd(st->sizey);
      width = nrnd( st->sizex/2 ) + st->sizex/2 - left;
      height = nrnd(st->sizey - top);
      toleft = left;
      totop = top+1;

    } else if (st->mode == FUZZ) {  /* By Vince Levey <vincel@vincel.org>;
                                   inspired by the "melt" mode of the
                                   "scrhack" IrisGL program by Paul Haeberli
                                   circa 1991. */
      left = nrnd(st->sizex - 1);
      top  = nrnd(st->sizey - 1);
      st->fuzz_toggle = !st->fuzz_toggle;
      if (st->fuzz_toggle)
        {
          totop = top;
          height = 1;
          toleft = nrnd(st->sizex - 1);
          if (toleft > left)
            {
              width = toleft-left;
              toleft = left;
              left++;
            }
          else
            {
              width = left-toleft;
              left = toleft;
              toleft++;
            }
        }
      else
        {
          toleft = left;
          width = 1;
          totop  = nrnd(st->sizey - 1);
          if (totop > top)
            {
              height = totop-top;
              totop = top;
              top++;
            }
          else
            {
              height = top-totop;
              top = totop;
              totop++;
            }
        }

    } else {

      left = nrnd(st->sizex - 1);
      top = nrnd(st->sizey);
      width = nrnd(st->sizex - left);
      height = nrnd(st->sizey - top);
      
      toleft = left;
      totop = top;
      if (st->mode == IN || st->mode == OUT) {
	int x = left+(width/2);
	int y = top+(height/2);
	int cx = st->sizex/2;
	int cy = st->sizey/2;
	if (st->mode == IN) {
	  if      (x > cx && y > cy)   st->current_bias = upleft_bias;
	  else if (x < cx && y > cy)   st->current_bias = upright_bias;
	  else if (x < cx && y < cy)   st->current_bias = downright_bias;
	  else /* (x > cx && y < cy)*/ st->current_bias = downleft_bias;
	} else {
	  if      (x > cx && y > cy)   st->current_bias = downright_bias;
	  else if (x < cx && y > cy)   st->current_bias = downleft_bias;
	  else if (x < cx && y < cy)   st->current_bias = upleft_bias;
	  else /* (x > cx && y < cy)*/ st->current_bias = upright_bias;
	}
      }
      
      switch (st->current_bias[random() % (sizeof(no_bias)/sizeof(*no_bias))]) {
      case L: toleft = left-1; break;
      case R: toleft = left+1; break;
      case U: totop = top-1; break;
      case D: totop = top+1; break;
      default: abort(); break;
      }
    }
    
    if (st->mode == STRETCH) {
      XCopyArea (st->dpy, st->window, st->window, st->gc, 0, st->sizey-top-2, st->sizex, top+1, 
		 0, st->sizey-top-1); 
    } else {
      XCopyArea (st->dpy, st->window, st->window, st->gc, left, top, width, height,
		 toleft, totop);
    }

#undef nrnd

    return st->delay;
}

static void
decayscreen_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->sizex = w;
  st->sizey = h;
}

static Bool
decayscreen_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
decayscreen_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}



static const char *decayscreen_defaults [] = {
  ".background:			Black",
  ".foreground:			Yellow",
  "*dontClearRoot:		True",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*mode:			random",
  0
};

static XrmOptionDescRec decayscreen_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("DecayScreen", decayscreen)
