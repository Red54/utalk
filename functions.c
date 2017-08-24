/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima  <roger.espel.llima@pobox.com>

   functions.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "termcap.h"
#include "screen.h"
#include "util.h"
#include "comm.h"
#include "functions.h"
#include "struct.h"
#include "kbd.h"
#include "menu.h"

static next_pos = 1;				/* for my window */
struct logical_line *current_lline;		/* lline with the cursor */

/* conventions: no nulls at the end of a logical line (use shorten_lline
   instead of overwriting with nulls), next_pos never points to a null */

#define U0 users[0]
#define L current_lline

int active_window = -1;		/* -1 = mine, read-write
				    0 = mine, read-only
				    1 = first other, read-only
				   ...
				*/

static int last_find_char = 0;
static ftype last_find_function = NULL, reverse_find_function = NULL;

struct function the_functions[] = {
  { "self-insert",	f_self_insert },
  { "insert-in-place",	f_insert_in_place },
  { "tab",		f_tab },
  { "new-line",		f_new_line },
  { "delete",		f_delete },
  { "delete-end-of-line",	f_delete_end_of_line },
  { "delete-beginning-of-line",	f_delete_beginning_of_line },
  { "delete-line",	f_delete_line },
  { "delete-word",	f_delete_word },
  { "delete-end-of-word",	f_delete_end_of_word },
  { "backspace",	f_backspace },
  { "backspace-word",	f_backspace_word },
  { "backward",		f_backward },
  { "forward",		f_forward },
  { "backward-word",	f_backward_word },
  { "forward-word",	f_forward_word },
  { "end-of-word",	f_end_of_word },
  { "beginning-of-line",	f_beginning_of_line },
  { "end-of-line",	f_end_of_line },
  { "nop",		f_nop },
  { "beep",		f_beep },
  { "up",		f_up },
  { "down",		f_down },
  { "up-page",		f_up_page },
  { "down-page",	f_down_page },
  { "up-half-page",	f_up_half_page },
  { "down-half-page",	f_down_half_page },
  { "top-of-screen",	f_top_of_screen },
  { "middle-of-screen",		f_middle_of_screen },
  { "bottom-of-screen",		f_bottom_of_screen },
  { "top-or-up-page",	f_top_or_up_page },
  { "bottom-or-down-page",	f_bottom_or_down_page },
  { "top",		f_top },
  { "bottom",		f_bottom },
  { "vi-goto-line",	f_vi_goto_line },
  { "redisplay",	f_redisplay },
  { "resynch",		f_resynch },
  { "next-window",	f_next_window },
  { "vi-insert-mode",	f_vi_insert_mode },
  { "vi-command-mode",	f_vi_command_mode },
  { "emacs-mode",	f_emacs_mode },
  { "quit",		f_quit },
  { "vi-escape",	f_vi_escape },
  { "vi-add",		f_vi_add },
  { "vi-add-at-end-of-line",	f_vi_add_at_end_of_line },
  { "vi-insert-at-beginning-of-line",	f_vi_insert_at_beginning_of_line },
  { "vi-open",		f_vi_open },
  { "vi-open-above",	f_vi_Open },
  { "vi-replace-char",	f_vi_replace_char },
  { "vi-find-char",	f_vi_find_char },
  { "vi-reverse-find-char",	f_vi_reverse_find_char },
  { "vi-till-char",	f_vi_till_char },
  { "vi-reverse-till-char",	f_vi_reverse_till_char },
  { "vi-repeat-find",	f_vi_repeat_find },
  { "vi-reverse-repeat-find",	f_vi_reverse_repeat_find },
  { "vi-delete-find-char",	f_vi_delete_find_char },
  { "vi-delete-reverse-find-char",	f_vi_delete_reverse_find_char },
  { "vi-delete-till-char",	f_vi_delete_till_char },
  { "vi-delete-reverse-till-char",	f_vi_delete_reverse_till_char },
  { "vi-flip-case",	f_vi_flip_case },
  { "quote-char",	f_quote_char },
  { "do-help",		f_do_help },
  { "set-topic", 	f_set_topic },
  { "do-command",	f_do_command },
  { "test-menu",	f_test_menu },	/* xxx */
  { "test-entry",	f_test_entry },	/* xxx */
  { "test-selection",	f_test_selection },	/* xxx */
  { "",			(ftype)0 }
};

