/* SRDP, Semi-Reliable Datagram Protocol
   Copyright (C) 1995 Roger Espel Llima
    
   srdp.c

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include "srdpdata.h"
#include "srdp.h"

static struct srdp_infolist *srdp_allinfos = NULL;
static srdp_u8 packbuf[MAXPACKETLEN], readbuf[MAXPACKETLEN], *reading;
static int sofar, toread;

/* internal functions */

static int cansend(struct srdp_info *info) {
  if (info->remoteport == 0) return 0;
  if (info->remote.s_addr == htonl(INADDR_ANY)) return 0;
  return 1;
}

static void initchunk (struct srdp_chunk *c) {
  c->u.noseq.version = SRDP_VERSION;
  c->u.noseq.revision = SRDP_REVISION;
}

static void sendpacket(struct srdp_info *info, unsigned char *packet, 
		       int size) {
  struct sockaddr_in to;

#ifdef DEBUG
  if (((rand()>>1)%7)<1) /* DEBUG */
    fprintf(stderr, "xxx deciding not to send packet\n"); /* DEBUG */
  else /* DEBUG */
#endif /* DEBUG */
  if (size) {
    memset((char *)&to, 0, sizeof(struct sockaddr));
    to.sin_port = htons(info->remoteport);
    to.sin_family = AF_INET;
    to.sin_addr = info->remote;
    sendto(info->sockfd, (char *)packet, size, 0, (struct sockaddr *)&to,
	   sizeof(struct sockaddr));
  }
}

static void sendit(struct srdp_info *info) {
  sendpacket(info, packbuf, sofar);
  sofar = 0;
}

static void closeandforget(struct srdp_info *info) {
  struct srdp_chunk_list *lists, *l0;
  struct srdp_infolist *ilist, *prv;

  close(info->sockfd);
  info->flags = SRDP_FL_CLOSED;
  free(info->recvdarray);
  lists = info->recvlist_first;
  while (lists != NULL) {
    l0 = lists;
    lists = lists->next;
    free(l0);
  }
  lists = info->sentlist_first;
  while (lists != NULL) {
    l0 = lists;
    lists = lists->next;
    free(l0);
  }
  ilist = srdp_allinfos;
  prv = NULL;
  while (ilist != NULL) {
    if (ilist->theinfo == info) {
      if (prv == NULL)
	srdp_allinfos = ilist->next;
      else
	prv->next = ilist->next;
      free(ilist);
      break;
    } else {
      prv = ilist;
      ilist = ilist->next;
    }
  }
}

static int missing(struct srdp_info *info, srdp_u32 last) {
/* tells if there are any missing packets on *info, where last is the last
   one that should be there */
  int idx;
  srdp_u32 refto, msk;

  refto = info->rarr_seqbase;
  idx = info->rarr_start;
  while (refto+31 <= last) {
    if (info->recvdarray[idx] != 0xffffffff) return(1);
    idx = (idx+1)&SRDP_RARRMASK;
    refto += 32;
  }
  if (refto>last) return 0;
  msk = (1<<(last+1-refto))-1;
  if ((info->recvdarray[idx]&msk) != msk)
    return(1);
  else
    return(0);
}

static void make_missing(struct srdp_info *info, srdp_u32 last) {
/* makes a MISSLST packet on packbuf after sofar, updates sofar;
   considers "last" to be the last that should have been received,
   and assumes rarr goes all the way to last  */

  srdp_u8 *where = packbuf+sofar, *wlen = packbuf+sofar+4, howmany;
  int idx, adding = 0, sofar0 = sofar;
  srdp_u32 refto, msk, nref;

  *where = SRDP_VERSION;
  *(where+1) = SRDP_REVISION;
  *(where+2) = 0;
  *(where+3) = SRDP_MISSLST;
  sofar += 8;
  where += 8;

  idx = (info->rarr_start+((last-info->rarr_seqbase)>>5))&SRDP_RARRMASK;
  msk = 1<<((last-info->rarr_seqbase)&0x1f);
  refto = last;
  
  do {
    if (adding) {
      if ((info->recvdarray[idx]&msk) == 0 && howmany<255)
	howmany++;
      else {
	where += 4;
	*(where++) = howmany-1;
	sofar += 5;
	adding = 0;
      }
    }
    if (!adding && (info->recvdarray[idx]&msk) == 0) {
      adding = 1;
      nref = htonl(refto);
      memcpy((char *)where, (char *)&nref, 4);
      howmany = 1;
    }
    if (msk>1)
      msk = msk>>1;
    else {
      msk = (srdp_u32)0x80000000;
      idx = (idx-1)&SRDP_RARRMASK;
    }
    refto--;
  } while (refto >= info->rarr_seqbase && sofar<info->max_packet_len-16);
  idx = htonl(sofar-sofar0);
  memcpy((char *)wlen, (char *)&idx, 4);
}

