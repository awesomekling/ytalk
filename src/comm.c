/* comm.c -- firewall between socket and terminal I/O */

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
#include "socket.h"
#include "ymenu.h"
#include "mem.h"

#ifdef HAVE_IOVEC_H
#include <iovec.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef YTALK_COLOR
#  ifdef HAVE_NCURSES_H
#    include <ncurses.h>
#  else
#    include <curses.h>
#  endif
#endif

/* this isn't the most elegant solution... */
#define DRAINFUNC(f)	((void (*) (yuser *, void *))f)

#include "cwin.h"

#define IN_ADDR(s)	((s).sin_addr.s_addr)

/* oops, some systems don't have this one */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

ychar *io_ptr;			/* user input pointer */
int io_len = 0;			/* user input count */

int dont_change_my_addr = 0;	/* set if the user specified a vhost, or
				 * after the first time ytalk guesses the
				 * local addr from a getsockname() call */

extern int input_flag;		/* see fd.c */

/* ---- local functions ---- */

static y_parm parm;
static v2_pack v2p;
static v3_pack v3p;
static v3_flags v3f;
static v3_winch v3w;

/*
 * Set up a drain of out-of-band data.
 */
static void
drain_user(yuser *user, int len, void (*func) (yuser *, void *))
{
	if (len > user->dbuf_size) {
		user->dbuf_size = len + 64;
		user->dbuf = (ychar *) realloc_mem(user->dbuf, user->dbuf_size);
	}
	user->drain = len;
	user->dptr = user->dbuf;
	user->dfunc = func;
}

/*
 * Send out-of-band data.
 */
static void
send_oob(int fd, yaddr ptr, int len)
{
	ychar oob, size;
	static struct iovec iov[3];
	int r;

	if (len <= 0 || len > V3_MAXPACK) {
		errno = 0;
		show_error("send_oob: packet too large");
		return;
	}
	oob = V3_OOB;
	iov[0].iov_base = (yaddr) (&oob);
	iov[0].iov_len = 1;

	size = len;
	iov[1].iov_base = (yaddr) (&size);
	iov[1].iov_len = 1;

	iov[2].iov_base = ptr;
	iov[2].iov_len = len;

	if ((r = writev(fd, iov, 3)) != len + 2) {
		if (r >= 0 || (errno != EPIPE && errno != ECONNRESET))
			show_error("send_oob: write failed");
	}
}

/*
 * Ask another ytalk connection if he wants to import a user I've just now
 * connected to.
 */
static void
send_import(yuser *to, yuser *from)
{
	if (to->remote.vmajor > 2) {
		v3p.code = V3_IMPORT;
		v3p.host_addr = htonl(from->host_addr);
		v3p.pid = htonl(from->remote.pid);
		strncpy(v3p.name, from->user_name, V3_NAMELEN);
		strncpy(v3p.host, from->host_fqdn, V3_HOSTLEN);
		send_oob(to->fd, &v3p, V3_PACKLEN);
	} else if (to->remote.vmajor == 2) {
		v2p.code = V2_IMPORT;
		strncpy(v2p.name, from->user_name, V2_NAMELEN);
		strncpy(v2p.host, from->host_fqdn, V2_HOSTLEN);
		write(to->fd, &v2p, V2_PACKLEN);
	}
}

/*
 * Tell another ytalk connection to connect to a user.
 */
static void
send_accept(yuser *to, yuser *from)
{
	if (to->remote.vmajor > 2) {
		v3p.code = V3_ACCEPT;
		v3p.host_addr = htonl(from->host_addr);
		v3p.pid = htonl(from->remote.pid);
		strncpy(v3p.name, from->user_name, V3_NAMELEN);
		strncpy(v3p.host, from->host_fqdn, V3_HOSTLEN);
		send_oob(to->fd, &v3p, V3_PACKLEN);
	} else if (to->remote.vmajor == 2) {
		v2p.code = V2_ACCEPT;
		strncpy(v2p.name, from->user_name, V2_NAMELEN);
		strncpy(v2p.host, from->host_fqdn, V2_HOSTLEN);
		write(to->fd, &v2p, V2_PACKLEN);
	}
}

/*
 * Process a Ytalk version 2.? data packet.
 */
