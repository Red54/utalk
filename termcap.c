/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   termcap.c

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "globals.h"
#include "util.h"
#include "termcap.h"

static char *termtype, termcap[2048], *tc, capabilities[2048];
static char termcapbuf[2000];
static int tcb = 0;

char *t_cm, *t_cl, *t_ce, *t_me, *t_mr, *t_md, *t_us, *t_cs, *t_sr, *t_sf,
     *t_le, *t_do, *t_nd, *t_up, *t_bl;
int ansi_cs = 0;
volatile int xcursor = -1, ycursor = -1;   /* absolute; -1, -1 = unknown */

static unsigned char **the_screen, **the_attrs;
/* the_screen[y] = string for the y-th line, followed by spaces
   the_screen[y][x] = character at x,y
   the_attrs[y][x]  = attribute at x,y
   the_screen[y][0] = number of chars in the line
*/

static int myputchar(int c) {
  if (tcb>1998) {
    write(TTYWRITE, termcapbuf, tcb);
    tcb = 0;
  }
  termcapbuf[tcb++] = (char )c;
  return 0;
}

static void mywrite(char *s, int l) {
  if (l+tcb>1998) {
    write(TTYWRITE, termcapbuf, tcb);
    tcb = 0;
  }
  if (l>0) memcpy(termcapbuf+tcb, s, l);
  tcb += l;
}

void flush_term(void) {
  if (tcb>0) {
    write(TTYWRITE, termcapbuf, tcb);
    tcb = 0;
  }
}

void putcap(char *s) {
  tputs(s, 0, myputchar);
}

static void empty_line(int n) {
  memset((char *)the_attrs[n], 0, cols+2);
  memset((char *)the_screen[n], ' ', cols+2);
  the_screen[n][0] = 0;
}
  
static void alloc_blank_line(int n) {
/* alloc line and put it in place */
  the_screen[n] = mymalloc(cols+8);
  the_attrs[n] = mymalloc(cols+8);
  empty_line(n);
}

void alloc_termcap_screen(void) {
/* alloc the whole screen and blank it */
  int n;
  the_screen = (void *)mymalloc((lines+8)*sizeof(char *));
  the_attrs = (void *)mymalloc((lines+8)*sizeof(char *));
  for (n = 1; n <= lines; ++n)
    alloc_blank_line(n);
}

static void empty_screen(void) {
  int n;
  for (n = 1; n <= lines; ++n)
    empty_line(n);
}

void init_termcap(void) {
/* check we're runnign on a tty, initialize the reading of the termcap
   database, read all the capabilities we'll be using (except for "li" 
   and "co", which are read elsewhere */

  if (!isatty(0)) barf("I can only run on a tty, sorry");
  if ((termtype = (char *)getenv("TERM")) == NULL) barf("No terminal type set");
  if (tgetent(termcap, termtype)<1) barf("No termcap info for your terminal");
  tc = capabilities;
  if ((t_cm = tgetstr("cm", &tc)) == NULL)
    barf("Can't find a way to move the cursor around with your terminal");
  if ((t_cl = tgetstr("cl", &tc)) == NULL)
    barf("Can't find a way to clear the screen with your terminal");
  if ((t_ce = tgetstr("ce", &tc)) == NULL)
    barf("Can't find a way to clear to end of line with your terminal");
  if ((t_cs = tgetstr("cs", &tc)) == NULL) {
    if (strncmp(termtype, "xterm", 5) == 0 || strncmp(termtype, "vt100", 5) == 0)
      ansi_cs = 1;
  }
  t_sr = tgetstr("sr", &tc);
  if ((t_sf = tgetstr("sf", &tc)) == NULL) {
    t_sf = tc;
    (*tc++) = '\n';
    (*tc++) = 0;
  }
  t_le = tgetstr("le", &tc);
  t_do = tgetstr("do", &tc);
  t_nd = tgetstr("nd", &tc);
  t_up = tgetstr("up", &tc);
  t_bl = tgetstr("bl", &tc);
  if ((t_me = tgetstr("me", &tc)) != NULL) {
    if ((t_mr = tgetstr("mr", &tc)) == NULL) t_mr = t_me;
    if ((t_md = tgetstr("md", &tc)) == NULL) t_md = t_me;
    if ((t_us = tgetstr("us", &tc)) == NULL) t_us = t_me;
  } else if ((t_me = tgetstr("se", &tc)) != NULL &&
             (t_mr = tgetstr("so", &tc)) != NULL) {
    t_md = t_mr;
    t_us = tc;
    (*tc++) = '\0';
  } else {
    t_me = t_md = t_mr = t_us = tc;
    (*tc++) = '\0';
  }
}