static void srdp_sendcurr(struct srdp_info *info, int force) {
/* sends a SRDP_CURRENT and updates everything; will also send a
   SRDP_MISSLIST if the list is not empty  */
  struct srdp_chunk *c;
  struct timeval now;

  c = (struct srdp_chunk *)packbuf;
  initchunk(c);
  c->u.noseq.type = SRDP_CURRENT;
  c->u.noseq.len = htonl(SRDP_HEADER_USEQ_LEN+4);
  *(int *)&(c->u.noseq.data) = htonl(info->current_seq);
  sofar = SRDP_HEADER_USEQ_LEN+4;
  gettimeofday(&now, NULL);
  info->lastsentsomething = now;
  info->lastsentcurrent = now;
  info->current_sent = 1;
  if (force || now.tv_sec>info->lastsentlist.tv_sec+info->miss_time ||
      (now.tv_sec == info->lastsentlist.tv_sec+info->miss_time &&
       now.tv_usec >= info->lastsentlist.tv_usec)) {
    if (missing(info, info->current_seq)) {
      make_missing(info, info->current_seq);
      info->lastsentlist = now;
    }
  }
  sendit(info);
}

static void rollrarr(struct srdp_info *info, srdp_u32 last) {
/* rolls the array around so that the sequence number "last" falls
within it, forgetting old data */
  while (info->rarr_seqbase+(SRDP_RARRSIZE<<5) <= last) {
    info->rarr_seqbase += 32;
    info->recvdarray[info->rarr_start] = 0;
    info->rarr_start = (info->rarr_start+1)&SRDP_RARRMASK;
  }
}

static void rarrforget(struct srdp_info *info, srdp_u32 oldest) {
/* marks as received anything older than oldest */
  int idx;
  srdp_u32 refto, msk;

  refto = info->rarr_seqbase;
  idx = info->rarr_start;
  while (refto+31<oldest) {
    info->recvdarray[idx] = (srdp_u32)0xffffffff;
    idx = (idx+1)&SRDP_RARRMASK;
    refto += 32;
  }
  if (refto >= oldest) return;
  msk = (1<<(oldest-refto))-1;
  info->recvdarray[idx]|= msk;
}

static void resend_all(struct srdp_info *info) {
  struct timeval now;
  struct srdp_chunk_list *cl;

  gettimeofday(&now, NULL);
  cl = info->sentlist_first;
  while (cl != NULL) {
    if (info->max_packet_len < sofar + 16 + ntohl(cl->chunk->u.sequenced.len))
      sendit(info);
    memcpy((char *)packbuf+sofar, (char *)cl->chunk, 
	   ntohl(cl->chunk->u.sequenced.len));
    sofar += ntohl(cl->chunk->u.sequenced.len);
    info->lastsentsomething = now;
    cl = cl->next;
  }
}

