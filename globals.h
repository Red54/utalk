/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   globals.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>

#define UTALK_VERSION "1.0.1 beta"

extern int errno;

extern struct user *users[300];
extern int eight_bit_clean;
extern int cols, lines;
extern int beep_on;
extern int cannotrun;
extern int connected;
extern int wordwrap;
extern int in_main_loop;
extern int meta_esc;

#define TTYREAD 0
#define TTYWRITE 1

extern volatile int screen_inited;
extern volatile int mustdie;
extern volatile int winch, newcols, newlines;
extern volatile int mustredisplay;

#endif

