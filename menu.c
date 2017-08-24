/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   menu.c

   Started: 1 Dec 96 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "termcap.h"
#include "util.h"
#include "screen.h"
#include "rc.h"
#include "menu.h"
#include "comm.h"

struct menu *the_menu = NULL;
int menu_top, menu_bottom, menu_left, menu_right;	/* valid only if
							   the_menu != NULL */

static int last_esc = 0;

static char **help_txt = NULL;
static char *help_jumpkeys = NULL;
static int *help_jumplines = NULL;

static char free_keys[] = "acdefghilmnopqrstuvwxyzCDEFGHIJKLMNOPQRSTUVWXZY0123456789";

/* puts the chars on the screen, unless there's a menu */
void m_putscreen(char *s, int n, char attr) {
  if (the_menu == NULL)
    putscreen(s, n, attr);
}

void m_cleareol(void) {
  if (the_menu == NULL)
    cleareol();
}

void m_gotoxy(int x, int y) {
  if (the_menu == NULL)
    gotoxy(x, y);
}

void m_scrollup(int y1, int y2) {
  if (the_menu == NULL)
    scrollup(y1, y2);
}

void m_scrolldown(int y1, int y2) {
  if (the_menu == NULL)
    scrolldown(y1, y2);
}

void m_backspace(void) {
  if (the_menu == NULL)
    backspace();
}

static void remove_menu(void) {
  struct menu *tmp;

  if (the_menu) {
    tmp = the_menu;
    the_menu = NULL;
    pl_redraw_all();
    if (tmp->callback)
      (*tmp->callback)(tmp);
  }
}

static void draw_bottom(int more) {
  int i;

  gotoxy(menu_left, menu_bottom);
  putscreen("`", 1, 0);
  i = menu_left + 1;
  if (more) {
    if (menu_right - i >= 8) {
      putscreen("-[more]-", 8, 0);
      i += 8;
    } else if (menu_right - i >= 3) {
      putscreen("-m-", 3, 0);
      i += 3;
    }
  }
  for (; i<menu_right; i++)
    putscreen("-", 1, 0);
  putscreen("'", 1, 0);
}

/* draws the frame, optionnally with the "[more]" at the bottom left */
static void draw_frame(int more) {
  int i;

  gotoxy(menu_left, menu_top);
  putscreen(".", 1, 0);
  for (i=menu_left+1; i<menu_right; i++)
    putscreen("-", 1, 0);
  putscreen(".", 1, 0);
  for (i=menu_top+1; i<menu_bottom; i++) {
    gotoxy(menu_left, i);
    putscreen("|", 1, 0);
    gotoxy(menu_right, i);
    putscreen("|", 1, 0);
  }
  draw_bottom(more);
}

/* draws the text menu in place */
static void draw_text_menu(void) {
  int y, len, tofill;
  char **s;

  y = menu_top + 1;
  s = the_menu->text + (the_menu->showfrom);

  for (; y < menu_bottom; ++y, (*s && ++s)) {
    gotoxy(menu_left + 1, y);
    tofill = menu_right - menu_left - 1;
    if (*s) {
      len = strlen(*s) - the_menu->showfromcol;
      if (len <= 0) {
      } else if (len <= tofill) {
	putscreen(*s + the_menu->showfromcol, len, 0);
	tofill -= len;
      } else if (tofill >= 5) {
	putscreen(*s + the_menu->showfromcol, tofill - 4, 0);
	putscreen(" ...", 4, 0);
	tofill = 0;
      }
    }
    for (; tofill; tofill--)
      putscreen(" ", 1, 0);
  }
}

