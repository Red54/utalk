/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima   <roger.espel.llima@pobox.com>

   rc.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "util.h"
#include "kbd.h" 
#include "rc.h"

static struct toggle togs[] = {
  { "beep", &beep_on },
  { "eight-bit", &eight_bit_clean },
  { "eightbit", &eight_bit_clean },
  { "eb", &eight_bit_clean },
  { "word-wrap", &wordwrap },
  { "wordwrap", &wordwrap },
  { "ww", &wordwrap },
  { "meta-esc", &meta_esc },
  { "metaesc", &meta_esc },
  { "me", &meta_esc },
  { NULL, NULL }
};

static struct alias *alias0 = NULL;

char *resolve_alias(char *uh) {
  struct alias *a;
  static char uh1[256], *at;
  int found = 0;

  for (a=alias0; a; a=a->next)
    if (a->type == ALIAS_ALL && strcmp(uh, a->from) == 0)
      return a->to;

  strncpy(uh1, uh, 255);
  uh1[254] = 0;
  if ((at = strchr(uh1, '@')) != NULL)
    *at = 0;
  for (a=alias0; a; a=a->next) {
    if (a->type == ALIAS_BEFORE && strcmp(uh1, a->from) == 0) {
      found = 1;
      strncpy(uh1, a->to, 255);
      uh1[254] = 0;
      if ((at = strchr(uh, '@')) != NULL)
	if (strlen(uh1) + strlen(at) < 256)
	  strcat(uh1, at);
      uh = uh1;
      break;
    }
  }
  if (!found) {
    strncpy(uh1, uh, 255);
    uh1[254] = 0;
  }
  at = strchr(uh1, '@');
  if (at && at[1]) {
    at++;
    for (a=alias0; a; a=a->next) {
      if (a->type == ALIAS_AFTER && strcmp(at, a->from) == 0) {
	found = 1;
	if (strlen(a->to) + (at - uh1) < 255)
	  strcpy(at, a->to);
	break;
      }
    }
  }
  if (found)
    return uh1;
  else
    return uh;
}

char *do_rc_line(unsigned char *b) {
  unsigned char *r, *nx, *at;
  struct toggle *t;
  struct alias *a;
  int onoff;
  
  r = b + strlen(b);
  while (r > b && (*(r-1) == 13 || *(r-1) == 10))
    *--r = 0;
  r = b;
  while (*r) if (*(r++) == '#') *(r-1) = 0;
  nx = b;
  r = getword(&nx);
  if (r == NULL) return NULL;

  if (strcmp((char *)r, "set") == 0 ||
      strcmp((char *)r, "se") == 0 ||
      strcmp((char *)r, "toggle") == 0) {
    if (nx == NULL) return "Missing argument for set";
    r = getword(&nx);
    if (nx != NULL)
      nx = getword(&nx);
    if (*r == 'n' && r[1] == 'o') {
      r += 2;
      onoff = 0;
      if (nx != NULL) return "Too many arguments for set";
    } else if (nx == NULL || strcmp(nx, "on") == 0) {
      onoff = 1;
    } else if (strcmp(nx, "off") == 0) {
      onoff = 0;
    } else return "Bad value";
    for (t=&togs[0]; t->var; t++) {
      if (strcmp(r, t->name) == 0) {
	*t->var = onoff;
	return NULL;
      }
    }
    return "No such setting";
  } else if (strcmp((char *)r, "alias") == 0) {
    if (nx == NULL) return "Missing argument";
    r = getword(&nx);
    if (nx == NULL) return "Missing argument";
    nx = getword(&nx);
    a = mymalloc(sizeof (struct alias));
    at = strchr(r, '@');
    if (at == r) {
      a->type = ALIAS_AFTER;
      strncpy(a->from, r+1, 255);
      a->from[254] = 0;
      strncpy(a->to, (*nx=='@' ? nx+1 : nx), 255);
      a->to[254] = 0;
    } else if (at == r + strlen(r) - 1) {
      a->type = ALIAS_BEFORE;
      *at = 0;
      strncpy(a->from, r, 255);
      a->from[254] = 0;
      strncpy(a->to, nx, 255);
      a->to[254] = 0;
      if ((at = strchr(a->to, '@')) != NULL)
	*at = 0;
    } else {
      a->type = ALIAS_ALL;
      strncpy(a->from, r, 255);
      a->from[254] = 0;
      strncpy(a->to, nx, 255);
      a->to[254] = 0;
    }
    a->next = alias0;
    alias0 = a;
  } else if (strcmp((char *)r, "bind") == 0 ||
	     strcmp((char *)r, "bindkey") == 0) {
    if (nx == NULL) return "Missing argument";
    return new_binding(nx, 0);
  } else if (strcmp((char *)r, "bind!") == 0 ||
	     strcmp((char *)r, "bindkey!") == 0) {
    if (nx == NULL) return "Missing argument";
    return new_binding(nx, 1);
  } else if (strcmp((char *)r, "emacs-mode") == 0 ||
	     strcmp((char *)r, "emacs") == 0) {
    current_mode = &modes[EMACS];
  } else if (strcmp((char *)r, "vi-mode") == 0 ||
	     strcmp((char *)r, "vi") == 0) {
    current_mode = &modes[VI_INS];
  } else return "Invalid toggle";
  return NULL;
}

void read_rc(void) {
  unsigned char b[300];
  char *warning, *home;
  int warnings = 0, rcline = 0;
  FILE *f;

  kbd_defaults();
  if ((home = (unsigned char *)getenv("HOME")) == NULL) 
    return;
  if (strlen((char *)home)>285) 
    return;
  strcpy((char *)b, (char *)home);
  strcat((char *)b, "/.utalkrc");
  if ((f = fopen((char *)b, "r")) == NULL) 
    return;
  while (fgets((char *)b, 299, f) != NULL) {
    rcline++;
    if ((warning = do_rc_line(b)) != NULL) {
      fprintf(stderr, "%s in rc file, line %d\n", warning, rcline);
      warnings++;
    }
  }
  fclose(f);
  if (warnings) {
    printf("\nPress Enter to continue...\n");
    fgets(b, 300, stdin);
  }
}

