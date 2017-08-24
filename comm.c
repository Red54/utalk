/* utalk, a UDP-based "talk" replacement, using srdp
   Copyright (C) 1995 Roger Espel Llima

   comm.c

   Started: 19 Oct 95 by  <roger.espel.llima@ens.fr>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include "srdp.h"
#include "globals.h"
#include "rc.h"
#include "screen.h"
#include "util.h"
#include "termcap.h"
#include "kbd.h"
#include "menu.h"
#include "comm.h"

static struct srdp_chunk *rc;
static struct srdp_chunk *c;
static struct srdp_chunk *topic_c;

static int buffered_data = 0, buffered_len = 0;

static int ofd = -1, nfd = -1;
struct sockaddr_in osock, nsock;

static int next_id = 1781, my_pid;

static struct mesg last_invite;
static struct sockaddr_in last_invite_addr;
static int invite_sent = 0;

static struct timeval send_when; /* valid only when buffered_data != 0 */
static struct timeval now;

static int has_blocked = 1;

static char errstr[200];

/* return host address in network order */
static srdp_u32 gethostaddr(char *s) {
  struct hostent *h;
  srdp_u32 tmp;

  if ((tmp = inet_addr(s)) == 0xffffffff) {
    if ((h = gethostbyname(s)) == NULL)
      cleanupexit(1,"Unknown host");
    else
      return *(srdp_u32 *)(h->h_addr_list[0]);
  } else
    return tmp;
  return 0; /* so gcc will shut up */
}

static void send_msg(struct sockaddr_in *to, struct mesg *msg) {
  int n;
  struct old_msg omsg;
  struct new_msg nmsg;

  memset(&omsg, 0, sizeof(omsg));
  memset(&nmsg, 0, sizeof(nmsg));
  switch(msg->daemon) {
    case ODAEMON:
      omsg.type = msg->type;
      if (msg->type == ANNOUNCE && strlen(msg->local_name) < 8) {
	omsg.l_name[0] = '!';
	strncpy(&omsg.l_name[1], msg->local_name, NAME_SIZE-1);
      } else {
	strncpy(omsg.l_name, msg->local_name, NAME_SIZE);
      }
      strncpy(omsg.r_name, msg->remote_name, NAME_SIZE);
      strncpy(omsg.r_tty, msg->remote_tty, TTY_SIZE);
      /* omsg.addr = msg->addr; xxx */
      omsg.id_num = htonl(msg->id);
      memcpy(&omsg.ctl_addr, &osock, sizeof(omsg.ctl_addr));
      memcpy(&omsg.addr, &osock, sizeof(omsg.addr));
      omsg.addr.sin_port = htons(users[1]->info.localport);
      omsg.ctl_addr.sin_family = omsg.addr.sin_family = htons(AF_INET);
      omsg.pid = htonl(my_pid);
      to->sin_port = htons(517);

      n = sendto(ofd, (char *)&omsg, sizeof(omsg),0, (struct sockaddr *)to, sizeof (*to));
      if (n != sizeof(omsg))
        cleanupexit(1, "sendto() failed");
      break;

    case NDAEMON:
      nmsg.type = msg->type;
      if (msg->type == ANNOUNCE) {
        nmsg.l_name[0] = '!';
        strncpy(&nmsg.l_name[1], msg->local_name, NAME_SIZE);
      } else {
	strncpy(nmsg.l_name, msg->local_name, NAME_SIZE);
      }
      strncpy(nmsg.r_name, msg->remote_name, NAME_SIZE);
      strncpy(nmsg.r_tty, msg->remote_tty, TTY_SIZE);
      /* nmsg.addr = msg->addr; xxx */
      /* nmsg.ctl_addr = msg->ctl_addr; xxx */
      nmsg.id_num = htonl(msg->id);
      memcpy(&nmsg.ctl_addr, &nsock, sizeof(nmsg.ctl_addr));
      memcpy(&nmsg.addr, &nsock, sizeof(nmsg.addr));
      nmsg.addr.sin_port = htons(users[1]->info.localport);
      nmsg.ctl_addr.sin_family = nmsg.addr.sin_family = htons(AF_INET);
      nmsg.vers = NTALKD_VERSION;
      nmsg.pid = htonl(my_pid);
      to->sin_port = htons(518);

      n = sendto(nfd, (char *)&nmsg, sizeof(nmsg),0, (struct sockaddr *)to, sizeof (*to));
      if (n != sizeof(nmsg))
      cleanupexit(1, "sendto() failed");
  }
}