/* draws the entry text in place */
static void draw_entry_text(void) {
  int y, tofill, len;
  char *s;

  y = menu_top + 1;
  gotoxy(menu_left + 1, y);
  s = the_menu->entrytxt;
  tofill = menu_right - menu_left - 1;
  len = strlen(s);
  if (len <= tofill) {
    putscreen(s, len, 0);
    tofill -= len;
  } else if (tofill >= 5) {
    putscreen(s, tofill - 4, 0);
    putscreen(" ...", 4, 0);
    tofill = 0;
  }
  for (; tofill; tofill--)
    putscreen(" ", 1, 0);
  gotoxy(menu_left + 1, y + 1);
  tofill = menu_right - menu_left - 1;
  s = the_menu->entry;
  len = the_menu->len - the_menu->showfromcol;
  if (len <= 0) {
  } else if (len <= tofill) {
    putscreen(s + the_menu->showfromcol, len, 0);
    tofill -= len;
  } else {
    putscreen(s + the_menu->showfromcol, tofill, 0);
    tofill = 0;
  }
  for (; tofill; tofill--)
    putscreen(" ", 1, 0);
  gotoxy(menu_left + 1 + the_menu->len - the_menu->showfromcol, y + 1);
}

/* draws the given menu, clearing any prev. one and setting the_menu to it */
static void draw_menu(struct menu *m) {
  char **s;

  if (cannotrun)
    return;

  if (the_menu != NULL) {
    the_menu->preempted = 1;
    remove_menu();
  }
  
  the_menu = m;
  m->preempted = 0;

  last_esc = 0;

  switch(the_menu->type) {
    case MENU_TEXT:
    case MENU_CHOOSE:
      m->showfrom = 0;
      m->showfromcol = 0;
      s = m->text;
      m->nlines = 0;
      m->maxcols = 0;
      for (; *s; ++s) {
	int len;

	m->nlines++;
	len = strlen(*s);
	if (len > m->maxcols)
	  m->maxcols = len;
      }
      if (m->nlines + 4 > lines)
      	menu_top = 2;
      else
	menu_top = 2 + ((lines - (m->nlines + 4)) / 3);
      menu_bottom = menu_top + m->nlines + 1;
      m->more = 0;
      if (menu_bottom >= lines) {
	menu_bottom = lines - 1;
	m->more = 1;
      }
      if (m->maxcols + 4 > cols)
	menu_left = 2;
      else
	menu_left = 2 + ((cols - (m->maxcols + 4)) / 2);
      menu_right = menu_left + m->maxcols + 1;
      if (menu_right >= cols)
	menu_right = cols - 1;
      m->wlines = menu_bottom - menu_top - 1;
      m->wcols = menu_right - menu_left - 1;

      if (m->type == MENU_CHOOSE)
	m->text[m->arrowat][3] = '<';

      draw_frame(m->more);
      draw_text_menu();
      break;

    case MENU_ENTRY:
      m->showfromcol = 0;
      m->nlines = 2;
      m->wlines = 2;
      menu_top = 2 + ((lines - 6) / 3);
      menu_bottom = menu_top + 3;
      if (cols < 64)
	menu_left = 2;
      else
	menu_left = 2 + ((cols - 64) / 2);
      menu_right = menu_left + 60 + 1;
      if (menu_right >= cols)
	menu_right = cols - 1;
      m->wcols = menu_right - menu_left - 1;

      m->len = strlen(m->entry);
      m->showfromcol = m->len - m->wcols + 8;
      if (m->showfromcol < 0)
	m->showfromcol = 0;
      draw_frame(0);
      draw_entry_text();
      break;
  }
}

void redraw_menu(void) {
  struct menu *m;

  m = the_menu;
  if (m) {
    the_menu = NULL;
    draw_menu(m);
  }
}

