bin_PROGRAMS = ytalk

ytalk_SOURCES = \
	main.c \
	term.c \
	user.c \
	fd.c \
	comm.c \
	ymenu.c \
	socket.c \
	rc.c \
	exec.c \
	cwin.c \
	mem.c \
	scroll.c \
	vt100.c \
	header.h \
	cwin.h \
	mem.h \
	socket.h \
	ymenu.h

ytalk_LDADD = ../libytk/libytk.a $(INTLLIBS)

INCLUDES = -I../libytk
LIBS = @LIBS@

DEFS = @DEFS@ @SYSTEM_DEFS@
