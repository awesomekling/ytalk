/* cwin.c -- curses interface */

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
#include "mem.h"

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

/* Some systems, notably Solaris, don't have sys/signal.h */
#include <signal.h>

#include "cwin.h"
#include "ymenu.h"

typedef struct _ywin {
	struct _ywin *next;	/* next ywin in linked list */
	yuser *user;		/* user pointer */
	WINDOW *win;		/* window pointer */
	WINDOW *swin;		/* scroll viewport window */
	int height, width;	/* height and width in characters */
	int row, col;		/* row and column position on screen */
	char *title;		/* window title string */
} ywin;

static ywin *head;		/* head of linked list */

#ifdef YTALK_COLOR
extern int newui_colors;
extern int newui_attr;
#endif

/* ---- local functions ---- */

/*
 * Take input from the user.
 */
static void
curses_input(fd)
	int fd;
{
	register int rc;
	static ychar buf[MAXBUF];

	if ((rc = read(fd, buf, MAXBUF)) <= 0) {
		if (rc == 0)
			bail(YTE_SUCCESS);
		bail(YTE_ERROR);
	}
	my_input(me, buf, rc);
}

static ywin *
new_ywin(user, title)
	yuser *user;
	char *title;
{
	register ywin *out;

	out = (ywin *) get_mem(sizeof(ywin) + strlen(title) + 1);
	(void) memset(out, 0, sizeof(ywin));
	out->user = user;
	out->title = ((char *) out) + sizeof(ywin);
	strcpy(out->title, title);
	return out;
}

static void
make_win(w, height, width, row, col)
	ywin *w;
	int height, width, row, col;
{
	if ((w->win = newwin(height, width, row, col)) == NULL) {
		w = (ywin *) (me->term);
		if (w->win != NULL)
			show_error("make_win: newwin() failed");
		bail(YTE_ERROR);
	}
	w->height = height;
	w->width = width;
	w->row = row;
	w->col = col;
	scrollok(w->win, FALSE);
	wmove(w->win, 0, 0);
}

#ifdef YTALK_COLOR
static void
new_draw_title(w)
	ywin *w;
{
	int x;
	char *t = w->title;
	move(w->row - 1, w->col);
	for (x = 0; x < w->width; x++) {
		if (x >= 1 && *t) {
			addch(*t | COLOR_PAIR(newui_colors) | newui_attr);
			t++;
		} else if (x == (w->width - 7)) {
			if (w->user == scuser)
				addch('*' | COLOR_PAIR(newui_colors) | newui_attr);
			else
				addch(' ' | COLOR_PAIR(newui_colors) | newui_attr);
		} else if (x == (w->width - 5)) {
			/*
			 * FIXME: This should be optional. Add a
			 * "show_version" flag to rc.
			 */
			if (w->user->remote.vmajor >= 2) {
				addch('Y' | COLOR_PAIR(newui_colors) | newui_attr);
				if (w->user->remote.vmajor >= 0 && w->user->remote.vmajor < 10)
					addch((w->user->remote.vmajor + '0') | COLOR_PAIR(newui_colors) | newui_attr);
				else
					addch('?' | COLOR_PAIR(newui_colors) | newui_attr);
				addch('.' | COLOR_PAIR(newui_colors) | newui_attr);
				if (w->user->remote.vmajor >= 3) {
					if (w->user->remote.vminor >= 0 && w->user->remote.vminor < 10)
						addch((w->user->remote.vminor + '0') | COLOR_PAIR(newui_colors) | newui_attr);
					else
						addch('?' | COLOR_PAIR(newui_colors) | newui_attr);
				} else {
					addch('?' | COLOR_PAIR(newui_colors) | newui_attr);
				}
			} else {
				addch('U' | COLOR_PAIR(newui_colors) | newui_attr);
				addch('N' | COLOR_PAIR(newui_colors) | newui_attr);
				addch('I' | COLOR_PAIR(newui_colors) | newui_attr);
				addch('X' | COLOR_PAIR(newui_colors) | newui_attr);
			}
			x += 3;
		} else {
			addch(' ' | COLOR_PAIR(newui_colors) | newui_attr);
		}
	}
}
#endif