/* dispatch a key in menu mode */
void menu_keyboard(unsigned char c) {
  struct menu *m = the_menu;
  char *k;
  int toline;

  switch (m->type) {
    case MENU_TEXT:
      {
	int sf = m->showfrom, mr = m->more;

	if (c == 0x1b) {
	  if (last_esc)
	    remove_menu();
	  else 
	    last_esc = 1;
	  return;
	}
	last_esc = 0;

	if (m->jumpkeys && (k = strchr(m->jumpkeys, c))) {
	  toline = m->jumplines[k - m->jumpkeys];
	  if (toline >= 0 && toline < m->nlines) {
	    m->showfrom = toline;
	    draw_text_menu();
	  }
	} else {
	  switch(c) {
	    case 7:
	    case 8:
	    case 0x7f:
	    case 10:
	    case 13:
	      remove_menu();
	      return;

	    case 14:
	    case 'B':
	    case 'j':
	      if (m->more) {
		m->showfrom++;
		draw_text_menu();
	      }
	      break;
	    
	    case 'l':
	    case 'C':
	    case 6:
	      if (m->showfromcol + m->wcols < m->maxcols) {
		m->showfromcol++;
		draw_text_menu();
	      }
	      break;
	    
	    case 'h':
	    case 'D':
	    case 2:
	      if (m->showfromcol > 0) {
		m->showfromcol--;
		draw_text_menu();
	      }
	      break;
	    
	    case 9:
	      if (m->showfromcol + m->wcols < m->maxcols) {
		m->showfromcol += 8;
		if (m->showfromcol + m->wcols > m->maxcols)
		  m->showfromcol = m->maxcols - m->wcols;
		draw_text_menu();
	      }
	      break;

	    case 1:
	    case '0':
	    case '^':
	      if (m->showfromcol > 0) {
		m->showfromcol = 0;
		draw_text_menu();
	      }
	      break;

	    case 5:
	    case '$':
	      if (m->showfromcol + m->wcols < m->maxcols) {
		m->showfromcol = m->maxcols - m->wcols;
		draw_text_menu();
	      }
	      break;
   
	    case 'k':
	    case 16:
	    case 'A':
	      if (m->showfrom > 0) {
		m->showfrom--;
		draw_text_menu();
	      }
	      break;
	    
	    case ' ':
	    case 'f':
	    case '+':
	      if (m->more) {
		m->showfrom += m->wlines;
		if (m->showfrom + m->wlines > m->nlines)
		  m->showfrom = m->nlines - m->wlines;
		if (m->showfrom != sf)
		  draw_text_menu();
	      } else if (c == ' ') {
		remove_menu();
		return;
	      }
	      break;

	    case 4:
	      if (m->more) {
		m->showfrom += m->wlines / 2;
		if (m->showfrom + m->wlines > m->nlines)
		  m->showfrom = m->nlines - m->wlines;
		if (m->showfrom != sf)
		  draw_text_menu();
	      }
	      break;
	    
	    case '-':
	    case 'b':
	      m->showfrom -= m->wlines;
	      if (m->showfrom < 0)
		m->showfrom = 0;
	      if (m->showfrom != sf)
		draw_text_menu();
	      break;
	    
	    case 21:
	      m->showfrom -= m->wlines / 2;
	      if (m->showfrom < 0)
		m->showfrom = 0;
	      if (m->showfrom != sf)
		draw_text_menu();
	      break;

	  }
	}
	if (sf != m->showfrom) {
	  m->more = (m->showfrom + m->wlines < m->nlines);
	  if (mr != m->more)
	    draw_bottom(m->more);
	}
      }
      break;

    case MENU_CHOOSE:
      {
	int nsf = m->showfrom, naw = m->arrowat, mr = m->more;
	char **s;

	if (c == 0x1b) {
	  if (last_esc) {
	    m->arrowat = -1;
	    remove_menu();
	  } else last_esc = 1;
	  return;
	}
	last_esc = 0;

	for (s=m->text; *s; s++) {
	  if (c == s[0][1] && s[0][2] == ' ' && s[0][4] == '-') {
	    m->arrowat = s - m->text;
	    remove_menu();
	    return;
	  }
	}

	switch(c) {
	  case 7:
	  case 8:
	  case 0x7f:
	    m->arrowat = -1;
	  case 10:
	  case 13:
	    remove_menu();
	    return;

	  case 14:
	  case 'B':
	  case 'j':
	    if (m->arrowat+1 < m->nlines)
	      naw++;
	    break;

	  case 'k':
	  case 16:
	  case 'A':
	    if (m->arrowat > 0)
	      naw--;
	    if (naw <= m->arrowmin)
	      nsf--;
	    break;
	  
	  case ' ':
	  case '+':
	    if (m->arrowat+1 < m->nlines) {
	      naw += m->wlines;
	      nsf += m->wlines;
	    }
	    break;

	  case 4:
	    if (m->arrowat+1 < m->nlines) {
	      naw += m->wlines / 2;
	      nsf += m->wlines / 2;
	    }
	    break;
	  
	  case '-':
	  case 'b':
	    if (m->arrowat > 0) {
	      naw -= m->wlines;
	      nsf -= m->wlines;
	    }
	    break;
	  
	  case 21:
	    if (m->arrowat > 0) {
	      naw -= m->wlines / 2;
	      nsf -= m->wlines / 2;
	    }
	    break;
	}

	if (naw > nsf + m->wlines - 1)
	  nsf = naw - m->wlines + 1;
	if (naw < nsf)
	  nsf = naw;
	
	if (naw < m->arrowmin)
	  naw = m->arrowmin;
	else if (naw > m->arrowmax)
	  naw = m->arrowmax;

	if (nsf > m->nlines - m->wlines)
	  nsf = m->nlines - m->wlines;
	if (nsf < 0)
	  nsf = 0;
	
	if (naw != m->arrowat) {
	  m->text[m->arrowat][3] = '-';
	  m->text[naw][3] = '<';
	}
	
	if (nsf != m->showfrom) {
	  m->showfrom = nsf;
	  m->arrowat = naw;
	  draw_text_menu();
	  m->more = (m->showfrom + m->wlines < m->nlines);
	  if (mr != m->more)
	    draw_bottom(m->more);
	} else {
	  if (m->arrowat >= m->showfrom && 
	      m->arrowat < m->showfrom + m->wlines) {
	    gotoxy(menu_left + 4, menu_top + 1 + m->arrowat - m->showfrom);
	    putscreen("-", 1, 0);
	  }
	  if (naw >= m->showfrom && naw < m->showfrom + m->wlines) {
	    gotoxy(menu_left + 4, menu_top + 1 + naw - m->showfrom);
	    putscreen("<", 1, 0);
	  }
	  m->arrowat = naw;
	}
      }
      break;

    case MENU_ENTRY:
      {
	if (c == 0x1b) {
	  if (last_esc) {
	    m->entry[0] = m->entry[m->promptlen] = 0;
	    remove_menu();
	    return;
	  } else last_esc = 1;
	  return;
	}
	last_esc = 0;

	switch(c) {
	  case 13:
	  case 10:
	    m->entry[m->len] = 0;
	    remove_menu();
	    return;
	    
	  case 21:
	    if (m->len > m->promptlen) {
	      m->len = m->promptlen;
	      m->showfromcol = 0;
	      draw_entry_text();
	    }
	    break;
	  
	  case 8:
	  case 0x7f:
	    if (m->len > m->promptlen) {
	      if (m->len - m->showfromcol <= 4) {
		m->showfromcol -= m->wcols - 8;
		if (m->showfromcol < 0)
		  m->showfromcol = 0;
		m->len--;
		draw_entry_text();
	      } else {
		gotoxy(menu_left + m->len - m->showfromcol, menu_top+2);
		putscreen(" ", 1, 0);
		gotoxy(menu_left + m->len - m->showfromcol, menu_top+2);
		m->len--;
	      }
	    }
	    break;
	  
	  default:
	    if (c >= ' ' && (c < 0x7f || (eight_bit_clean && c > 0x7f))) {
	      if (m->len < 255) {
		if (m->len - m->showfromcol >= m->wcols - 4) {
		  m->entry[m->len++] = c;
		  m->showfromcol += m->wcols - 8;
		  draw_entry_text();
		} else {
		  m->entry[m->len++] = c;
		  gotoxy(menu_left + m->len - m->showfromcol, menu_top+2);
		  putscreen(&c, 1, 0);
		}
	      }
	    }
	    break;
	}
      }
      break;
    /* xxx */
  }
}

