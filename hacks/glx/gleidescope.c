/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
/*static const char sccsid[] = "@(#)gleidescope.c	1.0 03/06/27 xlockmore";*/
#endif

/* enable -grab switch */
/*#define	GRAB*/

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
 *	Revision History:
 *
 *	20030627	1.0		acd		First Release.
 *								Texture loading code from 'glplanet'
 *									by Jamie Zawinski <jwz@jwz.org>
 *	20030810	1.1		acd		Added size flag.
 *								Now grabs screen / video / picture
 *									(uses code from 'glslideshow' by
 *									Mike Oliphant, Ben Buxton, Jamie Zawinski).
 *								Added -duration.
 *								Added mouse code.
 *								Added fade code (also from glslideshow).
 *	20031013	1.2		acd		Migrated to compile without warnings under
 *									xscreensaver 4.13.
 *	20031023	1.3		acd		Better code to limit twisting speeds.
 *								Tweaked initial rotation values.
 *								Move, Rotate, Zoom now chosen at random if
 *									no preference is given.
 *								Made grid slightly bigger so you can't see
 *									the edge when zooming and moving.
 */

/*
**----------------------------------------------------------------------------
** Defines
**----------------------------------------------------------------------------
*/

#ifdef STANDALONE
# define DEFAULTS \
		"*delay:		20000		\n"	\
		"*showFPS:		False		\n"	\
		"*size:			-1			\n"	\
		"*useSHM:		True		\n"

# define refresh_gleidescope 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "colors.h"
#include "xpm-ximage.h"
#include "grab-ximage.h"

/* acd TODO should all these be in gleidestruct? */
/* they can't be, because of the idiotic way the xlockmore "argtype vars"
   interface works. -jwz */
#ifdef GRAB
static Bool		grab;			/* grab images */
#endif
static Bool		move;			/* moving camera */
static Bool		nomove;			/* no moving camera */
static Bool		rotate;			/* rotate in place */
static Bool		norotate;		/* no rotate in place */
static Bool		zoom;			/* zooming camera */
static Bool		nozoom;			/* no zooming camera */
static char		*image;			/* name of texture to load */
static int		duration;		/* length of time to display grabbed image */

#define	MAX_TANGLE_VEL	2.0
#define	MAX_RANGLE_VEL	1.5

static XrmOptionDescRec opts[] =
{
#ifdef GRAB
	{"-grab",		".gleidescope.grab",		XrmoptionNoArg,		"true"},
#endif
	{"-move",		".gleidescope.move",		XrmoptionNoArg,		"true"},
	{"-no-move",	".gleidescope.nomove",		XrmoptionNoArg,		"true"},
	{"-rotate",		".gleidescope.rotate",		XrmoptionNoArg,		"true"},
	{"-no-rotate",	".gleidescope.norotate",	XrmoptionNoArg,		"true"},
	/*{"-size",		".gleidescope.size",		XrmoptionNoArg,		"-1"},*/
	{"-zoom",		".gleidescope.zoom",		XrmoptionNoArg,		"true"},
	{"-no-zoom",	".gleidescope.nozoom",		XrmoptionNoArg,		"true"},
	{"-image",		".gleidescope.image",		XrmoptionSepArg,	"DEFAULT"},
	{"-duration",	".gleidescope.duration",	XrmoptionSepArg,	"30"},
};


static argtype vars[] = {
#ifdef GRAB
	{&grab,			"grab",		"Grab",		"False",	t_Bool},
#endif
	{&move,			"move",		"Move",		"False",	t_Bool},
	{&nomove,		"nomove",	"noMove",	"False",	t_Bool},
	{&rotate,		"rotate",	"Rotate",	"False",	t_Bool},
	{&norotate,		"norotate",	"noRotate",	"False",	t_Bool},
	/*{&size,		"size",		"Size",		"-1",		t_Int},*/
	{&zoom,			"zoom",		"Zoom",		"False",	t_Bool},
	{&nozoom,		"nozoom",	"noZoom",	"False",	t_Bool},
	{&image,		"image",	"Image",	"DEFAULT",	t_String},
	{&duration,		"duration",	"Duration",	"30",		t_Int},
};

