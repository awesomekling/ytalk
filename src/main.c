/*
 * src/main.c
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

#include "header.h"
#include "cwin.h"
#include "ymenu.h"

#include <signal.h>

char errstr[MAXERR];		/* temporary string for errors */
char msgstr[MAXERR];		/* temporary string for bottom messages */
char *vhost = NULL;		/* specified virtual host */
char *gshell = NULL;		/* global shell */
unsigned long int ui_colors = 40;		/* TODO: change default */
unsigned long int ui_attr = 0;		/* UI output attributes */
unsigned long int menu_colors = 0;		/* TODO: change default */
unsigned long int menu_attr = 0;
char *title_format = NULL;
char *user_format = NULL;

bool track_idle_time = false;
static void
sigalrm_handle(int signal)
{
	if (!track_idle_time) {
		alarm(0);
		return;
	}

	retitle_all_terms();
	alarm(1);
}

/*
 * Clean up and exit.
 */
void
bail(int n)
{
	kill_auto();
	if (n == YTE_SUCCESS_PROMPT && (def_flags & FL_PROMPTQUIT)) {
		if (show_error_ymenu("Press any key to quit.", NULL, NULL) == 0) {
			update_ymenu();
			bail_loop();
		}
	}
	end_term();
	free_users();
	if (gshell != NULL)
		free(gshell);
	if (title_format != NULL)
		free(title_format);
	if (user_format != NULL)
		free(user_format);
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
	char *syserr;
	static bool in_error = false;

	syserr = errno ? strerror(errno) : NULL;

	if (def_flags & FL_BEEP)
		putc(7, stderr);
	if (!in_error && what_term() != 0 && can_ymenu()) {
		in_error = true;
		if (show_error_ymenu(str, syserr, "Error") < 0) {
			show_error("show_error: show_error_ymenu() failed");
			show_error(str);
		} else
			update_ymenu();
		in_error = false;
	} else {
		if (syserr)
			fprintf(stderr, "%s: %s\n", str, syserr);
		else
			fprintf(stderr, "%s\n", str);
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
	out = malloc(len);
	memcpy(out, str, len);
	return out;
}

int
ya_strlen(yachar *str)
{
	int ret;
	for (ret = 0; str->data; ++str, ++ret);
	return ret;
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
	bool flag_v = false;
	bool flag_h = false;
	char *c;

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

	argv++, argc--;

	while (argc > 0 && **argv == '-') {
		for (c = (*argv) + 1; *c; c++) {
			switch (*c) {
			case 'v': flag_v = true; break;
			case 'h': flag_h = true; break;
			default:
				fprintf(stderr, "Unknown option '%c'\n", *c);
				return YTE_INIT;
			}
		}
		argv++, argc--;
	}

	if (flag_v) {
		/* print version and exit */
		fprintf(stderr, "YTalk %s\n", PACKAGE_VERSION);
		exit(YTE_SUCCESS);
	}

	/* set default options */

	def_flags = FL_PROMPTRING | FL_RING | FL_BEEP | FL_SCROLL;

	/* read user/system configuration */

	read_ytalkrc();

	/* check for users */

	if (flag_h || (!(def_flags & FL_PERSIST) && (argc <= 0))) {
		fprintf(stderr,
			"Usage:    ytalk [options] user[@host][#tty]...\n\
Options:     -v    print program version\n\
             -h    show this help message\n");
		exit(YTE_INIT);
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
	signal(SIGQUIT, SIG_IGN);
	signal(SIGALRM, sigalrm_handle);

	/* go for it! */

	errno = 0;
	init_fd();
	init_user(vhost);
	init_term();
	init_ymenu();
	init_socket();
	for (; argc > 0; argc--, argv++)
		invite(*argv, 1);

	if (argc != 0)
		msg_term("Waiting for connection...");

	redraw_all_terms();

	main_loop();
	bail(YTE_SUCCESS_PROMPT);

	return 0;		/* make lint happy */
}

#ifndef HAVE_SNPRINTF
int
snprintf(char *buf, size_t len, char const *fmt, ...)
{
	int ret;
	va_list ap;

	(void) len;

	va_start(ap, fmt);
	ret = vsprintf(buf, fmt, ap);
	va_end(ap);
	return ret;
}
#endif