static void srdp_stufftoread(struct srdp_info *info) {
/* reads from the fd and updates everything */
  struct sockaddr_in from;
  struct timeval now;
  srdp_u32 chunksize, junk, junk2;
  size_t fromlen = sizeof(struct sockaddr_in);
  int readsize;

  readsize = recvfrom(info->sockfd, (char *)readbuf, MAXPACKETLEN, 0, 
		    (struct sockaddr *)&from, &fromlen);
  if (readsize < SRDP_HEADER_USEQ_LEN) return;

  sofar = 0;
  if (info->remoteport != 0 && from.sin_port != htons(info->remoteport)) return;
  if (info->remote.s_addr != htonl(INADDR_ANY) &&
      info->remote.s_addr != from.sin_addr.s_addr) return;
  if (info->remoteport == 0 || info->remote.s_addr == htonl(INADDR_ANY)) {
    info->remoteport = ntohs(from.sin_port); 
    info->remote.s_addr = from.sin_addr.s_addr;
    resend_all(info);
  }
  
  gettimeofday(&now, NULL);
  info->lastheard = now;
  info->current_sent = 0;
  info->flags&=~SRDP_FL_BROKEN;
  info->flags|= SRDP_FL_RECVD;

  reading = readbuf;
  toread = readsize;


  while (toread) {
    if (toread<SRDP_HEADER_USEQ_LEN) break;
    memcpy((char *)&junk, (char *)reading+4, 4);
    chunksize = ntohl(junk);
    if (chunksize > toread) break;

    switch(reading[3]) {
      case SRDP_PING:
	if (info->max_packet_len  < sofar + chunksize) sendit(info);
	memcpy((char *)packbuf+sofar, (char *)reading, chunksize);
	packbuf[sofar+3] = SRDP_PINGREP;
	sofar += chunksize;
	reading += chunksize;
	toread -= chunksize;
	info->lastsentsomething = now;
	break;
      
      case SRDP_ALIVE:
	reading += chunksize;
	toread -= chunksize;
	break;
      
      case SRDP_CURRENT:
      {
	struct srdp_chunk_list *cl;

	if (chunksize >= 12) {
	  memcpy((char *)&junk, (char *)reading+8, 4);
	  junk2 = ntohl(junk);
	  if (junk2<info->next_seq-1) {
	    cl = info->sentlist_last;
	    while (cl != NULL && ((cl->chunk->u.noseq.type&1) != 0 ||
		  (ntohl(cl->chunk->u.sequenced.seq)>junk2)))
	      cl = cl->prev;
	    if (cl == NULL) {
	    /* maybe we should send an oldest here */
	    } else while (cl->next != NULL) {
	      if (info->max_packet_len-sofar < 16 + 
		  ntohl(cl->next->chunk->u.sequenced.len))
		sendit(info);
	      memcpy((char *)packbuf+sofar, (char *)cl->next->chunk,
		     ntohl(cl->next->chunk->u.sequenced.len));
	      sofar += ntohl(cl->next->chunk->u.sequenced.len);
	      info->lastsentsomething = now;
	      cl = cl->next;
	    }
	  }
	}
	reading += chunksize;
	toread -= chunksize;
	break;
      }

      case SRDP_OLDEST:
	if (chunksize >= 12) {
	  memcpy((char *)&junk, (char *)reading+8, 4);
	  junk2 = ntohl(junk);
	  rarrforget(info, junk2);
	}
	reading += chunksize;
	toread -= chunksize;
	break;
      
      case SRDP_MISSLST:
	{
	  srdp_u32 todo, seq, junk;
	  unsigned int extra, send_oldest = 0;
	  struct srdp_chunk_list *cl;

	  todo = chunksize-8;
	  toread -= 8;
	  reading += 8;
	  cl = info->sentlist_last;
	  while (todo >= 5 && cl != NULL) {
	    memcpy((char *)&junk, (char *)reading, 4);
	    extra = 1+(unsigned int)reading[4];
	    seq = ntohl(junk);
	    for (; extra != 0; extra--) {
	      while (cl != NULL && ((cl->chunk->u.noseq.type&1) != 0 || 
		     ntohl(cl->chunk->u.sequenced.seq) != seq))
		cl = cl->prev;
	      if (cl != NULL) {
		if (info->max_packet_len-sofar < 16 +
		    ntohl(cl->chunk->u.sequenced.len))
		  sendit(info);
		memcpy((char *)packbuf+sofar, (char *)cl->chunk, 
		       ntohl(cl->chunk->u.sequenced.len));
		sofar += ntohl(cl->chunk->u.sequenced.len);
		info->lastsentsomething = now;
		cl = cl->prev;
	      } else {
		send_oldest++;
		break;
	      }
	      seq--;
	    }
	    todo -= 5;
	    toread -= 5;
	    reading += 5;
	  }
	  if (todo || send_oldest) {
	    cl = info->sentlist_first;
	    while (cl != NULL && (cl->chunk->u.noseq.type&1) != 0)
	      if (cl) cl = cl->next;
	    if (cl) {
	      if (info->max_packet_len-sofar<32) sendit(info);
	      packbuf[sofar++] = SRDP_VERSION;
	      packbuf[sofar++] = SRDP_REVISION;
	      packbuf[sofar++] = 0;
	      packbuf[sofar++] = SRDP_OLDEST;
	      junk = htonl(SRDP_HEADER_USEQ_LEN+4);
	      memcpy((char *)packbuf+sofar, (char *)&junk, 4);
	      junk = htonl(cl->chunk->u.sequenced.seq);
	      memcpy((char *)packbuf+sofar+4, (char *)&junk, 4);
	      sofar += 8;
	    }
	  }
	  reading += todo;
	  toread -= todo;
	}
	break;

      case SRDP_CLOSE:
	packbuf[0] = SRDP_VERSION;
	packbuf[1] = SRDP_REVISION;
	packbuf[2] = 0;
	packbuf[3] = SRDP_DROP;
	junk = htonl(8);
	memcpy((char *)packbuf+4, (char *)&junk, 4);
	sendpacket(info, packbuf, 8);
	sendpacket(info, packbuf, 8);
	sendpacket(info, packbuf, 8);
      /* drops (no pun intended) into the next: */
      
      case SRDP_DROP:
	closeandforget(info);
	return;
      
      default:
	{
	  struct srdp_chunk *c;
	  struct srdp_chunk_list *cl, *ncl;
	  int skipit = 0;

	  c = (struct srdp_chunk *)malloc(chunksize);
	  ncl = (struct srdp_chunk_list *)malloc(sizeof (struct srdp_chunk_list));
	  if (c == NULL || ncl == NULL) {
	    reading += chunksize;
	    toread -= chunksize;
	    break;
	  }
	  memcpy((char *)c, (char *)reading, chunksize);
	  reading += chunksize;
	  toread -= chunksize;

	  if ((c->u.noseq.type&1) == 0) {
	    memcpy((char *)&junk, (char *)&(c->u.sequenced.seq), 4);
	    junk2 = ntohl(junk);
	    if (junk2>info->current_seq) info->current_seq = junk2;
	    rollrarr(info, junk2);
	    if (junk2 >= info->rarr_seqbase) {
	      skipit = info->recvdarray[(((junk2-info->rarr_seqbase)>>5)+
		info->rarr_start)&SRDP_RARRMASK] &
		(1<<((junk2-info->rarr_seqbase)&31));
	      info->recvdarray[(((junk2-info->rarr_seqbase)>>5)+
		info->rarr_start)&SRDP_RARRMASK] |=
		1<<((junk2-info->rarr_seqbase)&31);
	    }
	  }
	  if (!skipit) {
	    while (info->recvlist_size + CHUNKLIST_EXTRA + 
		    ntohl(c->u.sequenced.len) > info->max_rcv_lst_size) {
	      cl = info->recvlist_first;
	      if (cl->next != NULL) cl->next->prev = NULL;
	      info->recvlist_first = cl->next;
	      info->recvlist_size -= cl->lsize;
	      if (info->recvlist_last == cl) info->recvlist_last = NULL;
	      free(cl->chunk);
	      free(cl);
	    }
	    info->flags|= SRDP_FL_READY;
	    ncl->prev = info->recvlist_last;
	    ncl->next = NULL;
	    ncl->lsize = CHUNKLIST_EXTRA+ntohl(c->u.sequenced.len);
	    info->recvlist_size += CHUNKLIST_EXTRA+ntohl(c->u.sequenced.len);
	    ncl->chunk = c;
	    if (ncl->prev)
	      ncl->prev->next = info->recvlist_last = ncl;
	    else
	      info->recvlist_last = info->recvlist_first = ncl;
	  }
	  if (!toread) {
	    if (now.tv_sec>info->lastsentlist.tv_sec+info->miss_time ||
		(now.tv_sec == info->lastsentlist.tv_sec+info->miss_time &&
		 now.tv_usec >= info->lastsentlist.tv_usec)) {
	      if (missing(info, info->current_seq)) {
		if (info->max_packet_len-sofar-16<256)
		  sendit(info);
		make_missing(info, info->current_seq);
		info->lastsentlist = now;
	      }
	    }
	  }
	}
    }
  }
  sendit(info);
}

