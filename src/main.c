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
#include "cwin.h"
#include "ymenu.h"
#include "mem.h"

#include <signal.h>

char errstr[MAXERR];		/* temporary string for errors */
char msgstr[MAXERR];		/* temporary string for bottom messages */
char *vhost = NULL;		/* specified virtual host */
ylong myuid;			/* global uid */
char *gshell = NULL;		/* global shell */
#ifdef YTALK_COLOR
int ui_colors = 40;		/* TODO: change default */
int ui_attr = 0;		/* UI output attributes */
int menu_colors = 0;		/* TODO: change default */
int menu_attr = 0;
#endif
char *title_format = NULL;
char *user_format = NULL;

/*
 * Clean up and exit.
 */
void
bail(int n)
{
	kill_auto();
	if (n == YTE_SUCCESS_PROMPT && (def_flags & FL_PROMPTQUIT)) {
		if (show_message_ymenu(_("Press any key to quit.")) == 0) {
			update_ymenu();
			bail_loop();
		}
	}
	end_term();
	free_users();
	if (gshell != NULL)
		free_mem(gshell);
	if (title_format != NULL)
		free_mem(title_format);
	if (user_format != NULL)
		free_mem(user_format);
#ifdef YTALK_DEBUG
	clear_all();
#endif
	exit(n == YTE_SUCCESS_PROMPT ? YTE_SUCCESS : n);
}

#ifndef HAVE_STRERROR
#define strerror(n)	(sys_errlist[(n)])
#endif

/*
 * Display an error.
 */
void
show_error(char *str)
{
	register char *syserr;
	static int in_error = 0;

	if (errno == 0)
		syserr = "";
	else
		syserr = strerror(errno);

	if (def_flags & FL_BEEP)
		putc(7, stderr);
	if (in_error == 0 && what_term() != 0 && can_ymenu()) {
		in_error = 1;
		if (show_error_ymenu(str, syserr) < 0) {
			show_error("show_error: show_error_ymenu() failed");
			show_error(str);
		} else
			update_ymenu();
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
str_copy(char *str)
{
	register char *out;
	register int len;

	if (str == NULL)
		return NULL;
	len = strlen(str) + 1;
	out = get_mem(len);
	memcpy(out, str, len);
	return out;
}

/*
 * Process signals.
 */
static RETSIGTYPE
got_sig(int n)
{
	if (n == SIGINT) {
		if (def_flags & FL_IGNBRK)
			return;
		bail(YTE_SUCCESS);
	}
	bail(YTE_SIGNAL);
}

/* MAIN  */
int
main(int argc, char **argv)
{
	int sflg = 0, yflg = 0, iflg = 0, vflg = 0, qflg = 0, hflg = 0;
	char *prog, *c;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

#ifdef YTALK_DEBUG
	/* check for a 64-bit mis-compile */
	if (sizeof(ylong) != 4) {
		fprintf(stderr, "The definition for ylong in header.h is wrong;\n\
please change it to an unsigned 32-bit type that works on your computer,\n\
then type 'make clean' and 'make'.\n");
		exit(YTE_INIT);
	}
#endif

	/* search for options */

	prog = *argv;
	argv++, argc--;

	while (argc > 0 && **argv == '-') {
		for (c = (*argv) + 1; *c; c++) {
			switch (*c) {
			case 'Y': yflg++; break;
			case 'i': iflg++; break;
			case 's': iflg++; break;
			case 'q': qflg++; break;
			case 'v': vflg++; break;
			case 'h': hflg++; break;
			default:
				fprintf(stderr, _("Unknown option '%c'\n"), *c);
				return YTE_INIT;
			}
		}
		argv++, argc--;
	}

	if (vflg) {
		/* print version and exit */
		fprintf(stderr, "YTalk %d.%d.%d\n", VMAJOR, VMINOR, VPATCH);
		exit(YTE_SUCCESS);
	}
	/* check for users */

	if (argc <= 0 || hflg) {
		fprintf(stderr,
			_("Usage:    %s [options] user[@host][#tty]...\n\
Options:     -i             --    no auto-invite port\n\
             -Y             --    require caps on all y/n answers\n\
             -s             --    start a shell\n\
             -q             --    prompt before quitting\n\
             -v             --    print program version\n\
             -h             --    show this help message\n"), prog);
		exit(YTE_INIT);
	}
	/* check that STDIN is a valid tty device */

	if (!isatty(0)) {
		fprintf(stderr, _("Standard input is not a valid terminal device.\n"));
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
	if (yflg)
		def_flags |= FL_CAPS;
	if (iflg)
		def_flags |= FL_NOAUTO;
	if (qflg)
		def_flags |= FL_PROMPTQUIT;

	init_term();
	init_ymenu();
	init_socket();
	for (; argc > 0; argc--, argv++)
		invite(*argv, 1);
	if (sflg)
		execute(NULL);
	else {
		msg_term(_("Waiting for connection..."));
		redraw_all_terms();
	}
	main_loop();
	bail(YTE_SUCCESS_PROMPT);

	return 0;		/* make lint happy */
}