void beep(void) {
  if (beep_on && t_bl) putcap(t_bl);
}

void cleareol(void) {
  if (xcursor<0 || ycursor<0 || xcursor>cols || ycursor>lines)
    cleanupexit(2, "Error: can't clear to eol from unknown cursor position");
  putcap(t_ce);
  if (xcursor <= (int)the_screen[ycursor][0]) the_screen[ycursor][0] = xcursor-1;
  memset((char *)&the_screen[ycursor][xcursor], ' ', cols+3-xcursor);
  memset((char *)&the_attrs[ycursor][xcursor], 0, cols+3-xcursor);
}

void putscreen(char *s, int n, char attr) {
/* writes to the screen, assuming the chars are all printable and nothing
   is going to wrap; remmebers them with the attribute attr */
  int c;
  if (xcursor<0 || ycursor<0 || xcursor>cols || ycursor>lines)
    cleanupexit(2, "Error: can't write to screen from unknown cursor position");
  mywrite(s, n);
  for (c = 0; c<n; ++c) {
    the_screen[ycursor][xcursor+c] = s[c];
    the_attrs[ycursor][xcursor+c] = attr;
  }
  xcursor += n;
  if ((int)the_screen[ycursor][0]<xcursor-1) the_screen[ycursor][0] = xcursor-1;
  if (xcursor>cols) {
    xcursor = -1;
    ycursor = -1;
  }
}

void gotoxy(int x, int y) {
/* moves the cursor to position x, y (assumed to fit), updating
   xcursor and ycursor; attempts to be smart and use the current values 
   of xcursor/ycursor if they are adjacent */

  if (x == xcursor+1 && x>0 && y == ycursor && t_nd != NULL)
    putcap(t_nd);
  else if (x == xcursor-1 && y == ycursor && t_le != NULL)
    putcap(t_le);
  else if (x == xcursor && y == ycursor+1 && y>0 && t_do != NULL)
    putcap(t_do);
  else if (x == xcursor && y == ycursor-1 && t_up != NULL)
    putcap(t_up);
  else if (x != xcursor || y != ycursor)
    putcap((char *)tgoto(t_cm, x-1, y-1));

  xcursor = x;
  ycursor = y;
}

void backspace(void) {
  if (xcursor<0 || ycursor<0 || xcursor>cols || ycursor>lines)
    cleanupexit(2, "Error: can't backspace from unknown cursor position");
  if (xcursor>1) {
    if (t_le != NULL) {
      putcap(t_le);
      myputchar(' ');
      putcap(t_le);
      xcursor--;
    } else {
      gotoxy(xcursor-1, ycursor);
      myputchar(' ');
      gotoxy(xcursor-1, ycursor);
    }
  }
}

void clearscreen(void) {
/* clears the screen and puts the cursor at 1,1 */
  putcap(t_cl);
  xcursor = ycursor = -1;
  empty_screen();
  gotoxy(1, 1);
}