static void
old_draw_title(w)
	ywin *w;
{
	register int pad, x;
	register char *t;

	pad = (w->width - strlen(w->title)) / 2;
	move(w->row - 1, w->col);
	x = 0;
	for (; x < pad - 2; x++)
		if (def_flags & FL_VT100)
			addch(ACS_HLINE);
		else
			addch('-');
	if (pad >= 2) {
		if (def_flags & FL_VT100)
			addch(ACS_RTEE);
		else
			addch('=');
		addch(' ');
		x += 2;
	}
	for (t = w->title; *t && x < w->width; x++, t++)
		addch(*t);
	if (pad >= 2) {
		addch(' ');
		if (def_flags & FL_VT100)
			addch(ACS_LTEE);
		else
			addch('=');
		x += 2;
	}
	for (; x < w->width; x++)
		if (def_flags & FL_VT100)
			addch(ACS_HLINE);
		else
			addch('-');
}

static void
draw_title(w)
	ywin *w;
{
#ifdef YTALK_COLOR
	if (def_flags & FL_NEWUI)
		new_draw_title(w);
	else
#endif
		old_draw_title(w);
}

/*
 * Return number of lines per window, given "wins" windows.
 */
static int
win_size(wins)
	int wins;
{
	if (wins == 0)
		return 0;
	return (LINES - 1) / wins;
}

/*
 * Break down and redraw all user windows.
 */
static void
curses_redraw()
{
	register ywin *w;
	register int row, wins, wsize;

	/* kill old windows */

	wins = 0;
	for (w = head; w; w = w->next) {
		if (w->win) {
			delwin(w->win);
			w->win = NULL;
		}
		if (w->swin) {
			delwin(w->swin);
			w->swin = NULL;
		}
		wins++;
	}
	if ((wsize = win_size(wins)) < 3) {
		end_term();
		errno = 0;
		show_error("curses_redraw: window size too small");
		bail(YTE_ERROR);
	}
	/* make new windows */

	clear();
	wnoutrefresh(stdscr);
	row = 0;
	for (w = head; w; w = w->next) {
		if (w->next) {
			make_win(w, wsize - 1, COLS, row + 1, 0);
			resize_win(w->user, wsize - 1, COLS);
		} else {
			make_win(w, LINES - row - 2, COLS, row + 1, 0);
			resize_win(w->user, LINES - row - 2, COLS);
		}
		draw_title(w);
		row += wsize;
		if (w->user->scroll) {
			w->swin = dupwin(w->win);
			if (w->swin == NULL) {
				end_term();
				show_error("Couldn't create scrolling viewport");
				bail(YTE_ERROR);
			}
		}
		wnoutrefresh(stdscr);
		if (w->user->scroll) {
			update_scroll_curses(w->user);
			wnoutrefresh(w->swin);
		} else
			wnoutrefresh(w->win);
	}
	if (in_ymenu()) {
		resize_ymenu();
		refresh_ymenu();
	}
	doupdate();
}

/*
 * Start curses and set all options.
 */
static void
curses_start()
{
#ifdef YTALK_COLOR
	int i, fg, bg;
#endif
	/*
	 * This little snippet is currently on probation. Although the
	 * comment implies it would be absolutely necessary to keep it
	 * around, it seems to cause nothing but trouble.
	 */
	/* LINES = COLS = 0; *//* so resizes will work */
	initscr();
	noraw();
	crmode();
	noecho();
#ifdef YTALK_COLOR

#ifndef COLOR_DEFAULT
#define COLOR_DEFAULT (-1)
#endif

	if (has_colors()) {
		start_color();
		use_default_colors();
		for (i = 0; i < 64; i++) {
			fg = i & 7;
			bg = i >> 3;
			if (fg == COLOR_WHITE)
				fg = COLOR_DEFAULT;
			if (bg == COLOR_BLACK)
				bg = COLOR_DEFAULT;
			init_pair(i + 1, fg, bg);
		}
	} else {
		/*
		 * We are on a monochrome terminal. Use reverse video for
		 * NEWUI.
		 */
		if (def_flags & FL_NEWUI) {
			newui_attr = A_REVERSE;
		}
	}
#endif
	clear();
}

/*
 * Restart curses... window size has changed.
 */
