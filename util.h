/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   util.h

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef UTIL_H
#define UTIL_H

extern void barf(char *m);
extern void cleanupexit(int n, char *m);
extern void *mymalloc(int n);
extern void *myrealloc(void *p, int n);
extern unsigned char *getword(unsigned char **s);

#endif

