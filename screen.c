/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima    <roger.espel.llima@pobox.com>

   screen.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <string.h>
#include <stdlib.h>
#include "globals.h"
#include "termcap.h"
#include "util.h"
#include "srdpdata.h"
#include "struct.h"
#include "functions.h"
#include "screen.h"
#include "menu.h"

static char ascii_trans[] = " !cLxY|$'ca<--R_o+23'mP.,1o>123?AAAAAAACEEEEIIIIDNOOOOO*0UUUUYPBaaaaaaaceeeeiiiidnooooo/0uuuuypy";

int active_buffers = 2;

/* all absolute coordinates assume left upper screen = 1,1 */
/* all lines are numbered from 1 */

static char statusbuf[300];

static char *make_left(struct user *u) {
  static char left[12];

  if (u == users[0])
    strcpy(left, "[m]");
  else if ((u->info.flags&SRDP_FL_RECVD) == 0)
    strcpy(left, "[n]");
  else if ((u->info.flags&SRDP_FL_BROKEN) != 0)
    strcpy(left, "[b]");
  else
    strcpy(left, "[c]");
  if (u == users[0] && active_window == -1)
    strcat(left, "[*]");
/*  else if (u == 0 && active_window == 0)
    strcat(left, "[R]"); */
  else if (u == users[active_window])
    strcat(left, "[R]");
  else
    strcat(left, "---");
  return left;
}

void doflags(struct user *u) {
/* recalculates the flags for window u, and redisplays them if they
   have changed */
  char *left;
  int len, x = xcursor, y = ycursor;

  if (active_window == u->last_active &&
      u->info.flags == u->last_flags)
    return;
  u->last_active = active_window;
  u->last_flags = u->info.flags;
  left = make_left(u);
  if (strcmp(u->lefts, left) != 0) {
    strcpy(u->lefts, left);
    len = strlen(left);
    if (len>cols-2) len = cols-2;
    left[len] = '-';
    left[len+1] = 0;
    m_gotoxy(2, u->ystatus);
    m_putscreen(left, len+1, 0);
    if (x >= 0 && y >= 0) m_gotoxy(x, y);
  }
}

void setstatus(struct user *u, char *s) {
/* sets the status line, or just redisplays it if fed a NULL; always
   recalculates the flags on the left */

  int len, llen, x = xcursor, y = ycursor;
  char *left;

  left = make_left(u);
  llen = strlen(left);
  if (llen>cols-2) llen = cols-2;
  strcpy(u->lefts, left);
  if (s != NULL)
    strncpy(u->statuslines, s, 290);
  memset(statusbuf, '-', cols);
  len = strlen(u->statuslines)+2;
  if (len > cols-4-llen) len = cols-4-llen;
  if (((cols-len)>>1) >= llen+3) {
    statusbuf[((cols-len)>>1)] = statusbuf[((cols+len-2)>>1)] = ' ';
    memcpy(statusbuf+((cols+2-len)>>1), u->statuslines, len-2);
  } else {
    statusbuf[llen+3] = statusbuf[llen+len+2] = ' ';
    memcpy(statusbuf+llen+4, u->statuslines, len-2);
  }
  memcpy(statusbuf+1, left, llen);
  statusbuf[llen+1] = '-';
  m_gotoxy(1, u->ystatus);
  m_putscreen(statusbuf, cols, 0);
  if (x >= 0 && y >= 0) m_gotoxy(x, y);
}

void settopic(unsigned char *s) {
  setstatus(users[0], s);
}

static void add_pline(struct user *u, int idx, struct logical_line *lline,
		      int offset, int length) {
/* inserts an empty pline with number idx, pointing at lline, offset, length;
   we assume 1 <= idx <= (nplines+1) */
  int c;

  if (u->nplines >= u->p_arr_length) {
    u->p_arr_length += PLINE_CHUNK;                               
    u->plines = (struct physical_line *)myrealloc(u->plines,
	       (u->p_arr_length+8)*sizeof(struct physical_line));
  }
  for (c = u->nplines; c >= idx; c--)
    u->plines[c+1] = u->plines[c];
  u->nplines++;
  u->plines[idx].line = lline;
  u->plines[idx].offset = offset;
  u->plines[idx].length = length;
}