static void srdp_sendalive(struct srdp_info *info) {
/* sends a SRDP_ALIVE and updates everything */
  struct srdp_chunk *c = (struct srdp_chunk *)packbuf;
  struct timeval now;

  initchunk(c);
  c->u.noseq.type = SRDP_ALIVE;
  c->u.noseq.len = htonl(SRDP_HEADER_USEQ_LEN);
  sendpacket(info, packbuf, SRDP_HEADER_USEQ_LEN);
  gettimeofday(&now, NULL);
  info->lastsentsomething = now;
}


/* interface functions */

int srdp_open(struct srdp_info *info) {
     /* opens the socket, adds it to the global select table, returns
	0 if ok, -1 if error */
  int fd;
  size_t length;
  struct srdp_infolist *ti;

  info->myaddr.sin_port = htons(info->localport);
  info->myaddr.sin_family = AF_INET;
  info->myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if ((fd = socket(PF_INET, SOCK_DGRAM, 0))<0)
    return -1;
  if (bind(fd, (struct sockaddr *)&(info->myaddr), sizeof(struct sockaddr))<0) {
    close(fd);
    return -1;
  }
  length = sizeof(info->myaddr);
  if (getsockname(fd, (struct sockaddr *)&(info->myaddr), &length)<0) {
    close(fd);
    return -1;
  }
  info->sockfd = fd;
  info->localport = ntohs(info->myaddr.sin_port);

  if (info->max_rcv_lst_size == 0) info->max_rcv_lst_size = SRDP_DEFL_MAX_RCV;
  if (info->max_snt_lst_size == 0) info->max_snt_lst_size = SRDP_DEFL_MAX_SNT;
  if (info->max_packet_len == 0) info->max_packet_len = SRDP_DEFL_MAX_PACKET;
  if (info->max_packet_len>MAXPACKETLEN) info->max_packet_len = MAXPACKETLEN;
  if (info->curr_1_time == 0) info->curr_1_time = SRDP_CURR_1_TM;
  if (info->curr_nxt_time == 0) info->curr_nxt_time = SRDP_CURR_NXT_TM;
  if (info->miss_time == 0) info->miss_time = SRDP_MISS_TM;
  if (info->alive_time == 0) info->alive_time = SRDP_ALIVE_TM;
  if (info->oldest_time == 0) info->oldest_time = SRDP_OLDEST_TM;
  if (info->broken_time == 0) info->broken_time = SRDP_BROKEN_TM;

  info->flags&= (SRDP_FL_NOSELECT|SRDP_FL_SELRETURN);
  info->recvlist_first = NULL;
  info->recvlist_last = NULL;
  info->sentlist_first = NULL;
  info->sentlist_last = NULL;
  info->recvlist_size = 0;
  info->sentlist_size = 0;

  info->current_seq = 0;
  info->next_seq = 1;
  info->recvdarray = (srdp_u32 *)malloc(SRDP_RARRSIZE<<2);
  if (info->recvdarray == NULL) {
    close(fd);
    return -1;
  }
  memset((char *)info->recvdarray, 0, SRDP_RARRSIZE<<2);
  
  info->rarr_seqbase = 1;
  info->rarr_start = 0;

  info->lastheard.tv_sec = 0L;
  info->lastheard.tv_usec = 0L;
  info->lastsentcurrent.tv_sec = 0L;
  info->lastsentcurrent.tv_usec = 0L;
  info->lastsentsomething.tv_sec = 0L;
  info->lastsentsomething.tv_usec = 0L;
  info->lastsentlist.tv_sec = 0L;
  info->lastsentlist.tv_usec = 0L;
  info->lastsentoldest.tv_sec = 0L;
  info->lastsentoldest.tv_usec = 0L;
  /* wakeup{1,2} is initialized and used by srdp_select() */

  info->current_sent = 0;

  ti = (struct srdp_infolist *)malloc(sizeof(struct srdp_infolist));
  if (ti == NULL) {
    close(fd);
    return -1;
  }
  ti->theinfo = info;
  ti->next = srdp_allinfos;
  srdp_allinfos = ti;
  if (cansend(info))
    srdp_sendalive(info);

  return 0;
}

