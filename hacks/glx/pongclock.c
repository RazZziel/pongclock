/* pongclock, Copyright (c) 2007-2008 Ismael Barros <razielmine@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#define DEF_COLOR      "#00FFCC"

#define DEFAULTS                                \
    "*delay:        30000       \n"             \
    "*count:        30          \n"             \
    "*showFPS:      False       \n"             \
    ".foreground: " DEF_COLOR  "\n"             \

#define GL_GLEXT_PROTOTYPES
#include "xlockmore.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#ifdef USE_GL /* whole file */

#define pong_font_width  59
#define pong_font_height 5
#define pong_char_height pong_font_height
#define pong_char_width  (pong_font_width/10)
#define pong_char_offset (pong_char_width+1)
static unsigned char pong_font_bits[] = {
    0x1f, 0xf6, 0x7d, 0xdb, 0xf7, 0x7d, 0xdf, 0x07, 0x1b, 0x86, 0x61, 0xdb,
    0x30, 0x60, 0xdb, 0x06, 0x1b, 0xf6, 0x7d, 0xdf, 0xf7, 0x61, 0xdf, 0x07,
    0x1b, 0x36, 0x60, 0x18, 0xb6, 0x61, 0x1b, 0x06, 0x1f, 0xf6, 0x7d, 0xd8,
    0xf7, 0x61, 0x1f, 0x06 };


#define refresh_pong 0
#define release_pong 0

#define countof(x) (sizeof(x)/sizeof(*(x)))


#define DRAW_TEXT_

typedef struct {
    GLint   x;
    GLint   y;
    GLsizei width;
    GLsizei height;
    GLuint  fbo;
    GLuint  depth;
    GLuint  texture;
    GLfloat modelview[16];
    GLfloat projection[16];
} surface_t;

typedef struct {
    GLXContext *glx_context;

    GLfloat ball_r,
        stick_w, stick_h,
        normal_range, loser_range,
        net_w, net_h,
        wall_h,
        ball_v, stick_v;

    GLuint textures[10];
    surface_t window_surface;
    surface_t surfaces[5];

    Bool button_down_p;


    struct s_stick {
        GLfloat x, y, o; /* o = orientation */
        int score, ball_reached;
        Bool must_lose;
    } stick_l, stick_r;

    struct s_ball {
        GLfloat x, y, vx, vy;
    } ball;

} pong_configuration;


static pong_configuration *pps = NULL;


static Bool do_fbo;

static XrmOptionDescRec opts[] = {
    { "-fbo", ".fbo", XrmoptionNoArg, "True" },
    { "+fbo", ".fbo", XrmoptionNoArg, "False" }
};

#if 1
#define DEF_FBO "True"
#else
#define DEF_FBO "False"
#endif

static argtype vars[] = {
    { &do_fbo, "fbo", "FBO", DEF_FBO, t_Bool }
};

ENTRYPOINT ModeSpecOpt pong_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void draw_background(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    int i;

    /* Walls */

    glBegin(GL_QUADS);
    {
        glVertex2f( 0.0f,         0.0f );
        glVertex2f( MI_WIDTH(mi), 0.0f );
        glVertex2f( MI_WIDTH(mi), pp->wall_h );
        glVertex2f( 0.0f,         pp->wall_h );

        glVertex2f( 0.0f,         MI_HEIGHT(mi) - pp->wall_h );
        glVertex2f( MI_WIDTH(mi), MI_HEIGHT(mi) - pp->wall_h );
        glVertex2f( MI_WIDTH(mi), MI_HEIGHT(mi) );
        glVertex2f( 0.0f,         MI_HEIGHT(mi) );
    }
    glEnd();

    /* Net */
    for (i=0; i < MI_HEIGHT(mi); i+=5*pp->net_h)
    {
        glBegin(GL_QUADS);
        {
            glVertex2f( MI_WIDTH(mi)/2 - pp->net_w, i );
            glVertex2f( MI_WIDTH(mi)/2 + pp->net_w, i );
            glVertex2f( MI_WIDTH(mi)/2 + pp->net_w, i + pp->net_h );
            glVertex2f( MI_WIDTH(mi)/2 - pp->net_w, i + pp->net_h );
        }
        glEnd();
    }
}