static struct logical_line *add_empty_lline(struct user *u, srdp_u32 seq) {
/* add a lline at the end of the buffer, and make a pline point to it */

  struct logical_line *l;

  l = (struct logical_line *)mymalloc(sizeof (struct logical_line));
  if (u->last_lline != NULL)
    l->number = 1+u->last_lline->number;
  else
    l->number = 1;
  l->length = 0;
  l->arr_length = LINE_CHUNK;
  l->chars = mymalloc(LINE_CHUNK+8);
  l->seqs = (srdp_u32 *)mymalloc((LINE_CHUNK+8)*sizeof(srdp_u32));
  l->len_seq = seq;
  l->any_deleted = 0;
  l->next = NULL;
  l->prev = u->last_lline;
  if (l->prev != NULL) l->prev->next = l;
  u->last_lline = l;
  if (u->first_lline == NULL) u->first_lline = l;
  add_pline(u, u->nplines+1, l, 1, 0);
  return l;
}

void init_screen(void) {
  int i;
  struct user *u;

  users[0] = mymalloc(sizeof (struct user));
  users[1] = mymalloc(sizeof (struct user));
  users[0]->winsize = (lines-2)>>1;
  users[1]->winsize = (lines-3)>>1;
  users[0]->ystatus = 1;
  users[1]->ystatus = 2+users[0]->winsize;
  users[0]->yfirst = 2;
  users[1]->yfirst = 1+users[1]->ystatus;
  users[0]->last_active = users[1]->last_active = -2;
  setstatus(users[0], "utalk version " UTALK_VERSION);
  setstatus(users[1], "not connected");
  m_gotoxy(1, 2);
  flush_term();

  for (i = 0; i<active_buffers; ++i) {
    u = users[i];
    u->first_lline = u->last_lline = NULL;
    u->p_arr_length = PLINE_CHUNK;
    u->plines = (struct physical_line *)mymalloc((u->p_arr_length+8)*
	    sizeof(struct physical_line));
    u->nplines = 0;
    add_empty_lline(u, 0);
    u->first_visible_pline = 1;
  }
  current_lline = users[0]->first_lline;
}

static void printchar(unsigned char c) {
  if (c == 0) {
  } else if (c == 0xff) {
    c = ' ';
    m_putscreen((char *)&c, 1, 0);
  } else if (c<' ') {
    setinv();
    c += '@';
    m_putscreen((char *)&c, 1, ATTR_INVERSE);
    normal();
  } else if (c >= 0x7f && c<0xa0) {
    if (!eight_bit_clean || c == 0x7f) {
      c += '@';
      c &= 0x7f;
      setinv();
      m_putscreen((char *)&c, 1, ATTR_INVERSE);
      normal();
    } else
      m_putscreen((char *)&c, 1, 0);
  } else if (c >= 0xa0) {
    if (!eight_bit_clean)
      c = ascii_trans[c-0xa0];
    m_putscreen((char *)&c, 1, 0);
  } else m_putscreen((char *)&c, 1, 0);
}

static void draw_line_from_plines(struct user *u, int pl) {
/* pl is a pline number; assumes pl is a visible line, but it can
   be pl>u->nplines */

  struct logical_line *l;
  int idx, max;

  m_gotoxy(1, u->yfirst+pl-u->first_visible_pline);
  if (pl>u->nplines)
    m_cleareol();
  else {
    l = u->plines[pl].line;
    idx = u->plines[pl].offset;
    max = l->length;
    while (idx <= max && xcursor>0 && xcursor <= cols)
      printchar((unsigned char)l->chars[idx++]);
    if (xcursor >= 0) m_cleareol();
  }
}

