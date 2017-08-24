# Makefile for srdp/utalk - Roger Espel Llima

#BINDIR = /usr/local/bin
#MANDIR = /usr/local/man
#LIBDIR = /usr/local/lib
#BINDIR = /usr/local/util/bin
#MANDIR = /usr/local/util/man
#LIBDIR = /usr/local/util/lib
BINDIR = /home/e/espel/bin
MANDIR = /home/e/espel/man
LIBDIR = /home/e/espel/lib

SRCS = srdp.c utalk.c util.c termcap.c termio.c globals.c signal.c screen.c\
       functions.c kbd.c comm.c rc.c menu.c
OBJS = srdp.o utalk.o util.o termcap.o termio.o globals.o signal.o screen.o\
       functions.o kbd.o comm.o rc.o menu.o
HEADS = comm.h kbd.h signal.h struct.h util.h functions.h rc.h srdp.h\
       termcap.h globals.h screen.h srdpdata.h termio.h menu.h

# The termcap library;  -ltermcap does the job on most systems, but if
# you have ncurses installed you may want to change it to -lncurses.
# On weird systems without a separate termcap library but with termcap
# emulation in curses, you may need to set it to -lcurses.
LIBS = -ltermcap

# Extra libraries; uncomment this for Solaris, change it for other machines
EXTRALIBS = -lsocket -lnsl

# Uncomment this on non-POSIX BSD machines (NeXT, Sequent...) if you
# have trouble compiling without it.
# OPT1 = -DUSE_SGTTY
# OPT2 = -DUSE_SIGVEC

# Uncomment this if you have trouble compiling because of sigaction()
# and USE_SIGVEC doesn't work:
# OPT3 = -DNO_SIGACTION

# Uncomment this if you want eight-bit-stripping on by default
# OPT4 = -DSEVEN_BIT

OPT5 = -DLIBDIR="\"$(LIBDIR)\""
OPTS = $(OPT1) $(OPT2) $(OPT3) $(OPT4) $(OPT5)

#CFLAGS = -g -O -D__USE_FIXED_PROTOTYPES__ $(OPTS)
#CFLAGS = -g -Wall -DDEBUG -D__USE_FIXED_PROTOTYPES__ $(OPTS)
CFLAGS = -g -Wall -D__USE_FIXED_PROTOTYPES__ $(OPTS)
#LDFLAGS = -g

CC = gcc

ROFF = nroff

all:	utalk

utalk.cat:	utalk.1
		$(ROFF) -man utalk.1 > utalk.cat

utalk:	$(OBJS)
	$(CC) $(LDFLAGS) -o utalk $(OBJS) $(LIBS) $(EXTRALIBS)

test:	stest.o srdp.o
	$(CC) $(LDFLAGS) -o stest stest.o $(LIBS) $(EXTRALIBS)

install:	utalk utalk.1
	# -strip utalk
	-umask 022; mkdir $(BINDIR) 2>/dev/null
	-umask 022; mkdir $(MANDIR) 2>/dev/null
	-umask 022; mkdir $(LIBDIR) 2>/dev/null
	-umask 022; mkdir $(MANDIR)/man1 2>/dev/null
	cp utalk $(BINDIR)
	chmod 755 $(BINDIR)/utalk
	cp utalk.1 $(MANDIR)/man1
	chmod 644 $(MANDIR)/man1/utalk.1
	cp utalk.help $(LIBDIR)
	chmod 644 $(LIBDIR)/utalk.help

clean:
	rm -f $(OBJS) stest.o stest utalk core

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