void redrawlines(int y1, int y2) {
/* redraws the screen from line y1 to y2, using the saved info */
  int l, s, ls, x = xcursor, y = ycursor;
  char sattr;

  normal();
  for (l = y1; l <= y2; ++l) {
    xcursor = ycursor = -1;
    gotoxy(1, l);
    sattr = 0;
    ls = s = 1;
    while(s <= (int)the_screen[l][0]) {
      if (sattr == the_attrs[l][s])
	s++;
      else {
	if (s != ls) mywrite((char *)&the_screen[l][ls], s-ls);
	ls = s;
	if ((the_attrs[l][s]&sattr) == sattr) {
	  if ((sattr&ATTR_BOLD) == 0 && (the_attrs[l][s]&ATTR_BOLD) != 0)
	    setbold();
	  if ((sattr&ATTR_UNDERLINED) == 0 && 
	      (the_attrs[l][s]&ATTR_UNDERLINED) != 0)
	    setunder();
	  if ((sattr&ATTR_INVERSE) == 0 && (the_attrs[l][s]&ATTR_INVERSE) != 0)
	    setinv();
	} else {
	  normal();
	  if ((the_attrs[l][s]&ATTR_BOLD) != 0)
            setbold();
	  if ((the_attrs[l][s]&ATTR_UNDERLINED) != 0)
            setunder();
	  if ((the_attrs[l][s]&ATTR_INVERSE) != 0)
            setinv();
	}
	sattr = the_attrs[l][s];
      }
    }
    if (s != ls) mywrite((char *)&the_screen[l][ls], s-ls);
    if (sattr) normal();
    putcap(t_ce);
  }
  xcursor = ycursor = -1;
  if (x != -1 && y != -1) gotoxy(x, y);
}

void redraw(void) {
/* redraws the whole screen using the saved info */
  redrawlines(1, lines);
}

void do_cs(int y1, int y2) {
/* assumes either is possible, and does a t_cs or an ansi one */ 
  static char temp[16];
  if (ansi_cs) {
    sprintf(temp, "\e[%d;%dr", y1, y2);
    mywrite(temp, strlen(temp));
  } else putcap((char *)tgoto(t_cs, y2-1, y1-1));
}

void scrolldown(int y1, int y2) {
/* scrolls the lines from y1 to y2 down by one line */
  int n;
  free(the_screen[y2]);
  free(the_attrs[y2]);
  for (n = y2; n>y1; n--) {
    the_screen[n] = the_screen[n-1];
    the_attrs[n] = the_attrs[n-1];
  }
  alloc_blank_line(y1);
  if (y1 == y2) {
    gotoxy(1, y2);
    cleareol();
  } else if (t_sr != NULL && ((y1 == 1 && y2 == lines) || t_cs != NULL || ansi_cs)) {
    if (y1>1 || y2<lines) {
      do_cs(y1, y2);
      putcap((char *)tgoto(t_cm, 0, y1-1));
      flush_term();
      putcap(t_sr);
      flush_term();
      do_cs(0, 0);
      flush_term();
    } else {
      gotoxy(1, 1);
      putcap(t_sr);
    }
    xcursor = ycursor = -1;
  } else redrawlines(y1, y2);
}

void scrollup(int y1, int y2) {
/* scrolls the lines from y1 to y2 up by one line */
  int n;
  free(the_screen[y1]);
  free(the_attrs[y1]);
  for (n = y1; n<y2; n++) {
    the_screen[n] = the_screen[n+1];
    the_attrs[n] = the_attrs[n+1];
  }
  alloc_blank_line(y2);
  if (y1 == y2) {
    gotoxy(1, y2);
    cleareol();
  } else if (t_sf != NULL && ((y1 == 1 && y2 == lines) || t_cs != NULL || ansi_cs)) {
    if (y1>1 || y2<lines) {
      do_cs(y1, y2);
      putcap((char *)tgoto(t_cm, 0, y2-1));
      putcap(t_sf);
      do_cs(0, 0);
    } else {
      gotoxy(1, lines);
      putcap(t_sf);
      xcursor = ycursor = -1;
    }
    xcursor = ycursor = -1;
  } else redrawlines(y1, y2);
}

void resize_termcap_screen(int oldlines) {
  int n;
  for (n = 1; n <= oldlines; ++n) {
    free(the_screen[n]);
    free(the_attrs[n]);
  }
  free((char *)the_screen);
  free((char *)the_attrs);
  alloc_termcap_screen();
  clearscreen();
}

