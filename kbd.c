/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima  <roger.espel.llima@pobox.com>

   kbd.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "functions.h"
#include "struct.h"
#include "rc.h"
#include "util.h"
#include "globals.h"
#include "kbd.h"
#include "menu.h"

struct mode modes[N_MODES], *current_mode;

ftype force_next_func = NULL;		/*  if set, overrides the binding
					    for the current key and gets unset
					    afterwards; used by 
					    vi-replace-char and quote-char to
					    force a self-insert */

int vi_prefix = 0;			/*  numeric prefix to vi commands */

unsigned char *emacs_bindings[] = {
  "^i tab",
  "^m new-line",
  "^j new-line",
  "^q quote-char",	/* probably not usable b/c of flow control */
  "^d delete",
  "^k delete-end-of-line",
  "^u delete-line",
  "^h backspace",
  "^? backspace",
  "^w backspace-word",
  "\\ed delete-word",
  "\\ef forward-word",
  "\\eb backward-word",
  "^a beginning-of-line",
  "^e end-of-line",
  "^v down-page",
  "\\ev up-page",
  "\\e< top",
  "\\e> bottom",
  "^l redisplay",
  "^r resynch",
  "^xb next-window",
  "^xo next-window",
  "^g next-window",
  "^xc quit",
  "^f forward",
  "^b backward",
  "^p up",
  "^n down",
  "^t set-topic",
  "\\eH do-help",
  "\\ex do-command",
  "\\e[D backward",
  "\\e[C forward",
  "\\e[A up",
  "\\e[B down",
  NULL
};

unsigned char *vi_cmd_bindings[] = {
  "\\n new-line",
  "\\r new-line",
  "h backward",
  "j down",
  "k up",
  "l forward",
  "i vi-insert-mode",
  "a vi-add",
  "I vi-insert-at-beginning-of-line",
  "A vi-add-at-end-of-line",
  "R vi-insert-mode",
  "o vi-open",
  "O vi-open-above",
  "r vi-replace-char",
  "x delete",
  "X backspace",
  "dd delete-line",
  "db backspace-word",
  "dw delete-word",
  "dW delete-word",
  "de delete-end-of-word",
  "d$ delete-end-of-line",
  "d0 delete-beginning-of-line",
  "d\\^ delete-beginning-of-line",
  "df vi-delete-find-char",
  "dF vi-delete-reverse-find-char",
  "dt vi-delete-till-char",
  "dT vi-delete-reverse-till-char",
  "D delete-end-of-line",
  "w forward-word",
  "W forward-word",
  "e end-of-word",
  "b backward-word",
  "B backward-word",
  "0 beginning-of-line",
  "\\^ beginning-of-line",
  "$ end-of-line",
  "^d down-half-page",
  "^f down-page",
  "^u up-half-page",
  "^b up-page",
  "H top-of-screen",
  "M middle-of-screen",
  "L bottom-of-screen",
  "G vi-goto-line",
  "\e nop",
  "^l redisplay",
  "g next-window",
  "f vi-find-char",
  "F vi-reverse-find-char",
  "t vi-till-char",
  "T vi-reverse-till-char",
  "; vi-repeat-find",
  ", vi-reverse-repeat-find",
  "~ vi-flip-case",
  "^t set-topic",
  "^r resynch",
  ": do-command",
  "ZZ quit",
  "H do-help",
  "[D backward",
  "[C forward",
  "[A up",
  "[B down",
  NULL
};

unsigned char *vi_ins_bindings[] = {
  "\\e vi-escape",
  "^? backspace",
  "^i tab",
  "^m new-line",
  "^j new-line",
  "^u delete-line",
  "^h backspace",
  "^w backspace-word",
  "^l redisplay",
  "^r resynch",
  "^v quote-char",
  NULL
};

struct function *find_function(unsigned char *s) {
  struct function *f = the_functions;

  for (; f->name[0] != 0; f++)
    if (strcmp(f->name, (char *)s) == 0)
      return f;
  return NULL;
}

static int hexparse(unsigned char c) {
  if (c <= '9' && c >= '0')
    return c - '0';
  else if (c <= 'F' && c >= 'A')
    return c - 'A' + 10;
  else if (c <= 'f' && c >= 'a')
    return c - 'a' + 10;
  return 0;
}

#define is_hex(c) (((c) <= '9' && (c) >= '0') || ((c) <= 'f' && (c) >= 'a') ||\
		   ((c) <= 'F' && (c) >= 'A'))

