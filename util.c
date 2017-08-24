/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   util.c

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include "globals.h"
#include "termio.h"
#include "termcap.h"
#include "comm.h"
#include "util.h"

void barf(char *m) {
  fprintf(stderr, "%s\n", m);
  exit(1);
}

void cleanupexit(int n, char *m) {
  quick_clear_invite();
  if (connected)
     c_close();
  if (screen_inited) {
    normal();
    xcursor = ycursor = -1;
    gotoxy(0, lines-1);
    cleareol();
    flush_term();
    settty(SET_COOKED, 1);
  }
  if (m) fprintf(stderr, "%s\n", m);
  exit(n);
}

void *mymalloc(int n) {
  char *r;
  r = (void *)malloc(n);
  if (r == NULL) cleanupexit(1, "Insufficient memory");
  return r;
}

void *myrealloc(void *p, int n) {
  char *r;
  r = (void *)realloc(p, n);
  if (r == NULL) cleanupexit(1, "Insufficient memory");
  return r;
}

unsigned char *getword(unsigned char **s) {
/* returns the beginning of the word and sets the pointer to after it
   or to NULL if none after; will return NULL if no word at all; puts
   a '\0' after the word */

  unsigned char *r =*s;

  while (*r == ' ' || *r == 9 || *r == 10 || *r == 13) r++;
  if (*r == 0) {
    *s = NULL;
    return NULL;
  }
  *s = r;
  while (**s != ' ' && **s != 0 && **s != 9 && **s != 10 && **s != 13) (*s)++;
  if (**s == 0) {
    *s = NULL;
    return r;
  }
  **s = 0;
  (*s)++;
  while (**s == ' ' || **s == 9 || **s == 10 || **s == 13) (*s)++;
  if (**s == 0) *s = NULL;
  return r;
}