static int check_write(void) {
  if (active_window != -1) {
    beep();
    return 0;
  } else return 1;
}

static int skip_nulls(struct logical_line *l, int pos0, int delta) {
/* assumes l is the line next_pos applies to, and skips nulls to the
   right if delta == 1, to the left if delta == -1 */

  int r = pos0;
  while (l->chars[r] == 0 && r+delta <= l->length && r+delta>0) r += delta;
  return r;
}

/* the argument to all f_* functions is the last char in the
   sequence */

void f_new_line(unsigned char c) {
  if (!check_write()) return;
  L = find_lline(U0, L->number + 1, 0);
  next_pos = skip_nulls(L, 1, 1);
  set_cursor(U0, L, 0, next_pos);
  c_set_cursor(L->number, next_pos);
}

void f_self_insert(unsigned char c) {
  struct logical_line *l1;
  int pl, ppos, nl, nl0;

  if (!check_write()) return;
  if (c == 0 || c == 0xff || next_pos>0xfffc) 
    beep();
  else {
    if (c == 7) {
      beep();
      c_send_beep(L->number, next_pos);
    } else {
      if (wordwrap && L->next == NULL && L->length == next_pos - 1) {
	pl = find_pline(U0, L);
	ppos = get_plength(U0, &pl, next_pos);
	if (ppos > cols) {
	  if (c == ' ') {
	    f_new_line(c);
	    return;
	  }
	  next_pos = 1;
	  l1 = find_lline(U0, L->number + 1, 0);
	  nl = L->length;
	  while (nl>0 && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl--;
	  if (nl>0) {
	    nl0 = nl+1;
	    nl = skip_nulls(L, nl0, 1);
	    if (L->chars[nl] != ' ' && L->chars[nl] != 0xff) {
	      for (; nl <= L->length; nl++) {
		if (L->chars[nl] != 0 && L->chars[nl] != 0xff) {
		  write_char(U0, l1, 0, next_pos, L->chars[nl]);
		  c_write_char(l1->number, next_pos, L->chars[nl]);
		}
		if (L->chars[nl] != 0) next_pos++;
	      }
	      shorten_lline(U0, L, 0, nl0-1);
	      c_shorten_lline(L->number, nl0-1);
	    }
	  }
	  L = l1;
	}
      }
      write_char(U0, L, 0, next_pos, c);
      c_write_char(L->number, next_pos, c);
      next_pos = skip_nulls(L, ++next_pos, 1);
    }
  }
}

void f_insert_in_place(unsigned char c) {
  if (!check_write()) return;
  if (c == 0 || c == 0xff || next_pos>0xfffc) 
    beep();
  else {
    if (c == 7) {
      beep();
      c_send_beep(L->number, next_pos);
    } else {
      write_char(U0, L, 0, next_pos, c);
      c_write_char(L->number, next_pos, c);
      set_cursor(U0, L, 0, next_pos);
      c_set_cursor(L->number, next_pos);
    }
  }
}

void f_tab(unsigned char c) {
  int pl, ppos, toadd;

  if (!check_write()) return;
  if (next_pos>0xfff8)
    beep();
  else {
    pl = find_pline(U0, L);
    ppos = get_plength(U0, &pl, next_pos);
    toadd = 9+((ppos-1)&0xfff8)-ppos;
    for (; toadd>0; toadd--) {
      next_pos++;
      if (next_pos <= L->length) next_pos = skip_nulls(L, next_pos, +1);
    }
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_delete(unsigned char c) {
  int npos;

  if (!check_write()) return;
  if (next_pos == L->length) {
    if (next_pos>1) {
      npos = skip_nulls(L, next_pos-1, -1);
      if (L->chars[npos] == 0)
	npos = 0;
    } else npos = 0;
    next_pos = npos+1;
    shorten_lline(U0, L, 0, npos);
    c_shorten_lline(L->number, npos);
  } else if (next_pos<L->length) {
    write_char(U0, L, 0, next_pos, 0);
    c_write_char(L->number, next_pos, 0);
    next_pos = skip_nulls(L, next_pos, 1);
  }
}

static char nulls[512];

static void delete_till(struct logical_line *l, int till) {
  int nl, nl0, n;

  if (till < next_pos) {
    n = till;
    till = next_pos - 1;
    next_pos = n;
    if (next_pos < 1)
      next_pos = 1;
  }

  if (till >= l->length) {
    f_delete_end_of_line(0);
  } else {
    nl0 = next_pos;
    nl = skip_nulls(l, till, -1);
    n = nl - nl0 + 1;
    memset(nulls, 0, 512);
    while (n > 0) {
      if (n > 512) {
	c_write_string(l->number, nl0, nulls, 512);
	nl0 += 512;
	n -= 512;
      } else {
	c_write_string(l->number, nl0, nulls, n);
	nl0 += n;
	n = 0;
      }
    }
    for (; next_pos <= nl; next_pos++)
      write_char(U0, l, 0, next_pos, 0);
    next_pos = skip_nulls(l, next_pos, 1);
    set_cursor(U0, l, 0, next_pos);
    c_set_cursor(l->number, next_pos);
  }
}

void f_delete_beginning_of_line(unsigned char c) {
  if (!check_write()) return;
  if (next_pos > skip_nulls(L, 1, 1))
    delete_till(L, 1);
}

void f_delete_end_of_line(unsigned char c) {
  int npos;

  if (!check_write()) return;
  if (next_pos <= L->length+1) {
    if (next_pos>1) {
      npos = skip_nulls(L, next_pos-1, -1);
      if (L->chars[npos] == 0)
	npos = 0;
    } else npos = 0;
    next_pos = npos+1;
    shorten_lline(U0, L, 0, npos);
    c_shorten_lline(L->number, npos);
  }
}

void f_delete_line(unsigned char c) {
  if (!check_write()) return;
  if (L->length) {
    next_pos = 1;
    shorten_lline(U0, L, 0, 0);
    c_shorten_lline(L->number, 0);
  }
}

void f_backspace(unsigned char c) {
  int opos, npos;

  if (!check_write()) return;
  if (next_pos>1) {
    if (L->length == next_pos-1) {
      opos = skip_nulls(L, next_pos-1, -1);
      if (opos>1) {
	npos = skip_nulls(L, opos-1, -1);
	if (L->chars[npos] == 0)
	  npos = 0;
      } else npos = 0;
      shorten_lline(U0, L, 0, npos);
      c_shorten_lline(L->number, npos);
      next_pos = npos+1;
    } else {
      npos = skip_nulls(L, next_pos-1, -1);
      if (L->chars[npos] != 0) {
	write_char(U0, L, 0, npos, 0);
	c_write_char(L->number, npos, 0);
      }
    }
  }
}
      
void f_backspace_word(unsigned char c) {
  int nl, nl0, n;

  if (!check_write()) return;
  if (next_pos>1) {
    if (L->length == next_pos-1) {
      nl = L->length;
      while (nl>0 && (L->chars[nl] == ' ' || L->chars[nl] == 0xff ||
	     L->chars[nl] == 0)) nl--;
      while (nl>0 && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl--;
      shorten_lline(U0, L, 0, nl);
      c_shorten_lline(L->number, nl);
      next_pos = nl+1;
    } else {
      nl = nl0 = next_pos-1;
      while (nl>0 && (L->chars[nl] == ' ' || L->chars[nl] == 0xff ||
	     L->chars[nl] == 0)) nl--;
      while (nl>0 && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl--;
      nl = skip_nulls(L, nl+1, 1);
      nl0 = skip_nulls(L, nl0, -1);
      n = nl0-nl+1;
      memset(nulls, 0, 512);
      while (n > 0) {
	if (n > 512) {
	  n -= 512;
	  c_write_string(L->number, nl0-n+1, nulls, 512);
	} else {
	  c_write_string(L->number, nl0-n+1, nulls, n);
	  n = 0;
	}
      }
      for (; nl <= nl0; nl++)
	write_char(U0, L, 0, nl, 0);
    }
  }
}

void f_delete_word(unsigned char c) {
  int n;

  if (!check_write()) return;
  n = next_pos;

  if (n <= L->length) {
    while (n <= L->length && L->chars[n] != ' ' && L->chars[n] != 0xff) n++;
    while (n <= L->length+1 && (L->chars[n] == ' ' || L->chars[n] == 0xff
			       || L->chars[n] == 0)) n++;
    delete_till(L, n-1);
  }
}

void f_delete_end_of_word(unsigned char c) {
  int n;
  
  if (!check_write()) return;
  n = next_pos + 1;

  if (n <= L->length) {
    while (n <= L->length && (L->chars[n] == ' ' || L->chars[n] == 0xff ||
	   L->chars[n] == 0)) n++;
    while (n <= L->length && L->chars[n] != ' ' && L->chars[n] != 0xff) n++;
    delete_till(L, n-1);
  }
}

void f_backward(unsigned char c) {
  int npos;

  if (!check_write()) return;
  if (vi_prefix) {
    npos = next_pos;
    while (vi_prefix > 0) {
      vi_prefix--;
      npos = skip_nulls(L, npos-1, -1);
      if (npos <= 1)
	break;
    }
  } else npos = skip_nulls(L, next_pos-1, -1);
  if (npos>0 && L->chars[npos] != 0) {
    next_pos = npos;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_forward(unsigned char c) {
  int npos;

  if (!check_write()) return;
  if (vi_prefix) {
    npos = next_pos;
    while (vi_prefix > 0) {
      vi_prefix--;
      npos = skip_nulls(L, npos+1, 1);
      if (npos > L->length)
	break;
    }
  } else npos = skip_nulls(L, next_pos+1, 1);
  if (npos <= L->length+1) {
    next_pos = npos;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_backward_word(unsigned char c) {
  int nl;

  if (!check_write()) return;
  if (next_pos>1) {
    nl = next_pos-1;
    while (nl>0 && (L->chars[nl] == ' ' || L->chars[nl] == 0xff ||
	   L->chars[nl] == 0)) nl--;
    while (nl>0 && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl--;
    next_pos = skip_nulls(L, nl+1, 1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_forward_word(unsigned char c) {
  int nl;

  if (!check_write()) return;
  if (next_pos <= L->length) {
    nl = next_pos;
    while (nl <= L->length && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl++;
    while (nl <= L->length && (L->chars[nl] == ' ' || L->chars[nl] == 0xff ||
	   L->chars[nl] == 0)) nl++;
    next_pos = nl;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_end_of_word(unsigned char c) {
  int nl;

  if (!check_write()) return;
  if (next_pos <= L->length) {
    nl = next_pos + 1;
    while (nl <= L->length && (L->chars[nl] == ' ' || L->chars[nl] == 0xff ||
	   L->chars[nl] == 0)) nl++;
    while (nl <= L->length && L->chars[nl] != ' ' && L->chars[nl] != 0xff) nl++;
    next_pos = skip_nulls(L, nl-1, -1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_beginning_of_line(unsigned char c) {
  if (!check_write()) return;
  if (next_pos>1) {
    next_pos = skip_nulls(L, 1, 1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_end_of_line(unsigned char c) {
  if (!check_write()) return;
  if (next_pos <= L->length) {
    next_pos = L->length+1;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

void f_vi_insert_at_beginning_of_line(unsigned char c) {
  if (!check_write()) return;
  if (next_pos>1) {
    next_pos = skip_nulls(L, 1, 1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
  f_vi_insert_mode(c);
}

void f_vi_add_at_end_of_line(unsigned char c) {
  if (!check_write()) return;
  if (next_pos <= L->length) {
    next_pos = L->length+1;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
  f_vi_insert_mode(c);
}

void f_up(unsigned char c) {
  struct logical_line *ol;
  int pl, ppos, s;

  if (active_window == -1) {
    if (L->number > 1) {
      ol = L;
      pl = find_pline(U0, ol);
      ppos = get_plength(U0, &pl, next_pos);
      if (vi_prefix) {
	while (vi_prefix > 0 && L->number > 1) {
	  vi_prefix--;
	  L = L->prev;
	}
      } else {
	L = ol->prev;
      }
      next_pos = 1;
      s = skip_nulls(L, 1, 1);
      while (--ppos>0)
	if (s <= L->length) 
	  s = skip_nulls(L, ++s, 1);
	else
	  break;
      if (s > L->length) s = L->length;
      if (s == 0) s = 1;
      set_cursor(U0, L, 0, s);
      c_set_cursor(L->number, s);
      next_pos = s;
    }
  } else if (users[active_window]->first_visible_pline>1)
    scroll_to_pline(users[active_window], 
    		    users[active_window]->first_visible_pline-1);
}

void f_down(unsigned char c) {
  struct logical_line *ol;
  int pl, ppos, s;

  if (active_window == -1) {
    if (L->number < U0->last_lline->number) {
      ol = L;
      pl = find_pline(U0, ol);
      ppos = get_plength(U0, &pl, next_pos);
      if (vi_prefix) {
	while (vi_prefix > 0 && L->number < U0->last_lline->number) {
	  vi_prefix--;
	  L = L->next;
	}
      } else {
	L = ol->next;
      }
      next_pos = 1;
      s = skip_nulls(L, 1, 1);
      while (--ppos>0)
	if (s <= L->length) 
	  s = skip_nulls(L, ++s, 1);
	else
	  break;
      if (s > L->length) s = L->length;
      if (s == 0) s = 1;
      set_cursor(U0, L, 0, s);
      c_set_cursor(L->number, s);
      next_pos = s;
    }
  } else if (users[active_window]->first_visible_pline<
  	     users[active_window]->nplines-1)
    scroll_to_pline(users[active_window],
		    users[active_window]->first_visible_pline+1);
}

static void move_by_plines(struct logical_line *l, int pl, int n) {
  pl += n;
  if (pl < 1) 
    pl = 1;
  if (pl > U0->nplines)
    pl = U0->nplines;
  if (U0->plines[pl].line != l || 
      next_pos != skip_nulls(l, U0->plines[pl].offset, 1)) {
    L = U0->plines[pl].line;
    next_pos = skip_nulls(L, U0->plines[pl].offset, 1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  }
}

static void scroll_by_plines(int n) {
  int pl;
  pl = users[active_window]->first_visible_pline + n;
  if (pl < 1) 
    pl = 1;
  if (pl >= users[active_window]->nplines) 
    pl = users[active_window]->nplines - 1;
  scroll_to_pline(users[active_window], pl);
}

static void move_to_top(void) {
  L = U0->plines[U0->first_visible_pline].line;
  next_pos = skip_nulls(L, U0->plines[U0->first_visible_pline].offset, 1);
  set_cursor(U0, L, 0, next_pos);
  c_set_cursor(L->number, next_pos);
}

static void move_to_middle(void) {
  int pl;

  pl = U0->first_visible_pline + (U0->winsize / 2);
  if (pl > U0->nplines) pl = U0->nplines;
  L = U0->plines[pl].line;
  next_pos = skip_nulls(L, U0->plines[pl].offset, 1);
  set_cursor(U0, L, 0, next_pos);
  c_set_cursor(L->number, next_pos);
}

static void move_to_bottom(void) {
  int pl;

  pl = U0->first_visible_pline + U0->winsize - 1;
  if (pl > U0->nplines) pl = U0->nplines;
  L = U0->plines[pl].line;
  next_pos = skip_nulls(L, U0->plines[pl].offset, 1);
  set_cursor(U0, L, 0, next_pos);
  c_set_cursor(L->number, next_pos);
}

void f_top_of_screen(unsigned char c) {
  if (!check_write()) return;
  move_to_top();
}

void f_middle_of_screen(unsigned char c) {
  if (!check_write()) return;
  move_to_middle();
}

void f_bottom_of_screen(unsigned char c) {
  if (!check_write()) return;
  move_to_bottom();
}

static void goto_line_number(int n) {
  if (n < 1) n = 1;
  if (active_window == -1) {
    if (n > U0->last_lline->number)
      n = U0->last_lline->number;
    L = find_lline(U0, n, 0);
    next_pos = skip_nulls(L, 1, 1);
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  } else {
    if (n > users[active_window]->nplines - users[active_window]->winsize + 1)
      n = users[active_window]->nplines - users[active_window]->winsize + 1;
    if (n < 1) n = 1;
    scroll_to_pline(users[active_window], n);
  }
}

void f_top(unsigned char c) {
  goto_line_number(1);
}

void f_bottom(unsigned char c) {
  if (active_window == -1)
    goto_line_number(U0->last_lline->number);
  else
    goto_line_number(users[active_window]->nplines);
}

void f_vi_goto_line(unsigned char c) {
  if (vi_prefix == 0)
    f_bottom(c);
  else
    goto_line_number(vi_prefix);
}

void f_top_or_up_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl<U0->nplines && 
	   skip_nulls(L, U0->plines[pl].offset, 1) < next_pos &&
	   U0->plines[pl+1].line == L) pl++;
    if (U0->first_visible_pline == pl && 
	next_pos <= skip_nulls(L, U0->plines[pl].offset, 1)) {
      move_by_plines(L, pl, -U0->winsize);
    } else {
      move_to_top();
    }
  } else {
    scroll_by_plines(-users[active_window]->winsize);
  }
}

void f_up_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl < U0->nplines && 
	   skip_nulls(L, U0->plines[pl].offset, 1) < next_pos &&
	   U0->plines[pl+1].line == L) pl++;
    move_by_plines(L, pl, -U0->winsize);
  } else {
    scroll_by_plines(-users[active_window]->winsize);
  }
}

void f_up_half_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl<U0->nplines && 
	   skip_nulls(L, U0->plines[pl].offset, 1) < next_pos &&
	   U0->plines[pl+1].line == L) pl++;
    move_by_plines(L, pl, -(U0->winsize / 2));
  } else {
    scroll_by_plines(-(users[active_window]->winsize / 2));
  }
}

void f_bottom_or_down_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl<U0->nplines &&
	   skip_nulls(L, U0->plines[pl].offset, 1)<next_pos &&
	   U0->plines[pl].line == U0->plines[pl+1].line) pl++;
    if (U0->first_visible_pline+U0->winsize-1 == pl &&
	next_pos >= U0->plines[pl].offset && pl<U0->nplines) {
      move_by_plines(L, pl, U0->winsize);
    } else {
      move_to_bottom();
    }
  } else {
    scroll_by_plines(users[active_window]->winsize);
  }
}

void f_down_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl<U0->nplines &&
	   skip_nulls(L, U0->plines[pl].offset, 1)<next_pos &&
	   U0->plines[pl].line == U0->plines[pl+1].line) pl++;
    move_by_plines(L, pl, U0->winsize);
  } else {
    scroll_by_plines(users[active_window]->winsize);
  }
}

void f_down_half_page(unsigned char c) {
  int pl;

  if (active_window == -1) {
    pl = find_pline(U0, L);
    while (pl<U0->nplines &&
	   skip_nulls(L, U0->plines[pl].offset, 1)<next_pos &&
	   U0->plines[pl].line == U0->plines[pl+1].line) pl++;
    move_by_plines(L, pl, (U0->winsize) / 2);
  } else {
    scroll_by_plines(users[active_window]->winsize / 2);
  }
}

void f_redisplay(unsigned char c) {
  redraw();
}

void f_resynch(unsigned char c) {
  c_resync();
}

void f_next_window(unsigned char c) {
  active_window++;
  if (active_window >= active_buffers) active_window = -1;
}

void f_nop(unsigned char c) {
}

void f_beep(unsigned char c) {
  beep();
}

void f_vi_insert_mode(unsigned char c) {
  if (!check_write()) return;
  current_mode = &modes[VI_INS];
  vi_prefix = 0;
}

void f_vi_command_mode(unsigned char c) {
  current_mode = &modes[VI_CMD];
  vi_prefix = 0;
}

void f_emacs_mode(unsigned char c) {
  current_mode = &modes[EMACS];
  vi_prefix = 0;
}

void f_quit(unsigned char c) {
  cleanupexit(0, "");
}

void f_vi_escape(unsigned char c) {
  f_backward(c);
  f_vi_command_mode(c);
}

void f_vi_add(unsigned char c) {
  if (!check_write()) return;
  f_forward(c);
  f_vi_insert_mode(c);
}

void f_vi_open(unsigned char c) {
  if (!check_write()) return;
  f_new_line(c);
  f_vi_insert_mode(c);
}

void f_vi_Open(unsigned char c) {
  if (!check_write()) return;
  if (L->number > 1)
    L = L->prev;
  next_pos = skip_nulls(L, 1, 1);
  set_cursor(U0, L, 0, next_pos);
  c_set_cursor(L->number, next_pos);
  f_vi_insert_mode(c);
}

void f_vi_replace_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = f_insert_in_place;
}

void f_quote_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = f_self_insert;
}

static void find_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos + 1;
  while (n <= L->length && L->chars[n] != c) n++;
  if (n <= L->length && L->chars[n] == c) {
    next_pos = n;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  } else beep();
}

static void reverse_find_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos - 1;
  while (n >= 1 && L->chars[n] != c) n--;
  if (n >= 1 && L->chars[n] == c) {
    next_pos = n;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  } else beep();
}

void f_vi_find_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = find_char;
  last_find_function = find_char;
  reverse_find_function = reverse_find_char;
}

void f_vi_reverse_find_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = reverse_find_char;
  last_find_function = reverse_find_char;
  reverse_find_function = find_char;
}

static void till_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos + 1;
  while (n <= L->length && L->chars[n] != c) n++;
  if (n <= L->length && L->chars[n] == c) {
    n = skip_nulls(L, n-1, -1);
    if (n >= 1 && L->chars[n] != 0) {
      next_pos = n;
      set_cursor(U0, L, 0, next_pos);
      c_set_cursor(L->number, next_pos);
    } else beep();
  } else beep();
}

static void reverse_till_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos - 1;
  while (n >= 1 && L->chars[n] != c) n--;
  if (n >= 1 && L->chars[n] == c) {
    if (n <= L->length) n++;
    n = skip_nulls(L, n, 1);
    next_pos = n;
    set_cursor(U0, L, 0, next_pos);
    c_set_cursor(L->number, next_pos);
  } else beep();
}

void f_vi_till_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = till_char;
  last_find_function = till_char;
  reverse_find_function = reverse_till_char;
}

void f_vi_reverse_till_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = reverse_till_char;
  last_find_function = reverse_till_char;
  reverse_find_function = till_char;
}

void f_vi_repeat_find(unsigned char c) {
  if (!check_write()) return;
  if (last_find_function == NULL)
    beep();
  else
    (*last_find_function)(last_find_char);
}

void f_vi_reverse_repeat_find(unsigned char c) {
  if (!check_write()) return;
  if (reverse_find_function == NULL)
    beep();
  else
    (*reverse_find_function)(last_find_char);
}

static void delete_find_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos + 1;
  while (n <= L->length && L->chars[n] != c) n++;
  if (n <= L->length && L->chars[n] == c) {
    delete_till(L, n);
  } else beep();
}

static void delete_reverse_find_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos - 1;
  while (n >= 1 && L->chars[n] != c) n--;
  if (n >= 1 && L->chars[n] == c) {
    delete_till(L, n);
  } else beep();
}

static void delete_till_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos + 1;
  while (n <= L->length && L->chars[n] != c) n++;
  if (n <= L->length && L->chars[n] == c) {
    n = skip_nulls(L, n-1, -1);
    if (n >= 1 && L->chars[n] != 0) {
      delete_till(L, n);
    } else beep();
  } else beep();
}

