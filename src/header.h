/*
 * src/header.h
 *
 * YTalk
 *
 * Copyright (C) 1990,1992,1993 Britt Yenne
 * Currently maintained by Andreas Kling
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "config.h"

#ifdef HAVE_NCURSES_H
#  include <ncurses.h>
#else
#  include <curses.h>
#endif

#ifdef HAVE_STDBOOL_H
#  include <stdbool.h>
#else
typedef unsigned char bool;
#  define true 1
#  define false 0
#endif

#include <sys/types.h>
#include <sys/param.h>

#ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  ifdef HAVE_SYS_TIME
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#ifdef YTALK_HPUX
#  define _XOPEN_SOURCE_EXTENDED
#  include <sys/socket.h>
#  undef _XOPEN_SOURCE_EXTENDED
#else
#  include <sys/socket.h>
#endif

#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

/* We may not have prototypes for these functions. */
#if defined(HAVE_PTSNAME) && defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)
extern int grantpt(int);
extern int unlockpt(int);
extern char *ptsname(int);
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#define VMAJOR	4		/* major version number */
#define VMINOR	0		/* minor version number */
#define VPATCH	0		/* patch level */

#define EIGHT_BIT_CLEAN		/* take this one out if you don't want it */

#ifdef EIGHT_BIT_CLEAN
#  define is_printable(x)  ( (((x) >= ' ' && (x) <= '~') || \
			    (unsigned char)(x) >= 0xa0) \
			  && (x) != RUBDEF )
#else
#  define is_printable(x) ( (x) >= ' ' && (x) <= '~' )
#endif

/* ---- types ---- */

typedef void *yaddr;		/* any pointer address */
typedef yaddr yterm;		/* terminal cookie */
typedef unsigned char ychar;		/* we use unsigned chars */

#define BITFIELD(name, size) unsigned int name : size

#define YATTR_BOLD      0x01
#define YATTR_DIM       0x02
#define YATTR_UNDERLINE 0x04
#define YATTR_BLINK     0x08
#define YATTR_REVERSE   0x10

typedef struct {
	unsigned char data;
	BITFIELD(background, 4);
	BITFIELD(foreground, 4);
	BITFIELD(attributes, 7);
	BITFIELD(alternate_charset, 1);
} yachar;

#ifdef HAVE_STDINT_H
typedef uint32_t ylong;
#else
/* this should work both on 32-bit and 64-bit machines  -Roger */
typedef unsigned int ylong;
#endif

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#ifdef YTALK_OSF1		/* this is to avoid a burst of warnings when */
typedef int ysocklen_t;		/* compiling on OSF/1 based systems. */
#else
#  define ysocklen_t	socklen_t
#endif

typedef struct {
	unsigned char w_rows, w_cols;	/* window size FOR PROTOCOL YTP_OLD */
	char protocol;		/* ytalk protocol -- see above */
	char pad1;		/* zeroed out */
	short vmajor, vminor;	/* version numbers */
	unsigned short rows, cols;	/* his window size over there */
	unsigned short my_rows, my_cols;	/* my window size over there */
	ylong pid;		/* my process id */
	char pad[44];		/* zeroed out */
} y_parm;

#define MAXARG	8		/* max ESC sequence arg count */

typedef struct _yuser {
	struct _yuser *next;	/* next user in group lists */
	struct _yuser *unext;	/* next user in full user list */
	int fd;			/* file descriptor */
	ylong flags;		/* active FL_* flags below */
	ychar edit[3];		/* edit characters */
	unsigned short t_rows, t_cols;	/* his rows and cols on window over here */
	unsigned short rows, cols;	/* his active region rows and cols over here */
	y_parm remote;		/* remote parms */
	yachar **scrollback;	/* scrollback buffer */
	long int scrollend;             /* highest used index in scrollback buffer */
	long int scrollpos;	/* position in scrollback buffer */
	yachar **scr;		/* screen data */
	int *scr_tabs;		/* screen tab positions */
	ychar old_rub;		/* my actual rub character */
	char key;		/* this user's ident letter for menus */
	int y, x;		/* current cursor position */
	int sy, sx;		/* saved cursor position */
	int sc_top, sc_bot;	/* scrolling region */
	BITFIELD(region_set, 1);        /* set if using a screen region */
	BITFIELD(csx, 1);               /* set if in charset crossover mode */
	BITFIELD(altchar, 1);           /* set if in alternate charset mode */
	BITFIELD(bump, 1);              /* set if at far right */
	BITFIELD(onend, 1);             /* set if we are stomping on the far right */
	BITFIELD(crlf, 1);              /* 1 if users wants CRLF data */
	BITFIELD(scroll, 1);            /* set if currently being scrolled */
	yachar yac;
	yachar saved_yac;
	char *full_name;	/* full name (up to 50 chars) */
	char *user_name;	/* user name */
	char *host_name;	/* host name */
	char *host_fqdn;	/* host fully qualified name */
	char *tty_name;		/* tty name */
	ylong host_addr;	/* host inet address */
	int daemon;		/* daemon type to use */
	ylong l_id, r_id;	/* local and remote talkd invite list index */
	ylong d_id;		/* talk daemon process id -- see socket.c */
	ylong last_invite;	/* timestamp of last invitation sent */
	struct sockaddr_in sock;/* communication socket */
	struct sockaddr_in orig_sock;	/* original socket -- another sick
					 * hack */
	/* out-of-band data */

	int dbuf_size;		/* current buffer size */
	ychar *dbuf, *dptr;	/* buffer base and current pointer */
	int drain;		/* remaining bytes to drain */
	void (*dfunc) (struct _yuser *, void *);	/* function to call with drained data */
	int got_oob;		/* got OOB flag */

	struct {
		short int av[MAXARG];	/* ESC sequence arguments */
		unsigned int ac;	/* ESC sequence arg count */
		BITFIELD(lparen, 1);    /* lparen escape? */
		BITFIELD(hash, 1);      /* hash escape? */
		BITFIELD(got_esc, 3);   /* received an ESC */
	} vt;

	struct {
		char *buf;	/* gtalk string buffer */
		char type;	/* type of current gtalk string */
		int len;	/* length of current gtalk string */
		char *version;	/* user's GNUTALK version */
		char *system;	/* user's *NIX flavor */
		BITFIELD(got_gt, 1);
	} gt;

	void *term;                     /* terminal cookie */

} yuser;

