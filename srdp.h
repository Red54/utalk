/* SRDP, Semi-Reliable Datagram Protocol 
   Copyright (C) 1995 Roger Espel Llima 

   srdp.h 

   Started: 19 Oct 95 by <roger.espel.llima@pobox.com>

   This program is free software; you can redistribute it and/or modify 
   it under the terms of the GNU General Public License as published by 
   the Free Software Foundation. See the file LICENSE for details. 
*/

#ifndef SRDP_H
#define SRDP_H

#include "srdpdata.h"
#include <sys/time.h>

extern int srdp_open(struct srdp_info *info); 
     /* opens the socket, adds it to the global select table, returns
	0 if ok, -1 if error */

extern int srdp_select(int width, fd_set *readfds, fd_set *writefds,
		       fd_set *exceptfds, struct timeval *timeout);
     /* does a select on the given fds + on any open sockets, and updates
	the SRDP_FL_READY flag on any sockets that are ready for reading
	(which actually means there are data chunks already read) */

extern int srdp_read(struct srdp_info *info, struct srdp_chunk *chunk,
		      int csize);
     /* reads one data chunk (from the srdp buffers) and returns 1 if ok,
        or 0 if there was none to read, or 2 if the packet was
        truncated, -1 if the connection is closed; updates SRDP_FL_READY */
     /* the fields sequenced.seq and sequenced|noseq.len get returned in
	network order! */

extern int srdp_write(struct srdp_info *info, struct srdp_chunk *chunk);
     /* sends out a data chunk; returns -1 if error, 0 if ok */
     /* the field sequenced|noseq.len must be set in network order! */

extern void srdp_synch(struct srdp_info *info);
     /* sends out an SRDP_CURRENT and SRDP_MISSLST */

extern void srdp_close(struct srdp_info *info, char *message);
     /* attempts to gracefully close the connection; may block for
        a few seconds */

extern void srdp_drop(struct srdp_info *info, char *message);
     /* drops the connection; will not block */


#endif