static OptionStruct desc[] = {
#ifdef GRAB
	{"-grab",		"grab images to create animation"},
#endif
	{"-move",		"camera will move"},
	{"-no-move",	"camera won't move"},
	{"-rotate",		"camera will rotate"},
	{"-no-rotate",	"camera won't rotate"},
	/*{"-size",		"size of the hexagons (1-10)"},*/
	{"-zoom",		"camera will zoom"},
	{"-no-zoom",	"camera won't zoom"},
	{"-image",		"xpm / xbm image file to use for texture"},
	{"-duration",	"length of time texture will be used"},
};

ENTRYPOINT ModeSpecOpt gleidescope_opts = {
	sizeof opts / sizeof opts[0], opts,
	sizeof vars / sizeof vars[0], vars,
	desc
};

#ifdef USE_MODULES
ModStruct   gleidescope_description = { 
     "gleidescope", "init_gleidescope", "draw_gleidescope", "release_gleidescope",
     "draw_gleidescope", "init_gleidescope", NULL, &gleidescope_opts,
     1000, 1, 2, 1, 4, 1.0, "",
     "GL Kaleidescope", 0, NULL};
#endif

/*
**-----------------------------------------------------------------------------
**	Typedefs
**-----------------------------------------------------------------------------
*/

typedef struct hex_s {
	GLfloat	x, y, z;		/* position */
} hex_t;

typedef struct {
	GLfloat x;
	GLfloat y;
	GLfloat z;
} vectorf;

#define	MAX_FADE	500	/* number of fade cycles */

typedef struct {
	float			cam_x_speed, cam_z_speed, cam_y_speed;
	int				cam_x_phase, cam_z_phase, cam_y_phase;
	float			tic;
	GLXContext		*glx_context;
	Window			window;
	GLfloat			max_tx, max_ty;	/* maximum texture sizes */
	GLuint			textures[2];	/* texture handles */
	GLuint			visible;		/* texture handle for new texture */
	GLint			fade;
	time_t			start_time;
	Bool			button_down_p;

    int		size;

    float	tangle;		/* texture angle */
    float	tangle_vel;	/* texture velocity */
    float	tangle_acc;	/* texture acceleration */

    float	rangle;		/* rotate angle */
    float	rangle_vel;	/* rotate velocity */
    float	rangle_acc;	/* rotate acceleration */

    /* mouse */
    int xstart;
    int ystart;
    double xmouse;
    double ymouse;

    Bool mipmap_p;
    Bool waiting_for_image_p;

} gleidestruct;

#define	XOFFSET	(0.8660254f)	/* sin 60' */
#define	YOFFSET	(1.5000000f)	/* cos 60' + 1 */

#if 0

#define	SIZE	3

/* generates a grid with edges of given size */
/* acd TODO - replace hex[] with this and allow size and distance as parameters */

int
generate_grid(int size)

	int	i, x, y;

	gp->size--;

	i = gp->size;
	for (y = -size ; y <= size ; y++) {
		for (x = -i ; x <= i ; x += 2) {
			printf("{XOFFSET * %d, YOFFSET * %d, 0},\n", x, y);
		}
		printf("\n");
		if (y < 0) {
			i++;
		} else {
			i--;
		}
	}
	return 0;
}
#endif