static void
v2_process(yuser *user, v2_pack *pack)
{
	register yuser *u;
	ylong host_addr;
	static char name[V2_NAMELEN + 1];
	static char host[V2_HOSTLEN + 1];
	static char estr[V2_NAMELEN + V2_HOSTLEN + 20];

	/*
	 * Ytalk version 2.* didn't have very clever import/export
	 * capabilities.  We'll just go with the flow.
	 */
	strncpy(name, pack->name, V2_NAMELEN);
	strncpy(host, pack->host, V2_HOSTLEN);
	name[V2_NAMELEN] = '\0';
	host[V2_HOSTLEN] = '\0';
	if ((host_addr = get_host_addr(host)) == (ylong) - 1) {
		errno = 0;
#ifdef HAVE_SNPRINTF
		snprintf(errstr, MAXERR, _("Unknown host: '%s'"), host);
#else
		sprintf(errstr, _("Unknown host: '%s'"), host);
#endif
		show_error(errstr);
		show_error("port from ytalk V2.? failed");
		return;
	}
	switch (pack->code) {
	case V2_IMPORT:
		/*
		 * Don't import a user with the same name of an existing user
		 * at this end.  yukk.
		 */
		if (find_user(name, host_addr, (ylong) - 1) != NULL)
			break;
		if (!(def_flags & FL_IMPORT)) {
#ifdef HAVE_SNPRINTF
			snprintf(estr, sizeof(estr), _("Import %s@%s?"), name, host);
#else
			sprintf(estr, _("Import %s@%s?"), name, host);
#endif
			if (yes_no(estr) == 'n')
				break;
		}
		/* invite him but don't ring him */

#ifdef HAVE_SNPRINTF
		snprintf(estr, sizeof(estr), "%s@%s", name, host);
#else
		sprintf(estr, "%s@%s", name, host);
#endif
		invite(estr, 0);

		/* now tell him to connect to us */

		pack->code = V2_EXPORT;
		write(user->fd, pack, V2_PACKLEN);

		break;
	case V2_EXPORT:
		/*
		 * We don't need to check if he's not connected, since
		 * send_accept() will think his version number is zero and
		 * won't send anything.
		 */
		if ((u = find_user(name, host_addr, (ylong) - 1)) == NULL)
			break;
		send_accept(u, user);
		break;
	case V2_ACCEPT:
#ifdef HAVE_SNPRINTF
		snprintf(estr, sizeof(estr), "%s@%s", name, host);
#else
		sprintf(estr, "%s@%s", name, host);
#endif
		invite(estr, 1);	/* we should be expected */
		break;
	}
}

/*
 * Process a Ytalk version 3.? data packet.
 */
static void
v3_process_pack(yuser *user, v3_pack *pack)
{
	register yuser *u, *u2;
	ylong host_addr, pid;
	static char name[V3_NAMELEN + 1];
	static char host[V3_HOSTLEN + 1];
	static char estr[V3_NAMELEN + V3_HOSTLEN + 20];

	strncpy(name, pack->name, V3_NAMELEN);
	strncpy(host, pack->host, V3_HOSTLEN);
	name[V3_NAMELEN] = '\0';
	host[V3_HOSTLEN] = '\0';
	if ((host_addr = get_host_addr(host)) == (ylong) - 1)
		host_addr = ntohl(pack->host_addr);
	pid = ntohl(pack->pid);

	switch (pack->code) {
	case V3_IMPORT:
		/*
		 * Don't import a user which is already in this session.
		 * This is defined as a user with a matching name, host
		 * address, and process id.
		 */
		if (find_user(name, host_addr, pid) != NULL)
			break;
		if (!(def_flags & FL_IMPORT)) {
#ifdef HAVE_SNPRINTF
			snprintf(estr, sizeof(estr), _("Import %s@%s?"), name, host);
#else
			sprintf(estr, _("Import %s@%s?"), name, host);
#endif
			if (yes_no(estr) == 'n')
				break;
		}
		/* invite him but don't ring him */

#ifdef HAVE_SNPRINTF
		snprintf(estr, sizeof(estr), "%s@%s", name, host);
#else
		sprintf(estr, "%s@%s", name, host);
#endif
		u2 = invite(estr, 0);

		/*
		 * fix the pid, so that two quick V3_IMPORT requests of the
		 * same user still get caught  -roger
		 */
		if (u2 != NULL && strcmp(u2->user_name, name) == 0 &&
		    host_addr == u2->host_addr) {
			u2->remote.pid = pid;
		}
		/* now tell him to connect to us */

		pack->code = V3_EXPORT;
		send_oob(user->fd, pack, V3_PACKLEN);

		break;
	case V3_EXPORT:
		/*
		 * We don't need to check if he's not connected, since
		 * send_accept() will think his version number is zero and
		 * won't send anything.
		 */
		if ((u = find_user(name, host_addr, pid)) == NULL)
			break;
		send_accept(u, user);
		break;
	case V3_ACCEPT:
#ifdef HAVE_SNPRINTF
		snprintf(estr, sizeof(estr), "%s@%s", name, host);
#else
		sprintf(estr, "%s@%s", name, host);
#endif
		invite(estr, 1);	/* we should be expected */
		break;
	}
}

/*
 * Process a Ytalk version 3.? flags packet.  Other users can request that
 * their flags be locked to a particular value until they unlock them later.
 */
static void
v3_process_flags(yuser *user, v3_flags *pack)
{
	switch (pack->code) {
	case V3_LOCKF:
		user->flags = ntohl(pack->flags) | FL_LOCKED;
		break;
	case V3_UNLOCKF:
		user->flags = def_flags;
		break;
	}
}

/*
 * Process a Ytalk version 3.? winch packet.
 */
