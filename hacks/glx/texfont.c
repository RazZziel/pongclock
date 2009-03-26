/* texfonts, Copyright (c) 2005-2007 Jamie Zawinski <jwz@jwz.org>
 * Loads X11 fonts into textures for use with OpenGL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_COCOA
# include <OpenGL/glu.h>
#else
# include <GL/glx.h>
# include <GL/glu.h>
#endif

#include "resources.h"
#include "texfont.h"

/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

/* screenhack.h */
extern char *progname;

struct texture_font_data {
  Display *dpy;
  XFontStruct *font;
  int cell_width, cell_height;  /* maximal charcell */
  int tex_width, tex_height;    /* size of each texture */

  int grid_mag;			/* 1,  2,  4, or 8 */
  int ntextures;		/* 1,  4, 16, or 64 (grid_mag ^ 2) */

  GLuint texid[64];		/* must hold ntextures */
};


/* return the next larger power of 2. */
static int
to_pow2 (int i)
{
  static const unsigned int pow2[] = { 
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 
    2048, 4096, 8192, 16384, 32768, 65536 };
  int j;
  for (j = 0; j < sizeof(pow2)/sizeof(*pow2); j++)
    if (pow2[j] >= i) return pow2[j];
  abort();  /* too big! */
}


/* Given a Pixmap (of screen depth), converts it to an OpenGL luminance mipmap.
   RGB are averaged to grayscale, and the resulting value is treated as alpha.
   Pass in the size of the pixmap; the size of the texture is returned
   (it may be larger, since GL like powers of 2.)

   We use a screen-depth pixmap instead of a 1bpp bitmap so that if the fonts
   were drawn with antialiasing, that is preserved.
 */
static void
bitmap_to_texture (Display *dpy, Pixmap p, Visual *visual, int *wP, int *hP)
{
  Bool mipmap_p = True;
  int ow = *wP;
  int oh = *hP;
  int w2 = to_pow2 (ow);
  int h2 = to_pow2 (oh);
  int x, y;
  XImage *image = XGetImage (dpy, p, 0, 0, ow, oh, ~0L, ZPixmap);
  unsigned char *data = (unsigned char *) calloc (w2, (h2 + 1));
  unsigned char *out = data;
  GLuint iformat = GL_INTENSITY;
  GLuint format = GL_LUMINANCE;
  GLuint type = GL_UNSIGNED_BYTE;

  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++) {
      unsigned long pixel = (x >= ow || y >= oh ? 0 : XGetPixel (image, x, y));
      /* instead of averaging all three channels, let's just use red,
         and assume it was already grayscale. */
      unsigned long r = pixel & visual->red_mask;
      pixel = ((r >> 24) | (r >> 16) | (r >> 8) | r) & 0xFF;
      *out++ = pixel;
    }
  XDestroyImage (image);
  image = 0;

  if (mipmap_p)
    gluBuild2DMipmaps (GL_TEXTURE_2D, iformat, w2, h2, format, type, data);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, iformat, w2, h2, 0, format, type, data);

  {
    char msg[100];
    sprintf (msg, "texture font %s (%d x %d)",
             mipmap_p ? "gluBuild2DMipmaps" : "glTexImage2D",
             w2, h2);
    check_gl_error (msg);
  }


  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  free (data);

  *wP = w2;
  *hP = h2;
}


