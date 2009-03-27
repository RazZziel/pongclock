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

#define DEFAULTS "*delay:30000       \n"				\
    "*count:        30          \n"                                     \
    "*showFPS:      False       \n"                                     \
    ".foreground: " DEF_COLOR "\n"                                      \


#include "xlockmore.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <time.h>



#ifdef USE_GL /* whole file */

#define pong_font_width 59
#define pong_font_height 5
static unsigned char pong_font_bits[] = {
    0x1f, 0xf6, 0x7d, 0xdb, 0xf7, 0x7d, 0xdf, 0x07, 0x1b, 0x86, 0x61, 0xdb,
    0x30, 0x60, 0xdb, 0x06, 0x1b, 0xf6, 0x7d, 0xdf, 0xf7, 0x61, 0xdf, 0x07,
    0x1b, 0x36, 0x60, 0x18, 0xb6, 0x61, 0x1b, 0x06, 0x1f, 0xf6, 0x7d, 0xd8,
    0xf7, 0x61, 0x1f, 0x06 };


#define refresh_pong 0
#define release_pong 0

#define countof(x) (sizeof((x))/sizeof((*x)))
#define max(a, b) a < b ? b : a
#define min(a, b) a > b ? b : a


typedef struct {
    GLXContext *glx_context;

    GLfloat ball_r,
        stick_w, stick_h,
        stick_range_normal, stick_range_lose,
        net_w, net_h,
        wall_h,
        ball_v, stick_v;

    GLuint textures[10];

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

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
    { "-spin",   ".spin",   XrmoptionNoArg, "True" },
    { "+spin",   ".spin",   XrmoptionNoArg, "False" },
    { "-speed",  ".speed",  XrmoptionSepArg, 0 },
    { "-wander", ".wander", XrmoptionNoArg, "True" },
    { "+wander", ".wander", XrmoptionNoArg, "False" }
};

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "0.05"
static argtype vars[] = {
    {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
    {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
    {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt pong_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void draw_background(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    int i;

    /* Walls */

    glBegin(GL_QUADS);
    {
        glVertex2f(0.0f,          0.0f);
        glVertex2f(MI_WIDTH (mi), 0.0f);
        glVertex2f(MI_WIDTH (mi), pp->wall_h);
        glVertex2f(0.0f,          pp->wall_h);

        glVertex2f(0.0f,          MI_HEIGHT(mi) - pp->wall_h);
        glVertex2f(MI_WIDTH (mi), MI_HEIGHT(mi) - pp->wall_h);
        glVertex2f(MI_WIDTH (mi), MI_HEIGHT(mi));
        glVertex2f(0.0f,          MI_HEIGHT(mi));
    }
    glEnd();

    /* Net */
    for (i=0; i < MI_HEIGHT (mi); i+=5*pp->net_h)
    {
        glBegin(GL_QUADS);
        {
            glVertex2f(MI_WIDTH (mi)/2 - pp->net_w, i);
            glVertex2f(MI_WIDTH (mi)/2 + pp->net_w, i);
            glVertex2f(MI_WIDTH (mi)/2 + pp->net_w, i + pp->net_h);
            glVertex2f(MI_WIDTH (mi)/2 - pp->net_w, i + pp->net_h);
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

    if (((stick->o * pp->ball.vx) < 0) &&  /* Ball is aproaching to the stick */
        (fabsf(stick->x - pp->ball.x) < (stick->must_lose
                                         ? pp->stick_range_lose       /* Range for a losing stick */
                                         : pp->stick_range_normal)))  /* Range for a normal stick */
    {

        /* 1. Accelerate until the ball is near the center
         * 2. Follow the ball at constant speed */

        if (((dif > 0.0f) &&
             (MI_HEIGHT(mi) - pp->wall_h > stick->y + pp->stick_h)) ||
            ((dif < 0.0f) &&
             (stick->y - pp->stick_h > pp->wall_h)))
        {

            /*if (stick->ball_reached == 0)*/
            /*if (dif > 0.0f)*/

            /*stick->y += dif/(fabsf(dif))
             * logf(abs(dif/20.0)+1.0)
             * 1.2 * pp->stick_v;*/
            if (stick->must_lose)
                stick->y += sin(dif/160)*40;
            else
                stick->y += sin(dif/160)*160;

            /*if (abs(dif) == 0.0f)
              stick->y += dif;
              else {
              stick->y += dif/(fabsf(dif)) * logf(abs(dif)) * pp->stick_v;
              }*/

        }

        /* Collision detection */
        if ((((pp->ball.y > (stick->y - pp->stick_h)) &&
              (pp->ball.y < (stick->y + pp->stick_h)))) &&
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

    pp->ball.x = MI_WIDTH (mi)/2;
    pp->ball.y = MI_WIDTH (mi)/2;
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
        printf(" %02d  - [%02d]", pp->stick_l.score, pp->stick_r.score);
        reset_ball(mi, -1);
    }
    else if ((pp->ball.x - pp->ball_r) > MI_WIDTH(mi))
    {
        pp->stick_r.must_lose = False;
        /* Increment hours, reset minutes */
        if (++pp->stick_l.score == 24) pp->stick_l.score = 0;
        pp->stick_r.score = 0;
        printf("[%02d] -  %02d ", pp->stick_l.score, pp->stick_r.score);
        reset_ball(mi, 1);
    }

    if (((pp->ball.y + pp->ball_r) > (MI_HEIGHT (mi) - pp->wall_h)) ||
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
    int char_width=5;
    int i, j, ch, offset;
    char font_byte, font_bit;

    unsigned char *bits = malloc(char_width*pong_font_height);


    /* setup parameters for texturing */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures( 10, pp->textures );

    for (ch=0; ch<10; ch++)
    {
        /* Transform bitmap into  */
        for (j=0; j<pong_font_height; j++)
        {
            for (i=0; i<char_width; i++)
            {
                offset = (((pong_font_width/4)+2)*4)*j + (ch*6)+i;
                font_byte = pong_font_bits[offset/8];
                font_bit = font_byte >> (offset%8) & 0x1;
                bits[char_width*j+i] = font_bit * 255;
            }
        }
        /* Texture */
        glBindTexture(GL_TEXTURE_2D, pp->textures[ch]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, char_width, pong_font_height, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, bits);
    }
}

static void draw_number(ModeInfo *mi, int number)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    int i;

    for (i=10; i!=0; i/=10)
    {
        glBindTexture(GL_TEXTURE_2D, pp->textures[number/i%(i*10)]);
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0.0f, 1.0f); glVertex2f(0, 0);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(5, 0);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(5, 5);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 5);
        }
        glEnd();
        glTranslatef(6, 0, 0);
    }
}

static void draw_text(ModeInfo *mi, char *text)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    do
    {
        if (*text != ' ')
        {
            glBindTexture(GL_TEXTURE_2D, pp->textures[(int)(*text-48)]);
            glBegin(GL_QUADS);
            {
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0, 0);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(5, 0);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(5, 5);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 5);
            }
            glEnd();
        }
        glTranslatef(6, 0, 0);
    }
    while(*(++text));
}

static void draw_score(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    int h, m;
    int scale = MI_WIDTH(mi)/100.0f;

    get_time(&h, &m);
    if (pp->stick_r.score != m)
    {
        if (pp->stick_l.score != h)
            pp->stick_r.must_lose = True;
        else
            pp->stick_l.must_lose = True;
    }

#if 0
    glPushMatrix();
    {
        char sp[8];
        sprintf(sp, "%02d  %02d", pp->stick_l.score, pp->stick_r.score);
        glTranslatef((MI_WIDTH(mi)-(strlen(sp)+1)*5*scale)/2, MI_HEIGHT(mi)*17/20, 0);
        glScalef(scale, scale, 0);
        draw_text(mi, sp);
    }
    glPopMatrix();
#else
    glPushMatrix();
    {
        glTranslatef(MI_WIDTH(mi)/2, MI_HEIGHT(mi), 0);
        glScalef(scale, scale, 0);

        glTranslatef(-3*scale, -2*scale, 0);
        draw_number(mi, pp->stick_l.score);

        glTranslatef(2*scale, 0, 0);
        draw_number(mi, pp->stick_r.score);
    }
    glPopMatrix();
#endif
}


ENTRYPOINT void draw_pong(ModeInfo *mi)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];
    Display *dpy = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);

    if (!pp->glx_context)
        return;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(pp->glx_context));

    glClear(GL_COLOR_BUFFER_BIT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1,1,1);

    glLoadIdentity();

    draw_background(mi);

    draw_stick(mi, &pp->stick_l);
    draw_stick(mi, &pp->stick_r);
    draw_ball(mi);
    draw_score(mi);

    glColor3f(0,0,0);
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
    glFinish();

    glXSwapBuffers(dpy, window);
}