static void
v3_process_winch(yuser *user, v3_winch *pack)
{
	switch (pack->code) {
	case V3_YOURWIN:
		user->remote.my_rows = ntohs(pack->rows);
		user->remote.my_cols = ntohs(pack->cols);
		winch_exec();
		break;
	case V3_MYWIN:
		user->remote.rows = ntohs(pack->rows);
		user->remote.cols = ntohs(pack->cols);
		break;
	case V3_REGION:
		pack->rows = ntohs(pack->rows);
		pack->cols = ntohs(pack->cols);
		if (pack->rows > 0)
			set_win_region(user, (int) (pack->rows), (int) (pack->cols));
		else
			end_win_region(user);
		break;
	}
	user_winch = 1;
}

/*
 * Process a Ytalk version 3.? out-of-band packet.  Call the appropriate
 * function based on the type of packet.
 */
static void
v3_process(yuser *user, yaddr ptr)
{
	ychar *str;

	/* ignore anything we don't understand */

	str = (ychar *) ptr;
	switch (*str) {
	case V3_IMPORT:
	case V3_EXPORT:
	case V3_ACCEPT:
		v3_process_pack(user, (v3_pack *) ptr);
		break;
	case V3_LOCKF:
	case V3_UNLOCKF:
		v3_process_flags(user, (v3_flags *) ptr);
		break;
	case V3_YOURWIN:
	case V3_MYWIN:
	case V3_REGION:
		v3_process_winch(user, (v3_winch *) ptr);
		break;
	}
}

/*
 * Take input from a connected user.  If necessary, drain out-of-band data
 * from the canonical input stream.
 */
static void
read_user(int fd)
{
	register ychar *c, *p;
	register int rc;
	register yuser *user;
	static ychar buf[512];

	if (input_flag) {
		/* tell input_loop() to ignore this function for now */
		input_flag = 0;
		return;
	}
	if ((user = fd_to_user[fd]) == NULL) {
		remove_fd(fd);
		show_error("read_user: unknown contact");
		return;
	}
	if ((rc = read(fd, buf, 512)) <= 0) {
		if (rc < 0 && errno != ECONNRESET && errno != EPIPE)
			show_error("read_user: read() failed");
		free_user(user);
		return;
	}
	c = buf;
	while (rc > 0) {
		if (user->drain > 0) {	/* there is still some OOB data to
					 * drain */
			if (rc < user->drain) {
				memcpy(user->dptr, c, rc);
				user->dptr += rc;
				user->drain -= rc;
				rc = 0;
			} else {
				memcpy(user->dptr, c, user->drain);
				rc -= user->drain;
				c += user->drain;
				user->drain = 0;
				user->dfunc(user, user->dbuf);
			}
		} else {
			/*
			 * Ytalk version 3.0 Out-Of-Band data protocol:
			 *
			 * If I receive a V3_OOB character, I look at the next
			 * character.  If the next character is V3_OOB, then
			 * I send one V3_OOB through transparently.  Else,
			 * the next character is a packet length to be
			 * drained. The packet length can never be V3_OOB
			 * because the maximum out-of-band packet length is
			 * (V3_OOB - 1) bytes. If any packet requires more
			 * information, then it can always kick off another
			 * drain_user() inside v3_process().
			 */
			p = buf;
			if (user->got_oob) {
				user->got_oob = 0;
				if (*c <= V3_MAXPACK) {
					drain_user(user, *c, DRAINFUNC(v3_process));
					c++, rc--;
					continue;
				}
				*(p++) = *c;
				c++, rc--;
			}
			for (; rc > 0; c++, rc--) {
				if (*c > 127) {	/* could be inline data */
					if (user->remote.vmajor > 2) {	/* ytalk 3.0+ */
						if (*c == V3_OOB) {
							c++, rc--;
							if (rc > 0) {
								if (*c <= V3_MAXPACK) {
									drain_user(user, *c, DRAINFUNC(v3_process));
									c++, rc--;
									break;
								}
							} else {
								user->got_oob = 1;
								break;
							}
						}
					} else if (user->remote.vmajor == 2) {	/* ytalk 2.0+ */
						/*
						 * Version 2.* didn't support
						 * data transparency
						 */

						if (*c == V2_IMPORT || *c == V2_EXPORT
						    || *c == V2_ACCEPT || *c == V2_AUTO) {
							drain_user(user, V2_PACKLEN, DRAINFUNC(v2_process));
							/*
							 * don't increment c
							 * or decrement rc --
							 * they're part of
							 * the drain.  :-)
							 */
							break;
						}
					}
				}
				*(p++) = *c;
			}
			if (p > buf) {
				if (user->output_fd > 0)
					if (write(user->output_fd, buf, p - buf) <= 0) {
						show_error("write to user output file failed");
						close(user->output_fd);
						user->output_fd = 0;
					}
				show_input(user, buf, p - buf);
			}
		}
	}
}

/*
 * Initial Handshaking:  read the parameter pack from another ytalk user.
 */
