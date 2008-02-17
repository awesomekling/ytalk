/*
 * src/user.c
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
#include "ytp.h"
#include "cwin.h"
#include "ymenu.h"

#include <stdio.h>
#include <pwd.h>

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

extern int dont_change_my_addr;

yuser *me;			/* my user information */
yuser *user_list;		/* list of invited/connected users */
yuser *connect_list;		/* list of connected users */
yuser *wait_list;		/* list of connected users */
yuser *fd_to_user[MAX_FILES];	/* convert file descriptors to users */
ylong def_flags = 0L;		/* default FL_* flags */
static ylong daemon_id;		/* running daemon ID counter */

/* ---- local functions ----- */

static int passwd_opened = 0;

static char *
user_name(ylong uid)
{
	register struct passwd *pw;
	passwd_opened = 1;
	if ((pw = getpwuid(uid)) == NULL)
		return NULL;
	return pw->pw_name;
}

static void
close_passwd()
{
	if (passwd_opened) {
		endpwent();
		passwd_opened = 0;
	}
}

void
generate_full_name(yuser *user)
{
	register char *c, *d, *ce;

	if (user->full_name == NULL)
		user->full_name = malloc(50);
	c = user->full_name, ce = user->full_name + 49;

	for (d = user->user_name; *d && c < ce; d++)
		*(c++) = *d;

	if (c < ce)
		*(c++) = '@';
	for (d = user->host_fqdn; *d && c < ce; d++)
		*(c++) = *d;

	if (user->tty_name[0]) {
		if (c < ce)
			*(c++) = '#';
		for (d = user->tty_name; *d && c < ce; d++)
			*(c++) = *d;
	}
	*c = '\0';
}

/* ---- global functions ----- */

/*
 * Initialize user data structures.
 */
void
init_user(char *vhost)
{
	char *my_name, *my_vhost;
	char my_host[100];

	me = NULL;
	user_list = NULL;
	connect_list = NULL;
	wait_list = NULL;
	daemon_id = getpid() << 10;
	memset(fd_to_user, 0, MAX_FILES * sizeof(yuser *));

	/* get my username */

	my_name = user_name( getuid() );
	if (my_name == NULL || my_name[0] == '\0') {
		show_error("Who are you?");
		bail(YTE_ERROR);
	}
	/* get my hostname */

	if ((my_vhost = vhost) || (my_vhost = getenv("LOCALHOST"))) {
		strncpy(my_host, my_vhost, 99);
		dont_change_my_addr = 1;
	} else {
		if (gethostname(my_host, 100) < 0) {
			show_error("init_user: gethostname() failed");
			bail(YTE_ERROR);
		} else {
			/* try to find the fqdn */
			if (strchr(my_host, '.') == NULL) {
				ylong adr = get_host_addr(my_host);
				if (adr != 0 && adr != (ylong) - 1) {
					char *name = host_name(adr);
					if (name && strchr(name, '.'))
						strncpy(my_host, name, 99);
				}
			}
		}
	}
	my_host[99] = 0;

	/* get my user record */

	if ((me = new_user(my_name, my_host, NULL)) == NULL)
		bail(YTE_ERROR);
	me->remote.protocol = YTP_NEW;
	me->remote.vmajor = VMAJOR;
	me->remote.vminor = VMINOR;
	me->remote.pid = getpid();

	/* make sure we send CR/LF output to ourselves */
	me->crlf = 1;

	/* my key is '@' */
	me->key = '@';

	/* i am the first scrolled user */
	scuser = me;

	close_passwd();
}

/*
 * Create a new user record.
 */
yuser *
new_user(char *name, char *hostname, char *tty)
{
	register yuser *out;
	ylong addr;

	/* find the host address */

	if (hostname == NULL || *hostname == '\0') {
		if (me != NULL) {
			hostname = me->host_name;
			addr = me->host_addr;
		} else {
			show_error("new_user: Cannot run with empty local host");
			return NULL;
		}
	} else if ((addr = get_host_addr(hostname)) == (ylong) - 1) {
		snprintf(errstr, MAXERR, "new_user: bad host: '%s'", hostname);
		show_error(errstr);
		return NULL;
	}
	/* create the user record */

	out = (yuser *) malloc(sizeof(yuser));
	memset(out, 0, sizeof(yuser));
	out->user_name = str_copy(name);
	out->host_name = str_copy(hostname);
	if (strchr(hostname, '.'))
		out->host_fqdn = str_copy(hostname);
	else
		out->host_fqdn = str_copy(host_name(addr));
	out->host_addr = addr;
	if (tty)
		out->tty_name = str_copy(tty);
	else
		out->tty_name = str_copy("");
	out->d_id = daemon_id++;
	generate_full_name(out);
	out->flags = def_flags;

	if (user_list == NULL) {
		out->unext = user_list;
		user_list = out;
	} else {
		out->unext = user_list->unext;
		user_list->unext = out;
	}

	init_scroll(out);
	return out;
}

