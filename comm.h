/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   comm.h

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef COMM_H
#define COMM_H

#include <sys/types.h>
#include <netinet/in.h>
#include "srdp.h"

#define READBUFSIZE 2084
#define OUTBUFSIZE 2048
#define SENDAFTER 440           /* send a packet when buffered data exceeds..*/
#define MAXBUFDELAY 250000      /* max delay in microseconds for buffering */

#define UTALK_DATA 2
#define UTALK_TOPIC 4
#define UTALK_REVISION 1

#define ODAEMON 0
#define NDAEMON 1
#define BOTH 2

#define NAME_SIZE 9
#define TTY_SIZE 16

typedef struct {
  srdp_u16 sin_family;
  srdp_u16 sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
} BSD42_SOCK;

struct mesg {
  int daemon; 		/* type of daemon */
  char type;		/* type of message */
  char local_name[NAME_SIZE];
  char remote_name[NAME_SIZE];
  char remote_tty[TTY_SIZE];
/* struct sockaddr_in addr;
  struct sockaddr_in ctl_addr; xxx these get filled automatically */
  srdp_u32 id;
};

struct answer {
  int daemon;		/* type of daemon */
  char type;
  char answer;
  srdp_u32 id;
  BSD42_SOCK addr;
};

/* Control Message structure for earlier than BSD4.2 */
struct old_msg {
  char type;
  char l_name[NAME_SIZE];
  char r_name[NAME_SIZE];
  char filler;
  srdp_u32 id_num;
  srdp_u32 pid;
  char r_tty[TTY_SIZE];
  BSD42_SOCK addr;
  BSD42_SOCK ctl_addr;
};

/* Control Response structure for earlier than BSD4.2 */
struct old_reply {
  char type;
  char answer;
  srdp_u16 filler;
  srdp_u32 id_num;
  BSD42_SOCK addr;
};

/* Control Message structure for BSD4.2 */
struct new_msg {
  unsigned char vers;
  char type;
  char filler[2];
  srdp_u32 id_num;
  BSD42_SOCK addr;
  BSD42_SOCK ctl_addr;
  srdp_u32 pid;
  char l_name[NAME_SIZE];
  char l_name_filler[3];
  char r_name[NAME_SIZE];
  char r_name_filler[3];
  char r_tty[TTY_SIZE];
};

/* Control Response structure for BSD4.2 */
struct new_reply {
  unsigned char vers;
  char type;
  char answer;
  char filler;
  srdp_u32 id_num;
  BSD42_SOCK addr;
};

#define NTALKD_VERSION	1

/* dgram types */
#define LEAVE_INVITE    0       /* leave an invitation (local) */
#define LOOK_UP         1       /* look up an invitation (remote) */
#define DELETE          2       /* delete erroneous invitation (remote) */
#define ANNOUNCE        3       /* ring a user (remote) */
#define DELETE_INVITE   4       /* delete my invitation (local) */

/* answer values */
#define SUCCESS         0       /* operation completed properly */
#define NOT_HERE        1       /* callee not logged in */
#define FAILED          2       /* operation failed for unexplained reason */
#define MACHINE_UNKNOWN 3       /* caller's machine name unknown */
#define PERMISSION_DENIED 4     /* callee's tty doesn't permit announce */
#define UNKNOWN_REQUEST 5       /* request has invalid type value */
#define BADVERSION      6       /* request has invalid protocol version */
#define BADADDR         7       /* request has invalid addr value */
#define BADCTLADDR      8       /* request has invalid ctl_addr value */

extern void init_comm(int raw, char *h, int p, char *uh);
extern void quick_clear_invite(void);
extern void main_loop(void);
extern void c_write_char(int line, int col, unsigned char ch);
extern void c_write_string(int line, int col, unsigned char *s, int len);
extern void c_set_cursor(int line, int col);
extern void c_shorten_lline(int line, int len);
extern void c_send_beep(int line, int col);
extern void c_resync(void);
extern void c_send_topic(char *s);
extern void c_close(void);

#endif

