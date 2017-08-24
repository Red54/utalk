/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   screen.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef SCREEN_H
#define SCREEN_H

#include "struct.h"
#include "srdpdata.h"

extern int active_buffers;

extern void setstatus(struct user *u, char *s);
extern void settopic(unsigned char *s);
extern void doflags(struct user *u);
extern void init_screen(void);
extern void recalc_all(void);

extern struct logical_line *find_lline(struct user *u, int n, srdp_u32 seq);
extern int find_pline(struct user *u, struct logical_line *l);
extern int get_plength(struct user *u, int *pl, int pos);

extern void scroll_to_pline(struct user *u, int pl);
extern void shorten_lline(struct user *u, struct logical_line *l,
			  srdp_u32 seq, int len);
extern void set_cursor(struct user *u, struct logical_line *l, srdp_u32 seq,
		       int pos);
extern void write_char(struct user *u, struct logical_line *l, srdp_u32 seq,
		       int pos, unsigned char c);

extern void pl_redraw_screen_zone(int y1, int y2);
extern void pl_redraw_all(void);

#endif