void
free_user(yuser *user)
{
	register yuser *u;

	/* print a disco message */

	snprintf(msgstr, MAXERR, "%s disconnected.", user->full_name);
	msg_term(msgstr);

	/* make sure we're not stuck scrolling a long gone user */

	if (user == scuser)
		scuser = me;

	/* remove him from the various blacklists */

	if (user == user_list)
		user_list = user->unext;
	else
		for (u = user_list; u; u = u->unext)
			if (u->unext == user) {
				u->unext = user->unext;
				break;
			}
	if (user == connect_list)
		connect_list = user->next;
	else
		for (u = connect_list; u; u = u->next)
			if (u->next == user) {
				u->next = user->next;
				break;
			}
	if (user == wait_list)
		wait_list = user->next;
	else
		for (u = wait_list; u; u = u->next)
			if (u->next == user) {
				u->next = user->next;
				break;
			}
	/* close him down */

	if (connect_list == NULL
	    && wait_list == NULL
	    && running_process == 0
	    && !(def_flags & FL_PERSIST))
		bail(YTE_SUCCESS_PROMPT);

	close_term(user);
	free(user->full_name);
	free(user->user_name);
	free(user->host_name);
	free(user->tty_name);

	if (user->gt.buf)
		free(user->gt.buf);
	if (user->dbuf)
		free(user->dbuf);
	if (user->fd) {
		remove_fd(user->fd);
		fd_to_user[user->fd] = NULL;
		close(user->fd);
	}
	free_scroll(user);
	free(user);
	if (connect_list == NULL && wait_list != NULL)
		msg_term("Waiting for connection...");
	user_winch = 1;

	redraw_all_terms();
}

/*
 * Find a user by name/host/pid.  If name is NULL, then it is not checked. If
 * host_addr is (ylong)-1 then it is not checked.  If pid is (ylong)-1 then
 * it is not checked.
 */
yuser *
find_user(char *name, ylong host_addr, ylong pid)
{
	register yuser *u;

	for (u = user_list; u; u = u->unext)
		if (name == NULL || strcmp(u->user_name, name) == 0)
			if (host_addr == (ylong) - 1 || u->host_addr == host_addr)
				if (pid == (ylong) - 1 || u->remote.pid == pid)
					return u;

	/* it could be _me_! */

	if (name == NULL || strcmp(me->user_name, name) == 0)
		if (host_addr == (ylong) - 1 || me->host_addr == host_addr)
			if (pid == (ylong) - 1 || me->remote.pid == pid)
				return me;

	/* nobody I know */

	return NULL;
}

/*
 * Save a user's entire conversation history to a file.
 */
void
save_user_to_file(yuser *user, char *filename)
{
	yachar **p;
	int fd;
	char msgbuf[80];
	unsigned long lines = 0;
	unsigned short r;

	/* Let's not overwrite or underprotect */
	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd < 0) {
		show_error("Couldn't open file for writing.");
		return;
	}
	/* If user has scrollback history, dump 'em all! */
	if (user->scrollback != NULL) {
		for (p = user->scrollback; *p; p++, lines++) {
			spew_line(fd, *p, ya_strlen(*p));
			write(fd, "\n", 1);
		}
	}
	/* Dump his currently visible lines */
	if (user->scr && user->scr[0]) {
		for (r = 0; r < user->rows; r++, lines++) {
			spew_line(fd, user->scr[r], user->cols);
			write(fd, "\n", 1);
		}
	}

	/* Free any memory allocated by spew_line() */
	spew_free();

	if (close(fd) < 0) {
		show_error("Couldn't close output file.");
		return;
	}
	snprintf(msgbuf, sizeof(msgbuf), "Wrote %lu lines.", lines);
	msg_term(msgbuf);
}