ENTRYPOINT void reshape_pong(ModeInfo *mi, int width, int height)
{
    pong_configuration *pp = &pps[MI_SCREEN(mi)];

    /* Recalculate size of elements so they're always
       proportional to the windows size */

    pp->net_w   = MI_WIDTH (mi)/162.0f;
    pp->net_h   = MI_HEIGHT (mi)/512.0f;
    pp->wall_h  = MI_HEIGHT (mi)/32.0f;

    pp->ball_r  = MI_WIDTH (mi)/128.0f;
    pp->ball_v  = MI_HEIGHT(mi)/32.0f;

    reset_ball(mi, 1);

    pp->stick_w = MI_WIDTH (mi)/74.0f;
    pp->stick_h = MI_HEIGHT (mi)/11.0f;
    pp->stick_v = MI_HEIGHT(mi)/32.0f;
    pp->stick_range_lose = MI_WIDTH (mi)/8;
    pp->stick_range_normal =  MI_WIDTH(mi)/3;

    pp->stick_l.y = MI_HEIGHT (mi) / 2;
    pp->stick_l.x = pp->stick_w;
    pp->stick_r.y = MI_HEIGHT (mi) / 2;
    pp->stick_r.x = MI_WIDTH (mi) - pp->stick_w;

    /* Recalculate prespective */

    glViewport(0, 0, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0f, MI_WIDTH (mi), 0.0f, MI_HEIGHT (mi), 0.0, -0.1);

    glMatrixMode(GL_MODELVIEW);
}


ENTRYPOINT Bool pong_handle_event (ModeInfo *mi, XEvent *event) {
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
    glEnable( GL_TEXTURE_2D );

    load_fonts(mi);

    pp->stick_l.x            =  0.0f;
    pp->stick_l.y            =  0.0f;
    pp->stick_l.o            =  1.0f;
    pp->stick_l.must_lose    =  False;
    pp->stick_l.ball_reached =  0;

    pp->stick_r.x            =  pp->stick_l.x;
    pp->stick_r.y            =  pp->stick_l.y;
    pp->stick_r.o            = -pp->stick_l.o;
    pp->stick_l.must_lose    =  pp->stick_l.must_lose;
    pp->stick_r.ball_reached =  pp->stick_l.ball_reached;

    get_time(&pp->stick_l.score, &pp->stick_r.score);

    pp->ball.x        = 0.0f;
    pp->ball.y        = 0.0f;
    pp->ball.vx       = 0.0f;
    pp->ball.vy       = 0.0f;



    reshape_pong( mi, MI_WIDTH(mi), MI_HEIGHT(mi) );
    reset_ball(mi, 1);
}

XSCREENSAVER_MODULE_2 ("Pong Clock", pongclock, pong)

#endif /* USE_GL */