static RETSIGTYPE
curses_restart(sig)
	int sig;
{
	register ywin *w;
	(void) sig;

	/* kill old windows */

	for (w = head; w; w = w->next)
		if (w->win) {
			delwin(w->win);
			w->win = NULL;
			if (w->user->scroll) {
				delwin(w->swin);
				w->swin = NULL;
			}
		}
	/* restart curses */

	endwin();
	curses_start();

	/* fix for ncurses from Max <Marc.Espie@liafa.jussieu.fr> */
	refresh();
	curses_redraw();

	/* some systems require we do this again */

#ifdef SIGWINCH
	signal(SIGWINCH, curses_restart);
#endif
}

/* ---- global functions ---- */

void
init_curses()
{
	curses_start();
	refresh();
	head = NULL;
	add_fd(0, curses_input);/* set up user's input function */

	/* set up SIGWINCH signal handler */

#ifdef SIGWINCH
	signal(SIGWINCH, curses_restart);
#endif
}

void
end_curses()
{
	move(LINES - 1, 0);
	clrtoeol();
	refresh();
	endwin();
}

/*
 * Open a new window.
 */
int
open_curses(user, title)
	yuser *user;
	char *title;
{
	register ywin *w;
	register int wins;

	/*
	 * count the open windows.  We want to ensure at least three lines
	 * per window.
	 */
	wins = 0;
	for (w = head; w; w = w->next)
		wins++;
	if (win_size(wins + 1) < 3)
		return -1;

	/* add the new user */

	if (head == NULL)
		w = head = new_ywin(user, title);
	else
		for (w = head; w; w = w->next)
			if (w->next == NULL) {
				w->next = new_ywin(user, title);
				w = w->next;
				break;
			}
	user->term = w;
#ifdef YTALK_COLOR
	user->c_at = 0;
	user->c_bg = 0;
	user->c_fg = 7;
#endif

	/* redraw */

	curses_redraw();
	return 0;
}

/*
 * Close a window.
 */
void
close_curses(user)
	yuser *user;
{
	register ywin *w, *p;

	/* zap the old user */

	w = (ywin *) (user->term);
	if (w == head)
		head = w->next;
	else {
		for (p = head; p; p = p->next)
			if (w == p->next) {
				p->next = w->next;
				break;
			}
		if (p == NULL) {
			show_error("close_curses: user not found");
			return;
		}
	}
	delwin(w->win);
	free_mem(w);
	w = NULL;
	curses_redraw();
}

static void
raw_addch_curses(user, c)
	yuser *user;
	register ylong c;
{
	register ywin *w;
	register int x, y;
	w = (ywin *) (user->term);
	getyx(w->win, y, x);
	waddch(w->win, c);
	if (x >= COLS - 1)
		wmove(w->win, y, x);
}

void
addch_curses(user, c)
	yuser *user;
#ifdef YTALK_COLOR
	register yachar c;
#else
	register ylong c;
#endif
{
	register ywin *w;
	register int x, y;
#ifdef YTALK_COLOR
	chtype cc = c.l;
#endif

	w = (ywin *) (user->term);
	getyx(w->win, y, x);
#ifdef YTALK_COLOR
	if (c.v) {
		cc = acs_map[cc];
	}
	if (cc == 0) {
		cc = ' ';
	}
	waddch(w->win, cc | COLOR_PAIR(1 + (c.b | c.c << 3)) | c.a);
#else
	waddch(w->win, c);
#endif
	if (x >= COLS - 1)
		wmove(w->win, y, x);
}

void
move_curses(user, y, x)
	yuser *user;
	register int y, x;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wmove(w->win, y, x);
}

void
clreol_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtoeol(w->win);
}

void
clreos_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtobot(w->win);
}

void
scroll_curses(user)
	register yuser *user;
{
	register ywin *w;

	/*
	 * Curses has uses busted scrolling.  In order to call scroll()
	 * effectively, scrollok() must be TRUE.  However, if scrollok() is
	 * TRUE, then placing a character in the lower right corner will
	 * cause an auto-scroll.  *sigh*
	 */
	w = (ywin *) (user->term);
	scrollok(w->win, TRUE);
	scroll(w->win);
	scrollok(w->win, FALSE);

	/*
	 * Some curses won't leave the cursor in the same place, and some
	 * curses programs won't erase the bottom line properly.
	 */
	wmove(w->win, user->t_rows - 1, 0);
	wclrtoeol(w->win);
	wmove(w->win, user->y, user->x);
}

