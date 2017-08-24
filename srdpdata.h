/* SRDP, Semi-Reliable Datagram Protocol
   Copyright (C) 1995 Roger Espel Llima

   srdpdata.h

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation. See the file LICENSE for details.
*/

#ifndef SRDPDATA_H
#define SRDPDATA_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

/* Change these if necessary, to match your computer system;  the 3 must
   be unsigned, of respective sizes 32, 16 and 8 bits.
 */

typedef unsigned int srdp_u32;
typedef unsigned short srdp_u16;
typedef unsigned char srdp_u8;


#define SRDP_VERSION	0x01
#define SRDP_REVISION	0x00

#define SRDP_PING	0xff
#define SRDP_PINGREP	0xfd
#define SRDP_MISSLST	0xfb
#define SRDP_CURRENT	0xf9
#define SRDP_OLDEST	0xf7
#define SRDP_ALIVE	0xf5
#define SRDP_CLOSE	0xfe
#define SRDP_DROP	0xfc

struct srdp_chunk {
  union {
    struct {
      srdp_u8 version;			/* filled in by srdp */
      srdp_u8 revision;			/* filled in by srdp */
      srdp_u8 hi_version;		/* to be filled by the application */
      srdp_u8 type;			/* to be filled by the application */
      srdp_u32 len;			/* to be filled by the application */
      srdp_u32 seq;			/* filled in by srdp */
      char data;			/* to be filled by the application */
    } sequenced;
    struct {
      srdp_u8 version;
      srdp_u8 revision;
      srdp_u8 hi_version;
      srdp_u8 type;
      srdp_u32 len;
      char data;
    } noseq;
  } u;
};

#define SRDP_HEADER_USEQ_LEN	8
#define SRDP_HEADER_SEQ_LEN	12

struct srdp_chunk_list {
  struct srdp_chunk_list *prev;
  struct srdp_chunk_list *next;
  int lsize;			/* to evaluate the size of the list */
  struct srdp_chunk *chunk;
};

#define SRDP_FL_READY	0x01	/* set by srdp_select() if there's something
				   to read */
#define SRDP_FL_RECVD	0x02	/* set by srdp_select() if we've ever
				   received anything */
#define SRDP_FL_BROKEN  0x04	/* set by srdp_selct() if we haven't received
				   anything for a while */
#define SRDP_FL_NOSELECT 0x08	/* set _by_the_application_ if it doesn't
				   want srdp_select() to return just b/c
				   there's something to srdp_read() here */
#define SRDP_FL_CLOSED	0x10	/* set by srdp_select(), srdp_read() and
				   srdp_drop() and srdp_close() if the
				   connection is closed; the data in the
				   struct info can be reused */
#define SRDP_FL_SELRETURN 0x20	/* set _by_the_application_ if it wants
				   srdp_select() to return even if there
				   wasn't any data for the application,
				   whenever something is read from the
				   socket (the flags may have changed).
				   ignored if SRDP_FL_NOSELECT is set */


#define SRDP_RARRSIZE	64  	/* number of u32's in the binary array of
				   received packets; must be a power of 2 */
#define SRDP_RARRMASK	(SRDP_RARRSIZE-1)


#define SRDP_DEFL_MAX_RCV	32768
#define SRDP_DEFL_MAX_SNT	131072

#define SRDP_DEFL_MAX_PACKET	1472

#define SRDP_CURR_1_TM	4
#define SRDP_CURR_NXT_TM	8
#define SRDP_MISS_TM	5
#define SRDP_OLDEST_TM	5
#define SRDP_ALIVE_TM	10
#define SRDP_BROKEN_TM	30

struct srdp_info {
  /* these should be filled out before calling srdp_open(), with
     0 meaning "any" */
  struct in_addr remote;
  u_short remoteport;		/* in host order, 0=any */
  u_short localport;		/* in host order, 0=any */

  /* these should be filled before srdp_open() with either the values
     or 0 for the recommended defaults */
  int max_rcv_lst_size, max_snt_lst_size;	/* in bytes */
  int max_packet_len;	/* in bytes */
  int curr_1_time;	/* seconds between get packet and send SRDP_CURRENT */
  int curr_nxt_time;    /* seconds between SRDP_CURRENT and next one */
  int miss_time;	/* min seconds between two SRDP_MISSLIST */
  int oldest_time;	/* min seconds between two SRDP_OLDEST */
  int alive_time;	/* seconds of inactivity before sending SRDP_ALIVE */
  int broken_time;	/* seconds of not getting anything before raising
  			   FL_BROKEN */
  int flags;		/* the application can set one of two special modes */

  /* these will be filled in by srdp_open() */
  int sockfd;

  /* the remaining fields shouldn't be used by the clients */
  struct sockaddr_in myaddr;

  struct srdp_chunk_list *recvlist_first;
  struct srdp_chunk_list *recvlist_last;
  struct srdp_chunk_list *sentlist_first;
  struct srdp_chunk_list *sentlist_last;
  int recvlist_size;
  int sentlist_size;

  srdp_u32 current_seq;	/* greatest seq number received so far */
  srdp_u32 next_seq;	/* next seq number to send */
  srdp_u32 *recvdarray; /* bitmapped array of recvd packets, 
			   size = power of 2 */
  srdp_u32 rarr_seqbase;
  int rarr_start;	/* recvdarray[rarr_start] refers to rarr_seqbase,
  			   and the array wraps; less significant bits refer
			   to smaller seq numbers  */

  int current_sent;	/* 1 if we've sent a current since the last recvd
			   packet */
 
  struct timeval lastheard;
  struct timeval lastsentsomething;
  struct timeval lastsentcurrent;
  struct timeval lastsentlist;
  struct timeval lastsentoldest;

  struct timeval wakeup1;	/* time at which we need to wake up to
  				   send a CURRENT */
  struct timeval wakeup2;	/* time to wake up to send a ALIVE */
};

struct srdp_infolist {
  struct srdp_info *theinfo;
  struct srdp_infolist *next;
};

#define CHUNKLIST_EXTRA 32
#define MAXPACKETLEN    9000

#endif

