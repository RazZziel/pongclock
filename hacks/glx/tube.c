/* tube, Copyright (c) 2001, 2003, 2007 Jamie Zawinski <jwz@jwz.org>
 * Utility functions to create tubes and cones in GL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#ifdef HAVE_COCOA
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

#include "tube.h"

static int
unit_tube (int faces, int smooth, int caps_p, int wire_p)
{
  int i;
  int polys = 0;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s2 = step/2;
  GLfloat th;
  GLfloat x, y, x0=0, y0=0;
  int z = 0;

  /* side walls
   */
  glFrontFace(GL_CCW);
  glBegin (wire_p ? GL_LINES : (smooth ? GL_QUAD_STRIP : GL_QUADS));

  th = 0;
  x = 1;
  y = 0;

  if (!smooth)
    {
      x0 = cos (s2);
      y0 = sin (s2);
    }

  if (smooth) faces++;

  for (i = 0; i < faces; i++)
    {
      if (smooth)
        glNormal3f(x, 0, y);
      else
        glNormal3f(x0, 0, y0);

      glVertex3f(x, 0, y);
      glVertex3f(x, 1, y);

      th += step;
      x  = cos (th);
      y  = sin (th);

      if (!smooth)
        {
          x0 = cos (th + s2);
          y0 = sin (th + s2);

          glVertex3f(x, 1, y);
          glVertex3f(x, 0, y);
        }
      polys++;
    }
  glEnd();

  /* End caps
   */
  if (caps_p)
    for (z = 0; z <= 1; z++)
      {
        glFrontFace(z == 0 ? GL_CCW : GL_CW);
        glNormal3f(0, (z == 0 ? -1 : 1), 0);
        glBegin(wire_p ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
        if (! wire_p) glVertex3f(0, z, 0);
        for (i = 0, th = 0; i <= faces; i++)
          {
            GLfloat x = cos (th);
            GLfloat y = sin (th);
            glVertex3f(x, z, y);
            th += step;
            polys++;
          }
        glEnd();
      }
  return polys;
}


static int
unit_cone (int faces, int smooth, int cap_p, int wire_p)
{
  int i;
  int polys = 0;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s2 = step/2;
  GLfloat th;
  GLfloat x, y, x0, y0;

  /* side walls
   */
  glFrontFace(GL_CW);
  glBegin(wire_p ? GL_LINES : GL_TRIANGLES);

  th = 0;
  x = 1;
  y = 0;
  x0 = cos (s2);
  y0 = sin (s2);

  for (i = 0; i < faces; i++)
    {
      glNormal3f(x0, 0, y0);
      glVertex3f(0,  1, 0);

      if (smooth) glNormal3f(x, 0, y);
      glVertex3f(x, 0, y);

      th += step;
      x0 = cos (th + s2);
      y0 = sin (th + s2);
      x  = cos (th);
      y  = sin (th);

      if (smooth) glNormal3f(x, 0, y);
      glVertex3f(x, 0, y);
      polys++;
    }
  glEnd();

  /* End cap
   */
  if (cap_p)
    {
      glFrontFace(GL_CCW);
      glNormal3f(0, -1, 0);
      glBegin(wire_p ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      if (! wire_p) glVertex3f(0, 0, 0);
      for (i = 0, th = 0; i <= faces; i++)
        {
          GLfloat x = cos (th);
          GLfloat y = sin (th);
          glVertex3f(x, 0, y);
          th += step;
          polys++;
        }
      glEnd();
    }
  return polys;
}


static int
tube_1 (GLfloat x1, GLfloat y1, GLfloat z1,
        GLfloat x2, GLfloat y2, GLfloat z2,
        GLfloat diameter, GLfloat cap_size,
        int faces, int smooth, int caps_p, int wire_p,
        int cone_p)
{
  GLfloat length, X, Y, Z;
  int polys = 0;

  if (diameter <= 0) abort();

  X = (x2 - x1);
  Y = (y2 - y1);
  Z = (z2 - z1);

  if (X == 0 && Y == 0 && Z == 0)
    return 0;

  length = sqrt (X*X + Y*Y + Z*Z);

  glPushMatrix();

  glTranslatef(x1, y1, z1);
  glRotatef (-atan2 (X, Y)               * (180 / M_PI), 0, 0, 1);
  glRotatef ( atan2 (Z, sqrt(X*X + Y*Y)) * (180 / M_PI), 1, 0, 0);
  glScalef (diameter, length, diameter);

  /* extend the endpoints of the tube by the cap size in both directions */
  if (cap_size != 0)
    {
      GLfloat c = cap_size/length;
      glTranslatef (0, -c, 0);
      glScalef (1, 1+c+c, 1);
    }

  if (cone_p)
    polys = unit_cone (faces, smooth, caps_p, wire_p);
  else
    polys = unit_tube (faces, smooth, caps_p, wire_p);

  glPopMatrix();
  return polys;
}


int
tube (GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat diameter, GLfloat cap_size,
      int faces, int smooth, int caps_p, int wire_p)
{
  return tube_1 (x1, y1, z1, x2, y2, z2, diameter, cap_size,
                 faces, smooth, caps_p, wire_p,
                 0);
}


int
cone (GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat diameter, GLfloat cap_size,
      int faces, int smooth, int cap_p, int wire_p)
{
  return tube_1 (x1, y1, z1, x2, y2, z2, diameter, cap_size,
                 faces, smooth, cap_p, wire_p,
                 1);
}
