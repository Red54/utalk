/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima  <roger.espel.llima@pobox.com>

   globals.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

int cols, lines;

#ifdef SEVEN_BIT
int eight_bit_clean = 0;
#else
int eight_bit_clean = 1;
#endif

int beep_on = 1;
int connected = 0;
int cannotrun = 0;	/* if the screen is too small to run */
int wordwrap = 1;
int in_main_loop = 0;

#ifdef SEVEN_BIT
int meta_esc = 1;	/* if the meta key gets mapped to ESC-key 
			   in emacs mode */
#else
int meta_esc = 0;
#endif

struct user *users[300];

volatile int screen_inited = 0;

volatile int mustdie = 0;   /* set to 1 by SIGINT handler: shutdown connection
			   and quit */
volatile int winch = 0, newcols, newlines; /* set by SIGWINCH handler */
volatile int mustredisplay = 0;  /* set by SIGCONT handler */