static void draw_end_of_line_from_plines(struct user *u, int pl, int idx,
					 int x0) {
  struct logical_line *l;
  int max;

  m_gotoxy(x0, u->yfirst+pl-u->first_visible_pline);
  l = u->plines[pl].line;
  max = l->length;
  while (idx <= max && xcursor>0 && xcursor <= cols)
    printchar((unsigned char)l->chars[idx++]);
  if (xcursor >= 0) m_cleareol();
}

static void redraw_from_plines(struct user *u) {
  int pl;

  for (pl = u->first_visible_pline; pl<u->first_visible_pline+u->winsize; ++pl)
    draw_line_from_plines(u, pl);
}

void scroll_to_pline(struct user *u, int pl) {
#ifdef DEBUG
  fprintf(stderr, "scroll to pline %u %u\r\n", u, pl);  /* DEBUG */
#endif /* DEBUG */
  if (pl == u->first_visible_pline) {
    /* nothing! */
  } else if (pl == u->first_visible_pline+1) {
    if (pl <= u->nplines) {
      u->first_visible_pline++;
      m_scrollup(u->yfirst, u->yfirst+u->winsize-1);
      draw_line_from_plines(u, u->first_visible_pline+u->winsize-1);
    }
  } else if (pl == u->first_visible_pline-1) {
    if (pl >= 1) {
      u->first_visible_pline--;
      m_scrolldown(u->yfirst, u->yfirst+u->winsize-1);
      draw_line_from_plines(u, u->first_visible_pline);
    }
  } else if (pl >= 1 && pl <= u->nplines) {
    u->first_visible_pline = pl;
    redraw_from_plines(u);
  }
}

int find_pline(struct user *u, struct logical_line *l) {
/* finds the lline's first pline */
  
  int p1 = 1, p2 = u->nplines, p3;

  while (u->plines[p1].line->number < l->number && 
	u->plines[p2].line->number > l->number) {
    p3 = (p1+p2)>>1;
    if (u->plines[p3].line->number < l->number)
      p1 = p3;
    else
      p2 = p3;
  }
  if (u->plines[p2].line->number == l->number) p1 = p2;
  while (p1>1 && u->plines[p1-1].line->number == l->number) p1--;
  return p1;
}

struct logical_line *find_lline(struct user *u, int n, srdp_u32 seq) {
/* finds the lline, creating it if necessary but not displaying anything */
  
  int p1 = 1, p2 = u->nplines, p3;
  struct logical_line *l;

  if (n <= u->last_lline->number) {
    while (u->plines[p1].line->number < n && u->plines[p2].line->number > n) {
      p3 = (p1+p2)>>1;
      if (u->plines[p3].line->number < n)
	p1 = p3;
      else
	p2 = p3;
    }
    if (u->plines[p2].line->number == n) p1 = p2;
    return u->plines[p1].line;
  } else {
    while (n>u->last_lline->number)
      l = add_empty_lline(u, seq);
    return l;
  }
} 