static void drain_socket(int fd) {
  struct sockaddr_in s;
  struct timeval tv;
  int r;
  size_t sz;
  char buf[200];
  fd_set rfds;

  do {
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = tv.tv_usec = 0;
    r = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (r <= 0)
      break;
    sz = sizeof(s);
    r = recvfrom(fd, buf, 200, 0, (struct sockaddr *)&s, &sz);
  } while (r > 0); 
}

static void parseanswer(struct answer *answ, void *org, int daemontype) {
  struct old_reply *oansw;
  struct new_reply *nansw;
  switch (daemontype) {
    case ODAEMON:
      oansw = (struct old_reply *)org;
      answ->type = oansw->type;
      answ->answer = oansw->answer;
      answ->id = ntohl(oansw->id_num);
      answ->addr = oansw->addr;
      break;
    
    case NDAEMON:
      nansw = (struct new_reply *)org;
      answ->type = nansw->type;
      answ->answer = nansw->answer;
      answ->id = ntohl(nansw->id_num);
      answ->addr = nansw->addr; 
      break;
  }
}

static int recv_msg(struct sockaddr *from, struct answer *answ) {
  int n, fd;
  size_t sz;
  struct old_reply oansw;
  struct new_reply nansw;
  struct sockaddr_in sa;
  fd_set fds;
  struct timeval wait;

  fd = (answ->daemon == ODAEMON ? ofd : nfd);
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  wait.tv_sec = 2;
  wait.tv_usec = 0;
  n = select(fd+1, &fds, NULL, NULL, &wait);
  if (n <= 0)
    return -1;

  switch (answ->daemon) {
    case ODAEMON:
      sz = sizeof(sa);
      sa.sin_family = AF_INET;
      sa.sin_port = htons(0);
      sa.sin_addr.s_addr = htons(INADDR_ANY);
      n = recvfrom(ofd, (char *)&oansw, sizeof(oansw), 0, (struct sockaddr *)&sa, &sz);
      if (n<0) 
        return -1; /* recv failed */
      parseanswer(answ, &oansw, ODAEMON);
      break;

    case NDAEMON:
      sz = sizeof(sa);
      sa.sin_family = AF_INET;
      sa.sin_port = htons(0);
      sa.sin_addr.s_addr = htons(INADDR_ANY);
      n = recvfrom(nfd, (char *)&nansw, sizeof(nansw), 0, (struct sockaddr *)&sa, &sz);
      if (n<0) 
        return -1; /* recv failed */
      parseanswer(answ, &nansw, NDAEMON);
      break;
   }
  return 0;
}

