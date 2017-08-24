/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   utalk.c

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include "srdp.h"
#include "globals.h"
#include "termcap.h"
#include "termio.h"
#include "util.h"
#include "signal.h"
#include "screen.h"
#include "kbd.h"
#include "comm.h"
#include "rc.h"
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_SGTTY
#include <sgtty.h>
#else
#include <termios.h>
#endif

#ifdef _AIX
#include <sys/select.h>
#endif

#ifdef TIOCGWINSZ
struct winsize wsz;
#endif

#ifdef USE_SGTTY
struct sgttyb term, term0;
struct tchars tch, tch0;
struct ltchars lch, lch0;
#else
struct termios term, term0;
#endif

int cols, lines;

static void use(char *m) {
  if (m)
    fprintf(stderr, "utalk: %s;  try utalk --help for help\n", m);
  else {
    fprintf(stderr,
"Use:         utalk [options]  user[@host][#tty]\n"
" or:         utalk [options] !port@host\n"
" or:         utalk [options] -s port\n"
" or:         utalk [options] -c host port\n\n"
"Options:     -s, --server          --  raw server connection\n"
"             -c, --client          --  raw client connection\n"
"             -a, --announce-only   --  send port number in announcement\n"
"             -7, --seven-bit       --  run in 7-bit mode\n"
"             -8, --eight-bit       --  run in 8-bit mode\n"
	   );
  }
  exit(1);
}

int main(int argc, char *argv[]) {
  int port = 0, raw = 0, minus = -1;
  char *host = NULL, *uh = NULL;

  srdp_u32 u32;
  srdp_u16 u16;
  srdp_u8 u8;

  u32 = minus;
  if (sizeof(srdp_u32) != 4 || u32 <= 0)
    barf("srdp_u32 isn't an unsigned 32-bit type, please edit srdpdata.h and recompile");
  u16 = minus;
  if (sizeof(srdp_u16) != 2 || u16 <= 0)
    barf("srdp_u16 isn't an unsigned 16-bit type, please edit srdpdata.h and recompile");
  u8 = minus;
  if (sizeof(srdp_u8) != 1 || u8 <= 0)
    barf("srdp_u8 isn't an unsigned 8-bit type, please edit srdpdata.h and recompile");

  read_rc();

  while (argc>1 && argv[1][0] == '-') {
    if (strcmp(argv[1], "-7") == 0 || strcmp(argv[1], "--seven-bit") == 0) {
      eight_bit_clean = 0;
      argv++;
      argc--;
    } else if (strcmp(argv[1], "-8") == 0 || 
	       strcmp(argv[1], "--eight-bit") == 0) {
      eight_bit_clean = 1;
      argv++;
      argc--;
    } else if (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--server") == 0) {
      if (argc <= 2)
	use("missing argument for -s");
      raw = 1;
      port = atoi(argv[2]);
      argv += 2;
      argc -= 2;
    } else if (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--client") == 0) {
      raw = 2;
      if (argc <= 3)
	use("missing argument for -c");
      host = argv[2];
      port = atoi(argv[3]);
      argv += 3;
      argc -= 3;
    } else if (strcmp(argv[1], "-a") == 0 || 
	       strcmp(argv[1], "--announce-only") == 0) {
      raw = -1;
      argv++;
      argc--;
    } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      use(NULL);
    } else use("unrecognized option");
  }
  if (argc < 2 && !raw)
    use(NULL);

  allsignals();
  init_termcap();
  get_screensize(); 
  init_termio();
  alloc_termcap_screen();
  clearscreen();
  init_screen();

  if (!raw && argv[1][0] == '!' && argv[1][1] >= '0' && argv[1][1] <= '9') {
    raw = 2;
    port = atoi(&argv[1][1]);
    host = argv[1];
    while (*host && *host != '@')
      host++;
    if (*host == '@')
      host++;
  } else if (raw <= 0) {
    uh = argv[1];
    if (uh[0] == '!')
      uh++;
  }

  init_comm(raw, host, port, uh);

  gotoxy(1, 2);
  main_loop();

  cleanupexit(1, "");
  exit(0);
}

