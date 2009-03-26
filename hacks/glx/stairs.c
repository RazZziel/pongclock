/* -*- Mode: C; tab-width: 4 -*- */
/* stairs --- Infinite Stairs, and Escher-like scene. */

#if 0
static const char sccsid[] = "@(#)stairs.c	4.07 97/11/24 xlockmore";
#endif

#undef DEBUG_LISTS

/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * This mode shows some interesting scenes that are impossible OR very
 * weird to build in the real universe. Much of the scenes are inspirated
 * on Mauritz Cornelis stairs's works which derivated the mode's name.
 * M.C. Escher (1898-1972) was a dutch artist and many people prefer to
 * say he was a mathematician.
 *
 * Thanks goes to Brian Paul for making it possible and inexpensive to use 
 * OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistake.
 *
 * My e-mail address is
 * m-vianna@usa.net
 *
 * Marcelo F. Vianna (Jun-01-1997)
 *
 * Revision History:
 * 07-Jan-98: This would be a scene for the escher mode, but now escher mode
 *            was splitted in different modes for each scene. This is the
 *            initial release and is not working yet.
 *            Marcelo F. Vianna.
 *
 */

/*-
 * Texture mapping is only available on RGBA contexts, Mono and color index
 * visuals DO NOT support texture mapping in OpenGL.
 *
 * BUT Mesa do implements RGBA contexts in pseudo color visuals, so texture
 * mapping shuld work on PseudoColor, DirectColor, TrueColor using Mesa. Mono
 * is not officially supported for both OpenGL and Mesa, but seems to not crash
 * Mesa.
 *
 * In real OpenGL, PseudoColor DO NOT support texture map (as far as I know).
 */

#ifdef STANDALONE
# define DEFAULTS			"*delay:		20000   \n" \
							"*showFPS:      False   \n"
# define refresh_stairs 0
# define stairs_handle_event 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef USE_GL

#include "e_textures.h"

ENTRYPOINT ModeSpecOpt stairs_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   stairs_description =
{"stairs", "init_stairs", "draw_stairs", "release_stairs",
 "draw_stairs", "change_stairs", NULL, &stairs_opts,
 1000, 1, 1, 1, 4, 1.0, "",
 "Shows Infinite Stairs, an Escher-like scene", 0, NULL};

#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

/*************************************************************************/

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	Bool        direction;
	int         AreObjectsDefined[1];
	int         sphere_position;
	int         sphere_tick;
	GLXContext *glx_context;
    GLuint objects;
} stairsstruct;

static const float front_shininess[] = {60.0};
static const float front_specular[] = {0.7, 0.7, 0.7, 1.0};
static const float ambient[] = {0.0, 0.0, 0.0, 1.0};
static const float diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const float position0[] = {1.0, 1.0, 1.0, 0.0};
static const float position1[] = {-1.0, -1.0, 1.0, 0.0};
static const float lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const float lmodel_twoside[] = {GL_TRUE};

#if 0
static const float MaterialRed[] = {0.7, 0.0, 0.0, 1.0};
static const float MaterialGreen[] = {0.1, 0.5, 0.2, 1.0};
static const float MaterialBlue[] = {0.0, 0.0, 0.7, 1.0};
static const float MaterialCyan[] = {0.2, 0.5, 0.7, 1.0};
static const float MaterialMagenta[] = {0.6, 0.2, 0.5, 1.0};
static const float MaterialGray[] = {0.2, 0.2, 0.2, 1.0};
static const float MaterialGray5[] = {0.5, 0.5, 0.5, 1.0};
static const float MaterialGray6[] = {0.6, 0.6, 0.6, 1.0};
static const float MaterialGray8[] = {0.8, 0.8, 0.8, 1.0};

#endif
static const float MaterialYellow[] = {0.7, 0.7, 0.0, 1.0};
static const float MaterialWhite[] = {0.7, 0.7, 0.7, 1.0};

static const float positions[] =
{
	-2.5, 4.0, 0.0,		/* First one is FUDGED :) */
	-3.0, 3.25, 1.0,
	-3.0, 4.4, 1.5,
	-3.0, 3.05, 2.0,
	-3.0, 4.2, 2.5,

	-3.0, 2.85, 3.0,
	-2.5, 4.0, 3.0,
	-2.0, 2.75, 3.0,
	-1.5, 3.9, 3.0,
	-1.0, 2.65, 3.0,
	-0.5, 3.8, 3.0,
	0.0, 2.55, 3.0,
	0.5, 3.7, 3.0,
	1.0, 2.45, 3.0,
	1.5, 3.6, 3.0,
	2.0, 2.35, 3.0,

	2.0, 3.5, 2.5,
	2.0, 2.25, 2.0,
	2.0, 3.4, 1.5,
	2.0, 2.15, 1.0,
	2.0, 3.3, 0.5,
	2.0, 2.05, 0.0,
	2.0, 3.2, -0.5,
	2.0, 1.95, -1.0,
	2.0, 3.1, -1.5,
	2.0, 1.85, -2.0,

	1.5, 2.9, -2.0,
	1.0, 1.65, -2.0,
	0.5, 2.7, -2.0,
	0.0, 1.55, -2.0,
	-0.5, 2.5, -2.0,
	-1.0, 1.45, -2.0,
};

