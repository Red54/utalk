/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   termio.h

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef TERMIO_H
#define TERMIO_H

#define SET_RAW 0
#define SET_COOKED 1

extern void get_screensize(void);
extern void init_termio(void);
extern void settty(int how, int drain);

#endif