static void extend_lline(struct user *u, struct logical_line *l, srdp_u32 seq,
			 int len) {
/* extends the lline to length len, if it was shorter, updating the
   plines, and the screen if visible, and updating u->first_visible_pline
   if it changed; ignores it if seq<l->len_seq */

  int pl, toadd, addhere, pointing, c;

  if (seq>l->len_seq)
    l->len_seq = seq;
  else if (seq<l->len_seq)
    return;
  if (l->length >= len) return;
  pointing = 1+l->length;
  toadd = len-l->length;
  if (len>l->arr_length) {
    l->arr_length += LINE_CHUNK;
    l->chars = (unsigned char *)myrealloc(l->chars, l->arr_length+8);
    l->seqs = (srdp_u32 *)myrealloc(l->seqs, (l->arr_length+8)*sizeof(srdp_u32));
  }
  for (c = 0; c<toadd; c++) {
    l->chars[c+1+l->length] = 0xff;
    l->seqs[c+1+l->length] = 0;
  }
  l->length = len;
  l->len_seq = seq;
  pl = find_pline(u, l);
  while (pl<u->nplines && u->plines[pl+1].line->number == l->number) pl++;
  /* pl = last pline for l */

  addhere = cols-u->plines[pl].length;
  if (addhere>toadd) addhere = toadd;
  if (addhere>0) {
    toadd -= addhere;
    u->plines[pl].length += addhere;
    pointing += addhere;
  }

  while (toadd>0) {
    addhere = (toadd <= cols ? toadd : cols);
    add_pline(u, pl+1, l, pointing, addhere);
    if (pl >= u->first_visible_pline && pl<u->first_visible_pline+u->winsize) {
      /* extending a line the last pline of which is visible */
      if (u->nplines == u->first_visible_pline+u->winsize) {
	/* and we're at the end of the buffer (special case) */
	u->first_visible_pline++;
	m_scrollup(u->yfirst, pl+1-u->first_visible_pline+u->yfirst);
      } else if (pl != u->first_visible_pline+u->winsize-1)
      m_scrolldown(u->yfirst+pl+1-u->first_visible_pline,
		 u->yfirst+u->winsize-1);
    } else if (pl<u->first_visible_pline) u->first_visible_pline++;
    pl++;
    pointing += addhere;
    toadd -= addhere;
  }
}

static void remove_pline(struct user *u, int pl) {
/* removes pline number pl */
  int c;

  for (c = pl; c<u->nplines; c++)
    u->plines[c] = u->plines[c+1];
  u->nplines--;
}

int get_plength(struct user *u, int *pl, int pos) {
/* returns the position in the pline pl or one of the following (if they
   refer to the same lline), not counting nulls, at which the character 
   'pos' in the lline is, i.e a cursor position (left = 1); updates
   pl to point to that line; can be called with pos = 0; if called with
   pos>length-of-the-right-lline, returns the position after, which
   can be cols+1 */

  int r = 1, i = 0;

  while (*pl<u->nplines && u->plines[*pl].line == u->plines[(*pl)+1].line &&
	 u->plines[(*pl)+1].offset <= pos)
    (*pl)++;
  if (!u->plines[*pl].line->any_deleted) {
    r = pos - u->plines[*pl].offset + 1;
    if (r > u->plines[*pl].line->length - u->plines[*pl].offset + 2)
      r = u->plines[*pl].line->length - u->plines[*pl].offset + 2;
    if (r < 1)
      r = 1;
  } else {
    while (u->plines[*pl].offset+i < pos &&
	   (u->plines[*pl].offset+i <= u->plines[*pl].line->length)) {
      if (u->plines[*pl].line->chars[u->plines[*pl].offset+i] != 0) r++;
      ++i;
    }
  }
  return r;
}

void shorten_lline(struct user *u, struct logical_line *l, srdp_u32 seq,
		   int len) {
/* shortens the lline to length len, if it was shorter, and lengthens
   if it was longer, updating the plines, and the screen if visible, and 
   updating u->first_visible_pline if it changed ; ignores it if 
   seq<l->len_seq */

  int pl, pos, len0;

#ifdef DEBUG
  fprintf(stderr, "shorten_lline %d %d %d\r\n", u, l->number, len); /* DEBUG */
#endif /* DEBUG */
  
  if (seq>l->len_seq)
    l->len_seq = seq;
  else if (seq<l->len_seq)
    return;
  extend_lline(u, l, seq, len);
  len0 = l->length;
  l->length = len;
  l->len_seq = seq;
  pl = find_pline(u, l);
  while (pl<u->nplines && u->plines[pl+1].line->number == l->number) pl++;
  while (u->plines[pl].offset>len && u->plines[pl].offset>1) {
    remove_pline(u, pl);
    if (pl >= u->first_visible_pline && pl<u->first_visible_pline+u->winsize) {
      m_scrollup(u->yfirst+pl-u->first_visible_pline, u->yfirst+u->winsize-1);
      if (u->first_visible_pline>u->nplines) {
	u->first_visible_pline--;
	draw_line_from_plines(u, u->first_visible_pline);
      }
    } else if (pl<u->first_visible_pline) u->first_visible_pline--;
    pl--;
  }

  if (len>0)
    pos = get_plength(u, &pl, len);
  else
    pos = 0;
  u->plines[pl].length = pos;

  if (pos<cols && pl >= u->first_visible_pline &&
      pl<u->first_visible_pline+u->winsize)
    if (len == len0-1 && pos<cols-1) {
      m_gotoxy(pos+2, u->yfirst+pl-u->first_visible_pline);
      m_backspace();
    } else {
      m_gotoxy(pos+1, u->yfirst+pl-u->first_visible_pline);
      m_cleareol();
    }
}