static void delete_reverse_till_char(unsigned char c) {
  int n;

  if (c == 0) return;
  last_find_char = c;
  n = next_pos - 1;
  while (n >= 1 && L->chars[n] != c) n--;
  if (n >= 1 && L->chars[n] == c) {
    if (n <= L->length) n++;
    n = skip_nulls(L, n, 1);
    delete_till(L, n);
  } else beep();
}

void f_vi_delete_find_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = delete_find_char;
  last_find_function = find_char;
  reverse_find_function = reverse_find_char;
}

void f_vi_delete_reverse_find_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = delete_reverse_find_char;
  last_find_function = reverse_find_char;
  reverse_find_function = find_char;
}

void f_vi_delete_till_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = delete_till_char;
  last_find_function = till_char;
  reverse_find_function = reverse_till_char;
}

void f_vi_delete_reverse_till_char(unsigned char c) {
  if (!check_write()) return;
  force_next_func = delete_reverse_till_char;
  last_find_function = reverse_till_char;
  reverse_find_function = till_char;
}

void f_vi_flip_case(unsigned char c) {
  if (!check_write()) return;
  if (next_pos <= L->length) {
    c = L->chars[next_pos];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
	(c >= 192 && c <= 254)) {
      c ^= 0x20;
      write_char(U0, L, 0, next_pos, c);
      c_write_char(L->number, next_pos, c);
      next_pos = skip_nulls(L, ++next_pos, 1);
    } else if (next_pos <= L->length) {
      set_cursor(U0, L, 0, ++next_pos);
      c_set_cursor(L->number, next_pos);
    }
  }
}

void f_do_help(unsigned char c) {
  do_help();
}

void f_set_topic(unsigned char c) {
  entertopic();
}

void f_do_command(unsigned char c) {
  popup_command(current_mode == &modes[EMACS] ? "M-x " : ":");
}

void f_test_menu(unsigned char c) {
  /* xxx */
  test_menu();
}

void f_test_entry(unsigned char c) {
  /* xxx */
  test_entry();
}

void f_test_selection(unsigned char c) {
  /* xxx */
  test_selection();
}

