/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   termcap.h

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef TERMCAP_H
#define TERMCAP_H

#define ATTR_BOLD 1
#define ATTR_UNDERLINED 2
#define ATTR_INVERSE 4

extern int tgetent(char *bp, char *name);
extern char *tgetstr(char *id, char **area);
extern int tgetnum(char *id);
extern char *tgoto(char *cap, int col, int row);
extern int tputs(char *str, int affcnt, int (*putc)(int));

extern char *t_ce, *t_me, *t_mr, *t_md, *t_us;
extern int xcursor, ycursor;	/* absolute; -1, -1 = unknown */

extern void putcap(char *s);
extern void init_termcap(void);
extern void flush_term(void);
extern void alloc_termcap_screen(void);
extern void clearscreen(void);
extern void cleareol(void);
extern void beep(void);
extern void gotoxy(int x, int y);
extern void putscreen(char *s, int n, char attr);
extern void redraw(void);
extern void redrawlines(int y1, int y2);
extern void scrollup(int y1, int y2);
extern void scrolldown(int y1, int y2);
extern void backspace(void);
extern void resize_termcap_screen(int oldlines);

#define ATTR_BOLD 1
#define ATTR_UNDERLINED 2
#define ATTR_INVERSE 4

#define setinv() (putcap(t_mr))
#define setunder() (putcap(t_us))
#define setbold() (putcap(t_md))
#define normal() (putcap(t_me))

#endif

