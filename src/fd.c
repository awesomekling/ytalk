/* fd.c */

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
#include "menu.h"

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <signal.h>

#ifdef _AIX
# include <sys/select.h>
#endif

static fd_set fdset;		/* descriptors to select on */
static fd_set fdtmp;		/* descriptors to select on (input_loop) */
static fd_set sel;		/* currently readable descriptors */
static int high_fd = 0;		/* highest fd so far */
int input_flag = 0;		/* flag: waiting for user input */
int user_winch = 0;		/* flag: user window/status changed */

struct fd_func {
    void (*func)();			/* user function */
};
static struct fd_func tag[MAX_FILES];	/* one function per file descriptor */

/* Initialize fdset data.
 */
void
init_fd()
{
    FD_ZERO(&fdset);
}

/* Add a file descriptor to the current checklist.  The supplied
 * function will be called whenever the file descriptor has input
 * waiting.
 */
void
add_fd(fd, user_func)
  int fd;
  void (*user_func)();
{
    if(fd < 0 || fd >= MAX_FILES)
    {
	show_error("add_fd: descriptor out of range");
	return;
    }
    FD_SET(fd, &fdset);
    tag[fd].func = user_func;
    if(fd >= high_fd)
	high_fd = fd + 1;
}

/* Remove a file descriptor from the checklist.
 */
void
remove_fd(fd)
  int fd;
{
    if(fd < 0 || fd >= MAX_FILES)
    {
	show_error("remove_fd: descriptor out of range");
	return;
    }
    FD_CLR(fd, &fdset);
    FD_CLR(fd, &fdtmp);
    FD_CLR(fd, &sel);
}

/* Read an entire length of data.
 * Returns 0 on success, -1 on error.
 */
int
full_read(fd, buf, len)
  int fd;
  register char *buf;
  register int len;
{
    register int rc;

    while(len > 0)
    {
	if((rc = read(fd, buf, len)) <= 0)
	    return -1;
	buf += rc;
	len -= rc;
    }
    return 0;
}

/* -- MAIN LOOPS -- */

static ylong lastping, curtime;

void
main_loop()
{
    register int fd, rc;
    struct timeval tv;

#ifdef HAVE_SIGPROCMASK
    sigset_t mask, old_mask;
#endif
#ifdef HAVE_SIGSETMASK
    int mask, old_mask;
#endif

    /* Some signals need to be blocked while doing internal
     * processing, else some craziness might occur.
     */

#ifdef SIGWINCH
# ifdef HAVE_SIGPROCMASK
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
# endif
# ifdef HAVE_SIGSETMASK
    mask = sigmask(SIGWINCH);
# endif
#endif

#if defined(SIGCHLD)
    signal(SIGCHLD, SIG_IGN);
#else
# if defined(SIGCLD)
    signal(SIGCLD, SIG_IGN);
# endif
#endif

    /* For housecleaning to occur every CLEAN_INTERVAL seconds, we make
     * our own little timer system.  SIGALRM is nice; in fact it's so
     * useful that we'll be using it in other parts of YTalk.  Since
     * we therefore can't use it here, we affect the timer manually.
     */

    house_clean();
    curtime = lastping = (ylong)time(NULL);
    for(;;)
    {
	/* check if we're done */

	if(connect_list == NULL
	&& wait_list == NULL
	&& menu_ptr == NULL
	&& running_process == 0)
	    bail(0);

	/* select */

	sel = fdset;
	if(curtime > lastping + CLEAN_INTERVAL)
	    tv.tv_sec = 0;
	else
	    tv.tv_sec = (lastping + CLEAN_INTERVAL) - curtime;
	tv.tv_usec = 0;
	if((rc = select(high_fd, &sel, 0, 0, &tv)) < 0)
	    if(errno != EINTR)
		show_error("main_loop: select failed");

	/* block signals while doing internal processing */

#ifdef SIGWINCH
# ifdef HAVE_SIGPROCMASK
	sigprocmask(SIG_BLOCK, &mask, &old_mask);
# endif
# ifdef HAVE_SIGSETMASK
	old_mask = sigblock(mask);
# endif
# ifdef HAVE_SIGHOLD
	sighold(SIGWINCH);
# endif
#endif

	/* process file descriptors with input waiting */

	if(rc > 0)
	    for(fd = 0; fd < high_fd; fd++)
		if(FD_ISSET(fd, &sel))
		{
		    errno = 0;
		    tag[fd].func(fd);
		    if(--rc <= 0)
			break;
		}

	/* check timer */

	curtime = (ylong)time(NULL);
	if(curtime - lastping >= CLEAN_INTERVAL)
	{
	    house_clean();
	    lastping = (ylong)time(NULL);
	}

	/* re-allow signals */

#ifdef SIGWINCH
# ifdef HAVE_SIGPROCMASK
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
# endif
# ifdef HAVE_SIGSETMASK
	sigsetmask(old_mask);
# endif
# ifdef HAVE_SIGHOLD
	sigrelse(SIGWINCH);
# endif
#endif

	if(user_winch)
	{
	    /* This is a cute hack that updates a user menu
	     * dynamically as information changes.  So I had
	     * some free time.  there.
	     */
	    user_winch = 0;
	    update_user_menu();
	}
    }
}

/* Input loop.  This loop keeps everything except user input going until
 * input is received from <me>.  This is necessary for answering pressing
 * questions without needing to add a getch_term() function to the terminal
 * definition library.  Hack?  maybe.  Fun, tho.
 */
void
input_loop()
{
    register int fd, rc;
    struct timeval tv;
    static int left_loop;

    left_loop = 0;
    fdtmp = fdset;
    while(io_len <= 0)
    {
	/* select */

	sel = fdtmp;
	if(curtime > lastping + CLEAN_INTERVAL)
	    tv.tv_sec = 0;
	else
	    tv.tv_sec = (lastping + CLEAN_INTERVAL) - curtime;
	tv.tv_usec = 0;
	if((rc = select(high_fd, &sel, 0, 0, &tv)) < 0)
	    if(errno != EINTR)
		show_error("input_loop: select failed");

	/* process file descriptors with input waiting */

	if(rc > 0)
	    for(fd = 0; fd < high_fd; fd++)
		if(FD_ISSET(fd, &sel))
		{
		    /* Here the hack begins.  Any function that takes user
		     * input should clear "input_flag" and return.  This
		     * tells us to ignore this function for now.  Any
		     * function which receives input from <me> should leave
		     * my input in io_ptr/io_len.
		     */
		    errno = 0;
		    input_flag = 1;
		    tag[fd].func(fd);
		    if(left_loop) /* recursive input_loop()s.  argh! */
			return;   /* let my parent function re-call me */
		    if(input_flag == 0)
		    {
			/* don't check this descriptor anymore */
			FD_CLR(fd, &fdtmp);
		    }
		    if(--rc <= 0)
			break;
		}

	/* check timer */

	curtime = (ylong)time(NULL);
	if(curtime - lastping >= CLEAN_INTERVAL)
	{
	    input_flag = 1;
	    house_clean();
	    lastping = (ylong)time(NULL);
	}
    }
    input_flag = 0;
    left_loop = 1;
}
