/* main.c */

/*			   NOTICE
 *
 * Copyright (c) 1990,1992,1993 Britt Yenne.  All rights reserved.
 * 
 * This software is provided AS-IS.  The author gives no warranty,
 * real or assumed, and takes no responsibility whatsoever for any 
 * use or misuse of this software, or any damage created by its use
 * or misuse.
 * 
 * This software may be freely copied and distributed provided that
 * no part of this NOTICE is deleted or edited in any manner.
 * 
 */

/* Mail comments or questions to ytalk@austin.eds.com */

#include "header.h"
#include <signal.h>
#include "menu.h"

char errstr[132];	/* temporary string for errors */
char *vhost = NULL;	/* specified virtual host */

/* Clean up and exit.
 */
void
bail(n)
  int n;
{
    end_term();
    kill_auto();
    (void)exit(n);
}

#ifndef HAVE_STRERROR
#define strerror(n)	(sys_errlist[(n)])
#endif

/* Display an error.
 */
void
show_error(str)
  register char *str;
{
    register char *syserr;
    static int in_error = 0;

    if(errno == 0)
	syserr = "";
    else
	syserr = strerror(errno);

    putc(7, stderr);
    if(in_error == 0 && what_term() != 0)
    {
	in_error = 1;
	if(show_error_menu(str, syserr) < 0)
	{
	    show_error("show_error: show_error_menu() failed");
	    show_error(str);
	}
	else
	    update_menu();
	in_error = 0;
    }
    else
    {
	fprintf(stderr, "%s: %s\n", str, syserr);
	sleep(2);
    }
}

/* Allocate memory.
 */
yaddr
get_mem(n)
  int n;
{
    register yaddr out;
    if((out = (yaddr)malloc(n)) == NULL)
    {
	show_error("malloc() failed");
	bail(YTE_NO_MEM);
    }
    return out;
}

/* Copy a string.
 */
char *
str_copy(str)
  register char *str;
{
    register char *out;
    register int len;

    if(str == NULL)
	return NULL;
    len = strlen(str) + 1;
    out = get_mem(len);
    (void)memcpy(out, str, len);
    return out;
}

/* Reallocate memory.
 */
yaddr
realloc_mem(p, n)
  char *p;
  int n;
{
    register yaddr out;
    if(p == NULL)
	return get_mem(n);
    if((out = (yaddr)realloc(p, n)) == NULL)
    {
	show_error("realloc() failed");
	bail(YTE_NO_MEM);
    }
    return out;
}

/* Process signals.
 */
static RETSIGTYPE
got_sig(n)
  int n;
{
    if(n == SIGINT)
	bail(0);
    bail(YTE_SIGNAL);
}

/*  MAIN  */
int
main(argc, argv)
  int argc;
  char **argv;
{
    int xflg = 0, sflg = 0, yflg = 0, iflg = 0;
    char *prog;

    /* check for a 64-bit mis-compile */

    if(sizeof(ylong) != 4)
    {
    	fprintf(stderr, "The definition for ylong in header.h is wrong;\n\
please change it to an unsigned 32-bit type that works on your computer,\n\
then type 'make clean' and 'make'.\n");
	(void)exit(YTE_INIT);
    }

    /* search for options */

    prog = *argv;
    argv++, argc--;
    while(argc > 0 && **argv == '-')
    {
	if(strcmp(*argv, "-x") == 0
	|| strcmp(*argv, "-nw") == 0)
	{
	    xflg++;	/* disable X from the command line */
	    argv++, argc--;
	}
	else if(strcmp(*argv, "-Y") == 0)
	{
	    yflg++;
	    argv++, argc--;
	}
	else if(strcmp(*argv, "-i") == 0)
	{
	    iflg++;
	    argv++, argc--;
	}
	else if (strcmp(*argv, "-h") == 0)
        {
            argv++;
            vhost = *argv++;
 	    argc -= 2;
        }
	else if(strcmp(*argv, "-s") == 0)
	{
	    sflg++;	/* immediately start a shell */
	    argv++, argc--;
	}
	else
	    argc = 0;	/* force a Usage error */
    }

    /* check for users */

    if(argc <= 0)
    {
	fprintf(stderr, 
"Usage:    %s [options] user[@host][#tty]...\n\
Options:     -i             --    no auto-invite port\n\
             -x             --    do not use the X interface\n\
             -Y             --    require caps on all y/n answers\n\
             -s             --    start a shell\n\
             -h host_or_ip  --    select interface or virtual host\n", prog);
	(void)exit(YTE_INIT);
    }

    /* set up signals */

    signal(SIGINT, got_sig);
    signal(SIGHUP, got_sig);
    signal(SIGQUIT, got_sig);
    signal(SIGABRT, got_sig);
    signal(SIGPIPE, SIG_IGN);

    /* set default options */

    def_flags = FL_XWIN | FL_PROMPTRING | FL_RING;

    /* go for it! */

    errno = 0;
    init_fd();
    read_ytalkrc();
    init_user(vhost);
    if(xflg)
	def_flags &= ~FL_XWIN;
    if(yflg)
	def_flags |= FL_CAPS;
    if(iflg)
	def_flags |= FL_NOAUTO;

    init_term();
    init_socket();
    for(; argc > 0; argc--, argv++)
	invite(*argv, 1);
    if(sflg)
	execute(NULL);
    else
	msg_term(me, "Waiting for connection...");
    main_loop();
    bail(YTE_SUCCESS);

    return 0;	/* make lint happy */
}