static void empty_callback(struct menu *m) {
  free(m);
}

static void freeing_callback(struct menu *m) {
  char **s;

  for (s=m->text; *s; s++)
    free (*s);
  free(m->text);
  free(m);
}

/* freeit = 1 if the callback needs to free everything in s[] and s itself */
void make_text_menu(char **s, int *jumplines, char *jumpkeys, int freeit) {
  struct menu *m;

  m = mymalloc(sizeof(struct menu));
  m->type = MENU_TEXT;
  m->callback = (freeit ? freeing_callback : empty_callback);
  m->text = s;
  m->jumplines = jumplines;
  m->jumpkeys = jumpkeys;
  draw_menu(m);
}

static void default_txt_callback(struct menu *m) {
  if (m->preempted)
    (*m->txtcallback)(NULL);
  else {
    m->entry[m->len] = 0;
    strcpy(m->buf, m->entry + m->promptlen);
    (*m->txtcallback)(m->buf);
  }
  free(m);
}

/* prints text on a line, puts prompt and the initial text (from buf) on
   the next, and lets the user edit it; when done, calls the callback
   (passes it a NULL if preempted, otherwise buf itself.
   prompt must be 8 char at most (not checked); buffer must be 256 */
void input_text(char *text, char *prompt, char *buf, text_callback c) {
  struct menu *m;

  m = mymalloc(sizeof(struct menu));
  m->type = MENU_ENTRY;
  m->callback = default_txt_callback;
  m->entrytxt = text;
  if (strlen(buf) + strlen(prompt) > 256)
    buf[255 - strlen(prompt)] = 0;
  m->promptlen = strlen(prompt);
  m->buf = buf;
  strcpy(m->entry, prompt);
  strcat(m->entry, buf);
  m->txtcallback = c;
  draw_menu(m);
}