static void
ytalk_user(int fd)
{
	register yuser *user, *u;
	unsigned short cols;

	if ((user = fd_to_user[fd]) == NULL) {
		remove_fd(fd);
		show_error("ytalk_user: unknown contact");
		return;
	}
	if (full_read(user->fd, (char *) &parm, sizeof(y_parm)) < 0) {
		free_user(user);
		show_error("ytalk_user: bad ytalk contact");
		return;
	}
	switch (parm.protocol) {
	case YTP_OLD:
		cols = parm.w_cols;
		memset(&parm, 0, sizeof(y_parm));
		parm.vmajor = 2;
		parm.cols = cols;
		parm.my_cols = cols;
		spew_term(me, fd, me->t_rows, parm.cols);
		break;
	case YTP_NEW:
		parm.vmajor = ntohs(parm.vmajor);
		parm.vminor = ntohs(parm.vminor);
		parm.rows = ntohs(parm.rows);
		parm.cols = ntohs(parm.cols);
		parm.my_rows = ntohs(parm.my_rows);
		parm.my_cols = ntohs(parm.my_cols);
		parm.pid = ntohl(parm.pid);
		/* we spew_term later */
		break;
	default:
		free_user(user);
		show_error("ytalk_user: unsupported ytalk protocol");
		return;
	}
	user->remote = parm;

	/* determine whether remote user likes CRLF newlines */
	if ((user->remote.vmajor < 3) || (user->remote.vmajor == 3 && user->remote.vminor < 2))
		user->crlf = 0;
	else
		user->crlf = 1;

	user_winch = 1;
	add_fd(fd, read_user);

	/* update the lists */

	if (user == wait_list)
		wait_list = user->next;
	else
		for (u = wait_list; u; u = u->next)
			if (u->next == user) {
				u->next = user->next;
				break;
			}
	user->next = connect_list;
	connect_list = user;

	/* send him my status */

	if (user->remote.vmajor > 2) {
		if (me->region_set) {
			v3w.code = V3_REGION;
			v3w.rows = htons(me->rows);
			v3w.cols = htons(me->cols);
			send_oob(fd, &v3w, V3_WINCHLEN);
			winch_exec();
			spew_term(me, fd, me->rows, me->cols);
		} else
			spew_term(me, fd, parm.rows, parm.cols);

		if (me->flags & FL_LOCKED) {
			v3f.code = V3_LOCKF;
			v3f.flags = htonl(me->flags);
			send_oob(fd, &v3f, V3_FLAGSLEN);
		}
	}
	/* tell everybody else he's here! */

	for (u = connect_list; u; u = u->next)
		if (u != user)
			send_import(u, user);

	/* NEWUI: make sure we display the correct remote version */
	retitle_all_terms();
}

/*
 * Initial Handshaking:  read the edit keys and determine whether or not this
 * is another ytalk user.
 */
static void
connect_user(int fd)
{
	register yuser *user, *u;

	if ((user = fd_to_user[fd]) == NULL) {
		remove_fd(fd);
		show_error("connect_user: unknown contact");
		return;
	}
	if (full_read(fd, (char *)user->edit, 3) < 0) {
		free_user(user);
		show_error("connect_user: bad read");
		return;
	}
	if (open_term(user) < 0) {
		free_user(user);
		show_error("connect_user: open_term() failed");
		return;
	}

	/* check for ytalk connection */

	if (user->RUB == RUBDEF) {
		memset(&parm, 0, sizeof(y_parm));
		parm.protocol = YTP_NEW;
		parm.vmajor = htons(VMAJOR);
		parm.vminor = htons(VMINOR);
		parm.rows = htons(me->t_rows);
		parm.cols = htons(me->t_cols);
		parm.my_rows = htons(user->t_rows);
		parm.my_cols = htons(user->t_cols);
		parm.w_rows = parm.rows;
		parm.w_cols = parm.cols;
		parm.pid = htonl(me->remote.pid);
		write(user->fd, &parm, sizeof(y_parm));
		add_fd(fd, ytalk_user);
	} else {
		/* update the lists */

		if (user == wait_list)
			wait_list = user->next;
		else
			for (u = wait_list; u; u = u->next)
				if (u->next == user) {
					u->next = user->next;
					break;
				}
		user->next = connect_list;
		connect_list = user;

		spew_term(me, fd, me->t_rows, me->t_cols);
		user_winch = 1;
		add_fd(fd, read_user);
	}
#ifdef HAVE_SNPRINTF
	snprintf(msgstr, MAXERR, _("%s connected."), user->full_name);
#else
	sprintf(msgstr, _("%s connected."), user->full_name);
#endif
	msg_term(msgstr);
}

/*
 * Initial Handshaking:  delete his invitation (if it exists) and send my
 * edit keys.
 */
static void
contact_user(int fd)
{
	register yuser *user;
	register int n;
	ysocklen_t socklen;
	struct sockaddr_in peer;
	char *hname;

	remove_fd(fd);
	if ((user = fd_to_user[fd]) == NULL) {
		show_error("contact_user: unknown contact");
		return;
	}
	send_dgram(user, DELETE_INVITE);
	socklen = sizeof(struct sockaddr_in);
	if ((n = accept(fd, (struct sockaddr *) & (user->sock), &socklen)) < 0) {
		free_user(user);
		if (errno != EPIPE && errno != ECONNRESET)
			show_error("connect_user: accept() failed");
		return;
	}
	close(fd);
	fd_to_user[fd] = NULL;

	/*
	 * make sure the connection comes from the right place, else update
	 * the title to reflect the new name
	 */
	socklen = sizeof(struct sockaddr_in);
	if (getpeername(n, (struct sockaddr *) & peer, &socklen) >= 0) {
		if (IN_ADDR(peer) != user->host_addr) {
			if (user->host_name)
				free_mem(user->host_name);
			if (user->host_fqdn)
				free_mem(user->host_fqdn);
			user->host_addr = IN_ADDR(peer);
			hname = host_name(user->host_addr);
			user->host_name = str_copy(hname);
			user->host_fqdn = str_copy(hname);
			generate_full_name(user);
			show_error(_("Connection from unexpected host!"));
		}
	}
	user->fd = n;
	fd_to_user[user->fd] = user;
	add_fd(user->fd, connect_user);
	write(user->fd, me->edit, 3);	/* send the edit keys */
}