int srdp_select(int width, fd_set *readfds, fd_set *writefds,
		       fd_set *exceptfds, struct timeval *timeout) {
     /* does a select on the given fds + on any open sockets, and updates
	the SRDP_FL_READY flag on any sockets that are ready for reading
	(which actually means there are data chunks already read);
	returns when the timeout expires or there is anythign to read/write/err
	on the given fd_sets, or there is somethign to read on one of
	the open connections which has not been marked SRDP_FL_NOSELECT;
	the result and the resulting fd_sets don't include the fds from
	the connection */

  struct srdp_info *info;
  struct srdp_infolist *infol, *nextinfo;
  struct timeval now, wakeup, mytimeout, returntime;
  fd_set myreadfds, orgreadfds, orgwritefds, orgexceptfds;
  int res, returnit;

  infol = srdp_allinfos;
  while (infol != NULL) {
    if ((infol->theinfo->flags&(SRDP_FL_READY|SRDP_FL_NOSELECT|SRDP_FL_CLOSED))
 == SRDP_FL_READY) {
      if (readfds != NULL) FD_ZERO(readfds);
      if (writefds != NULL) FD_ZERO(writefds);
      if (exceptfds != NULL) FD_ZERO(exceptfds);
      return 0;
    }
    infol = infol->next;
  }

  if (readfds != NULL) 
    orgreadfds =*readfds;
  else
    FD_ZERO(&orgreadfds);
  if (writefds != NULL)
    orgwritefds =*writefds;
  else
    FD_ZERO(&orgwritefds);
  if (exceptfds != NULL)
    orgexceptfds =*exceptfds;
  else
    FD_ZERO(&orgexceptfds);

  gettimeofday(&now, NULL);
  returntime = now;
  if (timeout != NULL) {
    returntime.tv_usec += timeout->tv_usec;
    returntime.tv_sec += timeout->tv_sec;
    if (returntime.tv_usec>1000000L) {
      returntime.tv_sec++;
      returntime.tv_usec -= 1000000L;
    }
  } else returntime.tv_sec += 604800L;

  while (1) {
    myreadfds = orgreadfds;
    wakeup.tv_sec = 0;

    /* update all wakeup values + find earliest one */
    infol = srdp_allinfos;
    while (infol != NULL) {
      info = infol->theinfo;
      if ((info->flags&SRDP_FL_CLOSED) != 0) {
	infol = infol->next;
	continue;
      }
      if (info->sockfd >= width) width = info->sockfd+1;
      FD_SET(info->sockfd, &myreadfds);
      if (readfds != NULL) FD_CLR(info->sockfd, readfds);
      if (writefds != NULL) FD_CLR(info->sockfd, writefds);
      if (exceptfds != NULL) FD_CLR(info->sockfd, exceptfds);

      if (cansend(info)) {
	if (info->current_sent) {
	  info->wakeup1 = info->lastsentcurrent;
	  if (info->lastsentcurrent.tv_sec == 0) info->wakeup1 = now;
	  info->wakeup1.tv_sec += info->curr_nxt_time;
	} else {
	  info->wakeup1 = info->lastheard;
	  if (info->lastheard.tv_sec == 0) info->wakeup1 = now;
	  info->wakeup1.tv_sec += info->curr_1_time;
	}
	info->wakeup2 = info->lastsentsomething;
	if (info->lastsentsomething.tv_sec == 0) info->wakeup2 = now;
	info->wakeup2.tv_sec += info->alive_time;
	if (wakeup.tv_sec == 0 || wakeup.tv_sec>info->wakeup1.tv_sec ||
	    (wakeup.tv_sec == info->wakeup1.tv_sec && 
	     wakeup.tv_usec>info->wakeup1.tv_usec))
	  wakeup = info->wakeup1;
	if (wakeup.tv_sec == 0 || wakeup.tv_sec>info->wakeup2.tv_sec ||
	    (wakeup.tv_sec == info->wakeup2.tv_sec && 
	     wakeup.tv_usec>info->wakeup2.tv_usec))
	  wakeup = info->wakeup2;
      }
      infol = infol->next;
    }
    if (wakeup.tv_sec == 0) {
      wakeup = now;
      wakeup.tv_sec += 604800L;
    } else if (wakeup.tv_sec<now.tv_sec || (wakeup.tv_sec == now.tv_sec &&
	wakeup.tv_usec<now.tv_usec))
      wakeup = now;
    mytimeout = wakeup;
    if (mytimeout.tv_usec<now.tv_usec) {
      mytimeout.tv_usec += 1000000L;
      mytimeout.tv_sec--;
    }
    mytimeout.tv_sec -= now.tv_sec;
    mytimeout.tv_usec -= now.tv_usec;
    if (timeout != NULL)
      if (timeout->tv_sec<mytimeout.tv_sec || (timeout->tv_sec == mytimeout.tv_sec
	  && timeout->tv_usec<mytimeout.tv_usec))
	mytimeout =*timeout;

    res = select(width, &myreadfds, writefds, exceptfds, &mytimeout);
    if (res<0) return res;
    if (readfds != NULL) *readfds = myreadfds;
    gettimeofday(&now, NULL);

    infol = srdp_allinfos;
    returnit = 0;
    while (infol != NULL) {
      info = infol->theinfo;
      nextinfo = infol->next;
      if ((info->flags&SRDP_FL_CLOSED) != 0) {
	infol = infol->next;
	continue;
      }
      if (FD_ISSET(info->sockfd, &myreadfds)) {
	if (readfds != NULL) FD_CLR(info->sockfd, readfds);
	res--;
	srdp_stufftoread(info);
      } else {
	if (cansend(info) && (now.tv_sec>info->wakeup1.tv_sec || 
	    (now.tv_sec == info->wakeup1.tv_sec &&
	     now.tv_usec >= info->wakeup1.tv_usec)))
	  srdp_sendcurr(info, 0);
	  if ((info->flags&(SRDP_FL_SELRETURN|SRDP_FL_NOSELECT)) == 
	      SRDP_FL_SELRETURN) returnit++;
      }
      info->wakeup2 = info->lastsentsomething;
      if (info->lastsentsomething.tv_sec == 0) info->wakeup2 = now;
      info->wakeup2.tv_sec += info->alive_time;
      if (cansend(info) && (now.tv_sec>info->wakeup2.tv_sec ||
	  (now.tv_sec == info->wakeup2.tv_sec &&
	   now.tv_usec >= info->wakeup2.tv_usec)))
	srdp_sendalive(info);
      if (info->lastheard.tv_sec+info->broken_time <= now.tv_sec)
	info->flags|= SRDP_FL_BROKEN;
      else
	info->flags&=~SRDP_FL_BROKEN;
      if ((info->flags&(SRDP_FL_READY|SRDP_FL_NOSELECT)) == SRDP_FL_READY)
	returnit++;
      infol = nextinfo;
    }
    if (returnit || res>0 || now.tv_sec>returntime.tv_usec || 
        (now.tv_sec == returntime.tv_sec && now.tv_usec >= returntime.tv_usec))
      return res;

    if (readfds != NULL) *readfds = orgreadfds;
    if (readfds != NULL) *writefds = orgwritefds;
    if (exceptfds != NULL) *exceptfds = orgexceptfds;
    gettimeofday(&now, NULL);
  }
}