static void help_error(char *s) {
  help_txt = mymalloc(3 * sizeof(char *));
  help_txt[0] = "Help for utalk not available:";
  help_txt[1] = mymalloc(256);
  sprintf(help_txt[1], "help file %s %s.", HELPFILE, s);
  help_txt[2] = NULL;
  make_text_menu(help_txt, NULL, NULL, 0);
}

void do_help(void) {
  int hlines, jumps = 0, line = 0;
  char buf[256];
  char *cn, *r;
  FILE *f;

  if (help_txt == NULL) {
    f = fopen(HELPFILE, "r");
    if (f == NULL) {
      help_error("not found");
      return;
    }
    r = fgets(buf, 254, f);
    if (r == NULL) {
      help_error("corrupted");
      return;
    }
    hlines = atoi(buf);
    if (hlines < 2 || hlines > 5000) {
      help_error("corrupted");
      return;
    }
    help_txt = mymalloc((1 + hlines) * sizeof (char *));
    help_jumpkeys = mymalloc(128);
    help_jumplines = mymalloc(128 * sizeof(int));
    while (fgets(buf, 254, f) != NULL && --hlines > 0) {
      if ((cn = strchr(buf, 10)))
	*cn = 0;
      if ((cn = strchr(buf, 13)))
	*cn = 0;
      help_txt[line] = mymalloc(4 + strlen(buf));
      help_txt[line][0] = ' ';
      if (buf[0] == '@' && buf[2] == '@') {
	help_jumpkeys[jumps] = buf[1];
	help_jumplines[jumps++] = line;
	strcpy(&help_txt[line][1], buf+3);
      } else strcpy(&help_txt[line][1], buf);
      strcat(help_txt[line], " ");
      line++;
    }
    fclose(f);
    help_txt[line] = NULL;
    help_jumpkeys[jumps] = 0;
    help_jumplines[jumps] = 0;
  }
  make_text_menu(help_txt, help_jumplines, help_jumpkeys, 0);
}

static void command_callback(char *s) {
  char *r;
  char **txt;

  if (s && *s) {
    r = do_rc_line(s);
    if (r) {
      txt = mymalloc(3 * sizeof(char *));
      txt[0] = "Error:";
      txt[1] = mymalloc(2 + strlen(r));
      strcpy(txt[1], r);
      txt[2] = NULL;
      make_text_menu(txt, NULL, NULL, 1);
    }
  }
}

void popup_command(char *prompt) {
  static char buf[256];

  buf[0] = 0;
  input_text("Enter utalk command:", prompt, buf, command_callback);
}

void topic_callback(char *s) {
  if (s && *s) {
    settopic(s);
    c_send_topic(s);
  }
}

void entertopic(void) {
  static char buf[256];

  buf[0] = 0;
  input_text("New topic:", "", buf, topic_callback);
}


/* xxx */

void test_sel_callback(struct menu *m) {
  char **s;
  static char **txt;
  int n = m->arrowat;

  for (s=m->text; *s; s++)
    free (*s);
  free(m->text);
  free(m);
  if (n >= 0) {
    txt = mymalloc(3 * sizeof(char *));
    txt[0] = mymalloc(40);
    sprintf(txt[0], "Selection was: %d", n - 1);
    txt[1] = NULL;
    make_text_menu(txt, NULL, NULL, 1);
  }
}

