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
#include "ymenu.h"

/* Some systems, notably Solaris, don't have sys/signal.h */
#include <signal.h>

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#include "cwin.h"

typedef struct _ywin {
	struct _ywin *next;	/* next ywin in linked list */
	yuser *user;		/* user pointer */
	WINDOW *win;		/* window pointer */
	WINDOW *swin;		/* scroll viewport window */
	int height, width;	/* height and width in characters */
	int row, col;		/* row and column position on screen */
} ywin;

static ywin *head;		/* head of linked list */

#ifdef YTALK_COLOR
extern int ui_colors;
extern int ui_attr;
#endif

/* ---- local functions ---- */

/*
 * Take input from the user.
 */
static void
curses_input(int fd)
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
new_ywin(yuser *user)
{
	register ywin *out;

	out = (ywin *) get_mem(sizeof(ywin));
	memset(out, 0, sizeof(ywin));
	out->user = user;
	return out;
}

static void
make_win(ywin *w, int height, int width, int row, int col)
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

static void
draw_title(ywin *w)
{
	int x;
	int rl = 0, rj = 0;
	char *ta, *t;
	t = ta = (char *) get_mem(COLS * sizeof(char));
	user_title(t, COLS - 1, w->user);
	move(w->row - 1, w->col);
#ifdef YTALK_COLOR
	attron(COLOR_PAIR(ui_colors) | ui_attr);
#else
	attron(A_REVERSE);
#endif
	for (x = 0; x < w->width; x++) {
		if (x >= 1 && *t && !rj) {
			/* Do we want the rest on the right? */
			if (*t == 9) {
				rj = 1;
				rl = strlen(t);
				x--;
			} else if (*t > 31) {
				addch(*t);
			}
			t++;
		} else if (rj && *t && (x == (w->width - rl))) {
			for (; *t && (x < w->width); t++) {
				if (*t > 31) {
					addch(*t);
					x++;
				}
			}
			x--;
		} else {
			addch(' ');
		}
	}
#ifdef YTALK_COLOR
	attroff(COLOR_PAIR(ui_colors) | ui_attr);
#else
	attroff(A_REVERSE);
#endif

	/* Redundant refresh() to work around a bug (?) in newer ncurses
	 * versions. Without it, the top line in each user window will lose
	 * its attributes on redraw.
	 */
	refresh();

	free_mem(ta);
}

/*
 * Return number of lines per window, given "wins" windows.
 */
static int
win_size(int wins)
{
	if (wins == 0)
		return 0;
	return (LINES - 1) / wins;
}

/*
 * Break down and redraw all user windows.
 */
static void
curses_redraw(void)
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

	resize_ymenu();
	if (in_ymenu())
		__refresh_ymenu();

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
			__update_scroll_curses(w->user);
			wnoutrefresh(w->swin);
		} else
			wnoutrefresh(w->win);
	}
	doupdate();
}

/*
 * Start curses and set all options.
 */
static void
curses_start(void)
{
#ifdef YTALK_COLOR
	short i, fg, bg;
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

#  ifndef COLOR_DEFAULT
#    define COLOR_DEFAULT (-1)
#  endif

	if (has_colors()) {
		start_color();
#  ifdef NCURSES_VERSION
		use_default_colors();
#  endif
		for (i = 0; i < 64; i++) {
			fg = i & 7;
			bg = i >> 3;
#  ifdef NCURSES_VERSION
			if (fg == COLOR_WHITE) fg = COLOR_DEFAULT;
			if (bg == COLOR_BLACK) bg = COLOR_DEFAULT;
#  endif
			init_pair(i + 1, fg, bg);
		}
	} else {
		/*
		 * We are on a monochrome terminal. Use reverse video for title bars.
		 */
		ui_attr = A_REVERSE;
	}
#endif
	clear();
}

/*
 * Restart curses... window size has changed.
 */
static RETSIGTYPE
curses_restart(int sig)
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
init_curses(void)
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
end_curses(void)
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
open_curses(yuser *user)
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
		w = head = new_ywin(user);
	else
		for (w = head; w; w = w->next)
			if (w->next == NULL) {
				w->next = new_ywin(user);
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
close_curses(yuser *user)
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

void
waddyac(WINDOW *w, yachar c)
{
	int x, y;
#ifdef YTALK_COLOR
	chtype cc = c.l;
	getyx(w, y, x);
	if (c.v) {
		switch (cc) {
		case 'l': cc = ACS_ULCORNER; break;
		case 'm': cc = ACS_LLCORNER; break;
		case 'k': cc = ACS_URCORNER; break;
		case 'j': cc = ACS_LRCORNER; break;
		case 't': cc = ACS_LTEE; break;
		case 'u': cc = ACS_RTEE; break;
		case 'v': cc = ACS_BTEE; break;
		case 'w': cc = ACS_TTEE; break;
		case 'q': cc = ACS_HLINE; break;
		case 'x': cc = ACS_VLINE; break;
		case 'n': cc = ACS_PLUS; break;
		case 'o': cc = ACS_S1; break;
		case 's': cc = ACS_S9; break;
		case '`': cc = ACS_DIAMOND; break;
		case 'a': cc = ACS_CKBOARD; break;
		case 'f': cc = ACS_DEGREE; break;
		case 'g': cc = ACS_PLMINUS; break;
		case '~': cc = ACS_BULLET; break;
		}
	}
	if (cc == 0)
		cc = ' ';
	waddch(w, cc | COLOR_PAIR(1 + (c.b | c.c << 3)) | c.a);
	if (x >= COLS - 1)
		wmove(w, y, x);
#else
	getyx(w, y, x);
	waddch(w, c);
	if (x >= COLS - 1)
		wmove(w, y, x);
#endif
}

#ifdef YTALK_COLOR
void
addch_curses(yuser *user, yachar c)
{
	waddyac(((ywin *) (user->term))->win, c);
}
#else
void
addch_curses(yuser *user, ylong c)
{
	register ywin *w;
	register int x, y;
	w = (ywin *) (user->term);
	getyx(w->win, y, x);
	waddch(w->win, c);
	if (x >= COLS - 1)
		wmove(w->win, y, x);
}
#endif /* YTALK_COLOR */

void
move_curses(yuser *user, int y, int x)
{
	register ywin *w;

	w = (ywin *) (user->term);
	wmove(w->win, y, x);
}

void
clreol_curses(yuser *user)
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtoeol(w->win);
}