typedef struct _ywin {
	struct _ywin *next;             /* next ywin in linked list */
	yuser *user;                    /* user pointer */
	WINDOW *win;                    /* window pointer */
	WINDOW *swin;                   /* scroll viewport window */
	int height, width;              /* height and width in characters */
	int row, col;                   /* row and column position on screen */
} ywin;

#define FL_RAW		0x00000001L	/* raw input enabled */
#define FL_SCROLL	0x00000002L	/* scrolling enabled */
#define FL_WRAP		0x00000004L	/* word-wrap enabled */
#define FL_IMPORT	0x00000008L	/* auto-import enabled */
#define FL_INVITE	0x00000010L	/* auto-invite enabled */
#define FL_RING		0x00000020L	/* rering at all */
#define FL_CAPS		0x00000040L	/* want caps as answers */
#define FL_NOAUTO       0x00000080L	/* no auto-invite port */
#define FL_PROMPTRING	0x00000100L	/* prompt before reringing */
#define FL_BEEP		0x00000200L	/* allow ytalk to beep? */
#define FL_IGNBRK	0x00000400L	/* don't die when ^C is pressed */
#define FL_PROMPTQUIT	0x00001000L	/* prompt before quitting */
#define FL_PERSIST	0x00002000L	/* don't quit when alone */
#define FL_LOCKED	0x40000000L	/* flags locked by other end */

/* ---- defines and short-cuts ---- */

#ifdef NOFILE
#define MAX_FILES	NOFILE	/* max open file descriptors */
#else
#define MAX_FILES	256	/* better to be too high than too low */
#endif
#define CLEAN_INTERVAL	16	/* seconds between calls to house_clean() */
#define MAXBUF		4096	/* buffer size for I/O operations */
#define MAXERR		132	/* error text buffer size */
#define MAXTEXT		50	/* text entry buffer */

#define RUB	edit[0]
#define KILL	edit[1]
#define WORD	edit[2]
#define RUBDEF	0xfe

#define ALTESC	0x18		/* alternate escape key (^X) */

/* ---- exit codes ---- */

#define YTE_SUCCESS_PROMPT -1	/* successful completion, prompt, return 0 */
#define YTE_SUCCESS	0	/* successful completion */
#define YTE_INIT	1	/* initialization error */
#define YTE_NO_MEM	2	/* out of memory */
#define YTE_SIGNAL	3	/* fatal signal received */
#define YTE_ERROR	4	/* unrecoverable error */

/* ---- global variables ---- */

#ifndef HAVE_STRERROR
extern char *sys_errlist[];	/* system errors */
#endif

extern yuser *me;		/* just lil' ol' me */
extern yuser *user_list;	/* full list of invited/connected users */
extern yuser *connect_list;	/* list of connected users */
extern yuser *wait_list;	/* list of invited users */
extern yuser *fd_to_user[MAX_FILES];	/* convert file descriptors to users */
extern char errstr[MAXERR];	/* temporary string for errors */
extern char msgstr[MAXERR];	/* temporary string for messages */
extern ylong def_flags;		/* default FL_* flags */
extern int user_winch;		/* user window/status changed flag */

extern ychar *io_ptr;		/* user input pointer */
extern int io_len;		/* user input count */

extern yuser *scuser;		/* user being scrolled */
extern long int scrollback_lines;	/* max number of scrollback lines */