static const hex_t hex[] = {
	/* edges of size 7 */
	/* number of hexagons required to cover screen depends on camera distance */
	/* at a distance of 10 this is just about enough. */
	{XOFFSET * -6, YOFFSET * -6, 0},
	{XOFFSET * -4, YOFFSET * -6, 0},
	{XOFFSET * -2, YOFFSET * -6, 0},
	{XOFFSET * 0, YOFFSET * -6, 0},
	{XOFFSET * 2, YOFFSET * -6, 0},
	{XOFFSET * 4, YOFFSET * -6, 0},
	{XOFFSET * 6, YOFFSET * -6, 0},

	{XOFFSET * -7, YOFFSET * -5, 0},
	{XOFFSET * -5, YOFFSET * -5, 0},
	{XOFFSET * -3, YOFFSET * -5, 0},
	{XOFFSET * -1, YOFFSET * -5, 0},
	{XOFFSET * 1, YOFFSET * -5, 0},
	{XOFFSET * 3, YOFFSET * -5, 0},
	{XOFFSET * 5, YOFFSET * -5, 0},
	{XOFFSET * 7, YOFFSET * -5, 0},

	{XOFFSET * -8, YOFFSET * -4, 0},
	{XOFFSET * -6, YOFFSET * -4, 0},
	{XOFFSET * -4, YOFFSET * -4, 0},
	{XOFFSET * -2, YOFFSET * -4, 0},
	{XOFFSET * 0, YOFFSET * -4, 0},
	{XOFFSET * 2, YOFFSET * -4, 0},
	{XOFFSET * 4, YOFFSET * -4, 0},
	{XOFFSET * 6, YOFFSET * -4, 0},
	{XOFFSET * 8, YOFFSET * -4, 0},

	{XOFFSET * -9, YOFFSET * -3, 0},
	{XOFFSET * -7, YOFFSET * -3, 0},
	{XOFFSET * -5, YOFFSET * -3, 0},
	{XOFFSET * -3, YOFFSET * -3, 0},
	{XOFFSET * -1, YOFFSET * -3, 0},
	{XOFFSET * 1, YOFFSET * -3, 0},
	{XOFFSET * 3, YOFFSET * -3, 0},
	{XOFFSET * 5, YOFFSET * -3, 0},
	{XOFFSET * 7, YOFFSET * -3, 0},
	{XOFFSET * 9, YOFFSET * -3, 0},

	{XOFFSET * -10, YOFFSET * -2, 0},
	{XOFFSET * -8, YOFFSET * -2, 0},
	{XOFFSET * -6, YOFFSET * -2, 0},
	{XOFFSET * -4, YOFFSET * -2, 0},
	{XOFFSET * -2, YOFFSET * -2, 0},
	{XOFFSET * 0, YOFFSET * -2, 0},
	{XOFFSET * 2, YOFFSET * -2, 0},
	{XOFFSET * 4, YOFFSET * -2, 0},
	{XOFFSET * 6, YOFFSET * -2, 0},
	{XOFFSET * 8, YOFFSET * -2, 0},
	{XOFFSET * 10, YOFFSET * -2, 0},

	{XOFFSET * -11, YOFFSET * -1, 0},
	{XOFFSET * -9, YOFFSET * -1, 0},
	{XOFFSET * -7, YOFFSET * -1, 0},
	{XOFFSET * -5, YOFFSET * -1, 0},
	{XOFFSET * -3, YOFFSET * -1, 0},
	{XOFFSET * -1, YOFFSET * -1, 0},
	{XOFFSET * 1, YOFFSET * -1, 0},
	{XOFFSET * 3, YOFFSET * -1, 0},
	{XOFFSET * 5, YOFFSET * -1, 0},
	{XOFFSET * 7, YOFFSET * -1, 0},
	{XOFFSET * 9, YOFFSET * -1, 0},
	{XOFFSET * 11, YOFFSET * -1, 0},

	{XOFFSET * -12, YOFFSET * 0, 0},
	{XOFFSET * -10, YOFFSET * 0, 0},
	{XOFFSET * -8, YOFFSET * 0, 0},
	{XOFFSET * -6, YOFFSET * 0, 0},
	{XOFFSET * -4, YOFFSET * 0, 0},
	{XOFFSET * -2, YOFFSET * 0, 0},
	{XOFFSET * 0, YOFFSET * 0, 0},
	{XOFFSET * 2, YOFFSET * 0, 0},
	{XOFFSET * 4, YOFFSET * 0, 0},
	{XOFFSET * 6, YOFFSET * 0, 0},
	{XOFFSET * 8, YOFFSET * 0, 0},
	{XOFFSET * 10, YOFFSET * 0, 0},
	{XOFFSET * 12, YOFFSET * 0, 0},

	{XOFFSET * -11, YOFFSET * 1, 0},
	{XOFFSET * -9, YOFFSET * 1, 0},
	{XOFFSET * -7, YOFFSET * 1, 0},
	{XOFFSET * -5, YOFFSET * 1, 0},
	{XOFFSET * -3, YOFFSET * 1, 0},
	{XOFFSET * -1, YOFFSET * 1, 0},
	{XOFFSET * 1, YOFFSET * 1, 0},
	{XOFFSET * 3, YOFFSET * 1, 0},
	{XOFFSET * 5, YOFFSET * 1, 0},
	{XOFFSET * 7, YOFFSET * 1, 0},
	{XOFFSET * 9, YOFFSET * 1, 0},
	{XOFFSET * 11, YOFFSET * 1, 0},

	{XOFFSET * -10, YOFFSET * 2, 0},
	{XOFFSET * -8, YOFFSET * 2, 0},
	{XOFFSET * -6, YOFFSET * 2, 0},
	{XOFFSET * -4, YOFFSET * 2, 0},
	{XOFFSET * -2, YOFFSET * 2, 0},
	{XOFFSET * 0, YOFFSET * 2, 0},
	{XOFFSET * 2, YOFFSET * 2, 0},
	{XOFFSET * 4, YOFFSET * 2, 0},
	{XOFFSET * 6, YOFFSET * 2, 0},
	{XOFFSET * 8, YOFFSET * 2, 0},
	{XOFFSET * 10, YOFFSET * 2, 0},

	{XOFFSET * -9, YOFFSET * 3, 0},
	{XOFFSET * -7, YOFFSET * 3, 0},
	{XOFFSET * -5, YOFFSET * 3, 0},
	{XOFFSET * -3, YOFFSET * 3, 0},
	{XOFFSET * -1, YOFFSET * 3, 0},
	{XOFFSET * 1, YOFFSET * 3, 0},
	{XOFFSET * 3, YOFFSET * 3, 0},
	{XOFFSET * 5, YOFFSET * 3, 0},
	{XOFFSET * 7, YOFFSET * 3, 0},
	{XOFFSET * 9, YOFFSET * 3, 0},

	{XOFFSET * -8, YOFFSET * 4, 0},
	{XOFFSET * -6, YOFFSET * 4, 0},
	{XOFFSET * -4, YOFFSET * 4, 0},
	{XOFFSET * -2, YOFFSET * 4, 0},
	{XOFFSET * 0, YOFFSET * 4, 0},
	{XOFFSET * 2, YOFFSET * 4, 0},
	{XOFFSET * 4, YOFFSET * 4, 0},
	{XOFFSET * 6, YOFFSET * 4, 0},
	{XOFFSET * 8, YOFFSET * 4, 0},

	{XOFFSET * -7, YOFFSET * 5, 0},
	{XOFFSET * -5, YOFFSET * 5, 0},
	{XOFFSET * -3, YOFFSET * 5, 0},
	{XOFFSET * -1, YOFFSET * 5, 0},
	{XOFFSET * 1, YOFFSET * 5, 0},
	{XOFFSET * 3, YOFFSET * 5, 0},
	{XOFFSET * 5, YOFFSET * 5, 0},
	{XOFFSET * 7, YOFFSET * 5, 0},

	{XOFFSET * -6, YOFFSET * 6, 0},
	{XOFFSET * -4, YOFFSET * 6, 0},
	{XOFFSET * -2, YOFFSET * 6, 0},
	{XOFFSET * 0, YOFFSET * 6, 0},
	{XOFFSET * 2, YOFFSET * 6, 0},
	{XOFFSET * 4, YOFFSET * 6, 0},
	{XOFFSET * 6, YOFFSET * 6, 0},
};