/*
 * Do a word wrap.
 */
static int
word_wrap(yuser *user)
{
	register int i, x, bound;
	static yachar temp[20];

	x = user->x;
	if ((bound = (x >> 1)) > 20)
		bound = 20;
#ifdef YTALK_COLOR
	for (i = 1; i < bound && user->scr[user->y][x - i].l != ' '; i++)
#else
	for (i = 1; i < bound && user->scr[user->y][x - i] != ' '; i++)
#endif
		temp[i] = user->scr[user->y][x - i];
	if (i >= bound)
		return -1;
	move_term(user, user->y, x - i);
	clreol_term(user);
	newline_term(user);
	for (i--; i >= 1; i--)
#ifdef YTALK_COLOR
		addch_term(user, temp[i].l);
#else
		addch_term(user, temp[i]);
#endif
	return 0;
}

/*
 * Ring a user.  If he has an auto-invitation port established then talk to
 * that instead of messing up his screen.
 */
static int
announce(yuser *user)
{
	register int rc, fd;

	errno = 0;
	while ((rc = send_dgram(user, AUTO_LOOK_UP)) == 0) {
		/* he has an auto-invite port established */

		if ((fd = connect_to((yuser *) 0)) < 0) {
			if (fd == -3)	/* it's one of my sockets... *sigh* */
				break;
			if (fd == -2) {	/* connection refused -- they hung
					 * up! */
				send_dgram(user, AUTO_DELETE);
				errno = 0;
				continue;
			}
			return -1;
		}
		/*
		 * Go ahead and use the Ytalk version 2.? auto-announce
		 * packet.
		 */
		v2p.code = V2_AUTO;
		strncpy(v2p.name, me->user_name, V2_NAMELEN);
		strncpy(v2p.host, me->host_name, V2_HOSTLEN);
		v2p.name[V2_NAMELEN - 1] = '\0';
		v2p.host[V2_HOSTLEN - 1] = '\0';
		write(fd, &v2p, V2_PACKLEN);
		close(fd);
		return 0;
	}
	if (rc == -1)
		return -1;

	errno = 0;
	if ((rc = send_dgram(user, ANNOUNCE)) == 0)
		return 0;
	if (rc == 4)		/* mesg n (refusing messages) */
		return 1;
	return -1;
}

/* ---- global functions ---- */


/*
 * Invite a user into the conversation.
 */