int srdp_read(struct srdp_info *info, struct srdp_chunk *chunk, int csize) {
     /* reads one data chunk (from the srdp buffers) and returns 1 if ok,
        or 0 if there was none to read, or 2 if the packet was
	truncated, -1 if the connection is closed; updates SRDP_FL_READY */
  int ret = 1;
  struct srdp_chunk_list *cl;

  if (info->flags&SRDP_FL_CLOSED) return -1;
  if (info->recvlist_first == NULL || csize<SRDP_HEADER_USEQ_LEN) return 0;
  if (csize < ntohl(info->recvlist_first->chunk->u.noseq.len)) {
    memcpy((char *)chunk, (char *)info->recvlist_first->chunk, csize);
    ret = 2;
  } else
    memcpy((char *)chunk, (char *)info->recvlist_first->chunk,
	   ntohl(info->recvlist_first->chunk->u.noseq.len));
  info->recvlist_size -= ntohl(chunk->u.noseq.len)+CHUNKLIST_EXTRA;
  cl = info->recvlist_first;
  info->recvlist_first = cl->next;
  if (cl->next) {
    cl->next->prev = NULL;
    info->flags|= SRDP_FL_READY;
  } else {
    info->recvlist_last = NULL;
    info->flags&=~SRDP_FL_READY;
  }
  free(cl);
  return ret;
}

