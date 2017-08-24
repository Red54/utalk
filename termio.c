/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   termio.c

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "globals.h"
#include "util.h"
#include "termio.h"
#include "termcap.h"

#ifdef USE_SGTTY
#include <sgtty.h>
#else
#include <termios.h>
#endif

#ifdef TIOCGWINSZ
struct winsize wsz;
#endif

#ifdef USE_SGTTY
struct sgttyb term, term0;
struct tchars tch, tch0;
struct ltchars lch, lch0;
#else
struct termios term, term0;
#endif

void get_screensize(void) {
/* gets the best estimate of the screen size in 'cols' and 'lines';
   requires termcap to be initialized */

  char *vr;

#ifdef TIOCGWINSZ
  if (ioctl(TTYWRITE, TIOCGWINSZ, &wsz)<0 || wsz.ws_row<1 || wsz.ws_col<1) {
#endif
    lines = ((vr = getenv("LINES")) ? atoi(vr) : 0);
    cols = ((vr = getenv("COLUMNS")) ? atoi(vr) : 0);
    if (lines<1 || cols<1) {
      if ((lines = tgetnum("li"))<1 || (cols = tgetnum("co"))<1) {
        lines = 24; cols = 80;
      }
    }
#ifdef TIOCGWINSZ
  } else {
    lines = wsz.ws_row;
    cols = wsz.ws_col;
  }
#endif
  if (lines<5 || cols<4)
    cleanupexit(1, "window too small");
  if (cols>250) cols = 250;
  if (lines>250) lines = 250;
}

void init_termio(void) {
/* saves the tty modes and sets the new ones */

#ifdef USE_SGTTY
  if (ioctl(TTYREAD, TIOCGETP, &term)<0 || ioctl(TTYREAD, TIOCGETC, &tch)<0 ||
      ioctl(TTYREAD, TIOCGLTC, &lch)<0) {
    perror("sgtty get ioctl");
    exit(1);
  }
  term0 = term;
  tch0 = tch;
  lch0 = lch;
  term.sg_flags|= CBREAK;
  term.sg_flags&= ~ECHO & ~CRMOD;

  memset(&tch, -1, sizeof(tch));
  memset(&lch, -1, sizeof(lch));
  tch.t_intrc = (char)3;
  tch.t_quitc = (char)28;
  lch.t_suspc = (char)26;

  if (ioctl(TTYREAD, TIOCSETP, &term)<0 || ioctl(TTYREAD, TIOCSETC, &tch)<0 ||
      ioctl(TTYREAD, TIOCSLTC, &lch)<0) {
    perror("sgtty set ioctl");
    exit(1);
  }
#else
  if (tcgetattr(TTYREAD, &term)<0) {
    perror("tcgetattr");
    exit(1);
  }
  term0 = term;

  term.c_lflag &= ~ECHO & ~ICANON;
  term.c_oflag &= ~OPOST;
  term.c_cc[VTIME] = (char)0;
  term.c_cc[VMIN] = (char)1; 
  term.c_cc[VSTOP] = (char)0;
  term.c_cc[VSTART] = (char)0;
  term.c_cc[VQUIT] = (char)28;
  term.c_cc[VINTR] = (char)3;
  term.c_cc[VSUSP] = (char)26;
#ifdef VREPRINT
  term.c_cc[VREPRINT] = (char)0;
#endif
#ifdef VDISCARD
  term.c_cc[VDISCARD] = (char)0;
#endif
#ifdef VLNEXT
  term.c_cc[VLNEXT] = (char)0;
#endif
#ifdef VDSUSP
  term.c_cc[VDSUSP] = (char)0;
#endif
  if (tcsetattr(TTYREAD, TCSANOW, &term)<0) {
    perror("tcsetattr");
    exit(1);
  }
#endif

  ++screen_inited;
}

#define SET_RAW 0
#define SET_COOKED 1
void settty(int how, int drain) {
/* sets the tty mode to 'raw' (the mode we use) or 'cooked' (the mode
   the tty was originally in) according to 'how' and using the saved
   values; if 'drain' is set, uses TCSADRAIN (if using the POSIX 
   interface) */

#ifdef USE_SGTTY
  ioctl(TTYREAD, TIOCSETP, how == SET_RAW ? &term : &term0);
  ioctl(TTYREAD, TIOCSETC, how == SET_RAW ? &tch : &tch0);
  ioctl(TTYREAD, TIOCSLTC, how == SET_RAW ? &lch : &lch0);
#else
  tcsetattr(TTYREAD, drain ? TCSADRAIN : TCSANOW,
	    how == SET_RAW ? &term : &term0);
#endif
}