/* Loads the font named by the X resource "res" and returns
   a texture-font object.
*/
texture_font_data *
load_texture_font (Display *dpy, char *res)
{
  Screen *screen = DefaultScreenOfDisplay (dpy);
  Window root = RootWindowOfScreen (screen);
  XWindowAttributes xgwa;

  texture_font_data *data = 0;
  char *font = get_string_resource (dpy, res, "Font");
  const char *def1 = "-*-times-bold-r-normal-*-240-*";
  const char *def2 = "-*-times-bold-r-normal-*-180-*";
  const char *def3 = "fixed";
  XFontStruct *f;
  int which;

  check_gl_error ("stale texture font");

  XGetWindowAttributes (dpy, root, &xgwa);

  if (!res || !*res) abort();
  if (!font) font = strdup(def1);

  f = XLoadQueryFont(dpy, font);
  if (!f && !!strcmp (font, def1))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def1);
      free (font);
      font = strdup (def1);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f && !!strcmp (font, def2))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def2);
      free (font);
      font = strdup (def2);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f && !!strcmp (font, def3))
    {
      fprintf (stderr, "%s: unable to load font \"%s\", using \"%s\"\n",
               progname, font, def3);
      free (font);
      font = strdup (def3);
      f = XLoadQueryFont(dpy, font);
    }

  if (!f)
    {
      fprintf (stderr, "%s: unable to load fallback font \"%s\" either!\n",
               progname, font);
      exit (1);
    }

  free (font);
  font = 0;

  data = (texture_font_data *) calloc (1, sizeof(*data));
  data->dpy = dpy;
  data->font = f;

  /* Figure out how many textures to use.
     E.g., if we need 1024x1024 bits, use four 512x512 textures,
     to be gentle to machines with low texture size limits.
   */
  {
    int w = to_pow2 (16 * (f->max_bounds.rbearing - f->min_bounds.lbearing));
    int h = to_pow2 (16 * (f->max_bounds.ascent   + f->max_bounds.descent));
    int i = (w > h ? w : h);

    if      (i <= 512)  data->grid_mag = 1;  /*  1 tex of 16x16 chars */
    else if (i <= 1024) data->grid_mag = 2;  /*  4 tex of 8x8 chars */
    else if (i <= 2048) data->grid_mag = 4;  /* 16 tex of 4x4 chars */
    else                data->grid_mag = 8;  /* 32 tex of 2x2 chars */

    data->ntextures = data->grid_mag * data->grid_mag;

# if 0
    fprintf (stderr,
             "%s: %dx%d grid of %d textures of %dx%d chars (%dx%d bits)\n",
             progname,
             data->grid_mag, data->grid_mag,
             data->ntextures,
             16 / data->grid_mag, 16 / data->grid_mag,
             i, i);
# endif
  }

  for (which = 0; which < data->ntextures; which++)
    {
      /* Create a pixmap big enough to fit every character in the font.
         (modulo the "ntextures" scaling.)
         Make it square-ish, since GL likes dimensions to be powers of 2.
       */
      XGCValues gcv;
      GC gc;
      Pixmap p;
      int cw = f->max_bounds.rbearing - f->min_bounds.lbearing;
      int ch = f->max_bounds.ascent   + f->max_bounds.descent;
      int grid_size = (16 / data->grid_mag);
      int w = cw * grid_size;
      int h = ch * grid_size;
      int i;

      data->cell_width  = cw;
      data->cell_height = ch;

      p = XCreatePixmap (dpy, root, w, h, xgwa.depth);
      gcv.font = f->fid;
      gcv.foreground = BlackPixelOfScreen (xgwa.screen);
      gcv.background = BlackPixelOfScreen (xgwa.screen);
      gc = XCreateGC (dpy, p, (GCFont|GCForeground|GCBackground), &gcv);
      XFillRectangle (dpy, p, gc, 0, 0, w, h);
      XSetForeground (dpy, gc, WhitePixelOfScreen (xgwa.screen));
      for (i = 0; i < 256 / data->ntextures; i++)
        {
          int ii = (i + (which * 256 / data->ntextures));
          char c = (char) ii;
          int x = (i % grid_size) * cw;
          int y = (i / grid_size) * ch;

          /* See comment in print_texture_string for bit layout explanation.
           */
          int lbearing = (f->per_char
                          ? f->per_char[ii - f->min_char_or_byte2].lbearing
                          : f->min_bounds.lbearing);
          int ascent   = (f->per_char
                          ? f->per_char[ii - f->min_char_or_byte2].ascent
                          : f->max_bounds.ascent);
          int width    = (f->per_char
                          ? f->per_char[ii - f->min_char_or_byte2].width
                          : f->max_bounds.width);

          if (width == 0) continue;
          XDrawString (dpy, p, gc, x - lbearing, y + ascent, &c, 1);
        }
      XFreeGC (dpy, gc);

      glGenTextures (1, &data->texid[which]);
      glBindTexture (GL_TEXTURE_2D, data->texid[which]);
      check_gl_error ("texture font load");
      data->tex_width  = w;
      data->tex_height = h;

#if 0  /* debugging: splat the bitmap onto the desktop root window */
      {
        Window win = RootWindow (dpy, 0);
        GC gc2 = XCreateGC (dpy, win, 0, &gcv);
        XSetForeground (dpy, gc2, BlackPixel (dpy, 0));
        XSetBackground (dpy, gc2, WhitePixel (dpy, 0));
        XCopyArea (dpy, p, win, gc2, 0, 0, w, h, 0, 0);
        XFreeGC (dpy, gc2);
        XSync(dpy, False);
        usleep (100000);
      }
#endif

#if 0  /* debugging: write the bitmap to a pgm file */
      {
        char file[255];
        XImage *image;
        int x, y;
        FILE *f;
        sprintf (file, "/tmp/%02d.pgm", which);
        image = XGetImage (dpy, p, 0, 0, w, h, ~0L, ZPixmap);
        f = fopen (file, "w");
        fprintf (f, "P5\n%d %d\n255\n", w, h);
        for (y = 0; y < h; y++)
          for (x = 0; x < w; x++) {
            unsigned long pix = XGetPixel (image, x, y);
            unsigned long r = (pix & xgwa.visual->red_mask);
            r = ((r >> 24) | (r >> 16) | (r >> 8) | r);
            fprintf (f, "%c", (char) r);
          }
        fclose (f);
        XDestroyImage (image);
        fprintf (stderr, "%s: wrote %s\n", progname, file);
      }
#endif

      bitmap_to_texture (dpy, p, xgwa.visual, 
                         &data->tex_width, &data->tex_height);
      XFreePixmap (dpy, p);
    }

  return data;
}