yuser *
invite(char *name, int send_announce)
{
	register int rc;
	char *hisname, *hishost, *histty;
	yuser *user;

	/*
	 * First break down the username into login name and login host,
	 * assuming our host as a default.
	 */

	name = resolve_alias(name);
	hisname = str_copy(name);
	hishost = NULL;
	histty = NULL;
	for (name = hisname; *name; name++) {
		if (*name == '@') {
			*name = '\0';
			hishost = name + 1;
		}
		if (*name == '#') {
			*name = '\0';
			histty = name + 1;
		}
	}
	user = new_user(hisname, hishost, histty);
	free_mem(hisname);
	if (user == NULL)
		return NULL;

	/*
	 * Trick to handle multihomed hosts.  Bad placement, but this
	 * location in the code makes it easier to patch.  Otherwise, ytalk
	 * would need to be rewritten.  I also could be wrong.
	 *
	 * This trick is the same idea as the talk patch for multihomed hosts
	 * included in NetKit-0.09 for Linux.
	 *
	 * (patch by Sean Farley, simplified & integrated by Roger Espel Llima)
	 */

	if (!dont_change_my_addr) {
		struct sockaddr_in tmpsock;
		ysocklen_t i;
		int sock;

		sock = socket(PF_INET, SOCK_DGRAM, 0);

		if (sock >= 0) {
			tmpsock.sin_port = htons(518);
			tmpsock.sin_family = AF_INET;
			tmpsock.sin_addr.s_addr = user->host_addr;
			if (connect(sock, (struct sockaddr *) & tmpsock, sizeof(tmpsock)) == 0) {
				i = sizeof(struct sockaddr_in);

				/*
				 * SysV tends to return 0.0.0.0 here, must
				 * check
				 */
				if (getsockname(sock, (struct sockaddr *) & tmpsock, &i) == 0 &&
				    tmpsock.sin_addr.s_addr != htonl(INADDR_ANY) &&
				    tmpsock.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
					me->host_addr = tmpsock.sin_addr.s_addr;
					dont_change_my_addr++;
				}
			}
			close(sock);
		}
	}
	/* Now send off the invitation */

	user->next = wait_list;
	wait_list = user;
	user_winch = 1;
	while ((rc = send_dgram(user, LOOK_UP)) == 0) {
		/* We are expected... */
		if ((rc = connect_to(user)) < 0) {
			if (rc == -3)	/* it's one of my sockets... *sigh* */
				break;
			if (rc == -2) {	/* connection refused -- they hung
					 * up! */
				send_dgram(user, DELETE);
				continue;
			}
			free_user(user);
			return NULL;
		}
		user->last_invite = (ylong) time(NULL);
		add_fd(user->fd, connect_user);
		write(user->fd, me->edit, 3);	/* send the edit keys */
		return user;
	}
	if (rc == -1) {
		/*
		 * don't ask whether to re-ring for a while even if not
		 * successful! -roger
		 */
		user->last_invite = (ylong) time(NULL);
		return user;
	}
	/* Leave an invitation for him, and announce ourselves. */

	if (send_announce) {
#ifdef HAVE_SNPRINTF
		snprintf(msgstr, MAXERR, _("Ringing %s..."), user->full_name);
#else
		sprintf(msgstr, _("Ringing %s..."), user->full_name);
#endif
		msg_term(msgstr);
	}
	if (newsock(user) != 0) {
		free_user(user);
		return NULL;
	}
	send_dgram(user, LEAVE_INVITE);
	user->last_invite = (ylong) time(NULL);
	if (send_announce && (rc = announce(user)) != 0) {
		send_dgram(user, DELETE_INVITE);
		if (rc > 0)
#ifdef HAVE_SNPRINTF
			snprintf(errstr, MAXERR, _("%s refusing messages"), user->full_name);
#else
			sprintf(errstr, _("%s refusing messages"), user->full_name);
#endif
		else
#ifdef HAVE_SNPRINTF
			snprintf(errstr, MAXERR, _("%s not logged in"), user->full_name);
#else
			sprintf(errstr, _("%s not logged in"), user->full_name);
#endif
		show_error(errstr);
		free_user(user);
		return NULL;
	}
	add_fd(user->fd, contact_user);
	return user;
}

/*
 * Periodic housecleaning.
 */
void
house_clean(void)
{
	register yuser *u, *next;
	ylong t;
	static char estr[80];
	static ylong last_auto = 0;
	int answer, rc;

	t = (ylong) time(NULL);

	if (bottom_msg != NULL && bottom_time != 0) {
		if (t - bottom_time >= 10) {
			bottom_msg = NULL;
			update_message_curses();
		}
	}

	if (t - last_auto >= 30) {
		last_auto = t;
		if (send_auto(LEAVE_INVITE) != 0) {
			show_error("house_clean: send_auto() failed");
			kill_auto();
		}
	}
	for (u = wait_list; u; u = next) {
		next = u->next;
		if (t - u->last_invite >= 30) {
			send_dgram(u, LEAVE_INVITE);
			u->last_invite = t = (ylong) time(NULL);
			if (!(def_flags & FL_RING))
				continue;
			if (def_flags & FL_PROMPTRING) {
				if (input_flag)
					continue;
#ifdef HAVE_SNPRINTF
				snprintf(estr, sizeof(estr), _("Rering %s?"), u->full_name);
#else
				sprintf(estr, _("Rering %s?"), u->full_name);
#endif
				answer = yes_no(estr);
				t = (ylong) time(NULL);
				if (answer == 'n')
					continue;
			}
#ifdef HAVE_SNPRINTF
			snprintf(msgstr, MAXERR, _("Reringing %s\n"), u->full_name);
#else
			sprintf(msgstr, _("Reringing %s\n"), u->full_name);
#endif
			msg_term(msgstr);
			if ((rc = announce(u)) != 0) {
				send_dgram(u, DELETE_INVITE);
				if (rc > 0)
#ifdef HAVE_SNPRINTF
					snprintf(errstr, MAXERR, _("%s refusing messages"), u->full_name);
#else
					sprintf(errstr, _("%s refusing messages"), u->full_name);
#endif
				else
#ifdef HAVE_SNPRINTF
					snprintf(errstr, MAXERR, _("%s not logged in"), u->full_name);
#else
					sprintf(errstr, _("%s not logged in"), u->full_name);
#endif
				show_error(errstr);
				free_user(u);
			}
		}
	}
}

void
rering_all(void)
{
	yuser *u, *next;
	int rc;

	if (send_auto(LEAVE_INVITE) != 0) {
		show_error("rering_all: send_auto() failed");
		kill_auto();
	}
	for (u = wait_list; u; u = next) {
		next = u->next;
		send_dgram(u, LEAVE_INVITE);
		u->last_invite = (ylong) time(NULL);
		if ((rc = announce(u)) != 0) {
			send_dgram(u, DELETE_INVITE);
			if (rc > 0)
#ifdef HAVE_SNPRINTF
				snprintf(errstr, MAXERR, _("%s refusing messages"), u->full_name);
#else
				sprintf(errstr, _("%s refusing messages"), u->full_name);
#endif
			else
#ifdef HAVE_SNPRINTF
				snprintf(errstr, MAXERR, _("%s not logged in"), u->full_name);
#else
				sprintf(errstr, _("%s not logged in"), u->full_name);
#endif
			show_error(errstr);
			free_user(u);
		}
	}
}