/*
**----------------------------------------------------------------------------
** Local Variables
**----------------------------------------------------------------------------
*/

static	gleidestruct *gleidescope = NULL;

#if 0
/*
 *load defaults in config structure
 */
static void setdefaultconfig(void)
{
#ifdef GRAB
	grab = False;
#endif
	move = False;
	rotate = False;
	zoom = False;
	image = NULL;
}
#endif

ENTRYPOINT Bool
gleidescope_handle_event(ModeInfo *mi, XEvent *event)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	/*
	printf("event:%d\n", event->xany.type);
	printf("button:%d\n", event->xbutton.button);
	*/
	switch(event->xany.type)
	{
		case ButtonPress:

			if (event->xbutton.button == Button1 ||
                event->xbutton.button == Button3)
			{
				/* store initial values of mouse */
				gp->xstart = event->xbutton.x;
				gp->ystart = event->xbutton.y;

				/* button is down */
				gp->button_down_p = True;
				return True;
			}
#if 0	/* TODO */
			else if (event->xbutton.button == Button4)
			{
				/* zoom in */
				return True;
			}
			else if (event->xbutton.button == Button5)
			{
				/* zoom out */
				return True;
			}
#endif
			break;

		case ButtonRelease:

			if (event->xbutton.button == Button1 ||
                event->xbutton.button == Button3)
			{
				/* button is up */
				gp->button_down_p = False;
				return True;
			}
			break;

		case MotionNotify:

			if (gp->button_down_p)
			{
				/* update mouse position */
				gp->xmouse += (double)(event->xmotion.x - gp->xstart) / MI_WIDTH(mi);
				gp->ymouse += (double)(event->xmotion.y - gp->ystart) / MI_HEIGHT(mi);
				gp->xstart = event->xmotion.x;
				gp->ystart = event->xmotion.y;

				return True;
			}
			break;
	}

	return False;
}


