/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   signal.c

   Started: 19 Oct 95 by   <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "globals.h"
#include "termio.h"
#include "termcap.h"
#include "util.h"
#include "signal.h"

#ifndef USE_SGTTY
#include <termios.h>
#endif

#if !defined(SA_INTERRUPT) && !defined(SA_RESTART)
#define NO_SIGACTION
#endif

#ifdef USE_SIGVEC
#define NO_SIGACTION
#endif

static void putsignal(int n, v_i s) {
#ifdef USE_SIGVEC
  struct sigvec sig;

  sig.sv_handler = s;
  sig.sv_mask = 0;
  sig.sv_flags = 0;
#ifdef SV_INTERRUPT
  sig.sv_flags |= SV_INTERRUPT;
#endif
  sigvec(n, &sig, NULL);
#else
#ifndef NO_SIGACTION
  struct sigaction sig;

  sig.sa_flags = 0;
  sigemptyset(&sig.sa_mask);
  sig.sa_handler = s;
#ifdef SA_INTERRUPT
  sig.sa_flags |= SA_INTERRUPT;
#endif
  sigaction(n, &sig, NULL);
#else
  signal(n, s);
#endif
#endif
}

static void interrupted(int n) {
/* SIGINT + SIGHUP handler */
  static int interrupts = 0;

  if (!in_main_loop)
    cleanupexit(1, "^C");
  if (++interrupts >= 3)
    cleanupexit(1, "interrupted");
  mustdie = 1;
  putsignal(SIGINT, interrupted);
  putsignal(SIGHUP, interrupted);
}

static void sigpipe(int n) {
  cleanupexit(1, "broken pipe (received SIGPIPE)");
}

static void sigquit(int n) {
  cleanupexit(1, "aborting...");
}

static void sigcont(int n) {
  allsignals();
  if (in_main_loop)
    mustredisplay = 1;
  else {
    redraw();
    flush_term();
  }
  settty(SET_RAW, 0);
}

static void suspend(int n) {
  int x = xcursor, y = ycursor;

  normal();
  gotoxy(0, lines-1);
  flush_term();
  cleareol();
  xcursor = x;
  ycursor = y;
  settty(SET_COOKED, 0);
  putsignal(SIGTSTP, SIG_DFL);
  putsignal(SIGCONT, sigcont);
  kill(getpid(), SIGTSTP);
}

static void sigwinch(int n) {
#ifdef TIOCGWINSZ
  struct winsize wsz;

  if (ioctl(TTYREAD, TIOCGWINSZ, &wsz) >= 0 && wsz.ws_row != 0 && wsz.ws_col != 0) {
    winch = 1;
    newcols = wsz.ws_col;
    newlines = wsz.ws_row;
  }
  putsignal(SIGWINCH, sigwinch);
#endif
}

void allsignals(void) {
  putsignal(SIGHUP, interrupted);
  putsignal(SIGINT, interrupted);
  putsignal(SIGQUIT, sigquit);
  putsignal(SIGPIPE, sigpipe);
  putsignal(SIGTSTP, suspend);
  putsignal(SIGCONT, sigcont);
#ifdef TIOCGWINSZ
  putsignal(SIGWINCH, sigwinch);
#endif
}