#define NPOSITIONS ((sizeof positions) / (sizeof positions[0]))

#define SPHERE_TICKS 8

static stairsstruct *stairs = NULL;

#define ObjSphere    0

#define PlankWidth      3.0
#define PlankHeight     0.35
#define PlankThickness  0.15

static void
mySphere(float radius)
{
	GLUquadricObj *quadObj;

	quadObj = gluNewQuadric();
	gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
	gluSphere(quadObj, radius, 16, 16);
	gluDeleteQuadric(quadObj);
}

static void
draw_block(GLfloat width, GLfloat height, GLfloat thickness)
{
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(0, 0);
	glVertex3f(-width, -height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, height, thickness);
	glNormal3f(0, 0, -1);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, -thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, -thickness);
	glNormal3f(0, 1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, height, thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, height, -thickness);
	glNormal3f(0, -1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, -height, -thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, thickness);
	glNormal3f(1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(width, -height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(width, -height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(width, height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(width, height, thickness);
	glNormal3f(-1, 0, 0);
	glTexCoord2f(0, 0);
	glVertex3f(-width, height, thickness);
	glTexCoord2f(1, 0);
	glVertex3f(-width, height, -thickness);
	glTexCoord2f(1, 1);
	glVertex3f(-width, -height, -thickness);
	glTexCoord2f(0, 1);
	glVertex3f(-width, -height, thickness);
	glEnd();
}

static void
draw_stairs_internal(ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];
	GLfloat     X;

	glPushMatrix();
	glPushMatrix();
	glTranslatef(-3.0, 0.1, 2.0);
	for (X = 0; X < 2; X++) {
		draw_block(0.5, 2.7 + 0.1 * X, 0.5);
		glTranslatef(0.0, 0.1, -1.0);
	}
	glPopMatrix();
	glTranslatef(-3.0, 0.0, 3.0);
	glPushMatrix();

	for (X = 0; X < 6; X++) {
		draw_block(0.5, 2.6 - 0.1 * X, 0.5);
		glTranslatef(1.0, -0.1, 0.0);
	}
	glTranslatef(-1.0, -0.9, -1.0);
	for (X = 0; X < 5; X++) {
		draw_block(0.5, 3.0 - 0.1 * X, 0.5);
		glTranslatef(0.0, 0.0, -1.0);
	}
	glTranslatef(-1.0, -1.1, 1.0);
	for (X = 0; X < 3; X++) {
		draw_block(0.5, 3.5 - 0.1 * X, 0.5);
		glTranslatef(-1.0, -0.1, 0.0);
	}
	glPopMatrix();
	glPopMatrix();

	glPushMatrix();
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);

    {
      int pos  = sp->sphere_position;
      int ppos = sp->sphere_position - 3;
      int npos = sp->sphere_position + 3;
      GLfloat spx, spy, spz;
      GLfloat dx, dy, dz;
      int div;

      if (ppos < 0) ppos += NPOSITIONS;
      if (npos >= NPOSITIONS) npos -= NPOSITIONS;

      if (sp->sphere_tick < 0)
        {
          dx = positions[ppos]   - positions[pos];
          dy = positions[ppos+1] - positions[pos+1];
          dz = positions[ppos+2] - positions[pos+2];
          div = SPHERE_TICKS + sp->sphere_tick;
        }
      else
        {
          dx = positions[npos]   - positions[pos];
          dy = positions[npos+1] - positions[pos+1];
          dz = positions[npos+2] - positions[ppos+2];
          div = SPHERE_TICKS - sp->sphere_tick;
        }
        
      spx = positions[pos];
      spy = positions[pos+1];
      spz = positions[pos+2];
      if (div != 0)
        {
          spx += dx / div;
          spy += dy / div;
          spz += dz / div;
        }


      spy -= 0.5;   /* move the bottom of the ball closer to the stairs */


#ifdef DEBUG
      fprintf(stderr, "%3d %3d   %2.2f %2.2f %2.2f  %2.2f %2.2f %2.2f\n",
              sp->sphere_position, sp->sphere_tick,
              dx, dy, dz,
              spx, spy, spz);

      glBegin(GL_LINE_LOOP);   /* path 1 */
      glVertex3f(positions[pos],  positions[pos+1],  positions[pos+2]);
      glVertex3f(positions[npos], positions[npos+1], positions[npos+2]);
      glEnd();

      glBegin(GL_LINE_LOOP);   /* path 2 */
      glVertex3f(positions[pos],  positions[pos+1],  positions[pos+2]);
      glVertex3f(positions[ppos], positions[ppos+1], positions[ppos+2]);
      glEnd();

      glBegin(GL_LINE_LOOP);  /* base origin */
      glVertex3f(positions[pos], positions[pos+1]-10, positions[pos+2]);
      glVertex3f(positions[pos], positions[pos+1]+10, positions[pos+2]);
      glEnd();

      glBegin(GL_LINE_LOOP);  /* base origin */
      glVertex3f(positions[pos]-10, positions[pos+1], positions[pos+2]);
      glVertex3f(positions[pos]+10, positions[pos+1], positions[pos+2]);
      glEnd();

      glBegin(GL_LINE_LOOP);  /* base origin */
      glVertex3f(positions[pos], positions[pos+1], positions[pos+2]-10);
      glVertex3f(positions[pos], positions[pos+1], positions[pos+2]+10);
      glEnd();
#endif /* DEBUG */

      glTranslatef(spx, spy, spz);

#ifdef DEBUG  /* ball origin */
      glBegin(GL_LINE_LOOP); glVertex3f(0,-2,0); glVertex3f(0,2,0); glEnd();
      glBegin(GL_LINE_LOOP); glVertex3f(-2,0,0); glVertex3f(2,0,0); glEnd();
      glBegin(GL_LINE_LOOP); glVertex3f(0,0,-2); glVertex3f(0,0,2); glEnd();
#endif /* DEBUG */
    }

	if (sp->sphere_position == 0)	/* FUDGE soo its not so obvious */
		mySphere(0.48);
	else
		mySphere(0.5);
	glPopMatrix();

    if (++sp->sphere_tick >= SPHERE_TICKS-1)
      {
        sp->sphere_tick = -(SPHERE_TICKS-2);
        sp->sphere_position += 3;
        sp->sphere_position += 3;
      }

	if (sp->sphere_position >= NPOSITIONS)
		sp->sphere_position = 0;
}

ENTRYPOINT void
reshape_stairs (ModeInfo * mi, int width, int height)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];

	glViewport(0, 0, sp->WindW = (GLint) width, sp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);
	if (width >= 1024) {
		glLineWidth(3);
		glPointSize(3);
	} else if (width >= 512) {
		glLineWidth(2);
		glPointSize(2);
	} else {
		glLineWidth(1);
		glPointSize(1);
	}
}

