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
	mem.c \
	scroll.c \
	vt100.c \
	gtalk.c \
	header.h \
	cwin.h \
	mem.h \
	socket.h \
	ymenu.h \
	gtalk.h

ytalk_LDADD = ../libytk/libytk.a $(INTLLIBS)

INCLUDES = -I../libytk -I../intl
LIBS = @LIBS@

DEFS = @DEFS@ @SYSTEM_DEFS@