static void draw_stick(ModeInfo *mi, struct s_stick *stick)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    GLfloat dif = pp->ball.y - stick->y;

    /* If the ball is in this stick's side of the
     * screen and approaching, move the stick */

    if (((stick->o * pp->ball.vx) < 0) &&                    /* Ball is aproaching the stick */
        (!stick->must_lose || (fabsf(dif) > pp->stick_h)) && /* If the stick must lose, make sure it does */
        (fabsf(stick->x - pp->ball.x) < (stick->must_lose
                                         ? pp->loser_range       /* Range for a losing stick */
                                         : pp->normal_range)))   /* Range for a normal stick */
    {
        /* 1. Accelerate until the ball is near the center
         * 2. Follow the ball at constant speed */

        if (((dif > 0.0f) &&
             (stick->y + pp->stick_h < MI_HEIGHT(mi) - pp->wall_h)) ||
            ((dif < 0.0f) &&
             (stick->y - pp->stick_h > pp->wall_h)))
        {

            /*if (stick->ball_reached == 0)*/
            /*if (dif > 0.0f)*/

            /*stick->y += dif/(fabsf(dif))
             * logf(abs(dif/20.0)+1.0)
             * 1.2 * pp->stick_v;*/

            stick->y += ( sin(dif/MI_HEIGHT(mi)*M_PI/2.0f) *
                          MI_HEIGHT(mi) /
                          (stick->must_lose ? 10.0f : 5.0f ) );

            /*if (abs(dif) == 0.0f)
              stick->y += dif;
              else {
              stick->y += dif/(fabsf(dif)) * logf(abs(dif)) * pp->stick_v;
              }*/

        }

        /* Collision detection */
        if ((fabsf(dif) <= pp->stick_h) &&
            ((stick->o * (pp->ball.x - (stick->o * pp->ball_r)))
             < (stick->o * (stick->x + (stick->o * pp->stick_w)))))
        {
            pp->ball.vx = -pp->ball.vx;
        }
    }


    /* Drawing */
    glBegin(GL_QUADS);
    {
        glVertex2f(stick->x - pp->stick_w, stick->y - pp->stick_h);
        glVertex2f(stick->x + pp->stick_w, stick->y - pp->stick_h);
        glVertex2f(stick->x + pp->stick_w, stick->y + pp->stick_h);
        glVertex2f(stick->x - pp->stick_w, stick->y + pp->stick_h);
    }
    glEnd();
}

static void reset_ball(ModeInfo *mi, int direction)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    pp->ball.x = MI_WIDTH(mi)/2;
    pp->ball.y = MI_HEIGHT(mi)/2;
    pp->ball.vx = direction * sqrtf(pp->ball_v * pp->ball_v / 2);
    pp->ball.vy = pp->ball.vx;
}

static void draw_ball(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    /* Collision detection */

    if ((pp->ball.x + pp->ball_r) < 0.0f)
    {
        /* Increment minutes */
        pp->stick_l.must_lose = False;
        if (++pp->stick_r.score == 60) pp->stick_r.score = 0;
        reset_ball(mi, -1);
    }
    else if ((pp->ball.x - pp->ball_r) > MI_WIDTH(mi))
    {
        pp->stick_r.must_lose = False;
        /* Increment hours, reset minutes */
        if (++pp->stick_l.score == 24) pp->stick_l.score = 0;
        pp->stick_r.score = 0;
        reset_ball(mi, 1);
    }

    if (((pp->ball.y + pp->ball_r) > (MI_HEIGHT(mi) - pp->wall_h)) ||
        ((pp->ball.y - pp->ball_r) < pp->wall_h))
    {
        pp->ball.vy = -pp->ball.vy;
    }

    /* Movement */

    pp->ball.x += pp->ball.vx;
    pp->ball.y += pp->ball.vy;

    /* Drawing */
    glBegin(GL_QUADS);
    {
        glVertex2f(pp->ball.x - pp->ball_r, pp->ball.y - pp->ball_r);
        glVertex2f(pp->ball.x + pp->ball_r, pp->ball.y - pp->ball_r);
        glVertex2f(pp->ball.x + pp->ball_r, pp->ball.y + pp->ball_r);
        glVertex2f(pp->ball.x - pp->ball_r, pp->ball.y + pp->ball_r);
    }
    glEnd();
}

