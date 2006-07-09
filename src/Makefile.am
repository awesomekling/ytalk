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
	getpty.c \
	cwin.c \
	scroll.c \
	vt100.c \
	gtalk.c \
	header.h \
	cwin.h \
	socket.h \
	ymenu.h \
	gtalk.h

ytalk_LDADD = ../libytk/libytk.a

INCLUDES = -I../libytk
LIBS = @LIBS@

DEFS = @DEFS@ @SYSTEM_DEFS@