int srdp_write(struct srdp_info *info, struct srdp_chunk *chunk) {
     /* sends out a data chunk; returns -1 if error, 0 if ok */
  struct srdp_chunk *c;
  struct timeval now;
  struct srdp_chunk_list *cl, *ncl;

  if ((info->flags&SRDP_FL_CLOSED) != 0) {
    errno = ENOTCONN;
    return -1;
  }
  if (ntohl(chunk->u.sequenced.len) < SRDP_HEADER_SEQ_LEN ||
      ntohl(chunk->u.sequenced.len) > info->max_packet_len) {
    errno = EMSGSIZE;
    return -1;
  }
  c = (struct srdp_chunk *)malloc(ntohl(chunk->u.sequenced.len));
  if (c == NULL) {
    errno = ENOMEM;
    return -1;
  }
  ncl = (struct srdp_chunk_list *)malloc(sizeof (struct srdp_chunk_list));
  if (ncl == NULL) {
    errno = ENOMEM;
    return -1;
  }
  initchunk(chunk);
  chunk->u.sequenced.seq = htonl(info->next_seq);
  info->next_seq++;
  memcpy((char *)c, (char *)chunk, ntohl(chunk->u.sequenced.len));
  if (cansend(info))
    sendpacket(info, (unsigned char *)chunk, ntohl(chunk->u.sequenced.len));
  gettimeofday(&now, NULL);
  info->lastsentsomething = now;

  while (info->sentlist_size + CHUNKLIST_EXTRA + ntohl(chunk->u.sequenced.len) >
  	info->max_snt_lst_size) {
    cl = info->sentlist_first;
    if (cl->next != NULL) cl->next->prev = NULL;
    info->sentlist_first = cl->next;
    info->sentlist_size -= cl->lsize;
    if (info->sentlist_last == cl) info->sentlist_last = NULL;
    free(cl->chunk);
    free(cl);
  }
  ncl->prev = info->sentlist_last;
  ncl->next = NULL;
  ncl->lsize = CHUNKLIST_EXTRA+ntohl(chunk->u.sequenced.len);
  ncl->chunk = c;
  info->sentlist_size += ncl->lsize;
  if (ncl->prev)
    ncl->prev->next = info->sentlist_last = ncl;
  else
    info->sentlist_last = info->sentlist_first = ncl;
  return 0;
}

void srdp_synch(struct srdp_info *info) {
  if (cansend(info)) srdp_sendcurr(info, 1);
}

