/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   menu.h

   Started: 1 Dec 96 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef MENU_H
#define MENU_H

#define MENU_TEXT 1	/* print text & wait for Enter */
#define MENU_CHOOSE 2	/* print text & list, pick one */
#define MENU_ENTRY 3	/* print text, input line */

#define HELPFILE LIBDIR "/utalk.help"

struct menu;

typedef void (*menu_callback)(struct menu *);
typedef void (*text_callback)(char *);

struct menu {
  int type;
  menu_callback callback;	/* gets called when done w/ the menu; is
  				   supposed to free the menu storage if it
				   was malloced */
  int preempted;	/* set to 1 for the callback to see, if preempted */
  char **text;		/* text to show, terminated by a null line pointer,
			   for types MENU_TEXT, MENU_CHOOSE, MENU_YESNO;
			   for MENU_CHOOSE, the strings must be writable
			   and be of the form " letter -- text " */
  int *jumplines;	/* line numbers to jump to with jump keys (or NULL)*/
  char *jumpkeys;	/* jump keys (or NULL) */
  char *entrytxt;	/* text to show (one line), for MENU_ENTRY */
  char entry[256];	/* gets filled; can be initialized with a prompt;
			   set promptlen to the len of the prompt and it 
			   won't be erased */
  text_callback txtcallback;	/* gets called when done entering text,
  				   with the text w/o the prompt, by the
				   default text-mode callback; -1 means
				   no selection */
  int promptlen;
  int arrowat;		/* line with the arrow, for selection menus;
  			   must be initialized, and read by the callback 
			   when done */
  int arrowmin, arrowmax;	/* must be initialized too... */

  			/* the rest is for internal use only */
  char *buf;		/* where to copy the entered string, used by the
  			   default text-mode callback */
  int len;		/* length of the entry, including prompt */
  int showfrom;		/* show starting from line n (start at 0) */
  int showfromcol;	/* show starting from col n (start at 0) */
  int nlines, maxcols;	/* size of text to show */
  int wlines;		/* text lines in menu window */
  int wcols;		/* text cols in menu window */
  int more;		/* if we're now showing the bottom */
 /*  xxx ... */

};

extern struct menu *the_menu;
extern int menu_top, menu_bottom, menu_left, menu_right;  /* valid only if
							the_menu != NULL */


extern void m_putscreen(char *s, int n, char attr);
extern void m_cleareol(void);
extern void m_gotoxy(int x, int y);
extern void m_scrollup(int y1, int y2);
extern void m_scrolldown(int y1, int y2);
extern void m_backspace(void);

extern void menu_keyboard(unsigned char c);
extern void make_text_menu(char **s, int *jumplines, char *jumpkeys, int freeit);
extern void input_text(char *text, char *prompt, char *buf, text_callback c);
extern void redraw_menu(void);
extern void do_help(void);
extern void entertopic(void);
extern void popup_command(char *prompt);

#endif

