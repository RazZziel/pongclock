/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

#ifndef __STONERVIEW_MOVE_H__
#define __STONERVIEW_MOVE_H__

typedef struct elem_struct {
  GLfloat pos[3];
  GLfloat vervec[2];
  GLfloat col[4];
} elem_t;

extern void init_move(stonerview_state *);
extern void final_move(stonerview_state *);
extern void move_increment(stonerview_state *);


extern stonerview_state * init_view(int wireframe_p);
extern void win_draw(stonerview_state *);
extern void win_release(stonerview_state *);

#endif /* __STONERVIEW_MOVE_H__ */
