/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   struct.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef STRUCT_H
#define STRUCT_H

#define LINE_CHUNK 40
#define PLINE_CHUNK 500
#define BUFFERS 2

#include "srdpdata.h"

typedef void (*ftype)(unsigned char c);

struct logical_line {
/* the arrays contain always at least 'length' elements */
  int number;
  int length;			/* of the logical line */
  int arr_length;		/* of the arrays */
  int any_deleted;		/* true if any chars have the special value 0 */
  unsigned char *chars;		/* entry 0 is unused */
  srdp_u32 *seqs;
  srdp_u32 len_seq;
  struct logical_line *next, *prev;
};

struct physical_line {
  struct logical_line *line;
  int offset;		/* offset into the lline */
  int length;		/* length of the pline in non-null chars */
};

struct user {
  char name[12], hostname[256], tty[16];
  srdp_u32 hostaddr;
  int daemon, ourdaemon;
  char statuslines[300], lefts[12];
  struct srdp_info info;
  int last_flags, last_active;	/* to avoid recalculating statuslines */
  struct logical_line *first_lline, *last_lline;
  struct physical_line *plines;
  int p_arr_length, nplines;
  int first_visible_pline;
  int winsize;
  int yfirst;		/* first y for each window */
  int ystatus;		/* y for each status line */
};

struct function {
  char name[40];
  ftype func;
};

struct mode {
  ftype funcs[256];
  struct binding *bindings;
};

struct binding {
  unsigned char sequence[16];
  ftype func;
  struct binding *next;
};

#define EMACS	0
#define VI_CMD	1
#define VI_INS	2
#define N_MODES 3

struct alias {
  char from[256], to[256];
  int type;
  struct alias *next;
};

#define ALIAS_ALL	0
#define ALIAS_BEFORE	1
#define ALIAS_AFTER	2

#endif