void
user_title(yuser *user, char *buf, int size)
{
	char *f, *b, *fmt;

	/* Use "title_format" for our own title bar, and "user_format"
	 * for everyone else's. */
	fmt = (user == me) ? title_format : user_format;

	/* If no format spec was found, use the user's full_name. */
	if (!fmt) {
		strncpy(buf, user->full_name, size);
		return;
	}

	for (f = fmt, b = buf; *f && (int) (b - buf) < size;) {
		if (*f == '%') {
			switch (*(++f)) {
			case 'u':
				/* %u - Username */
				if ((int) (b - buf) < (size - (int) strlen(user->user_name)))
					b += sprintf(b, "%s", user->user_name);
				break;
			case 'h':
				/* %h - Hostname */
				if ((int) (b - buf) < (size - (int) strlen(user->host_name)))
					b += sprintf(b, "%s", user->host_name);
				break;
			case 'f':
				/* %f - Fully Qualified Domain Name */
				if ((int) (b - buf) < (size - (int) strlen(user->host_fqdn)))
					b += sprintf(b, "%s", user->host_fqdn);
				break;
			case 't':
				/* %t - Terminal */
				if ((int) (b - buf) < (size - (int) strlen(user->tty_name)))
					b += sprintf(b, "%s", user->tty_name);
				break;
			case 'U':
				/* %U - UNIX flavor
				 * From SYSTEM_TYPE if user == me
				 * From gtalk version message if remote speaks "YTalk extended" gtalk
				 * "?" if unknown
				 */
				if (user == me) {
					if ((int) (b - buf) < (size - (int) strlen(SYSTEM_TYPE)))
						b += sprintf(b, "%s", SYSTEM_TYPE);
				} else if (user->gt.system != NULL) {
					if ((int) (b - buf) < (size - (int) strlen(user->gt.system)))
						b += sprintf(b, "%s", user->gt.system);
				} else {
					if ((int) (b - buf) < (size - 1))
						b += sprintf(b, "?");
				}
				break;
			case 'v':
				/* %v - Client version
				 * "Yx.x" for YTalk 3+
				 * "Y2.?" for YTalk 2
				 * "GNU" for GNU talk
				 * "BSD" for generic/unknown BSD-compatible talk clients
				 */
				if ((int) (b - buf) < (size - 4)) {
					if (user->remote.vmajor > 2)
						b += sprintf(b, "Y%d.%d", user->remote.vmajor, user->remote.vminor);
					else if (user->remote.vmajor == 2)
						b += sprintf(b, "Y2.?");
					else if (user->gt.version != NULL)
						b += sprintf(b, "GNU");
					else
						b += sprintf(b, "BSD");
				}
				break;
			case 'V':
				/* %V - Program version ("Program 3.2.1-SVN") */
				if ((int) (b - buf) < (int) (size - strlen(PACKAGE_VERSION)))
					b += sprintf(b, "%s", PACKAGE_VERSION);
				break;
			case 'S':
				/* %S - Scrolled amount
				 * 100% when not scrolling, between 100% and 0% when scrolling.
				 */
				if ((int) (b - buf) < (size - 4))
					b += sprintf(b, "%d%%", scrolled_amount(user));
				break;
			case 's':
				/* %s - Scrolling indicator:
				 * '*' for user being scrolled,
				 * ' ' for everyone else (or when scrollback isn't use)
				 */
				if (scrollback_lines)
					*(b++) = (user == scuser) ? '*' : ' ';
				else
					*(b++) = ' ';
				break;
			case '%':
				/* %% - Percent sign */
				*(b++) ='%';
				break;
			case '!':
				/* %! - Alignment break, this needs explanation (FIXME) */
				*(b++) = 9;
				break;
			}
		} else {
			/* Tabs ('\t') are changed to spaces (' ') to avoid curses
			 * terminal breakage. */
			*(b++) = (*f == '\t') ? ' ' : *f;
		}
		f++;
	}
	*b = 0;
}

/*
 * Clear out the user list, freeing memory as we go.
 */
void
free_users()
{
	yuser *u, *un;
	unsigned int i;

	for (u = user_list; u != NULL;) {
		un = u->unext;
		free_scroll(u);
		free(u->user_name);
		free(u->host_name);
		free(u->host_fqdn);
		free(u->tty_name);
		free(u->full_name);
		if (u->gt.buf)
			free(u->gt.buf);
		if (u->term != NULL)
			free(u->term);
		if (u->scr != NULL) {
			for (i = 0; i < u->rows; i++)
				free(u->scr[i]);
			free(u->scr);
			free(u->scr_tabs);
		}
		free(u);
		u = un;
	}
}