static int find_daemon(char *me, char *him, srdp_u32 addr, char *machine, 
		       int *port) {
  int n, i, r;
  size_t sz;
  struct timeval wait;
  struct sockaddr_in d, rs;
  struct mesg msg;
  struct answer answ;
  struct old_reply oansw;
  struct new_reply nansw;
  fd_set rfds;
  int old = 0, new = 0;

  d.sin_family = PF_INET;
  d.sin_addr.s_addr = addr;

  drain_socket(ofd);
  drain_socket(nfd);

  if (him)
    strncpy(msg.remote_name, him, NAME_SIZE);
  else
    strcpy(msg.remote_name,"^_^");
  if (me)
    strncpy(msg.local_name, me, NAME_SIZE);
  else
    strcpy(msg.local_name,"`&&'");
  msg.remote_tty[0] = '\0';
  msg.type = LOOK_UP;
  msg.id = next_id++;

  for (n = 0; n<5; n++) {
    msg.daemon = ODAEMON;
    send_msg(&d, &msg);

    msg.daemon = NDAEMON;
    send_msg(&d, &msg);
 
    FD_ZERO(&rfds);
    FD_SET(ofd, &rfds);
    FD_SET(nfd, &rfds);
    wait.tv_sec = 2;
    wait.tv_usec = 0;
    i = select(ofd > nfd ? ofd+1 : nfd+1, &rfds, NULL, NULL, &wait);
    if (i<0)
      cleanupexit(1, "select() failed");
    if (i == 0)
      continue;
    while (i) {
      if (FD_ISSET(ofd, &rfds)) {
        sz = sizeof(struct sockaddr_in);
	rs.sin_family = AF_INET;
	rs.sin_port = htons(0);
	rs.sin_addr.s_addr = htons(INADDR_ANY);
        r = recvfrom(ofd, (char *)&oansw, sizeof(oansw), 0, (struct sockaddr *)&rs, &sz);
        if (r < 0) {
	  if (errno != ECONNREFUSED && errno != EINTR)
	    cleanupexit(1, "recvfrom() failed in find_daemon");
	} else {
	  old++;
	  parseanswer(&answ, (void *)&oansw, ODAEMON);
	  if (answ.answer == SUCCESS && port)
	    *port = ntohs(answ.addr.sin_port);
	}
      }
      if (FD_ISSET(nfd, &rfds)) {
	sz = sizeof(struct sockaddr_in);
	rs.sin_family = AF_INET;
	rs.sin_port = htons(0);
	rs.sin_addr.s_addr = htons(INADDR_ANY);
	r = recvfrom(nfd, (char *)&nansw, sizeof(nansw), 0, (struct sockaddr *)&rs, &sz);
	if (r < 0) {
	  if (errno != ECONNREFUSED && errno != EINTR)
	    cleanupexit(1, "recvfrom() failed again in find_daemon");
	} else {
	  new++;
	  parseanswer(&answ, (void *)&nansw, NDAEMON);
	  if (answ.answer == SUCCESS && port)
	    *port = ntohs(answ.addr.sin_port);
	}
      }
      if (old && new)
	break;
      FD_ZERO(&rfds);
      FD_SET(ofd, &rfds);
      FD_SET(nfd, &rfds);
      wait.tv_sec = 0;
      wait.tv_usec = 500000L;
      i = select(ofd > nfd ? ofd+1 : nfd+1, &rfds, NULL, NULL, &wait);
      if (i<0)
	cleanupexit(1, "select() failed again");
    }
    if (new && old)
      return BOTH;
    else if (new)
      return NDAEMON;
    else return ODAEMON;
  }
  sprintf(errstr, "No talk daemon on %s\n", machine);
  cleanupexit(1, errstr);
  return 0; /* so gcc will shut up */
}