static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  gleidestruct *gp = (gleidestruct *) closure;

  gp->max_tx = (GLfloat) image_width  / texture_width;
  gp->max_ty = (GLfloat) image_height / texture_height;

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (gp->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  gp->waiting_for_image_p = False;
  gp->start_time = time ((time_t *) 0);
}


static void
getSnapshot(ModeInfo *mi, GLuint name)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	if (MI_IS_WIREFRAME(mi))
		return;

    gp->mipmap_p = True;
    load_texture_async (mi->xgwa.screen, mi->window,
                        *gp->glx_context, 0, 0, gp->mipmap_p, 
                        name, image_loaded_cb, gp);
}


static void
setup_file_texture (ModeInfo *mi, char *filename, GLuint name)
{
	Display *dpy = mi->dpy;
	Visual *visual = mi->xgwa.visual;
	char buf[1024];

	Colormap cmap = mi->xgwa.colormap;
	XImage *image = xpm_file_to_ximage (dpy, visual, cmap, filename);

	/* use this texture */
	glBindTexture(GL_TEXTURE_2D, name);

	clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			image->width, image->height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, image->data);
	sprintf (buf, "texture: %.100s (%dx%d)",
			filename, image->width, image->height);
	check_gl_error(buf);

	/* setup parameters for texturing */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

static void
setup_texture(ModeInfo * mi, GLuint id)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	if (!image || !*image || !strcmp(image, "DEFAULT")) {
		/* no image specified - grab screen */
		getSnapshot(mi, id);
		/* max_tx and max_ty set in getSnapshot() */
	} else {
		/* use supplied image file */
		setup_file_texture(mi, image, id);

		/* set tx params to use whole image */
		gp->max_tx = 1.0f;
		gp->max_ty = 1.0f;
	}

	check_gl_error("texture initialization");

	/* Need to flip the texture top for bottom for some reason. */
	glMatrixMode (GL_TEXTURE);
	glScalef (1, -1, 1);
	glMatrixMode (GL_MODELVIEW);
}

#define	VERTEX0	glVertex3f( 0.0000f,  0.000f, 0.0f);
#define	VERTEX1	glVertex3f( 0.0000f,  1.000f, 0.0f);
#define	VERTEX2	glVertex3f( XOFFSET,  0.500f, 0.0f);
#define	VERTEX3	glVertex3f( XOFFSET, -0.500f, 0.0f);
#define	VERTEX4	glVertex3f( 0.0000f, -1.000f, 0.0f);
#define	VERTEX5	glVertex3f(-XOFFSET, -0.500f, 0.0f);
#define	VERTEX6	glVertex3f(-XOFFSET,  0.500f, 0.0f);