void set_cursor(struct user *u, struct logical_line *l, srdp_u32 seq,
		int pos) {
/* puts the cursor at the given position, unless the line is shorter
   with a higher seq number; will scroll if necessary */

  int pl;

#ifdef DEBUG
  fprintf(stderr, "set_cursor %d %d %d\r\n", u, l->number, pos);  /* DEBUG */
#endif  /* DEBUG */

  if (seq<l->len_seq && pos<l->length) return;
  extend_lline(u, l, seq, pos-1);
  pl = find_pline(u, l);
  if (pos <= l->length)
    pos = get_plength(u, &pl, pos);
  else if (pos != 1) {
    pos = get_plength(u, &pl, pos-1);
    if (++pos>cols) pos = cols;
  }
  if (pl<u->first_visible_pline)
    scroll_to_pline(u, pl);
  else if (pl >= u->first_visible_pline+u->winsize)
    scroll_to_pline(u, pl-u->winsize+1);
  m_gotoxy(pos, u->yfirst+pl-u->first_visible_pline);
}

void write_char(struct user *u, struct logical_line *l, srdp_u32 seq, int pos, 
		unsigned char c) {
/* puts the character at the given position in the llines, will fix
   the plines if it's a null, and prints it if it's visible */

  int pl, ppos, cpl;
  char oc;

#ifdef DEBUG
  if (c >= ' ') /* DEBUG */
    fprintf(stderr, "write_char %d %d %d '%c'\r\n", u, l->number, pos, c); /* DEBUG */
  else /* DEBUG */
    fprintf(stderr, "write_char %d %d %d %.2x\r\n", u, l->number, pos, c); /* DEBUG */
#endif /* DEBUG */
  if (seq<l->len_seq && pos>l->length) return;
  extend_lline(u, l, seq, pos);
  pl = find_pline(u, l);
  if (l->seqs[pos]>seq) return;
  ppos = get_plength(u, &pl, pos);
  oc = l->chars[pos];
  l->chars[pos] = c;
  l->seqs[pos] = seq;
  if (c == 0) {
    l->any_deleted = 1;
    if (oc == 0)
      return;
    cpl = pl;
    u->plines[cpl].length--;
    if (pl >= u->first_visible_pline && pl<u->first_visible_pline+u->winsize)
      draw_end_of_line_from_plines(u, pl, pos, ppos);
    while (cpl<u->nplines && u->plines[cpl+1].line == l &&
	   u->plines[cpl+1].length>0) {
      u->plines[cpl].length++;
      u->plines[cpl+1].length--;
      while (u->plines[cpl+1].offset <= l->length && 
	     l->chars[u->plines[cpl+1].offset++] == 0);
      cpl++;
      if (cpl >= u->first_visible_pline && cpl<u->first_visible_pline+u->winsize)
	draw_line_from_plines(u, cpl);
    }
    if (cpl <= u->nplines && u->plines[cpl].line == l && 
	u->plines[cpl].length == 0) {
      /* need to destroy cpl */
      remove_pline(u, cpl);
      if (cpl >= u->first_visible_pline && 
	  cpl<u->first_visible_pline+u->winsize) {
	m_scrollup(u->yfirst+cpl-u->first_visible_pline, u->yfirst+u->winsize-1);
	if (u->first_visible_pline>u->nplines) {
	  u->first_visible_pline--;
	  draw_line_from_plines(u, u->first_visible_pline);
	}
      } else if (cpl<u->first_visible_pline) u->first_visible_pline--;
    }
    if (pl >= u->first_visible_pline && pl<u->first_visible_pline+u->winsize)
      m_gotoxy(ppos, u->yfirst+pl-u->first_visible_pline);
  } else if (pl >= u->first_visible_pline && 
	     pl<u->first_visible_pline+u->winsize) {
    m_gotoxy(ppos, u->yfirst+pl-u->first_visible_pline);
    printchar(c);
  } else if (u == users[0] || u != users[active_window == -1 ? 0 : active_window]) {
    cpl = pl+1-u->winsize;
    if (cpl<1) cpl = 1;
    scroll_to_pline(u, cpl);
    m_gotoxy(ppos, u->yfirst+pl-u->first_visible_pline);
    printchar(c);
  }
}