static void get_time(int *h, int *m)
{
    time_t t = time(NULL);
    *h = localtime(&t)->tm_hour;
    *m = localtime(&t)->tm_min;
}

static void load_fonts(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    int i, j, ch, offset;
    char font_byte, font_bit;

    unsigned char *bits = malloc(pong_char_width * pong_char_height);


    /* setup parameters for texturing */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures( 10, pp->textures );

    for (ch=0; ch<10; ch++)
    {
        /* Transform bitmap into texture */
        for (j=0; j<pong_char_height; j++)
        {
            for (i=0; i<pong_char_width; i++)
            {
                offset = (((pong_font_width/4)+2)*4)*j + (ch*6)+i;
                font_byte = pong_font_bits[offset/8];
                font_bit = font_byte >> (offset%8) & 0x1;
                bits[pong_char_width*j+i] = font_bit * 255;
            }
        }
        /* Texture */
        glBindTexture(GL_TEXTURE_2D, pp->textures[ch]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                     pong_char_width, pong_char_height,
                     0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bits);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
}

static void create_surface(surface_t *surface, Bool depth,
                           int x, int y,
                           int width, int height,
                           GLenum filter)
{
    GLenum status;

    surface->x = x;
    surface->y = y;
    surface->width = width;
    surface->height = height;
    glGetFloatv(GL_MODELVIEW_MATRIX, surface->modelview);
    glGetFloatv(GL_MODELVIEW_MATRIX, surface->projection);

    /* FBO */
    glGenFramebuffersEXT(1, &surface->fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo);

    if (depth)
    {
        /* Depth render buffer */
        glGenRenderbuffersEXT(1, &surface->depth);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, surface->depth);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
                                 width, height);

        /* Attach render buffer to FBO */
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                     GL_DEPTH_ATTACHMENT_EXT,
                                     GL_RENDERBUFFER_EXT,
                                     surface->depth);
    }
    else
    {
        surface->depth = 0;
    }

    /* Texture */
    glGenTextures(1, &surface->texture);
    glBindTexture(GL_TEXTURE_2D, surface->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); */
    /* glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); */
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Attach texture to FBO */
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, surface->texture, 0);


    status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
    {
        fprintf(stderr, "Couldn't set up framebuffer: 0x%x\n", status);
        exit(1);
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

static void bind_surface(surface_t *surface)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo);

    glViewport(surface->x, surface->y,
               surface->width, surface->height);

    /* glMatrixMode(GL_PROJECTION); */
    /* glLoadMatrixf(surface->projection); */

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(surface->modelview);

    /* Clear */
    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT | (surface->depth ? GL_DEPTH_BUFFER_BIT : 0));
    glLoadIdentity();
}


#if defined(DRAW_TEXT)
static void draw_text(ModeInfo *mi, char *text)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    glEnable(GL_TEXTURE_2D);
    do
    {
        if (*text != ' ')
        {
            glBindTexture(GL_TEXTURE_2D, pp->textures[(int)(*text-48)]);
            glBegin(GL_QUADS);
            {
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0,               0);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(pong_char_width, 0);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(pong_char_width, pong_char_width);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0,               pong_char_width);
            }
            glEnd();
        }
        glTranslatef(pong_char_offset, 0, 0);
    }
    while(*(++text));
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}
#else
static void draw_number(ModeInfo *mi, int number)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    int i;

    glEnable(GL_TEXTURE_2D);
    for (i=10; i!=0; i/=10)
    {
        glBindTexture(GL_TEXTURE_2D, pp->textures[number/i%(i*10)]);
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0.0f, 1.0f); glVertex2f(0,               0);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(pong_char_width, 0);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(pong_char_width, pong_char_width);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(0,               pong_char_width);
        }
        glEnd();
        glTranslatef(pong_char_offset, 0, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}