/* initializations that need to be done only if using the talk daemons */
static void init_talkd(char *h, int *port) {
  char *s, *t;
  size_t len = sizeof(struct sockaddr_in);
  char uh[256];

  strncpy(uh, h, 255);
  uh[254] = 0;
  if ((s = strchr(uh, '#')) != NULL) {
    *s++ = 0;
    strncpy(users[1]->tty, s, 16);
  } else {
    users[1]->tty[0] = 0;
  }

  h = resolve_alias(uh);

  for (t = users[1]->name, s = h; *s && *s != '@' ;)
    *t++ = *s++;
  *t = 0;

  if (*s == 0)
    strncpy(users[1]->hostname, users[0]->hostname, 254);
  else
    strncpy(users[1]->hostname, ++s, 254);

  users[1]->hostaddr = gethostaddr(users[1]->hostname);

  ofd = socket(PF_INET, SOCK_DGRAM, 0);
  if (ofd < 0)
    cleanupexit(1, "no more sockets");
  osock.sin_family = AF_INET;
  osock.sin_addr.s_addr = htonl(INADDR_ANY);
  osock.sin_port = 0;
  if (bind(ofd, (struct sockaddr *)&osock, sizeof(osock)) < 0)
    cleanupexit(1, "can't bind() socket");
  if (getsockname(ofd, (struct sockaddr *)&osock, &len) < 0)
    cleanupexit(1, "can't find out socket port");
  osock.sin_addr.s_addr = users[0]->hostaddr;

  nfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (nfd < 0)
    cleanupexit(1, "no more sockets");
  nsock.sin_family = AF_INET;
  nsock.sin_addr.s_addr = htonl(INADDR_ANY);
  nsock.sin_port = 0;
  if (bind(nfd, (struct sockaddr *)&nsock, sizeof(nsock)) < 0)
    cleanupexit(1, "can't bind() socket");
  if (getsockname(nfd, (struct sockaddr *)&nsock, &len) < 0)
    cleanupexit(1, "can't find out socket port");
  nsock.sin_addr.s_addr = users[0]->hostaddr;

  if (port) {
    users[0]->daemon = find_daemon(NULL, NULL, users[0]->hostaddr, users[0]->hostname, NULL);
    users[1]->daemon = find_daemon(users[0]->name, users[1]->name, users[1]->hostaddr, users[1]->hostname, port);

    if (users[1]->daemon == BOTH) {
      users[1]->daemon = users[0]->daemon;
      if (users[1]->daemon == BOTH)
	users[1]->daemon = NDAEMON;
    }
    users[1]->ourdaemon = users[0]->daemon;
    if (users[1]->ourdaemon == BOTH)
      users[1]->ourdaemon = users[1]->daemon;
  } else {
    users[1]->daemon = find_daemon(users[0]->name, users[1]->name, users[1]->hostaddr, users[1]->hostname, port);
    if (users[1]->daemon == BOTH)
      users[1]->daemon = NDAEMON;
  }
}

static void ring_user(struct user *u, int raw) {
  int i;
  struct mesg msg;
  struct answer answ;
  struct sockaddr_in d;

  msg.id = next_id++;

  drain_socket(u->daemon == ODAEMON ? ofd : nfd);
  for (i = 0; i<4; i++) {
    strncpy(msg.remote_tty, u->tty, TTY_SIZE);
    msg.remote_tty[TTY_SIZE-1] = 0;
    msg.type = ANNOUNCE;
    strncpy(msg.remote_name, u->name, NAME_SIZE);
    if (raw)
      sprintf(msg.local_name, "%d", u->info.localport);
    else
      strncpy(msg.local_name, users[0]->name, NAME_SIZE);
    answ.daemon = msg.daemon = u->daemon;

    d.sin_family = PF_INET;
    d.sin_addr.s_addr = u->hostaddr;
    send_msg(&d, &msg);

    if (recv_msg((struct sockaddr *)&d, &answ) != 0)
      continue;
    
    switch(answ.answer) {
      case SUCCESS:
	return;
      case NOT_HERE:
	cleanupexit(1, "User not logged in");
	return;
      case FAILED:
	cleanupexit(1, "Unknown talk daemon failure");
	return;
      case MACHINE_UNKNOWN:
	cleanupexit(1, "Remote talk daemon can't find our hostname");
	return;
      case PERMISSION_DENIED:
	cleanupexit(1, "User refusing messages");
	return;
      default:
	cleanupexit(1, "Unknown talkd protocol error");
	return;
    }
    return ;
  }
  cleanupexit(1, "Remote daemon not responding");
}

static void leave_invite(struct user *u) {
  int i;
  struct mesg msg;
  struct answer answ;
  struct sockaddr_in d;

  msg.id = next_id;
  next_id += 5;
  drain_socket(u->ourdaemon == ODAEMON ? ofd : nfd);

  for (i = 0; i<4; i++) {
    msg.remote_tty[0] = 0;
    msg.type = LEAVE_INVITE;
    strncpy(msg.remote_name, u->name, NAME_SIZE);
    strncpy(msg.local_name, users[0]->name, NAME_SIZE);
    answ.daemon = msg.daemon = u->ourdaemon;

    d.sin_family = PF_INET;
    d.sin_addr.s_addr = users[0]->hostaddr;
    last_invite = msg;
    last_invite_addr = d;
    invite_sent++;
    send_msg(&d, &msg);

    if (recv_msg((struct sockaddr *)&d, &answ) != 0)
      continue;

    last_invite.id = answ.id;

    return ;
  }
  cleanupexit(1, "Local daemon not responding");
}