void recalc_all(void) {
  struct logical_line **oldcll, *l;
  struct user *u;
  int *oldpos, lastpl, idx, pl, keeppl = 0, i;

  oldpos = (int *)mymalloc(active_buffers*sizeof(int));
  oldcll = (struct logical_line **)mymalloc(active_buffers*sizeof(l));
  for (i = 0; i<active_buffers; i++) {
    u = users[i];
    if (u->first_visible_pline+u->winsize-1>u->nplines)
      lastpl = u->nplines;
    else
      lastpl = u->first_visible_pline+u->winsize-1;
    oldcll[i] = u->plines[lastpl].line;
    if (lastpl == u->nplines || u->plines[lastpl+1].line != oldcll[i])
      oldpos[i] = oldcll[i]->length;
    else
      oldpos[i] = u->plines[lastpl+1].offset-1;
  }
  users[0]->winsize = (lines-2)>>1;
  users[1]->winsize = (lines-3)>>1;
  users[0]->ystatus = 1;
  users[1]->ystatus = 2+users[0]->winsize;
  users[0]->yfirst = 2;
  users[1]->yfirst = 1+users[1]->ystatus;
  setstatus(users[0], NULL);
  setstatus(users[1], NULL);
  for (i = 0; i<active_buffers; i++) {
  /* try to keep oldcll, oldpos visible at the bottom of the screen */
    u = users[i];
    u->nplines = 0;
    l = u->first_lline;
    while (l != NULL) {
      idx = 1;
      add_pline(u, u->nplines+1, l, 1, 0);
      pl = u->nplines;
      while (idx <= l->length) {
	if (u->plines[pl].length >= cols) {
	  add_pline(u, u->nplines+1, l, idx, 0);
	  pl = u->nplines;
	}
	if (l == oldcll[i] && idx == oldpos[i]) keeppl = pl;
	if (l->chars[idx++] != 0) u->plines[pl].length++;
      }
      l = l->next;
    }
    if (keeppl >= u->winsize)
      u->first_visible_pline = 1+keeppl-u->winsize;
    else
      u->first_visible_pline = 1;
    redraw_from_plines(u);
  }
  free(oldcll);
  free(oldpos);
}

void pl_redraw_screen_zone(int y1, int y2) {
  struct user *u;
  int i, pl;
  
  for (i=0; i<active_buffers; i++) {
    u = users[i];
    if (u->ystatus >= y1 && u->ystatus <= y2)
      setstatus(u, NULL);
    if (u->yfirst <= y2 && u->yfirst + u->winsize - 1 >= y1) {
      pl = u->first_visible_pline;
      if (u->yfirst <= y1)
	pl = u->first_visible_pline + y1 - u->yfirst;
      while (pl < u->first_visible_pline + u->winsize &&
	     pl <= u->first_visible_pline + y2 - u->yfirst)
	draw_line_from_plines(u, pl++);
    }
  }
}

void pl_redraw_all(void) {
  int i;

  for (i=0; i<active_buffers; i++) {
    setstatus(users[i], NULL);
    redraw_from_plines(users[i]);
  }
}
  