static void
draw_hexagons(ModeInfo *mi, int translucency, GLuint texture)
{
	int		i;
	GLfloat	col[4];
	GLfloat t1x, t1y, t2x, t2y, t3x, t3y;
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	GLfloat tangle2;

	col[0] = 1.0;
	col[1] = 1.0;
	col[2] = 1.0;
	col[3] = (float)translucency / MAX_FADE;

	/* calculate vertices of equilateral triangle within image. */
	/* t1 is always in centre */
	t1x = gp->max_tx / 2;
	t1y = gp->max_ty / 2;
	/* t2 rotates */
	t2x = (gp->max_tx / 2) * (1 + cos((gp->ymouse * 2 * M_PI) + (gp->tangle * M_PI / 180)));
	t2y = (gp->max_ty / 2) * (1 + sin((gp->ymouse * 2 * M_PI) + (gp->tangle * M_PI / 180)));
	/* t3 is always 60' further around than t2 */
	tangle2 = (gp->ymouse * 2 * M_PI) + (gp->tangle * M_PI / 180) + (M_PI * 2 / 6);
	t3x = (gp->max_tx / 2) * (1 + (cos(tangle2)));
	t3y = (gp->max_ty / 2) * (1 + (sin(tangle2)));
	/* NB image is flipped vertically hence: */
	t1y = 1 - t1y;
	t2y = 1 - t2y;
	t3y = 1 - t3y;
	/*printf("texcoords:[%f,%f]->[%f,%f](%f,%f)\n", t1x, t1y, t2x, t2y, gp->max_tx, gp->max_ty);*/

	glColor4f(1.0, 1.0, 1.0, (float)translucency / MAX_FADE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glBindTexture(GL_TEXTURE_2D, gp->textures[texture]);

	for (i = 0 ; i < sizeof(hex) / sizeof(hex[0]) ; i++) {

		glPushMatrix();

		glTranslatef(hex[i].x, hex[i].y, 0.0);

		glBegin(GL_TRIANGLES);

		/*
		** six triangles to each hexagon
		*/

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t2x, t2y);
		VERTEX1;
		glTexCoord2f(t3x, t3y);
		VERTEX6;

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t3x, t3y);
		VERTEX6;
		glTexCoord2f(t2x, t2y);
		VERTEX5;

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t2x, t2y);
		VERTEX5;
		glTexCoord2f(t3x, t3y);
		VERTEX4;

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t3x, t3y);
		VERTEX4;
		glTexCoord2f(t2x, t2y);
		VERTEX3;

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t2x, t2y);
		VERTEX3;
		glTexCoord2f(t3x, t3y);
		VERTEX2;

		glTexCoord2f(t1x, t1y);
		VERTEX0;
		glTexCoord2f(t3x, t3y);
		VERTEX2;
		glTexCoord2f(t2x, t2y);
		VERTEX1;

		glEnd();

		glPopMatrix();
	}
	
#ifdef DISPLAY_TEXTURE
	glPushMatrix();
	/* acd debug - display (bigger, centred) texture */
	glScalef(2.0, 2.0, 2.0);
	glTranslatef(-0.5, -0.5, 0.0);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(0.0, 0.0, -0.1);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(1.0, 0.0, -0.1);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(1.0, 1.0, -0.1);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(0.0, 1.0, -0.1);
	glEnd();
	/* acd debug - display texture triangle */
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glBegin(GL_LINE_LOOP);
	glVertex3f(t1x, t1y, -0.11);
	glVertex3f(t2x, t2y, -0.11);
	glVertex3f(t3x, t3y, -0.11);
	glEnd();
	glPopMatrix();
#endif

	glDisable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

/*
 * main rendering loop
 */
