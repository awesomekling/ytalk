/*
 * src/cwin.c
 * curses interface
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
#include "ymenu.h"
#include "cwin.h"

/* Some systems, notably Solaris, don't have sys/signal.h */
#include <signal.h>

static ywin *head;		/* head of linked list */

extern long unsigned int ui_colors;
extern long unsigned int ui_attr;

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

	out = (ywin *) malloc(sizeof(ywin));
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
	t = ta = (char *) malloc(COLS * sizeof(char));
	user_title(w->user, t, COLS - 1);
	move(w->row - 1, w->col);
	attron(COLOR_PAIR(ui_colors) | ui_attr);
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
	attroff(COLOR_PAIR(ui_colors) | ui_attr);

	/* Redundant refresh() to work around a bug (?) in newer ncurses
	 * versions. Without it, the top line in each user window will lose
	 * its attributes on redraw.
	 */
	refresh();

	free(ta);
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

	if (can_ymenu())
		resize_ymenu();

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
		if (scrolling(w->user)) {
			w->swin = dupwin(w->win);
			if (!w->swin) {
				end_term();
				show_error("Couldn't create scrolling viewport");
				bail(YTE_ERROR);
			}
		}
		wnoutrefresh(stdscr);
		if (scrolling(w->user)) {
			__update_scroll_curses(w->user);
			wnoutrefresh(w->swin);
		} else
			wnoutrefresh(w->win);
	}

	if (in_ymenu())
		__refresh_ymenu();

	doupdate();
}

/*
 * Start curses and set all options.
 */
static void
curses_start()
{
	char *term;
	short i, fg, bg;

	/*
	 * This little snippet is currently on probation. Although the
	 * comment implies it would be absolutely necessary to keep it
	 * around, it seems to cause nothing but trouble.
	 */
	/* LINES = COLS = 0; *//* so resizes will work */
	if (initscr() == NULL) {
		term = getenv("TERM");
		fprintf(stderr, "Error opening terminal: %s.\n", (term ? term : "(null)"));
		bail(YTE_INIT);
	}
	noraw();
	crmode();
	noecho();

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
			if (scrolling(w->user)) {
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
	user->yac.attributes = 0;
	user->yac.background = 0;
	user->yac.foreground = 7;

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
	if (scrolling(user))
		delwin(w->swin);
	free(w);
	w = NULL;
	curses_redraw();
}

static void
waddyac(WINDOW *w, yachar c)
{
	int x, y;
	chtype cc = c.data;

	getyx(w, y, x);
	if (c.alternate_charset) {
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
	if (c.attributes & YATTR_BOLD)
		cc |= A_BOLD;
	if (c.attributes & YATTR_DIM)
		cc |= A_DIM;
	if (c.attributes & YATTR_UNDERLINE)
		cc |= A_UNDERLINE;
	if (c.attributes & YATTR_BLINK)
		cc |= A_BLINK;
	if (c.attributes & YATTR_REVERSE)
		cc |= A_REVERSE;
	waddch(w, cc | COLOR_PAIR(1 + (c.foreground | c.background << 3)));
	if (x >= COLS - 1)
		wmove(w, y, x);
}

void
addch_curses(yuser *user, yachar c)
{
	waddyac(((ywin *) (user->term))->win, c);
}

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

void
keypad_curses(int bf)
{
#ifdef HAVE_KEYPAD
	keypad(((ywin *) (me->term))->win, bf);
#endif
}

void
flush_curses(yuser *user)
{
	register ywin *w;

	w = (ywin *) (user->term);
	if (scrolling(w->user))
		wnoutrefresh(w->swin);
	else
		wnoutrefresh(w->win);
	if (in_ymenu())
		__refresh_ymenu();
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
refresh_curses()
{
	register ywin *w;
	wnoutrefresh(stdscr);
	for (w = head; w; w = w->next) {
		if (scrolling(w->user)) {
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
redisplay_curses()
{
	register ywin *w;

	clear();

	if (bottom_msg != NULL)
		update_message_curses();

	wnoutrefresh(stdscr);
	for (w = head; w; w = w->next) {
		if (scrolling(w->user)) {
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
	doupdate();
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

void
start_scroll_curses(yuser *user)
{
	ywin *w = (ywin *) (user->term);
	w->swin = dupwin(w->win);
	if (!w->swin) {
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
	if (w->swin) {
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
	if (!w->swin)
		return;
	werase(w->swin);
	for (r = 0; r <= (user->rows - 1); r++) {
		wmove(w->swin, r, 0);
		if ((user->scrollpos + r) < 0 ||
			(user->scrollpos + r) >= scrollback_lines ||
			(!user->scrollback[user->scrollpos + r]))
		{
			/* Borrow lines from active screen */
			if (fb == -1)
				fb = r;
			for (i = 0; i < user->cols; i++)
				waddyac(w->swin, user->scr[r - fb][i]);
		} else {
			for (i = 0; (i < user->cols) && (user->scrollback[user->scrollpos + r][i].data != '\0'); i++)
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
update_message_curses()
{
	move(LINES - 1, 0);
	if (bottom_msg != NULL)
		addstr(bottom_msg);
	clrtoeol();
	move(1, COLS - 1);
	refresh();
}