void quick_clear_invite(void) {
  if (invite_sent) {
    last_invite.type = DELETE;
    send_msg(&last_invite_addr, &last_invite);
    send_msg(&last_invite_addr, &last_invite);
  }
}

static void clear_invite(void) {
  int i;
  struct answer answ;
  struct sockaddr_in d;
  struct mesg msg;

  if (invite_sent) {
    for (i = 0; i<4; i++)  {
      msg = last_invite;
      msg.type = DELETE;
      answ.daemon = msg.daemon;
      d = last_invite_addr;
      send_msg(&d, &msg);
      if (recv_msg((struct sockaddr *)&d, &answ) != 0)
	continue;
      invite_sent = 0;
    }
  }
}

void init_comm(int raw, char *h, int p, char *uh) {
  static char tmp[256];
  time_t when;
  struct timeval tv;
  int client = 0, port = 0;
  struct passwd *pw;

  next_id = my_pid = getpid();

  if ((pw = getpwuid(getuid())) != NULL)
    strncpy(users[0]->name, pw->pw_name, 8);
  else strcpy(users[0]->name, "unknown");

  if (gethostname(users[0]->hostname, 254) < 0)
    strcpy(users[0]->hostname, "unknown");

  users[0]->hostaddr = gethostaddr(users[0]->hostname);

  rc = (struct srdp_chunk *)mymalloc(READBUFSIZE);
  c = (struct srdp_chunk *)mymalloc(OUTBUFSIZE);
  topic_c = (struct srdp_chunk *)mymalloc(300);

  memset((char *)&users[1]->info, 0, sizeof(struct srdp_info));
  users[1]->hostname[0] = 0;

  if (raw == 0) {
    init_talkd(uh, &port);
    client = (port != 0);
  } else if (raw == -1) {
    init_talkd(uh, NULL);
    client = 0;
  } else {
    port = p;
    client = (raw == 2);
    if (client) {
      sprintf(users[1]->name, "%d", port);
      if (h && *h)
	strncpy(users[1]->hostname, h, 254);
      else {
	strcpy(users[1]->hostname, users[0]->hostname);
	h = users[0]->hostname;
      }
    }
  }

  if (!client && port)
    users[1]->info.localport = (u_short)port;
  else
    users[1]->info.localport = 0;
  if (client)
    users[1]->info.remoteport = (u_short)port;
  else
    users[1]->info.remoteport = 0;

  if (client)
    users[1]->info.remote.s_addr = 
      (raw>0 ? gethostaddr(h) : users[1]->hostaddr);
  else
    users[1]->info.remote.s_addr = htonl(INADDR_ANY);

  users[1]->info.curr_1_time = 4;
  users[1]->info.curr_nxt_time = 5;
  users[1]->info.miss_time = 3;
  users[1]->info.oldest_time = 5;
  users[1]->info.alive_time = 8;
  users[1]->info.broken_time = 30;
  if (srdp_open(&users[1]->info)<0) cleanupexit(1, "Can't open connection");
  connected++;
  users[1]->info.flags|= SRDP_FL_SELRETURN;

  setstatus(users[1], "waiting for connection...");
  gotoxy(1, 2);
  flush_term();

  if (!client && raw <= 0)
    ring_user(users[1], raw);
  do {
    if (!client && !raw)
      leave_invite(users[1]);
    when = time(NULL) + 28;
    tv.tv_sec = 28;
    tv.tv_usec = 0;
    do {
      srdp_select(1, NULL, NULL, NULL, &tv);
      if (users[1]->info.flags & SRDP_FL_RECVD)
	break;
      tv.tv_sec = when - time(NULL);
      tv.tv_usec = 0;
    } while (tv.tv_sec > 0 && tv.tv_sec < 30);
  } while ((users[1]->info.flags & SRDP_FL_RECVD) == 0);
  srdp_synch(&users[1]->info);

  clear_invite();
  if (raw>0 && !client) {
    sprintf(users[1]->name, "%d", users[1]->info.remoteport);
    strncpy(users[1]->hostname, inet_ntoa(users[1]->info.remote), 254);
  }
  sprintf(tmp, "%s@%s", users[1]->name, users[1]->hostname);
  setstatus(users[1], tmp);
}

