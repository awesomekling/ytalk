/* user.c -- user database */

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
#include "ytp.h"
#include "ymenu.h"
#include "mem.h"
#include <pwd.h>

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
user_name(uid)
	ylong uid;
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
		(void) endpwent();
		passwd_opened = 0;
	}
}

void
generate_full_name(user)
	yuser *user;
{
	register char *c, *d, *ce;

	if (user->full_name == NULL)
		user->full_name = get_mem(50);
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
init_user(vhost)
	char *vhost;
{
	char *my_name, *my_vhost;
	char my_host[100];

	user_list = NULL;
	connect_list = NULL;
	wait_list = NULL;
	daemon_id = getpid() << 10;
	(void) memset(fd_to_user, 0, MAX_FILES * sizeof(yuser *));

	/* get my username */

	my_name = user_name(myuid);
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

	/* i am the first scrolled user */
	scuser = me;

	close_passwd();
}

/*
 * Create a new user record.
 */
yuser *
new_user(name, hostname, tty)
	char *name, *hostname, *tty;
{
	register yuser *out;
	ylong addr;

	/* find the host address */

	if (hostname == NULL || *hostname == '\0') {
		hostname = me->host_name;
		addr = me->host_addr;
	} else if ((addr = get_host_addr(hostname)) == (ylong) - 1) {
		sprintf(errstr, "new_user: bad host: '%s'", hostname);
		show_error(errstr);
		return NULL;
	}
	/* create the user record */

	out = (yuser *) get_mem(sizeof(yuser));
	(void) memset(out, 0, sizeof(yuser));
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
free_user(user)
	yuser *user;
{
	register yuser *u;

	/* print a disco message */

	sprintf(msgstr, "%s disconnected.", user->full_name);
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
	    && !in_ymenu()
	    && running_process == 0)
		bail(YTE_SUCCESS_PROMPT);

	close_term(user);
	free_mem(user->full_name);
	free_mem(user->user_name);
	free_mem(user->host_name);
	free_mem(user->tty_name);
	if (user->dbuf)
		free_mem(user->dbuf);
	if (user->output_fd > 0)
		close(user->output_fd);
	if (user->fd) {
		remove_fd(user->fd);
		fd_to_user[user->fd] = NULL;
		close(user->fd);
	}
	free_scroll(user);
	free_mem(user);
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
find_user(name, host_addr, pid)
	char *name;
	ylong host_addr, pid;
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

void
user_title(char *buf, int size, yuser *user)
{
	char *f, *b, *fmt;
	long int i;

	if (user == me)
		fmt = title_format;
	else
		fmt = user_format;

	if (fmt == NULL) {
		if ((int) strlen(user->full_name) <= size)
			strcpy(buf, user->full_name);
		return;
	}
	for (f = fmt, b = buf; *f && (int) (b - buf) < size;) {
		if (*f == '%') {
			switch (*(++f)) {
			case 'u':
				if ((int) (b - buf) < (size - (int) strlen(user->user_name)))
					b += sprintf(b, "%s", user->user_name);
				break;
			case 'h':
				if ((int) (b - buf) < (size - (int) strlen(user->host_name)))
					b += sprintf(b, "%s", user->host_name);
				break;
			case 'f':
				if ((int) (b - buf) < (size - (int) strlen(user->host_fqdn)))
					b += sprintf(b, "%s", user->host_fqdn);
				break;
			case 't':
				if ((int) (b - buf) < (size - (int) strlen(user->tty_name)))
					b += sprintf(b, "%s", user->tty_name);
				break;
			case 'v':
				if ((int) (b - buf) < (size - 4)) {
					if (user->remote.vmajor > 2)
						b += sprintf(b, "Y%d.%d", user->remote.vmajor, user->remote.vminor);
					else if (user->remote.vmajor == 2)
						b += sprintf(b, "Y2.?");
					else
						b += sprintf(b, "UNIX");
				}
				break;
			case 'V':
				if ((int) (b - buf) < (size - 5))
					b += sprintf(b, "%d.%d.%d", VMAJOR, VMINOR, VPATCH);
				break;
			case 'S':
				if ((int) (b - buf) < (size - 4)) {
					for (i = 0; (i < scrollback_lines) && (user->scrollback[i] != NULL); i++);
					b += sprintf(b, "%d%%", (i == 0) ? 100 : (int) (((float) (user->scrollpos + 1) / (float) i) * 100));
				}
				break;
			case 's':
				if (scrollback_lines > 0) {
					if (user == scuser)
						*b = '*';
					else
						*b = ' ';
				} else {
					*b = ' ';
				}
				b++;
				break;
			case '%':
				*(b++) ='%';
				break;
			case '!':
				*(b++) = 9;
				break;
			}
		} else {
			if (*f == 9)
				*b = 32;
			else
				*b = *f;
			b++;
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
		free_mem(u->user_name);
		free_mem(u->host_name);
		free_mem(u->host_fqdn);
		free_mem(u->tty_name);
		free_mem(u->full_name);
		if (u->term != NULL)
			free_mem(u->term);
		if (u->scr != NULL) {
			for (i = 0; i < u->rows; i++)
				free_mem(u->scr[i]);
			free_mem(u->scr);
			free_mem(u->scr_tabs);
		}
		free_mem(u);
		u = un;
	}
}
