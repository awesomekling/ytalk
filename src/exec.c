/* exec.c -- run a command inside a window */

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

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <signal.h>
#include <sys/wait.h>

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_SGTTY_H
#  include <sgtty.h>
#  ifdef hpux
#   include <sys/bsdtty.h>
#  endif
# endif
#endif

#if defined(HAVE_PTSNAME) && defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)
# define USE_DEV_PTMX
#endif

int running_process = 0;	/* flag: is process running? */
static int pid;			/* currently executing process id */
static int pfd;			/* currently executing process fd */
static int prows, pcols;	/* saved rows, cols */

/* ---- local functions ---- */

#ifndef HAVE_SETSID
static int
setsid()
{
# ifdef TIOCNOTTY
    register int fd;

    if((fd = open("/dev/tty", O_RDWR)) >= 0)
    {
	ioctl(fd, TIOCNOTTY);
	close(fd);
    }
    return fd;
# endif
}
#endif

#ifdef USE_DEV_PTMX
int needtopush=0;
#endif

#ifndef SIGCHLD
# define SIGCLD SIGCHLD
#endif

static int
getpty(name)
  char *name;
{
    register int pty, tty;
    char *tt;
    extern char *ttyname();

#ifdef USE_DEV_PTMX
    RETSIGTYPE (*sigchld)();
    int r = 0;
#endif

    /* look for a Solaris/UNIX98-type pseudo-device */

#ifdef USE_DEV_PTMX
    if ((pty=open("/dev/ptmx", O_RDWR)) >= 0)
    {
    	/* grantpt() might want to fork/exec! */
	sigchld = signal(SIGCHLD, SIG_DFL);
	r |= grantpt(pty);
	r |= unlockpt(pty);
	tt = ptsname(pty);
	signal(SIGCHLD, sigchld);
	if (r == 0 && tt != NULL)
	{
	    strcpy(name, tt);
	    needtopush=1;
	    return pty;
	}
    }
#endif

#ifdef HAVE_TTYNAME

    /* look for an older SYSV-type pseudo device */

    if((pty = open("/dev/ptc", O_RDWR)) >= 0)
    {
	if((tt = ttyname(pty)) != NULL)
	{
	    strcpy(name, tt);
	    return pty;
	}
	close(pty);
    }

#endif

    /* scan Berkeley-style */

    strcpy(name, "/dev/ptyp0");
    while(access(name, 0) == 0)
    {
	if((pty = open(name, O_RDWR)) >= 0)
	{
	    name[5] = 't';
	    if((tty = open(name, O_RDWR)) >= 0)
	    {
		close(tty);
		return pty;
	    }
	    name[5] = 'p';
	    close(pty);
	}

	/* get next pty name */

	if(name[9] == 'f')
	{
	    name[8]++;
	    name[9] = '0';
	}
	else if(name[9] == '9')
	    name[9] = 'a';
	else
	    name[9]++;
    }
    errno = ENOENT;
    return -1;
}

static void
exec_input(fd)
  int fd;
{
    register int rc;
    static ychar buf[MAXBUF];

    if((rc = read(fd, buf, MAXBUF)) <= 0)
    {
	kill_exec();
	errno = 0;
	show_error("command shell terminated");
	return;
    }
    show_input(me, buf, rc);
    send_users(me, buf, rc);
}

static void
calculate_size(rows, cols)
  int *rows, *cols;
{
    register yuser *u;

    *rows = me->t_rows;
    *cols = me->t_cols;

    for(u = connect_list; u; u = u->next)
	if(u->remote.vmajor > 2)
	{
	    if(u->remote.my_rows > 1 && u->remote.my_rows < *rows)
		*rows = u->remote.my_rows;
	    if(u->remote.my_cols > 1 && u->remote.my_cols < *cols)
		*cols = u->remote.my_cols;
	}
}

/* ---- global functions ---- */

/* Execute a command inside my window.  If command is NULL, then execute
 * a shell.
 */
void
execute(command)
  char *command;
{
    int fd;
    char name[20], *shell;

    if(me->flags & FL_LOCKED)
    {
	errno = 0;
	show_error("alternate mode already running");
	return;
    }
    if((fd = getpty(name)) < 0)
    {
	msg_term(me, "cannot get pseudo terminal");
	return;
    }

/* init the pty a bit (inspired from the screen(1) sources, pty.c) */

#if defined(HAVE_TCFLUSH) && defined(TCIOFLUSH)
    tcflush(fd, TCIOFLUSH);
#else
# ifdef TIOCFLUSH
    ioctl(fd, TIOCFLUSH, NULL);
# else
#  ifdef TIOCEXCL
    ioctl(fd, TIOCEXCL, NULL);
#  endif
# endif
#endif

    if((shell = (char *)getenv("SHELL")) == NULL)
	shell = "/bin/sh";
    calculate_size(&prows, &pcols);
    if((pid = fork()) == 0)
    {
	close(fd);
	close_all();
        if(setsid() < 0)
            exit(-1);
        if((fd = open(name, O_RDWR)) < 0)
            exit(-1);

/* Solaris seems to need this... */
#if defined(HAVE_STROPTS_H) && defined(I_PUSH)
	if (needtopush)
	{
	    ioctl(fd, I_PUSH, "ptem");
	    ioctl(fd, I_PUSH, "ldterm");
	}
#endif

        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);

	/* tricky bit -- ignore WINCH */

#ifdef SIGWINCH
	signal(SIGWINCH, SIG_IGN);
#endif

	/* set terminal characteristics */

	set_terminal_flags(fd);
	set_terminal_size(fd, prows, pcols);
#ifndef NeXT
# ifdef HAVE_PUTENV
	putenv("TERM=vt100");
# endif
#endif

	/* execute the command */

	if(command)
	    execl(shell, shell, "-c", command, NULL);
	else
	    execl(shell, shell, NULL);
	perror("execl");
	(void)exit(-1);
    }
    if(pid < 0)
    {
	show_error("fork() failed");
	return;
    }
    set_win_region(me, prows, pcols);
    sleep(1);
    pfd = fd;
    running_process = 1;
    lock_flags(FL_RAW | FL_SCROLL);
    set_raw_term();
    add_fd(fd, exec_input);
}

/* Send input to the command shell.
 */
void
update_exec()
{
    (void)write(pfd, io_ptr, io_len);
    io_len = 0;
}

/* Kill the command shell.
 */
void
kill_exec()
{
    if(!running_process)
	return;
    remove_fd(pfd);
    close(pfd);
    running_process = 0;
    unlock_flags();
    set_cooked_term();
    end_win_region(me);
}

/* Send a SIGWINCH to the process.
 */
void
winch_exec()
{
    int rows, cols;

    if(!running_process)
	return;

    /* if the winch has no effect, return now */

    calculate_size(&rows, &cols);
    if(rows == prows && cols == pcols)
    {
	if(prows != me->rows || pcols != me->cols)
	    set_win_region(me, prows, pcols);
	return;
    }

    /* oh well -- redo everything */

    prows = rows;
    pcols = cols;
    set_terminal_size(pfd, prows, pcols);
    set_win_region(me, prows, pcols);
#ifdef SIGWINCH
    kill(pid, SIGWINCH);
#endif
}