/* Width of the string in pixels.
 */
int
texture_string_width (texture_font_data *data, const char *c,
                      int *line_height_ret)
{
  int w = 0;
  XFontStruct *f = data->font;
  while (*c)
    {
      int cc = *((unsigned char *) c);
      w += (f->per_char
            ? f->per_char[cc-f->min_char_or_byte2].width
            : f->max_bounds.width);
      c++;
    }
  if (line_height_ret)
    *line_height_ret = f->ascent + f->descent;
  return w;
}


/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
 */
void
print_texture_string (texture_font_data *data, const char *string)
{
  XFontStruct *f = data->font;
  GLfloat line_height = f->ascent + f->descent;
# ifdef DO_SUBSCRIPTS
  GLfloat sub_shift = (line_height * 0.3);
  Bool sub_p = False;
# endif /* DO_SUBSCRIPTS */
  int cw = texture_string_width (data, "m", 0);
  int tabs = cw * 7;
  int x, y;
  unsigned int i;

  clear_gl_error ();

  glPushMatrix();

  glNormal3f (0, 0, 1);

  x = 0;
  y = 0;
  for (i = 0; i < strlen(string); i++)
    {
      unsigned char c = string[i];
      if (c == '\n')
        {
          y -= line_height;
          x = 0;
        }
      else if (c == '\t')
        {
          x = ((x + tabs) / tabs) * tabs;  /* tab to tab stop */
        }
# ifdef DO_SUBSCRIPTS
      else if (c == '[' && (isdigit (string[i+1])))
        {
          sub_p = True;
          y -= sub_shift;
        }
      else if (c == ']' && sub_p)
        {
          sub_p = False;
          y += sub_shift;
        }
# endif /* DO_SUBSCRIPTS */
      else
        {
          /* For a small font, the texture is divided into 16x16 rectangles
             whose size are the max_bounds charcell of the font.  Within each
             rectangle, the individual characters' charcells sit in the upper
             left.

             For a larger font, the texture will itself be subdivided, to
             keep the texture sizes small (in that case we deal with, e.g.,
             4 grids of 8x8 characters instead of 1 grid of 16x16.)

             Within each texture:

               [A]----------------------------
                |     |           |   |      |
                |   l |         w |   | r    |
                |   b |         i |   | b    |
                |   e |         d |   | e    |
                |   a |         t |   | a    |
                |   r |         h |   | r    |
                |   i |           |   | i    |
                |   n |           |   | n    |
                |   g |           |   | g    |
                |     |           |   |      |
                |----[B]----------|---|      |
                |     |   ascent  |   |      |
                |     |           |   |      |
                |     |           |   |      |
                |--------------------[C]     |
                |         descent            |
                |                            | cell_width,
                ------------------------------ cell_height

             We want to make a quad from point A to point C.
             We want to position that quad so that point B lies at x,y.
           */
          int lbearing = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].lbearing
                          : f->min_bounds.lbearing);
          int rbearing = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].rbearing
                          : f->max_bounds.rbearing);
          int ascent   = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].ascent
                          : f->max_bounds.ascent);
          int descent  = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].descent
                          : f->max_bounds.descent);
          int cwidth   = (f->per_char
                          ? f->per_char[c - f->min_char_or_byte2].width
                          : f->max_bounds.width);

          char cc = c % (256 / data->ntextures);

          int gs = (16 / data->grid_mag);		  /* grid size */

          int ax = ((int) cc % gs) * data->cell_width;    /* point A */
          int ay = ((int) cc / gs) * data->cell_height;

          int bx = ax - lbearing;                         /* point B */
          int by = ay + ascent;

          int cx = bx + rbearing;                         /* point C */
          int cy = by + descent;

          GLfloat tax = (GLfloat) ax / data->tex_width;  /* tex coords of A */
          GLfloat tay = (GLfloat) ay / data->tex_height;

          GLfloat tcx = (GLfloat) cx / data->tex_width;  /* tex coords of C */
          GLfloat tcy = (GLfloat) cy / data->tex_height;

          GLfloat qx0 = x + lbearing;			 /* quad top left */
          GLfloat qy0 = y + ascent;
          GLfloat qx1 = qx0 + rbearing - lbearing;       /* quad bot right */
          GLfloat qy1 = qy0 - (ascent + descent);

          if (cwidth > 0 && c != ' ')
            {
              int which = c / (256 / data->ntextures);
              if (which >= data->ntextures) abort();
              glBindTexture (GL_TEXTURE_2D, data->texid[which]);

              glBegin (GL_QUADS);
              glTexCoord2f (tax, tay); glVertex3f (qx0, qy0, 0);
              glTexCoord2f (tcx, tay); glVertex3f (qx1, qy0, 0);
              glTexCoord2f (tcx, tcy); glVertex3f (qx1, qy1, 0);
              glTexCoord2f (tax, tcy); glVertex3f (qx0, qy1, 0);
              glEnd();
#if 0
              glDisable(GL_TEXTURE_2D);
              glBegin (GL_LINE_LOOP);
              glTexCoord2f (tax, tay); glVertex3f (qx0, qy0, 0);
              glTexCoord2f (tcx, tay); glVertex3f (qx1, qy0, 0);
              glTexCoord2f (tcx, tcy); glVertex3f (qx1, qy1, 0);
              glTexCoord2f (tax, tcy); glVertex3f (qx0, qy1, 0);
              glEnd();
              glEnable(GL_TEXTURE_2D);
#endif
            }

          x += cwidth;
        }
      }
  glPopMatrix();

  check_gl_error ("texture font print");
}

/* Releases the font and texture.
 */
void
free_texture_font (texture_font_data *data)
{
  int i;
  if (data->font)
    XFreeFont (data->dpy, data->font);
  for (i = 0; i < data->ntextures; i++)
    if (data->texid[i])
      glDeleteTextures (1, &data->texid[i]);
  free (data);
}