void
clreos_curses(yuser *user)
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtobot(w->win);
}

void
scroll_curses(yuser *user)
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

#ifdef HAVE_KEYPAD
void
keypad_curses(yuser *user, int bf)
{
	ywin *w;
	w = (ywin *) (user->term);
	keypad(w->win, bf);
}
#endif /* HAVE_KEYPAD */

void
flush_curses(yuser *user)
{
	register ywin *w;

	w = (ywin *) (user->term);
	if (w->user->scroll)
		wnoutrefresh(w->swin);
	else
		wnoutrefresh(w->win);
	if (in_ymenu())
		__refresh_ymenu();
	doupdate();
}

void
retitle_all_curses(void)
{
	ywin *w;
	for (w = head; w; w = w->next) {
		draw_title(w);
	}
	if (in_ymenu())
		__refresh_ymenu();
	move(LINES - 1, COLS - 1);
	wnoutrefresh(stdscr);
	doupdate();
}

/*
 * Refresh all visible windows
 *
 */
void
refresh_curses(void)
{
	register ywin *w;
	wnoutrefresh(stdscr);
	for (w = head; w; w = w->next) {
		if (w->user->scroll) {
			__update_scroll_curses(w->user);
			draw_title(w);
			touchwin(w->swin);
			wnoutrefresh(w->swin);
		} else {
			draw_title(w);
			touchwin(w->win);
			wnoutrefresh(w->win);
		}
	}
	if (in_ymenu()) {
		__refresh_ymenu();
	}
	move(LINES - 1, COLS - 1);
	wnoutrefresh(stdscr);
	doupdate();
}

/*
 * Clear and redisplay.
 */
void
__redisplay_curses(void)
{
	register ywin *w;

	clear();

	if (bottom_msg != NULL)
		update_message_curses();

	wnoutrefresh(stdscr);
	for (w = head; w; w = w->next) {
		if (w->user->scroll) {
			__update_scroll_curses(w->user);
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
		__refresh_ymenu();
	}
}

void
redisplay_curses(void)
{
	__redisplay_curses();
	doupdate();
}
/*
 * Set raw mode.
 */
void
set_raw_curses(void)
{
	raw();
}

/*
 * Set cooked mode.
 */
void
set_cooked_curses(void)
{
	noraw();
	crmode();
	noecho();
}

void
start_scroll_curses(yuser *user)
{
	ywin *w = (ywin *) (user->term);
	w->swin = dupwin(w->win);
	if (w->swin == NULL) {
		end_term();
		show_error("Couldn't create scrolling viewport");
		bail(YTE_ERROR);
	}
	if (in_ymenu())
		__refresh_ymenu();
	doupdate();
}

void
end_scroll_curses(yuser *user)
{
	ywin *w = (ywin *) (user->term);
	if (w->swin != NULL) {
		delwin(w->swin);
		w->swin = NULL;
		redraw_term(user, 0);
		draw_title(w);
		wnoutrefresh(stdscr);
		wnoutrefresh(w->win);
		if (in_ymenu())
			__refresh_ymenu();
		doupdate();
	}
}

void
__update_scroll_curses(yuser *user)
{
	ywin *w = (ywin *) (user->term);
	long int r, i, fb = -1;
	if (w->swin == NULL)
		return;
	werase(w->swin);
	for (r = 0; r <= (user->rows - 1); r++) {
		wmove(w->swin, r, 0);
		if (((user->scrollpos + r) < 0) ||
			((user->scrollpos + r ) >= scrollback_lines) ||
			((user->scrollback[user->scrollpos + r] == NULL)))
		{
			/* Borrow lines from active screen */
			if (fb == -1)
				fb = r;
			for (i = 0; i < user->cols; i++)
				waddyac(w->swin, user->scr[r - fb][i]);
		} else {
#ifdef YTALK_COLOR
			for (i = 0; (i < user->cols) && (user->scrollback[user->scrollpos + r][i].l != '\0'); i++)
#else
			for (i = 0; (i < user->cols) && (user->scrollback[user->scrollpos + r][i] != '\0'); i++)
#endif
				if ((user->scrollpos + r) >= 0)
					waddyac(w->swin, user->scrollback[user->scrollpos + r][i]);
		}
	}
	wnoutrefresh(w->swin);
	move(LINES - 1, COLS - 1);
	wnoutrefresh(stdscr);
	if (in_ymenu())
		__refresh_ymenu();
}

void
update_scroll_curses(yuser *user)
{
	__update_scroll_curses(user);
	doupdate();
}

void
update_message_curses(void)
{
	move(LINES - 1, 0);
	if (bottom_msg != NULL)
		addstr(bottom_msg);
	clrtoeol();
	move(1, COLS - 1);
	refresh();
}