static void
draw(ModeInfo * mi)
{
	GLfloat	x_angle, y_angle, z_angle;
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	vectorf	v1;
	GLfloat pos[4];

	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	gp->tic += 0.005f;

	x_angle = gp->cam_x_phase + gp->tic * gp->cam_x_speed;
	y_angle = gp->cam_y_phase + gp->tic * gp->cam_y_speed;
	z_angle = gp->cam_z_phase + gp->tic * gp->cam_z_speed;

	if (move) {
		v1.x = 2 * sin(x_angle);
		v1.y = 2 * sin(y_angle);
	} else {
		v1.x = 0;
		v1.y = 0;
	}

	/* size is changed in pinit() to be distance from plane */
	gp->size = MI_SIZE(mi);
	if (gp->size > 10) {
		gp->size = 10;
	}
	if (gp->size < -1) {
		gp->size = -1;
	}
	if (gp->size != -1) {
		/* user defined size */
		v1.z = gp->size;
	} else if (zoom) {
		/* max distance given by adding the constant and the multiplier */
		v1.z = 5.0 + 4.0 * sin(z_angle);
	} else {
		/* default */
		v1.z = 7.0;
	}

	/* update rotation angle (but not if mouse button down) */
	if (rotate && !gp->button_down_p)
	{
		float	new_rangle_vel = 0.0;

		/* update camera rotation angle and velocity */
		gp->rangle += gp->rangle_vel;
		new_rangle_vel = gp->rangle_vel + gp->rangle_acc;
		if (new_rangle_vel > -MAX_RANGLE_VEL && new_rangle_vel < MAX_RANGLE_VEL)
		{
			/* new velocity is within limits */
			gp->rangle_vel = new_rangle_vel;
		}

		/* randomly change twisting speed */
		if ((random() % 1000) < 1)
		{
			gp->rangle_acc = frand(0.002) - 0.001;
		}
	}

#ifdef WOBBLE
	/* this makes the image wobble - requires -move and a larger grid */
	gluLookAt(0, 0, v1.z, v1.x, v1.y, 0.0, 0.0, 1.0, 0.0);
#else
	/* no wobble - camera always perpendicular to grid */

	/* rotating camera rather than entire space - smoother */
	gluLookAt(
			v1.x, v1.y, v1.z,
			v1.x, v1.y, 0.0,
			sin((gp->xmouse * M_PI * 2) + gp->rangle * M_PI / 180),
			cos((gp->xmouse * M_PI * 2) + gp->rangle * M_PI / 180),
			0.0);
#endif

	/* light position same as camera */
	pos[0] = v1.x;
	pos[1] = v1.y;
	pos[2] = v1.z;
	pos[3] = 0;

	if (gp->fade == 0)
	{
		/* not fading */
		draw_hexagons(mi, MAX_FADE, gp->visible);
	}
	else
	{
		/* fading - show both textures with alpha */
		draw_hexagons(mi, MAX_FADE - gp->fade, gp->visible);
		draw_hexagons(mi, gp->fade, 1 - gp->visible);

		/* fade some more */
		gp->fade++;

		/* have we faded enough? */
		if (gp->fade > MAX_FADE)
		{
			/* stop fading */
			gp->fade = 0;
			gp->visible = 1 - gp->visible;
		}
	}

	/* increment texture angle based on time, velocity etc */
	/* but only if button is not down */
	if (!gp->button_down_p)
	{
		float		new_tangle_vel = 0.0;

		gp->tangle += gp->tangle_vel;

		/* work out new texture angle velocity */
		new_tangle_vel = gp->tangle_vel + gp->tangle_acc;
		if (new_tangle_vel > -MAX_TANGLE_VEL && new_tangle_vel < MAX_TANGLE_VEL)
		{
			/* new velocity is inside limits */
			gp->tangle_vel = new_tangle_vel;
		}

		/* randomly change texture angle acceleration */
		if ((random() % 1000) < 1)
		{
			gp->tangle_acc = frand(0.002) - 0.001;
		}
	}

	glFlush();
}

/* 
 * new window size or exposure 
 */
ENTRYPOINT void reshape_gleidescope(ModeInfo *mi, int width, int height)
{
	GLfloat		h = (GLfloat) height / (GLfloat) width;

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, 1/h, 0.1, 2000.0);
	glMatrixMode (GL_MODELVIEW);

	glLineWidth(1);
	glPointSize(1);   
}