extern bool running_process;        /* flag: is process running? */
extern char *gshell;		/* stores your shell */
extern char *title_format;
extern char *user_format;
extern char *bottom_msg;	/* current status message */
extern ylong bottom_time;	/* timestamp of the above */

/* ---- some machine compatibility definitions ---- */

#ifndef errno
extern int errno;
#endif

/* aliases -- added by Roger Espel Llima (borrowed from utalk) */

struct alias {
	char from[256], to[256];
	int type;
	struct alias *next;
};

#define ALIAS_ALL       0
#define ALIAS_BEFORE    1
#define ALIAS_AFTER     2

/* ---- global functions ---- */

/* main.c */
extern void bail(int);
extern char *str_copy(char *);
extern void show_error(char *);
extern int ya_strlen(yachar *);
#ifndef HAVE_SNPRINTF
int snprintf(char *buf, size_t len, char const *fmt, ...);
#endif

/* term.c */
extern void init_term();
extern void set_terminal_size(int, int, int);
extern void set_terminal_flags(int);
extern int what_term();
extern void end_term();
extern int open_term(yuser *);
extern void close_term(yuser *);
extern void addch_term(yuser *, ychar);
extern void move_term(yuser *, int, int);
extern void fill_term(yuser *user, int y1, int x1, int y2, int x2, ychar c);
extern void clreol_term(yuser *);
extern void clreos_term(yuser *);
extern void scroll_term(yuser *);
extern void rev_scroll_term(yuser *);
extern void rub_term(yuser *);
extern void word_term(yuser *);
extern void kill_term(yuser *);
extern void tab_term(yuser *);
extern void newline_term(yuser *);
extern void lf_term(yuser *);
extern void add_line_term(yuser *, int);
extern void del_line_term(yuser *, int);
extern void add_char_term(yuser *, int);
extern void del_char_term(yuser *, int);
extern void redraw_term(yuser *, int);
extern void keypad_term(yuser *, int);
extern void resize_win(yuser *, int, int);
extern void set_win_region(yuser *, int, int);
extern void end_win_region(yuser *);
extern void set_scroll_region(yuser *, int, int);
extern void msg_term(char *);
extern void spew_term(yuser *, int, int, int);
extern void spew_line(int, yachar *, int);
extern void spew_free();
extern char *get_tcstr(char *);

#define set_raw_term			set_raw_curses
#define set_cooked_term			set_cooked_curses
#define start_scroll_term		start_scroll_curses
#define end_scroll_term			end_scroll_curses
#define update_scroll_term		update_scroll_curses
#define retitle_all_terms		retitle_all_curses
#define redraw_all_terms		redisplay_curses
#define flush_term			flush_curses

/* user.c */
extern void init_user(char *);
extern yuser *new_user(char *, char *, char *);
extern void free_user(yuser *);
extern yuser *find_user(char *, ylong, ylong);
extern void generate_full_name(yuser *);
extern void free_users();
extern void user_title(yuser *, char *, int);
extern void save_user_to_file(yuser *, char *);

/* fd.c */
extern void init_fd();
extern void add_fd(int, void (*) (int));
extern void remove_fd(int);
extern int full_read(int, char *, size_t);
extern void main_loop();
extern void input_loop();
extern void bail_loop();

/* comm.c */
extern yuser *invite(char *, int);
extern void house_clean();
extern void send_winch(yuser *);
extern void send_region();
extern void send_end_region();
extern void send_users(yuser *, ychar *, int, ychar *, int);
extern void show_input(yuser *, ychar *, int);
extern void my_input(yuser *, ychar *, int);
extern void lock_flags(ylong);
extern void unlock_flags();
extern void rering_all();

/* socket.c */
extern void init_socket();
extern void close_all();
extern int send_dgram(yuser *, unsigned char);
extern int send_auto(unsigned char);
extern void kill_auto();
extern int newsock(yuser *);
extern int connect_to(yuser *);
extern ylong get_host_addr(char *);
extern char *host_name(ylong);

/* rc.c */
extern void read_ytalkrc();
extern char *resolve_alias(char *);

/* exec.c */
extern void execute(char *);
extern void update_exec();
extern void kill_exec();
extern void winch_exec();

/* getpty.c */
extern int getpty(char *);

/* vt100.c */
extern void vt100_process(yuser *, char);

/* scroll.c */
extern void init_scroll(yuser *);
extern void free_scroll(yuser *);
extern void scroll_up(yuser *);
extern void scroll_down(yuser *);
extern int scrolled_amount(yuser *);
extern void scroll_to_bottom(yuser *);
extern void scroll_add_line(yuser *, yachar *);
#define scrolling(user)				((user)->scroll == 1)
#define enable_scrolling(user)		((user)->scroll = 1)
#define disable_scrolling(user)		((user)->scroll = 0)

/* EOF */