#endif

static void draw_score(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    int h, m;
    int scale_x = MI_WIDTH(mi)/100.0f,
        scale_y = MI_HEIGHT(mi)/80.0f;

    get_time(&h, &m);
    pp->stick_r.must_lose = (pp->stick_l.score != h);
    pp->stick_l.must_lose = (pp->stick_r.score != m);

    glPushMatrix();
    {
        glTranslatef(MI_WIDTH(mi)/2, MI_HEIGHT(mi), 0);
        glScalef(scale_x, scale_y, 0);

#if defined(DRAW_TEXT)
        {
            char sp[8];
            sprintf(sp, "%02d  %02d", pp->stick_l.score, pp->stick_r.score);
            glTranslatef(-(int)strlen(sp)/2*pong_char_offset, -2*pong_char_offset, 0);
            draw_text(mi, sp);
        }
#else
        glTranslatef(-3*pong_char_offset, -2*pong_char_offset, 0);
        draw_number(mi, pp->stick_l.score);

        glTranslatef(2*pong_char_offset, 0, 0);
        draw_number(mi, pp->stick_r.score);
#endif
    }
    glPopMatrix();
}


ENTRYPOINT void draw_pong(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    Display *dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);

    if (!pp->glx_context)
        return;
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(pp->glx_context));

    if (do_fbo)
    {
        bind_surface(&pp->surfaces[0]);
    }
    else
    {
        glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
    }

    /* Draw scene */

    glColor3f(1.0f, 1.0f, 1.0f);

    draw_background(mi);

    draw_stick(mi, &pp->stick_l);
    draw_stick(mi, &pp->stick_r);
    draw_ball(mi);
    draw_score(mi);

    glColor3f(0.0f, 0.0f, 0.0f);

    if (mi->fps_p)
    {
        glBegin(GL_QUADS);
        {
            glVertex2f(0.0f,           0.0f);
            glVertex2f(MI_WIDTH(mi)/2, 0.0f);
            glVertex2f(MI_WIDTH(mi)/2, pp->wall_h);
            glVertex2f(0.0f,           pp->wall_h);
        }
        glEnd();
        do_fps(mi);
    }

    if (do_fbo)
    {
        int i;

        /* Downsample */
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, pp->surfaces[0].texture);

        for (i=1; i<countof(pp->surfaces); i++)
        {
            bind_surface(&pp->surfaces[i]);
            glBegin(GL_QUADS);
            {
                glTexCoord2i(0, 0); glVertex2i(-1, -1);
                glTexCoord2i(1, 0); glVertex2i( 1, -1);
                glTexCoord2i(1, 1); glVertex2i( 1,  1);
                glTexCoord2i(0, 1); glVertex2i(-1,  1);
            }
            glEnd();
        }

        /* Draw to screen */
        bind_surface(&pp->window_surface);

        glColor4f(1.0f,1.0f,1.0f,1.0f);

        glPushMatrix();
#if 1
        for (i=0; i<countof(pp->surfaces); i++)
        {
            glBindTexture(GL_TEXTURE_2D, pp->surfaces[i].texture);
            glBegin(GL_QUADS);
            {
                glNormal3i( 0, 0, 1 );
                glTexCoord2i(0, 0); glVertex2i( 0,                     0 );
                glTexCoord2i(1, 0); glVertex2i( pp->surfaces[i].width, 0 );
                glTexCoord2i(1, 1); glVertex2i( pp->surfaces[i].width, pp->surfaces[i].height );
                glTexCoord2i(0, 1); glVertex2i( 0,                     pp->surfaces[i].height );
            }
            glEnd();
            /* glTranslatef(0, pp->surfaces[0].height + 1, 0); */
        }