void test_selection(void) {
  struct menu *m;
  static char **txt;
  int i;

  txt = mymalloc(51 * sizeof(char *));
  for (i=0; i<50; i++)
    txt[i] = mymalloc(80);
  strcpy(txt[0], "     YaY Menu!    ");
  strcpy(txt[1], "    ");
  for (i=2; i<50; i++)
    sprintf(txt[i], " %c -- blah blah %d ", free_keys[i-2], i-1);
  txt[50] = NULL;
  m = mymalloc(sizeof(struct menu));
  m->type = MENU_CHOOSE;
  m->callback = test_sel_callback;
  m->text = txt;
  m->arrowat = 2;
  m->arrowmin = 2;
  m->arrowmax = 49;
  draw_menu(m);
}

void test_txt_callback(char *s) {
  static char **txt;

  txt = mymalloc(3 * sizeof(char *));
  txt[0] = mymalloc(20);
  strcpy(txt[0], "Entered text:");
  txt[1] = mymalloc(256);
  strcpy(txt[1], "\"");
  strcat(txt[1], s);
  strcat(txt[1], "\"");
  txt[2] = NULL;
  make_text_menu(txt, NULL, NULL, 1);
}

void test_entry(void) {
  static char buf[256];

  strcpy(buf, "some default");
  input_text("Enter something:", "> ", buf, test_txt_callback);
}

/* xxx */
void test_menu(void) {
  static char *txt[] = {
    "                        GNU GENERAL PUBLIC LICENSE",
    "",
    "                           Version 2, June 1991",
    "",
    "   Copyright (C) 1989, 1991 Free Software Foundation, Inc. 675 Mass Ave,",
    "   Cambridge, MA 02139, USA",
    "",
    "   Everyone is permitted to copy and distribute verbatim copies of this",
    "   license document, but changing it is not allowed.",
    "",
    "                                 Preamble",
    "",
    "The licenses for most software are designed to take away your freedom to",
    "share and change it. By contrast, the GNU General Public License is",
    "intended to guarantee your freedom to share and change free software--to",
    "make sure the software is free for all its users. This General Public",
    "License applies to most of the Free Software Foundation's software and",
    "to any other program whose authors commit to using it. (Some other Free",
    "Software Foundation software is covered by the GNU Library General",
    "Public License instead.) You can apply it to your programs, too.",
    "",
    "When we speak of free software, we are referring to freedom, not price.",
    "Our General Public Licenses are designed to make sure that you have the",
    "freedom to distribute copies of free software (and charge for this",
    "service if you wish), that you receive source code or can get it if you",
    "want it, that you can change the software or use pieces of it in new",
    "free programs; and that you know you can do these things.",
    "",
    "To protect your rights, we need to make restrictions that forbid anyone",
    "to deny you these rights or to ask you to surrender the rights. These",
    "restrictions translate to certain responsibilities for you if you",
    "distribute copies of the software, or if you modify it.",
    "",
    "For example, if you distribute copies of such a program, whether gratis",
    "or for a fee, you must give the recipients all the rights that you have.",
    "You must make sure that they, too, receive or can get the source code.",
    "And you must show them these terms so they know their rights.",
    "",
    "We protect your rights with two steps: (1) copyright the software, and",
    "(2) offer you this license which gives you legal permission to copy,",
    "distribute and/or modify the software.",
    "",
    "Also, for each author's protection and ours, we want to make certain",
    "that everyone understands that there is no warranty for this free",
    "software. If the software is modified by someone else and passed on, we",
    "want its recipients to know that what they have is not the original, so",
    "that any problems introduced by others will not reflect on the original",
    "authors' reputations.",
    "",
    "Finally, any free program is threatened constantly by software patents.",
    "We wish to avoid the danger that redistributors of a free program will",
    "individually obtain patent licenses, in effect making the program",
    "proprietary. To prevent this, we have made it clear that any patent must",
    "be licensed for everyone's free use or not licensed at all.",
    NULL
  };

  make_text_menu(txt, NULL, NULL, 0);
}