void gettime_cached(struct timeval *tv) {
  static struct timeval ltv;

  if (!has_blocked)
    *tv = ltv;
  else {
    gettimeofday(tv, NULL);
    ltv = *tv;
    has_blocked = 0;
  }
}

void flush_buf(void) {
  if (buffered_data) {
    c->u.sequenced.len = htonl(16+buffered_len);
    c->u.sequenced.hi_version = UTALK_REVISION;
    c->u.sequenced.type = UTALK_DATA;
    srdp_write(&users[1]->info, c);
    buffered_data = buffered_len = 0 ;
  }
}

void main_loop(void) {
  fd_set fds;
  unsigned char kbd_buf[256], *cp;
  int r;
  srdp_u32 chunksize;
  struct timeval tmv, *tmvp;

  in_main_loop = 1;
  while (!mustdie) {
    doflags(users[0]);
    doflags(users[1]);
    flush_term();

    if (buffered_data) {
      gettime_cached(&now);
      if (now.tv_sec>send_when.tv_sec || (now.tv_sec == send_when.tv_sec &&
	   now.tv_usec >= send_when.tv_usec)) {
	flush_buf();
	tmvp = NULL;
      } else {
	tmv.tv_sec = send_when.tv_sec-now.tv_sec;
	if (now.tv_usec>send_when.tv_usec) {
	  tmv.tv_usec = 1000000+send_when.tv_usec-now.tv_usec;
	  tmv.tv_sec--;
	} else tmv.tv_usec = send_when.tv_usec-now.tv_usec;
	tmvp = &tmv;
      }
    } else tmvp = NULL;

    FD_ZERO(&fds);
    FD_SET(0, &fds);

    r = srdp_select(1, &fds, NULL, NULL, tmvp);
    has_blocked = 1;

    if (buffered_data) {
      gettime_cached(&now);
      if (now.tv_sec>send_when.tv_sec || (now.tv_sec == send_when.tv_sec &&
	   now.tv_usec >= send_when.tv_usec))
	flush_buf();
    }

    if (mustredisplay) {
      mustredisplay = 0;
      redraw();
    }
    if (winch) {
      int oldlines = lines;
      cols = newcols;
      lines = newlines;
      winch = 0;
      resize_termcap_screen(oldlines);
      cannotrun = (cols<4 || lines<active_buffers+3);
      if (!cannotrun) {
	struct menu *m = the_menu;
	menu_top = menu_bottom = -1;
	the_menu = NULL;
        recalc_all();
	if (the_menu == NULL)
	  the_menu = m;
	redraw_menu();
      }
    }
    if (r<0) continue;
    if (FD_ISSET(0, &fds)) {
      r = read(0, (char *)kbd_buf, 256);
      if (r<0) {
	if (errno == EINTR)
	  continue;
	else
	  cleanupexit(1, "read error");
      }
      if (r == 0) cleanupexit(1, "EOF");
      if (!cannotrun) {
	cp = kbd_buf;
	while (r-- > 0)
	  keyboard(*cp++);
      }
    }
    if ((users[1]->info.flags&SRDP_FL_CLOSED) != 0) {
      connected = 0;
      cleanupexit(1, "Connection closed");
    }
    if (users[1]->info.flags&SRDP_FL_READY) {
      srdp_read(&users[1]->info, rc, 2000);
      chunksize = ntohl(rc->u.sequenced.len);
      if (rc->u.sequenced.type == UTALK_DATA) {
	srdp_u16 line, col;
	srdp_u32 seq;
	struct logical_line *l;
	unsigned char *s, *t;

	seq = ntohl(rc->u.sequenced.seq);
	memcpy((char *)&line, ((char *)rc)+12, 2);
	memcpy((char *)&col, ((char *)rc)+14, 2);
	s = ((unsigned char *)rc)+16;
	t = s + (chunksize - 16);
	line = ntohs(line);
	col = ntohs(col);
	l = find_lline(users[1], line, seq);
	if (s < t) {
	  while (s < t) {
	    if ((*s) == 0xff) {
	      s++;
	      if (*s == 0) {
		shorten_lline(users[1], l, seq, col-1);
		s++;
	      } else if (*s == 1) {
		s++;
		memcpy((char *)&line, (char *)s, 2);
		s += 2;
		memcpy((char *)&col, (char *)s, 2);
		s += 2;
		line = ntohs(line);
		col = ntohs(col);
		l = find_lline(users[1], line, seq);
		if (s >= t)
		  set_cursor(users[1], l, seq, col);
	      } else if (*s == 7) {
		beep();
		s++;
	      }
	    } else write_char(users[1], l, seq, col++, *(s++));
	  }
	} else set_cursor(users[1], l, seq, col);
      } else if (rc->u.sequenced.type == UTALK_TOPIC) {
	unsigned char *s;

	s = ((unsigned char *)rc)+12;
	if (chunksize < 300) {
	  s[chunksize - 12] = '\0';
	  settopic(s);
	}
      }
    }
  }
}