void
send_winch(yuser *user)
{
	register yuser *u;

	v3w.rows = htons(user->t_rows);
	v3w.cols = htons(user->t_cols);

	if (user == me) {
		v3w.code = V3_MYWIN;
		for (u = connect_list; u; u = u->next)
			if (u->remote.vmajor > 2)
				send_oob(u->fd, &v3w, V3_WINCHLEN);
		winch_exec();
	} else if (user->remote.vmajor > 2) {
		v3w.code = V3_YOURWIN;
		send_oob(user->fd, &v3w, V3_WINCHLEN);
	}
}

void
send_region(void)
{
	register yuser *u;

	v3w.code = V3_REGION;
	v3w.rows = htons(me->rows);
	v3w.cols = htons(me->cols);

	for (u = connect_list; u; u = u->next)
		if (u->remote.vmajor > 2)
			send_oob(u->fd, &v3w, V3_WINCHLEN);
}

void
send_end_region(void)
{
	register yuser *u;

	v3w.code = V3_REGION;
	v3w.rows = htons(0);
	v3w.cols = htons(0);

	for (u = connect_list; u; u = u->next)
		if (u->remote.vmajor > 2)
			send_oob(u->fd, &v3w, V3_WINCHLEN);
}

/*
 * Send some output to a given user.  Sends the output to all connected users
 * if the given user is either "me" or NULL.
 */
void
send_users(yuser *user, ychar *buf, int len, ychar *cl_buf, int cl_len)
{
	register ychar *o, *b;
	register yuser *u;
	static ychar *o_buf = NULL;
	static int o_len = 0;

	/* data transparency */

	if ((len << 1) > o_len) {
		o_len = (len << 1) + 512;
		o_buf = (ychar *) realloc_mem(o_buf, o_len);
	}
	for (b = buf, o = o_buf; len > 0; b++, len--) {
		*(o++) = *b;
		if (*b == V3_OOB)
			*(o++) = V3_OOB;
	}

	if (user && user != me) {
		if (user->fd > 0) {	/* just to be sure... */
			if (user->remote.vmajor > 2)
				if (user->crlf)
					write(user->fd, cl_buf, cl_len);
				else
					write(user->fd, o_buf, o - o_buf);
			else
				write(user->fd, buf, b - buf);
		}
	} else
		for (u = connect_list; u; u = u->next)
			if (u->remote.vmajor > 2)
				if (u->crlf)
					write(u->fd, cl_buf, cl_len);
				else
					write(u->fd, o_buf, o - o_buf);
			else
				write(u->fd, buf, b - buf);

}

/*
 * Display user input.  Emulate ANSI.
 */
void
show_input(yuser *user, ychar *buf, int len)
{
	if (user->got_esc) {
process_esc:
		for (; len > 0; len--, buf++) {
			vt100_process(user, *buf);
			if (user->got_esc == 0) {
				len--, buf++;
				break;
			}
		}
	}
	for (; len > 0; len--, buf++) {
		if (is_printable(*buf)) {
			if (user->x + 1 >= user->cols) {
				if (user->flags & FL_WRAP) {
					if (*buf == ' ')
						newline_term(user);
					else if (word_wrap(user) >= 0)
						addch_term(user, *buf);
					else {
						addch_term(user, *buf);
						newline_term(user);
					}
				} else {
					if (user->x == user->cols - 1) {
						if (user->onend) {
							newline_term(user);
							addch_term(user, *buf);
							user->onend = 0;
						} else {
							addch_term(user, *buf);
							user->onend = 1;
						}
					}
				}
			} else
				addch_term(user, *buf);
		} else if (*buf == user->RUB && !(user->flags & FL_RAW))
			rub_term(user);
		else if (*buf == user->WORD && !(user->flags & FL_RAW))
			word_term(user);
		else if (*buf == user->KILL && !(user->flags & FL_RAW))
			kill_term(user);
		else {
			switch (*buf) {
			case 7:/* Bell */
				if (def_flags & FL_BEEP)
					putc(7, stderr);
				break;
			case 8:/* Backspace */
				if (user->x > 0)
					move_term(user, user->y, user->x - 1);
				break;
			case 9:/* Tab */
				tab_term(user);
				break;
			case 10:	/* Line Feed */
				if (user->crlf)
					lf_term(user);
				else
					newline_term(user);
				break;
			case 13:	/* Carriage Return */
				if (user->flags & FL_RAW)
					move_term(user, user->y, 0);
				else if (user->crlf)
					cr_term(user);
				else
					newline_term(user);
				break;
#ifdef YTALK_COLOR
			case 14:
				user->csx = 1;
				break;
			case 15:
				user->csx = 0;
				break;
#endif
			case 27:	/* Escape */
				user->got_esc = 1;
				user->ac = 0;
				user->av[0] = 0;
				user->av[1] = 0;
				len--, buf++;
				goto process_esc;	/* ugly but _fast_ */
			default:
				if (*buf < ' ') {
					/* show a control char */
				}
			}
		}
	}
	flush_term(user);
}

