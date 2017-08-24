/* utalk, a UDP-based "talk" replacement, using srdp
   by Roger Espel Llima <roger.espel.llima@pobox.com>

   rc.h

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef RC_H
#define RC_H

struct toggle {
  char *name; 
  int *var;
};

char *resolve_alias(char *uh);

void read_rc(void);
char *do_rc_line(unsigned char *);

#endif