/* if the 2st char in s is a 0xff, the whole thing is assumed to be only
   control chars that do not add to the nextcol counter!   also, do not 
   pass the 0xff,0x01 sequence thru c_write_string, use the line,col params
   instead */
void c_write_string(int line, int col, unsigned char *s, int len) {
  srdp_u16 cc, ll;
  static int nextcol = -1, goodline = -1;

  if (buffered_len + len>SENDAFTER)
    flush_buf();

  cc = htons((srdp_u16)(col));
  ll = htons((srdp_u16)(line));

  if (!buffered_len) {
    memcpy(((char *)c)+12, (char *)&ll, 2);
    memcpy(((char *)c)+14, (char *)&cc, 2);
  } else {
    if (line != goodline || col != nextcol) {
      *(((char *)c) + 16 + buffered_len++) = 0xff;
      *(((char *)c) + 16 + buffered_len++) = 0x01;
      memcpy((((char *)c)+16+buffered_len), (char *)&ll, 2);
      buffered_len += 2;
      memcpy((((char *)c)+16+buffered_len), (char *)&cc, 2);
      buffered_len += 2;
    }
  }

  if (len > 0 && *s == 0xff)
    nextcol = col;
  else
    nextcol = col+len;

  goodline = line;

  if (len > 0) {
    memcpy((((char *)c)+16+buffered_len), (char *)s, len);
    buffered_len += len;
  }

  if (!buffered_data) {
    send_when = now;
    send_when.tv_usec += MAXBUFDELAY;
    if (send_when.tv_usec>1000000) {
      send_when.tv_usec -= 1000000;
      send_when.tv_sec++;
    }
    buffered_data = 1;
  }
}

void c_write_char(int line, int col, unsigned char ch) {
  c_write_string(line, col, &ch, 1);
}

void c_set_cursor(int line, int col) {
  c_write_string(line, col, NULL, 0);
}

void c_shorten_lline(int line, int len) {
  unsigned char wb[2];

  wb[0] = 0xff;
  wb[1] = 0;
  c_write_string(line, len+1, wb, 2);
}

void c_send_beep(int line, int col) {
  unsigned char wb[2];

  wb[0] = 0xff;
  wb[1] = 0;
  c_write_string(line, col, wb, 2);
}

void c_resync(void) {
  flush_buf();
  srdp_synch(&users[1]->info);
}

void c_close(void) {
  flush_buf();
  srdp_drop(&users[1]->info, "");
}

void c_send_topic(char *s) {
  int len;

  len = strlen(s);
  if (len > 255)
    s[255] = '\0';
  topic_c->u.sequenced.len = htonl(12+len);
  topic_c->u.sequenced.hi_version = UTALK_REVISION;
  topic_c->u.sequenced.type = UTALK_TOPIC;
  memcpy(((char *)topic_c)+12, s, len+1);
  srdp_write(&users[1]->info, topic_c);
}

