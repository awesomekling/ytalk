/* main.c */

/*
 * NOTICE
 *
 * Copyright (c) 1990,1992,1993 Britt Yenne.  All rights reserved.
 *
 * This software is provided AS-IS.  The author gives no warranty, real or
 * assumed, and takes no responsibility whatsoever for any use or misuse of
 * this software, or any damage created by its use or misuse.
 *
 * This software may be freely copied and distributed provided that no part of
 * this NOTICE is deleted or edited in any manner.
 *
 */


#include "header.h"
#include <signal.h>
#include "menu.h"
#include "mem.h"

char errstr[132];		/* temporary string for errors */
char *vhost = NULL;		/* specified virtual host */
ylong myuid;			/* global uid */
#ifdef YTALK_COLOR
int newui_colors = 40;		/* TODO: change default */
int newui_attr = 0;		/* newui output attributes */
int raw_color = 2;		/* raw output color */
int raw_attr = 0;		/* raw output attributes */
#endif

/*
 * Clean up and exit.
 */
void
bail(n)
	int n;
{
	end_term();
	kill_auto();
	free_users();
	clear_all();
	(void) exit(n);
}

#ifndef HAVE_STRERROR
#define strerror(n)	(sys_errlist[(n)])
#endif

/*
 * Display an error.
 */
void
show_error(str)
	register char *str;
{
	register char *syserr;
	static int in_error = 0;

	if (errno == 0)
		syserr = "";
	else
		syserr = strerror(errno);

	if (def_flags & FL_BEEP)
		putc(7, stderr);
	if (in_error == 0 && what_term() != 0) {
		in_error = 1;
		if (show_error_menu(str, syserr) < 0) {
			show_error("show_error: show_error_menu() failed");
			show_error(str);
		} else
			update_menu();
		in_error = 0;
	} else {
		fprintf(stderr, "%s: %s\n", str, syserr);
		sleep(2);
	}
}

/*
 * Copy a string.
 */
char *
str_copy(str)
	register char *str;
{
	register char *out;
	register int len;

	if (str == NULL)
		return NULL;
	len = strlen(str) + 1;
	out = get_mem(len);
	(void) memcpy(out, str, len);
	return out;
}

/*
 * Process signals.
 */
static RETSIGTYPE
got_sig(n)
	int n;
{
	if (n == SIGINT) {
		if (def_flags & FL_IGNBRK)
			return;
		bail(0);
	}
	bail(YTE_SIGNAL);
}

/* MAIN  */
int
main(argc, argv)
	int argc;
	char **argv;
{
#ifdef USE_X11
	int xflg = 0;
#endif
	int sflg = 0, yflg = 0, iflg = 0, vflg = 0;
	char *prog;

#ifdef YTALK_DEBUG
	/* check for a 64-bit mis-compile */
	if (sizeof(ylong) != 4) {
		fprintf(stderr, "The definition for ylong in header.h is wrong;\n\
please change it to an unsigned 32-bit type that works on your computer,\n\
then type 'make clean' and 'make'.\n");
		(void) exit(YTE_INIT);
	}
#endif

	/* search for options */

	prog = *argv;
	argv++, argc--;
	while (argc > 0 && **argv == '-') {
		if (strcmp(*argv, "-Y") == 0) {
			yflg++;
			argv++, argc--;
		}
#ifdef USE_X11
		else if (strcmp(*argv, "-x") == 0) {
			xflg++;	/* enable X from the command line */
			argv++, argc--;
		}
#endif
		else if (strcmp(*argv, "-i") == 0) {
			iflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-v") == 0) {
			vflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-h") == 0) {
			argv++;
			vhost = *argv++;
			argc -= 2;
		} else if (strcmp(*argv, "-s") == 0) {
			sflg++;	/* immediately start a shell */
			argv++, argc--;
		} else
			argc = 0;	/* force a Usage error */
	}

	if (vflg) {
		/* print version and exit */
		fprintf(stderr, "YTalk %d.%d.%d\n", VMAJOR, VMINOR, VPATCH);
		(void) exit(YTE_SUCCESS);
	}
	/* check for users */

	if (argc <= 0) {
		fprintf(stderr,
			"Usage:    %s [options] user[@host][#tty]...\n\
Options:     -i             --    no auto-invite port\n"
#ifdef USE_X11
		   "             -x             --    use the X interface\n"
#endif
			"             -Y             --    require caps on all y/n answers\n\
             -s             --    start a shell\n\
             -v             --    print program version\n\
             -h host_or_ip  --    select interface or virtual host\n", prog);
		(void) exit(YTE_INIT);
	}
	/* check that STDIN is a valid tty device */

	if (!isatty(0)) {
		fprintf(stderr, "Standard input is not a valid terminal device.\n");
		exit(1);
	}
	/* set up signals */

	signal(SIGINT, got_sig);
	signal(SIGHUP, got_sig);
	signal(SIGQUIT, got_sig);
	signal(SIGABRT, got_sig);
	signal(SIGPIPE, SIG_IGN);

	/* save the uid for later use */
	myuid = getuid();

	/* set default options */

	def_flags = FL_PROMPTRING | FL_RING | FL_BEEP | FL_SCROLL;

	/* go for it! */

	errno = 0;
	init_fd();
	read_ytalkrc();
	init_user(vhost);
#ifdef USE_X11
	if (xflg)
		def_flags |= FL_XWIN;
#endif
	if (yflg)
		def_flags |= FL_CAPS;
	if (iflg)
		def_flags |= FL_NOAUTO;

	init_term();
	init_socket();
	for (; argc > 0; argc--, argv++)
		invite(*argv, 1);
	if (sflg)
		execute(NULL);
	else
		msg_term(me, "Waiting for connection...");
	main_loop();
	bail(YTE_SUCCESS);

	return 0;		/* make lint happy */
}