char *new_binding(unsigned char *line, int insmode) {
  unsigned char *r, *nx, sequence[16], *w = sequence;
  static unsigned char s[200];
  struct function *func = NULL;
  struct mode *mode;
  struct binding *binding;
  int add = 0;

  if (current_mode == &modes[EMACS])
    mode = current_mode;
  else if (insmode)
    mode = &modes[VI_INS];
  else
    mode = &modes[VI_CMD];

  strncpy(s, line, 199);
  nx = s;
  r = getword(&nx);
  while (*r) {
    if (w>sequence+14)
      return "Sequence too long";
    if (*r == 'M' && r[1] == '-') {
      add += 128;
      r += 2;
    } else if (*r == '^' || (*r == 'C' && r[1] == '-')) {
      add -= '@';
      if (*r == '^')
	r++;
      else
	r += 2;
      if (*r >= 'a' && *r <= 'z')
	add -= 32;
      else if (*r == '?')
	add = 0x7f - '?';
    } else if (*r == '\\' && r[1] != 0) {
      r++;
      if (*r == 'e' || *r == 'E') {
	r++;
	*(w++) = 0x1b + add;
	add = 0;
      } else if (*r == 't' || *r == 'T') {
	r++;
	*(w++) = 9 + add;
	add = 0;
      } else if (*r == 'n' || *r == 'N') {
	r++;
	*(w++) = 10 + add;
	add = 0;
      } else if (*r == 'r' || *r == 'R') {
	r++;
	*(w++) = 13 + add;
	add = 0;
      } else if (*r == 'x') {
	int c = 0;
	r++;
	if (is_hex(*r)) {
	  c = hexparse(*r);
	  r++;
	  if (is_hex(*r)) {
	    c = (c * 16) + hexparse(*r);
	    r++;
	  }
	}
	*(w++) = c + add;
	add = 0;
      } else {
	*(w++) = (*(r++) + add);
	add = 0;
      }
    } else {
      *(w++) = (*(r++)+add);
      add = 0;
    }
  }
  if (w == sequence)
    return "Invalid key code";
  if (nx == NULL)
    return "Missing argument";

  r = getword(&nx);
  func = find_function(r);
  if (func == NULL)
    return "Unrecognized function name";
  if (nx != NULL)
    return "Too many arguments";

  *w = 0;
  if (w == sequence+1) {
    mode->funcs[*sequence] = func->func;
  } else {
    binding = (struct binding *)mymalloc(sizeof(struct binding));
    binding->next = mode->bindings;
    mode->bindings = binding;
    binding->func = func->func;
    strcpy((char *)binding->sequence, (char *)sequence);
  }
  return NULL;
}

static void nb(char *s, int n) {
  char *t = new_binding(s, n);
  if (t) {
    fprintf(stderr, "%s on line '%s'\n", t, s);
    sleep(1);
  }
}

void kbd_defaults(void) {
  int i;
  unsigned char **s;

  for (i=0; i<256; i++)
    modes[EMACS].funcs[i] = modes[VI_INS].funcs[i] = 
      modes[VI_CMD].funcs[i] = f_beep;

  for (i=' '; i<='~'; i++)
    modes[EMACS].funcs[i] = modes[VI_INS].funcs[i] = f_self_insert;

  for (i=0x80; i<0xff; i++)
    modes[EMACS].funcs[i] = modes[VI_INS].funcs[i] = f_self_insert;

  current_mode = &modes[EMACS];
  for (s=emacs_bindings; *s; s++)
    nb(*s, 0);
  current_mode = &modes[VI_CMD];
  for (s=vi_cmd_bindings; *s; s++)
    nb(*s, 0);
  current_mode = &modes[VI_INS];
  for (s=vi_ins_bindings; *s; s++)
    nb(*s, 1);

  current_mode = &modes[EMACS];
}

static int match(unsigned char *s, unsigned char *t) {
/* returns the number of matching chars in the 2 strings */
  int r = 0;

  while (*s && *t && *s == *t) {
    s++;
    t++;
    r++;
  }
  return r;
}

void keyboard(unsigned char c) {
  static unsigned char seq[17];
  static int inseq = 0;
  struct binding *b = NULL;
  ftype func;
  int found = 0;

  if (the_menu != NULL) {
    menu_keyboard(c);
    return;
  }

  if (force_next_func != NULL) {
    (*force_next_func)(c);
    force_next_func = NULL;
    return;
  }

  if (current_mode == &modes[VI_CMD] && inseq == 0 &&
      ((vi_prefix != 0 && c >= '0' && c <= '9') || (c >= '1' && c <= '9'))) {
    vi_prefix = vi_prefix * 10 + (c - '0');
    return;
  }

  if (meta_esc && current_mode == &modes[EMACS] && c > 0x7f) {
    keyboard(0x1b);
    keyboard(c & 0x7f);
    return;
  }

  if (inseq > 15)
    inseq = 0;
  seq[inseq++] = c;
  seq[inseq] = 0;
  
  for (b=current_mode->bindings; b; b=b->next) {
    if (match(b->sequence, seq) == inseq) {
      if (strlen(b->sequence) == inseq) {
	func = b->func;
	found = 2;
	break;
      }
      found = 1;
    }
  }

  if (found == 1)
    return;
  inseq = 0;
  if (found == 0)
    func = current_mode->funcs[c];

  (*func)(c);
  
  vi_prefix = 0;
}