void
keypad_curses(user, bf)
	yuser *user;
	int bf;
{
	ywin *w;
	w = (ywin *) (user->term);
	keypad(w->win, bf);
}

void
flush_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	if (w->user->scroll)
		wnoutrefresh(w->swin);
	else
		wnoutrefresh(w->win);
	if (in_ymenu())
		refresh_ymenu();
	doupdate();
}

void
retitle_all_curses()
{
	ywin *w;
	for (w = head; w; w = w->next) {
		draw_title(w);
	}
	if (in_ymenu())
		refresh_ymenu();
	move(LINES - 1, COLS - 1);
	refresh();
}

/*
 * Clear and redisplay.
 */
void
__redisplay_curses()
{
	register ywin *w;

	clear();
	wnoutrefresh(stdscr);
	for (w = head; w; w = w->next) {
		if (w->user->scroll) {
			update_scroll_curses(w->user);
			draw_title(w);
			wnoutrefresh(stdscr);
			wnoutrefresh(w->swin);
		} else {
			redraw_term(w->user, 0);
			draw_title(w);
			wnoutrefresh(stdscr);
			wnoutrefresh(w->win);
		}
	}
	if (in_ymenu()) {
		refresh_ymenu();
	}
}

void
redisplay_curses()
{
	__redisplay_curses();
	doupdate();
}
/*
 * Set raw mode.
 */
void
set_raw_curses()
{
	raw();
}

/*
 * Set cooked mode.
 */
void
set_cooked_curses()
{
	noraw();
	crmode();
	noecho();
}

/*
 * Curses handler for VT100 menu output. This is pretty ugly. Don't tell! :-)
 */
void
special_menu_curses(user, y, x, section, len)
	yuser *user;
	int y, x;
	int section;
	int len;
{
	register int i;

	if (y < 0 || y >= user->t_rows)
		return;
	if (x < 0 || x >= user->t_cols)
		return;
	move_curses(user, y, x);
	switch (section) {
	case 0:
		raw_addch_curses(user, ACS_ULCORNER);
		break;
	case 1:
	case 2:
		raw_addch_curses(user, ACS_VLINE);
		return;
		break;
	case 3:
	case 4:
		raw_addch_curses(user, ACS_BTEE);
		return;
		break;
	case 5:
		raw_addch_curses(user, ACS_LLCORNER);
		break;
	}

	for (i = 0; i < (len - 2); i++) {
		raw_addch_curses(user, ACS_HLINE);
	}

	switch (section) {
	case 0:
		raw_addch_curses(user, ACS_URCORNER);
		break;
	case 5:
		raw_addch_curses(user, ACS_LRCORNER);
		break;
	}
}

void
start_scroll_curses(user)
	yuser *user;
{
	ywin *w = (ywin *) (user->term);
	w->swin = dupwin(w->win);
	if (w->swin == NULL) {
		end_term();
		show_error("Couldn't create scrolling viewport");
		bail(YTE_ERROR);
	}
	if (in_ymenu())
		refresh_ymenu();
}

void
end_scroll_curses(user)
	yuser *user;
{
	ywin *w = (ywin *) (user->term);
	if (w->swin != NULL) {
		delwin(w->swin);
		w->swin = NULL;
		redraw_term(user, 0);
		draw_title(w);
		refresh();
		wrefresh(w->win);
		if (in_ymenu())
			refresh_ymenu();
	}
}

void
update_scroll_curses(user)
	yuser *user;
{
	u_short r, i;
	ylinebuf *b = user->sca;
	ywin *w = (ywin *) (user->term);
	werase(w->swin);
	if (b == NULL)
		b = user->logbot;
	for (r = 0; r <= (user->rows - 1); r++) {
		if (b->prev != NULL && b->line != NULL) {
			wmove(w->swin, (user->rows - 1) - r, 0);
			for (i = 0; ((i < user->cols) && (i < b->width)); i++) {
#ifdef YTALK_COLOR
				waddch(w->swin, b->line[i].l);
#else
				waddch(w->swin, b->line[i]);
#endif
			}
			b = b->prev;
		} else {
			break;
		}
	}
	wrefresh(w->swin);
	move(LINES - 1, COLS - 1);
	refresh();
	if (in_ymenu())
		refresh_ymenu();
}