#else
        glBindTexture(GL_TEXTURE_2D, pp->surfaces[0].texture);
        glTranslatef(MI_WIDTH(mi)/2, MI_HEIGHT(mi)/2, 200.0f);
        { static GLfloat i=0; glRotatef(30.0f+i++, 1.0f, 1.0f, 0.0f); }
        glScalef(200.0f, 200.0f, 200.0f);
        glBegin(GL_QUADS);
        {
            /* Front Face */
            glColor4f(0.0f,0.5f,0.0f,1.0f);
            glTexCoord2i(0, 0); glVertex3i(-1, -1,  1);
            glTexCoord2i(1, 0); glVertex3i( 1, -1,  1);
            glTexCoord2i(1, 1); glVertex3i( 1,  1,  1);
            glTexCoord2i(0, 1); glVertex3i(-1,  1,  1);
            /* Back Face */
            glColor4f(0.5f,0.0f,0.0f,1.0f);
            glTexCoord2i(0, 0); glVertex3i(-1, -1, -1);
            glTexCoord2i(1, 0); glVertex3i(-1,  1, -1);
            glTexCoord2i(1, 1); glVertex3i( 1,  1, -1);
            glTexCoord2i(0, 1); glVertex3i( 1, -1, -1);
            /* Top Face */
            glColor4f(0.0f,0.0f,0.5f,1.0f);
            glTexCoord2i(0, 0); glVertex3i(-1,  1, -1);
            glTexCoord2i(1, 0); glVertex3i(-1,  1,  1);
            glTexCoord2i(1, 1); glVertex3i( 1,  1,  1);
            glTexCoord2i(0, 1); glVertex3i( 1,  1, -1);
            /* Bottom Face */
            glColor4f(0.0f,0.5f,0.5f,1.0f);
            glTexCoord2i(0, 0); glVertex3i(-1, -1, -1);
            glTexCoord2i(1, 0); glVertex3i( 1, -1, -1);
            glTexCoord2i(1, 1); glVertex3i( 1, -1,  1);
            glTexCoord2i(0, 1); glVertex3i(-1, -1,  1);
            /* Right face */
            glColor4f(0.5f,0.5f,0.0f,1.0f);
            glTexCoord2i(0, 0); glVertex3i( 1, -1, -1);
            glTexCoord2i(1, 0); glVertex3i( 1,  1, -1);
            glTexCoord2i(1, 1); glVertex3i( 1,  1,  1);
            glTexCoord2i(0, 1); glVertex3i( 1, -1,  1);
            /* Left Face */
            glColor4f(0.5f,0.5f,0.5f,1.0f);
            glTexCoord2i(0, 0); glVertex3i(-1, -1, -1);
            glTexCoord2i(1, 0); glVertex3i(-1, -1,  1);
            glTexCoord2i(1, 1); glVertex3i(-1,  1,  1);
            glTexCoord2i(0, 1); glVertex3i(-1,  1, -1);
        }
        glEnd();
#endif
        glPopMatrix();

        glDisable(GL_TEXTURE_2D);
    }

    glFinish();
    glXSwapBuffers(dpy, window);
}