static void
pinit(void)
{
    int status;
	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, position1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_NORMALIZE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
	glShadeModel(GL_FLAT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    clear_gl_error();
    status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
                               WoodTextureWidth, WoodTextureHeight,
                               GL_RGB, GL_UNSIGNED_BYTE, WoodTextureData);
    if (status)
      {
        const char *s = (char *) gluErrorString (status);
        fprintf (stderr, "%s: error mipmapping %dx%d texture: %s\n",
                 progname, WoodTextureWidth, WoodTextureHeight,
                 (s ? s : "(unknown)"));
        exit (1);
      }
    check_gl_error("mipmapping");

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);
}

ENTRYPOINT void
init_stairs (ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	stairsstruct *sp;

	if (stairs == NULL) {
		if ((stairs = (stairsstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (stairsstruct))) == NULL)
			return;
	}
	sp = &stairs[screen];
	sp->step = 0.0;

    /* make multiple screens rotate at slightly different rates. */
    sp->step -= frand(5.0);

	sp->direction = LRAND() & 1;
	sp->sphere_position = NRAND(NPOSITIONS / 3) * 3;
	sp->sphere_tick = 0;

	if ((sp->glx_context = init_GL(mi)) != NULL) {

		reshape_stairs(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		if (!glIsList(sp->objects))
			sp->objects = glGenLists(1);
		pinit();
	} else {
		MI_CLEARWINDOW(mi);
	}
}

ENTRYPOINT void
draw_stairs (ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	if (!sp->glx_context)
		return;

	glXMakeCurrent(display, window, *(sp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_IS_ICONIC(mi)) {
		glScalef(Scale4Window * sp->WindH / sp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * sp->WindH / sp->WindW, Scale4Iconic, Scale4Iconic);
	}

	glRotatef(44.5, 1, 0, 0);
	glRotatef(50 + ((sp->direction) ? 1 : -1) *
	       ((sp->step * 100 > 120) ? sp->step * 100 - 120 : 0), 0, 1, 0);
	if (sp->step * 100 >= 360 + 120) {	/* stop showing secrets */
		sp->step = 0;
		sp->direction = LRAND() & 1;
	}
	draw_stairs_internal(mi);

	glPopMatrix();

    if (mi->fps_p) do_fps (mi);
	glFlush();

	glXSwapBuffers(display, window);

	sp->step += 0.025;
}

#ifndef STANDALONE
ENTRYPOINT void
change_stairs (ModeInfo * mi)
{
	stairsstruct *sp = &stairs[MI_SCREEN(mi)];

	if (!sp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sp->glx_context));
	pinit();
}
#endif /* !STANDALONE */

ENTRYPOINT void
release_stairs (ModeInfo * mi)
{
	if (stairs != NULL) {
      int i;
      for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
        stairsstruct *sp = &stairs[i];
        if (glIsList(sp->objects)) {
          glDeleteLists(sp->objects, 1);
        }
      }
      free(stairs);
      stairs = NULL;
	}
	FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("Stairs", stairs)

#endif
