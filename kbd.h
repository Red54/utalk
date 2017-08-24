/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   kbd.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef KBD_H
#define KBD_H

#include "struct.h"

extern struct mode *current_mode, modes[N_MODES];
extern ftype force_next_func;
extern int vi_prefix;

extern void kbd_defaults(void);
extern void keyboard(unsigned char c);
extern char *new_binding(unsigned char *s, int insmode);
extern struct mode *new_mode(unsigned char *s);

#endif