void srdp_close(struct srdp_info *info, char *message) {
/* attempts to gracefully close the connection; may block for
   a few seconds */
/*  we should actually send out that message... */

  struct srdp_chunk *c;
  struct timeval now, next, timeout;
  fd_set readfds;
  int times = 0, acked = 0, sres, readsize;
  size_t fromlen;
  struct sockaddr_in from;

  if (cansend(info)) {
    c = (struct srdp_chunk *)packbuf;
    initchunk(c);
    c->u.noseq.type = SRDP_CLOSE;
    c->u.noseq.len = htonl(8);
    sendpacket(info, packbuf, 8);

    gettimeofday(&next, NULL);

    while (1) {
      gettimeofday(&now, NULL);
      if (now.tv_sec>next.tv_sec || (now.tv_sec == next.tv_sec &&
	  now.tv_usec >= next.tv_usec)) {
	times++;
	if (times>4) break;
	sendpacket(info, packbuf, 8);
	next.tv_sec += 5;
      }
      timeout = next;
      if (timeout.tv_usec<now.tv_usec) {
	timeout.tv_usec += 1000000L;
	timeout.tv_sec--;
      }
      timeout.tv_usec -= now.tv_usec;
      timeout.tv_sec -= now.tv_sec;
      if (timeout.tv_sec<0 ) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
      }
      FD_ZERO(&readfds);
      FD_SET(info->sockfd, &readfds);
      sres = select(info->sockfd+1, &readfds, NULL, NULL, &timeout);
      if (sres) {
	readsize = recvfrom(info->sockfd, (char *)readbuf, MAXPACKETLEN, 0,
			  (struct sockaddr *)&from, &fromlen);
	if (readsize >= SRDP_HEADER_USEQ_LEN &&
	    from.sin_port == htons(info->remoteport) &&
	    info->remote.s_addr == from.sin_addr.s_addr &&
	    (readbuf[3] == SRDP_DROP || readbuf[3] == SRDP_CLOSE)) {
	  acked++;
	  break;
	}
      }
    }
    if (!acked) {
      c->u.noseq.type = SRDP_DROP;
      sendpacket(info, packbuf, 8);
      sendpacket(info, packbuf, 8);
      sendpacket(info, packbuf, 8);
    }
  }
  closeandforget(info);
}

void srdp_drop(struct srdp_info *info, char *message) {
     /* drops the connection; will not block */
  struct srdp_chunk *c;

  if (cansend(info)) {
    c = (struct srdp_chunk *)packbuf;
    initchunk(c);
    c->u.noseq.type = SRDP_DROP;
    c->u.noseq.len = htonl(8);
    sendpacket(info, packbuf, 8);
    sendpacket(info, packbuf, 8);
    sendpacket(info, packbuf, 8);
  }
  closeandforget(info);
}

/*  add a command to send out a ping */

#ifdef DEBUG

/* xxx debugging stuff */
void srdp_infos(struct srdp_info *info) {
  int idx;
  srdp_u32 refto, msk;

  if (info->recvlist_first == NULL)
    printf("nothing on the recvdlist\n");
  else
    printf("recvdlist goes from %ld to %ld\n", 
      ntohl(info->recvlist_first->chunk->u.sequenced.seq),
      ntohl(info->recvlist_last->chunk->u.sequenced.seq));
  printf("recvlist size = %d\n", info->recvlist_size);
    
  if (info->sentlist_first == NULL)
    printf("nothing on the sentdlist\n");
  else
    printf("sentlist goes from %ld to %ld\n", 
      ntohl(info->sentlist_first->chunk->u.sequenced.seq),
      ntohl(info->sentlist_last->chunk->u.sequenced.seq));
  printf("sentlist size = %d\n", info->sentlist_size);

  printf("current_seq = %ld, next_seq = %ld, rarr_seqbase = %ld, rarr_start = %d\n",
	 info->current_seq, info->next_seq, info->rarr_seqbase, info->rarr_start);

  idx = (info->rarr_start+((info->current_seq-info->rarr_seqbase)>>5))&
       SRDP_RARRMASK;
  msk = 1<<((info->current_seq-info->rarr_seqbase)&0x1f);
  refto = info->current_seq;
  
  printf("misslst is: ");
  if (!refto) printf("empty");
  else
  do {
    if ((info->recvdarray[idx]&msk) == 0)
	printf(" %ld", refto);
    if (msk>1)
      msk = msk>>1;
    else {
      msk = (srdp_u32)0x80000000;
      idx = (idx-1)&SRDP_RARRMASK;
    }
    refto--;
  } while (refto >= info->rarr_seqbase);
  printf("\n");
}

int srdp_showarr(struct srdp_info *info) {
  int idx;
  srdp_u32 refto, msk;

  idx = (info->rarr_start+((info->current_seq-info->rarr_seqbase)>>5))&
       SRDP_RARRMASK;
  msk = 1<<((info->current_seq-info->rarr_seqbase)&0x1f);
  refto = info->current_seq;
  
  printf("recvdarray is %.4lx %.4lx %.4lx %.4lx\n", info->recvdarray[0],
	 info->recvdarray[1], info->recvdarray[2], info->recvdarray[3]);
  printf("misslst is: ");
  if (!refto) printf("empty");
  else
  do {
    if ((info->recvdarray[idx]&msk) == 0)
	printf(" %ld", refto);
    if (msk>1)
      msk = msk>>1;
    else {
      msk = (srdp_u32)0x80000000;
      idx = (idx-1)&SRDP_RARRMASK;
    }
    refto--;
  } while (refto >= info->rarr_seqbase);
  printf("\n");
  return 0;
}

#endif