/*
 * Process keyboard input.
 */
void
my_input(yuser *user, ychar *buf, int len)
{
	register ychar *c, *n;
	register int i, j;
	ychar *nbuf = 0;

	/* If someone's waiting for input, give it to them! */

	if (input_flag) {
		io_ptr = buf;
		io_len = len;
		return;
	}
	/* Substitution buffer for LF -> CRLF */
	if (len > 0)
		nbuf = get_mem(len * 2 * sizeof(ychar));

	/* Process input normally */

	while (len > 0) {
		/* check for a menu in process */

		if (in_ymenu()) {
			io_ptr = buf;
			io_len = len;
			update_ymenu();
			buf = io_ptr;
			len = io_len;
			io_len = 0;
		}
		/* check for a running process */

		if (running_process) {
			io_ptr = buf;
			io_len = len;
			update_exec();
			buf = io_ptr;
			len = io_len;
			io_len = 0;
		} else {
			/* do normal input */

			while (len > 0) {
				c = buf;
				for (; len > 0; buf++, len--) {
					if (*buf == me->old_rub || *buf == 8 || *buf == 0x7f)
						*buf = me->RUB;
					else if (*buf == 3)	/* Ctrl-C */
						bail(0);
					else if (*buf == '\r')	/* CR */
						*buf = '\n';
					else if (*buf == 27 || *buf == ALTESC)	/* Esc or ALTESC */
						break;
#ifdef YTALK_COLOR
					else if (*buf == 11)	/* ^K - Color menu */
						break;
					else if (*buf == 2)		/* ^B - Bold */
						break;
#endif
					else if (*buf == 14 || *buf == 6 || *buf == 16)	/* ^N, ^F or ^P */
						break;
					else if (*buf == 12 || *buf == 18)	/* ^L or ^R */
						break;
				}
				if ((i = buf - c) > 0) {
					if (user != NULL && user != me) {
						if (def_flags & FL_BEEP)
							putc(7, stderr);
					} else {
						for (n = nbuf, j = 0; j < (buf - c); j++, n++) {
							if (c[j] == '\n') {
								*(n++) = '\r';
								*n = '\n';
							} else {
								*n = c[j];
							}
						}
						j = (n - nbuf);
						show_input(me, nbuf, j);
						send_users(user, c, i, nbuf, j);
					}
				}
				if (len > 0) {	/* we broke for a special
						 * char */
					if (*buf == 27)	/* ESC */
						break;
					else if (*buf == ALTESC) { /* ALTESC -- alternate menu key */
						if (!in_ymenu()) {
							show_ymenu();
							update_ymenu();
						}
						buf++, len--;
#ifdef YTALK_COLOR
					} else if (*buf == 11) {	/* ^K - Color menu ;) */
						show_colormenu();
						buf++, len--;
					} else if (*buf == 2) {		/* ^B - Bold */
						if (me->c_at & A_BOLD) {
							me->c_at &= ~A_BOLD;
							send_users(me, (ychar *)"[21m", 5, (ychar *)"[21m", 5);
						}
						else {
							me->c_at |= A_BOLD;
							send_users(me, (ychar *)"[01m", 5, (ychar *)"[01m", 5);
						}
						buf++, len--;
#endif
					} else if (*buf == 14) {	/* ^N - scroll down */
						scroll_down(scuser);
						retitle_all_terms();
						buf++, len--;
					} else if (*buf == 6) {	/* ^F - window forward */
						if (scrollback_lines > 0) {
							do {
								scuser = scuser->unext;
							} while (scuser && scuser->scr == NULL);
							if (scuser == NULL)
								scuser = me;
							retitle_all_terms();
						}
						buf++, len--;
					} else if (*buf == 16) {	/* ^P - scroll up */
						scroll_up(scuser);
						retitle_all_terms();
						buf++, len--;
					} else if (*buf == 12 || *buf == 18) {	/* ^L or ^R */
						redraw_all_terms();
						buf++, len--;
					}
				}
			}
		}

		/* start a menu if necessary */
		if (len > 0) {
			buf++, len--;
			if (len <= 0) {
				show_ymenu();
				update_ymenu();
			}
		}
	}

	if (nbuf != 0)
		free_mem(nbuf);
}

void
lock_flags(ylong flags)
{
	register yuser *u;

	me->flags = flags | FL_LOCKED;

	/* send to connected users... */

	v3f.code = V3_LOCKF;
	v3f.flags = htonl(me->flags);
	for (u = connect_list; u; u = u->next)
		if (u->remote.vmajor > 2)
			send_oob(u->fd, &v3f, V3_FLAGSLEN);
}

void
unlock_flags(void)
{
	register yuser *u;

	me->flags = def_flags;

	/* send to connected users... */

	v3f.code = V3_UNLOCKF;
	v3f.flags = htonl(me->flags);
	for (u = connect_list; u; u = u->next)
		if (u->remote.vmajor > 2)
			send_oob(u->fd, &v3f, V3_FLAGSLEN);
}
