/*
 * src/socket.h
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

#include <netdb.h>

/* ---- talk daemon information structure ---- */

#define MAXDAEMON	5

struct _talkd {
	struct sockaddr_in sock;/* socket */
	int fd;			/* socket file descriptor */
	short port;		/* port number */
	yaddr mptr;		/* message pointer */
	int mlen;		/* message length */
	yaddr rptr;		/* response pointer */
	int rlen;		/* response length */
};

typedef struct _hostinfo {
	struct _hostinfo *next;	/* next in linked list */
	ylong host_addr;	/* host address */
	int dtype;		/* active daemon types bitmask */
} hostinfo;

extern struct _talkd talkd[MAXDAEMON + 1];
extern int daemons;

/* ---- talk daemon I/O structures ---- */

#define NAME_SIZE 9
#define TTY_SIZE 16

typedef struct bsd42_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	struct in_addr sin_addr;
	char sin_zero[8];
} BSD42_SOCK;

/*
 * Control Message structure for earlier than BSD4.2
 */
typedef struct {
	char type;
	char l_name[NAME_SIZE];
	char r_name[NAME_SIZE];
	char filler;
	ylong id_num;
	ylong pid;
	char r_tty[TTY_SIZE];
	BSD42_SOCK addr;
	BSD42_SOCK ctl_addr;
} CTL_MSG;

/*
 * Control Response structure for earlier than BSD4.2
 */
typedef struct {
	char type;
	char answer;
	unsigned short filler;
	ylong id_num;
	BSD42_SOCK addr;
} CTL_RESPONSE;

#define NTALK_NAME_SIZE 12

/*
 * Control Message structure for BSD4.2
 */
typedef struct {
	unsigned char vers;
	char type;
	unsigned short filler;
	ylong id_num;
	BSD42_SOCK addr;
	BSD42_SOCK ctl_addr;
	ylong pid;
	char l_name[NTALK_NAME_SIZE];
	char r_name[NTALK_NAME_SIZE];
	char r_tty[TTY_SIZE];
} CTL_MSG42;

/*
 * Control Response structure for BSD4.2
 */
typedef struct {
	unsigned char vers;
	char type;
	char answer;
	char filler;
	ylong id_num;
	BSD42_SOCK addr;
} CTL_RESPONSE42;

#define	TALK_VERSION	1	/* protocol version */

/*
 * Dgram Types.
 *
 * These are the "type" arguments to feed to send_dgram().  Each acts either on
 * the remote daemon or the local daemon, as marked.
 */

#define LEAVE_INVITE	0	/* leave an invitation (local) */
#define LOOK_UP		1	/* look up an invitation (remote) */
#define DELETE		2	/* delete erroneous invitation (remote) */
#define ANNOUNCE	3	/* ring a user (remote) */
#define DELETE_INVITE	4	/* delete my invitation (local) */
#define AUTO_LOOK_UP	5	/* look up auto-invitation (remote) */
#define AUTO_DELETE	6	/* delete erroneous auto-invitation (remote) */

/* EOF */