static void
pinit(ModeInfo * mi)
{
	gleidestruct	*gp = &gleidescope[MI_SCREEN(mi)];

	/* set start time - star_time = 0 implies non-dynamic texture */
	gp->start_time = (time_t)0;

	/* set the texture size to default */
	gp->max_tx = 1.0;
	gp->max_ty = 1.0;

	/* no fading */
	gp->fade = 0;

	glShadeModel(GL_SMOOTH);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);

	/* space for textures */
	glGenTextures(1, &gp->textures[0]);
	glGenTextures(1, &gp->textures[1]);
	gp->visible = 0;

	setup_texture(mi, gp->textures[gp->visible]);

	/*
	**	want to choose a value for arg randomly if neither -arg nor -no-arg
	**	is specified. xscreensaver libraries don't seem to let you do this -
	**	if something isn't true then it is false (pesky two-state boolean values).
	**	so, i've defined both -arg and -no-arg to arguments and added the 
	**	following logic.
	**	(btw if both -arg and -no-arg are defined then arg is set to False)
	*/
	if (zoom == False && nozoom == False)
	{
		/* no zoom preference - randomise */
		zoom = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (nozoom == True)
	{
		/* definately no zoom */
		zoom = False;
	}

	if (move == False && nomove == False)
	{
		/* no move preference - randomise */
		move = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (nomove == True)
	{
		/* definately no move */
		move = False;
	}

	if (rotate == False && norotate == False)
	{
		/* no rotate preference - randomise */
		rotate = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (norotate == True)
	{
		/* definately no rotate */
		rotate = False;
	}

	/* define cam variables */
	gp->cam_x_speed = frand(3.0) - 1.5;
	gp->cam_x_phase = random() % 360;
	gp->cam_y_speed = frand(3.0) - 1.5;
	gp->cam_y_phase = random() % 360;
	gp->cam_z_speed = frand(3.0) - 1.5;
	gp->cam_z_phase = random() % 360;

	/* initial angular speeds */
	gp->rangle_vel = frand(0.2) - 0.1;
	gp->tangle_vel = frand(0.2) - 0.1;
	gp->rangle_acc = frand(0.002) - 0.001;
	gp->tangle_acc = frand(0.002) - 0.001;

    /* jwz */
    {
      GLfloat speed = 15;
      gp->rangle_vel *= speed;
      gp->tangle_vel *= speed;
      gp->rangle_acc *= speed;
      gp->tangle_acc *= speed;
    }

	/* distance is 11 - size */
	if (gp->size != -1) {
		if (zoom) {
			fprintf(stderr, "-size given. ignoring -zoom.\n");
			zoom = False;
		}
		if (gp->size < 1) {
			gp->size = 1;
		} else if (gp->size >= 10) {
			gp->size = 10;
		}
		gp->size = 11 - gp->size;
	}

#ifdef DEBUG
printf("phases [%d, %d, %d]\n", gp->cam_x_phase, gp->cam_y_phase, gp->cam_z_phase);
#endif
}

ENTRYPOINT void
init_gleidescope(ModeInfo * mi)
{
	gleidestruct *gp;
	int screen = MI_SCREEN(mi);


	if (gleidescope == NULL) {
		gleidescope = (gleidestruct *) calloc(MI_NUM_SCREENS(mi), sizeof (gleidestruct));
		if (gleidescope == NULL) {
			return;
		}
	}
	gp = &gleidescope[screen];
	gp->window = MI_WINDOW(mi);
    gp->size = -1;

	if ((gp->glx_context = init_GL(mi)) != NULL) {

		reshape_gleidescope(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

		glDrawBuffer(GL_BACK);

		/* do initialisation */
		pinit(mi);

	} else {
		MI_CLEARWINDOW(mi);
	}
}

ENTRYPOINT void
draw_gleidescope(ModeInfo * mi)
{
	gleidestruct	*gp = &gleidescope[MI_SCREEN(mi)];
	Display		*display = MI_DISPLAY(mi);
	Window		window = MI_WINDOW(mi);


	if (!gp->glx_context)
		return;

    /* Just keep running before the texture has come in. */
    /* if (gp->waiting_for_image_p) return; */

	glDrawBuffer(GL_BACK);

	glXMakeCurrent(display, window, *(gp->glx_context));
	draw(mi);

	if (mi->fps_p) {
		do_fps (mi);
	}

	glFinish();
	glXSwapBuffers(display, window);

#ifdef GRAB
	if (grab) {
		grab_frame(mi);
	}
#endif

	/* need to change texture? */
	if ((gp->start_time != 0) && (duration != -1) && gp->fade == 0) {
		if (gp->start_time + duration <= time((time_t *)0)) {
			/* get new snapshot (into back buffer) and start fade count */
			getSnapshot(mi, gp->textures[1 - gp->visible]);
			gp->fade = 1;
		}
	}
}

ENTRYPOINT void
release_gleidescope(ModeInfo * mi)
{
	if (gleidescope != NULL) {
		int screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			gleidestruct *gp = &gleidescope[screen];

			/* acd -  is this needed? */
			if (gp->glx_context) {
				/* Display lists MUST be freed while their glXContext is current. */
				glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

				/* acd - was code here for freeing things that are no longer in struct */
			}
		}
		(void) free((void *) gleidescope);
		gleidescope = NULL;
	}
	
	FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("Gleidescope", gleidescope)

#endif
