/*
 * src/getpty.c
 * pseudo-terminal allocation routines
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

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifndef SIGCHLD
# define SIGCLD SIGCHLD
#endif

#if !defined(HAVE_OPENPTY) || defined(YTALK_TEST)
int
getpty(name)
	char *name;
{
	register int pty, tty;
	char *tt;

#ifdef USE_DEV_PTMX
	RETSIGTYPE(*sigchld) ();
	int r = 0;
#endif /* USE_DEV_PTMX */

	/* look for a Solaris/UNIX98-type pseudo-device */

#ifdef USE_DEV_PTMX
	if ((pty = open("/dev/ptmx", O_RDWR)) >= 0) {
		/* grantpt() might want to fork/exec! */
		sigchld = signal(SIGCHLD, SIG_DFL);
		r |= grantpt(pty);
		r |= unlockpt(pty);
		tt = ptsname(pty);
		signal(SIGCHLD, sigchld);
		if (r == 0 && tt != NULL) {
			strcpy(name, tt);
#ifdef YTALK_SUNOS
			needtopush = 1;
#endif /* YTALK_SUNOS */
			return pty;
		}
	}
#endif /* USE_DEV_PTMX */

#ifdef HAVE_TTYNAME

	/* look for an older SYSV-type pseudo device */
	if ((pty = open("/dev/ptc", O_RDWR)) >= 0) {
		if ((tt = ttyname(pty)) != NULL) {
			strcpy(name, tt);
			return pty;
		}
		close(pty);
	}
#endif /* HAVE_TTYNAME */

	/* scan Berkeley-style */
	strcpy(name, "/dev/ptyp0");
	while (access(name, 0) == 0) {
		if ((pty = open(name, O_RDWR)) >= 0) {
			name[5] = 't';
			if ((tty = open(name, O_RDWR)) >= 0) {
				close(tty);
				return pty;
			}
			name[5] = 'p';
			close(pty);
		}

		/* get next pty name */

		if (name[9] == 'f') {
			name[8]++;
			name[9] = '0';
		} else if (name[9] == '9')
			name[9] = 'a';
		else
			name[9]++;
	}
	errno = ENOENT;
	return -1;
}
#endif /* !HAVE_OPENPTY || YTALK_TEST */