ENTRYPOINT void reshape_pong(ModeInfo *mi, int width, int height)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    /* Recalculate size of elements so they're always
       proportional to the windows size */

    pp->net_w  = MI_WIDTH(mi)/162.0f;
    pp->net_h  = MI_HEIGHT(mi)/512.0f;
    pp->wall_h = MI_HEIGHT(mi)/32.0f;

    pp->ball_r = MI_WIDTH(mi)/128.0f;
    pp->ball_v = MI_HEIGHT(mi)/32.0f;

    reset_ball(mi, 1);

    pp->stick_w      = MI_WIDTH(mi)/74.0f;
    pp->stick_h      = MI_HEIGHT(mi)/11.0f;
    pp->stick_v      = MI_HEIGHT(mi)/32.0f;
    pp->loser_range  = MI_WIDTH(mi)/8;
    pp->normal_range = MI_WIDTH(mi)/3;

    pp->stick_l.y = MI_HEIGHT(mi) / 2;
    pp->stick_l.x = pp->stick_w;
    pp->stick_r.y = MI_HEIGHT(mi) / 2;
    pp->stick_r.x = MI_WIDTH(mi) - pp->stick_w;


    if (do_fbo)
    {
        int i;

        /* Window surface */
        memset( &pp->window_surface, 0, sizeof(pp->window_surface) );
        pp->window_surface.width = MI_WIDTH(mi);
        pp->window_surface.height = MI_HEIGHT(mi);
        glLoadIdentity();

        glGetFloatv(GL_MODELVIEW_MATRIX, pp->window_surface.modelview);
        /* glOrtho(0.0f, MI_WIDTH(mi), MI_HEIGHT(mi), 0, 0, -600.0f); */
        glGetFloatv(GL_MODELVIEW_MATRIX, pp->window_surface.projection);
        glLoadIdentity();

        /* FBOs */
        create_surface(&pp->surfaces[0], True,
                       0, 0,
                       MI_WIDTH(mi),
                       MI_HEIGHT(mi),
                       GL_LINEAR);
        for (i=1; i<countof(pp->surfaces); i++)
        {
            create_surface(&pp->surfaces[i], False,
                           0, 0,
                           MI_WIDTH(mi) >> i,
                           MI_HEIGHT(mi) >> i,
                           GL_NEAREST);
        }
    }

    /* Recalculate prespective */

    glViewport(0, 0, MI_WIDTH(mi), MI_HEIGHT(mi));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0f, MI_WIDTH(mi), 0.0f, MI_HEIGHT(mi), 0.0, -600.0f);

    glMatrixMode(GL_MODELVIEW);
    /* glLoadIdentity(); */
}


ENTRYPOINT Bool pong_handle_event(ModeInfo *mi, XEvent *event) {
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    if (event->xany.type == ButtonPress &&
        event->xbutton.button == Button1)
    {
        pp->button_down_p = True;
        return True;
    }
    else if (event->xany.type == ButtonRelease &&
             event->xbutton.button == Button1)
    {
        pp->button_down_p = False;
        return True;
    }

    return False;
}

ENTRYPOINT void init_pong(ModeInfo *mi)
{
    pong_configuration *pp;

    if (!pps)
    {
        pps = (pong_configuration *)
            calloc (MI_NUM_SCREENS(mi), sizeof (pong_configuration));
        if (!pps)
        {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
        pp = &pps[MI_SCREEN(mi)];
    }

    pp = &pps[MI_SCREEN(mi)];
    pp->glx_context = init_GL(mi);


    glClearColor(0.0, 0.0, 0.0, 0.0);
    glShadeModel(GL_SMOOTH);

    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_BLEND );
    glEnable( GL_ALPHA_TEST );
    glEnable( GL_CULL_FACE );
    glEnable( GL_TEXTURE_2D );

    load_fonts(mi);

    pp->stick_l.x            = 0.0f;
    pp->stick_l.y            = 0.0f;
    pp->stick_l.o            = 1.0f;
    pp->stick_l.must_lose    = False;
    pp->stick_l.ball_reached = 0;

    pp->stick_r              = pp->stick_l;
    pp->stick_r.o            = -pp->stick_l.o;

    get_time(&pp->stick_l.score, &pp->stick_r.score);

    pp->ball.x  = 0.0f;
    pp->ball.y  = 0.0f;
    pp->ball.vx = 0.0f;
    pp->ball.vy = 0.0f;


    if (do_fbo && !strstr( (char *) glGetString(GL_EXTENSIONS),
                           "GL_EXT_framebuffer_object" ))
    {
        fprintf(stderr, "GL_EXT_framebuffer_object not supported, disabling post-processing\n");
        do_fbo = False;
    }

    reshape_pong( mi, MI_WIDTH(mi), MI_HEIGHT(mi) );
}

XSCREENSAVER_MODULE_2("Pong Clock", pongclock, pong)

#endif /* USE_GL */
