/* term.c */

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

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_SGTTY
#include <sgtty.h>
#ifdef hpux
#include <sys/bsdtty.h>
#endif
#define USE_SGTTY
#endif
#endif

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "cwin.h"
#include "xwin.h"
#include "menu.h"

static int (*_open_term) ();	/* open a new terminal */
static void (*_close_term) ();	/* close a terminal */
static void (*_addch_term) ();	/* write a char to a terminal */
static void (*_move_term) ();	/* move cursor to Y,X position */
static void (*_clreol_term) ();	/* clear to end of line */
static void (*_clreos_term) ();	/* clear to end of screen */
static void (*_scroll_term) ();	/* scroll up one line */
static void (*_rev_scroll_term) ();	/* scroll down one line */
static void (*_flush_term) ();	/* flush pending output */

static int term_type = 0;

extern int raw_color;
extern int raw_attr;

#ifdef USE_SGTTY
static int line_discipline;
static int local_mode;
static struct sgttyb sgttyb;
static struct tchars tchars;
static struct ltchars ltchars;
#else
static struct termios tio;
#endif

#ifdef USE_SGTTY
static void
init_sgtty()
{
	if (ioctl(0, TIOCGETD, &line_discipline) < 0) {
		show_error("TIOCGETD");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCLGET, &local_mode) < 0) {
		show_error("TIOCGETP");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGETP, &sgttyb) < 0) {
		show_error("TIOCGETP");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGETC, &tchars) < 0) {
		show_error("TIOCGETC");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGLTC, &ltchars) < 0) {
		show_error("TIOCGLTC");
		bail(YTE_INIT);
	}
	me->old_rub = sgttyb.sg_erase;
	me->RUB = RUBDEF;
	me->KILL = sgttyb.sg_kill;
	me->WORD = ltchars.t_werasc;
	me->CLR = '\024';	/* ^T */
}
#else
static void
init_termios()
{
	/* get edit chars */

	if (tcgetattr(0, &tio) < 0) {
		show_error("tcgetattr failed");
		bail(YTE_INIT);
	}
	me->old_rub = tio.c_cc[VERASE];
	me->RUB = RUBDEF;
#ifdef VKILL
	me->KILL = tio.c_cc[VKILL];
#else
	me->KILL = '\025';	/* ^U */
#endif
#ifdef VWERASE
	me->WORD = tio.c_cc[VWERASE];
	if (me->WORD == 0xff)
		me->WORD = '\027';	/* ^W */
#else
	me->WORD = '\027';	/* ^W */
#endif
	me->CLR = '\024';	/* ^T */
}
#endif

/*
 * Initialize terminal and input characteristics.
 */
void
init_term()
{
	char tmpstr[64];

#ifdef USE_SGTTY
	init_sgtty();
#else
	init_termios();
#endif

	/*
	 * Decide on a terminal (window) system and set up the function
	 * pointers.
	 */

	term_type = 0;		/* nothing selected yet */

#ifdef USE_X11
	if (term_type == 0 && (def_flags & FL_XWIN) && getenv("DISPLAY")) {
		_open_term = open_xwin;
		_close_term = close_xwin;
		_addch_term = addch_xwin;
		_move_term = move_xwin;
		_clreol_term = clreol_xwin;
		_clreos_term = clreos_xwin;
		_scroll_term = scroll_xwin;
		_rev_scroll_term = rev_scroll_xwin;
		_flush_term = flush_xwin;
		init_xwin();
		term_type = 2;	/* using xwin */
	}
#endif

	/* if no window system found, default to curses */

	if (term_type == 0) {
		_open_term = open_curses;
		_close_term = close_curses;
		_addch_term = addch_curses;
		_move_term = move_curses;
		_clreol_term = clreol_curses;
		_clreos_term = clreos_curses;
		_scroll_term = scroll_curses;
		_rev_scroll_term = NULL;
		_flush_term = flush_curses;
		init_curses();
		term_type = 1;	/* using curses */
	}
	/* set me up a terminal */

	if (def_flags & FL_NEWUI)
		(void) sprintf(tmpstr, "%s", me->full_name);
	else
		(void) sprintf(tmpstr, "YTalk version %d.%d.%d", VMAJOR, VMINOR, VPATCH);

	if (open_term(me, tmpstr) < 0) {
		end_term();
		show_error("init_term: open_term() failed");
		bail(0);
	}
}

/*
 * Set terminal size.
 */
void
set_terminal_size(fd, rows, cols)
	int fd, rows, cols;
{
#ifdef TIOCSWINSZ
	struct winsize winsize;

	winsize.ws_row = rows;
	winsize.ws_col = cols;
	(void) ioctl(fd, TIOCSWINSZ, &winsize);
#endif
}

/*
 * Set terminal and input characteristics for slave terminals.
 */
void
set_terminal_flags(fd)
	int fd;
{
#ifdef USE_SGTTY
	(void) ioctl(fd, TIOCSETD, &line_discipline);
	(void) ioctl(fd, TIOCLSET, &local_mode);
	(void) ioctl(fd, TIOCSETP, &sgttyb);
	(void) ioctl(fd, TIOCSETC, &tchars);
	(void) ioctl(fd, TIOCSLTC, &ltchars);
#else
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
		show_error("tcsetattr failed");
#endif
}

int
what_term()
{
	return term_type;
}

/*
 * Change terminal keypad mode (only for me, only with curses)
 */
void
keypad_term(user, bf)
	yuser *user;
	int bf;
{
	if (user != me)
		return;
	if (term_type == 1)
		keypad_curses(user, bf);
}

/*
 * Abort all terminal processing.
 */
void
end_term()
{
	switch (term_type) {
	case 1:		/* curses */
		end_curses();
		break;
#ifdef USE_X11
	case 2:		/* xwin */
		end_xwin();
		break;
#endif
	}
	term_type = 0;
}

/*
 * Open a new user window.
 */
int
open_term(user, title)
	register yuser *user;
	register char *title;
{
	if (_open_term(user, title) != 0)
		return -1;
	user->x = user->y = 0;
	if (user->scr == NULL)
		resize_win(user, 24, 80);
	return 0;
}

/*
 * Close a user window.
 */
void
close_term(user)
	register yuser *user;
{
	register int i;

	if (user->scr) {
		_close_term(user);
		for (i = 0; i < user->t_rows; i++)
			free_mem(user->scr[i]);
		free_mem(user->scr);
		user->scr = NULL;
		free_mem(user->scr_tabs);
		user->scr_tabs = NULL;
		user->t_rows = user->rows = 0;
		user->t_cols = user->cols = 0;
	}
}

#ifdef YTALK_COLOR
void
_addch_termc(user, c)
	register yuser *user;
	register ychar c;
{
	yachar ac;
	ac.l = c;
	ac.a = user->c_at;
	ac.b = user->c_fg;
	ac.c = user->c_bg;
	_addch_term(user, ac);
}
#endif

/*
 * Place a character.
 */
void
addch_term(user, c)
	register yuser *user;
	register ychar c;
{
#ifdef YTALK_COLOR
	yachar ac;
	ac.l = c;
	ac.a = user->c_at;
	ac.b = user->c_fg;
	ac.c = user->c_bg;
#endif
	if (is_printable(c)) {
#ifdef YTALK_COLOR
		user->scr[user->y][user->x] = ac;
		_addch_term(user, ac);
#else
		_addch_term(user, c);
		user->scr[user->y][user->x] = c;
#endif
		if (++(user->x) >= user->cols) {
			user->bump = 1;
			user->x = user->cols - 1;
			if (user->cols < user->t_cols)
				_move_term(user, user->y, user->x);
		}
	}
}

#ifdef YTALK_COLOR
static yachar
yac(c)
	char c;
{
	yachar ac;
	ac.l = c;
	ac.a = 0;
	ac.b = 7;
	ac.c = 0;
	return ac;
}

static yachar
uyac(user, c)
	yuser *user;
	char c;
{
	yachar ac;
	ac.l = c;
	ac.a = user->c_at;
	ac.b = user->c_fg;
	ac.c = user->c_bg;
	return ac;
}
#endif

/*
 * Move the cursor.
 */
void
move_term(user, y, x)
	register yuser *user;
	register int y, x;
{
	if (y < 0 || y > user->sc_bot)
		y = user->sc_bot;
	if (x < 0 || x >= user->cols) {
		user->bump = 1;
		x = user->cols - 1;
	} else
		user->bump = 0;
	_move_term(user, y, x);
	user->y = y;
	user->x = x;
}

/*
 * Clear to EOL.
 */
void
clreol_term(user)
	register yuser *user;
{
	register int j;
	register yachar *c;

	if (user->cols < user->t_cols) {
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++) {
#ifdef YTALK_COLOR
			_addch_term(user, *(c++) = uyac(user, ' '));
#else
			*(c++) = ' ';
			_addch_term(user, ' ');
#endif
		}
		move_term(user, user->y, user->x);
	} else {
		_clreol_term(user);
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++)
#ifdef YTALK_COLOR
			*(c++) = uyac(user, ' ');
#else
			*(c++) = ' ';
#endif
	}
}

/*
 * Clear to EOS.
 */
void
clreos_term(user)
	register yuser *user;
{
	register int j, i;
	register yachar *c;
	int x, y;

	if (user->cols < user->t_cols || user->rows < user->t_rows) {
		x = user->x;
		y = user->y;
		clreol_term(user);
		for (i = user->y + 1; i < user->rows; i++) {
			move_term(user, i, 0);
			clreol_term(user);
		}
		move_term(user, y, x);
	} else {
		_clreos_term(user);
		j = user->x;
		for (i = user->y; i < user->rows; i++) {
			c = user->scr[i] + j;
			for (; j < user->cols; j++)
#ifdef YTALK_COLOR
				*(c++) = yac(' ');
#else
				*(c++) = ' ';
#endif
			j = 0;
		}
	}
}

/*
 * Scroll window.
 */
void
scroll_term(user)
	register yuser *user;
{
	register int i;
	register yachar *c;
	int sy, sx;

	if (user->sc_bot > user->sc_top) {
		c = user->scr[user->sc_top];
		for (i = user->sc_top; i < user->sc_bot; i++)
			user->scr[i] = user->scr[i + 1];
		user->scr[user->sc_bot] = c;
		for (i = 0; i < user->cols; i++)
#ifdef YTALK_COLOR
			*(c++) = yac(' ');
#else
			*(c++) = ' ';
#endif
		if (_scroll_term
		    && user->rows == user->t_rows
		    && user->cols == user->t_cols
		    && user->sc_top == 0
		    && user->sc_bot == user->rows - 1)
			_scroll_term(user);
		else
			redraw_term(user, 0);
	} else {
		sy = user->y;
		sx = user->x;
		move_term(user, user->sc_top, 0);
		clreol_term(user);
		move_term(user, sy, sx);
	}
}

/*
 * Reverse-scroll window.
 */
void
rev_scroll_term(user)
	register yuser *user;
{
	register int i;
	register yachar *c;
	int sy, sx;

	if (user->sc_bot > user->sc_top) {
		c = user->scr[user->sc_bot];
		for (i = user->sc_bot; i > user->sc_top; i--)
			user->scr[i] = user->scr[i - 1];
		user->scr[user->sc_top] = c;
		for (i = 0; i < user->cols; i++)
#ifdef YTALK_COLOR
			*(c++) = yac(' ');
#else
			*(c++) = ' ';
#endif
		if (_rev_scroll_term
		    && user->rows == user->t_rows
		    && user->cols == user->t_cols
		    && user->sc_top == 0
		    && user->sc_bot == user->rows - 1)
			_rev_scroll_term(user);
		else
			redraw_term(user, 0);
	} else {
		sy = user->y;
		sx = user->x;
		move_term(user, user->sc_top, 0);
		clreol_term(user);
		move_term(user, sy, sx);
	}
}

/*
 * Flush window output.
 */
void
flush_term(user)
	register yuser *user;
{
	_flush_term(user);
}

/*
 * Rub one character.
 */
void
rub_term(user)
	register yuser *user;
{
	if (user->x > 0) {
		if (user->bump) {
			addch_term(user, ' ');
			user->bump = 0;
		} else {
			move_term(user, user->y, user->x - 1);
			addch_term(user, ' ');
			move_term(user, user->y, user->x - 1);
		}

	}
}

/*
 * Rub one word.
 */
int
word_term(user)
	register yuser *user;
{
	register int x, out;

#ifdef YTALK_COLOR
	for (x = user->x - 1; x >= 0 && user->scr[user->y][x].l == ' '; x--)
		continue;
	for (; x >= 0 && user->scr[user->y][x].l != ' '; x--)
		continue;
#else
	for (x = user->x - 1; x >= 0 && user->scr[user->y][x] == ' '; x--)
		continue;
	for (; x >= 0 && user->scr[user->y][x] != ' '; x--)
		continue;
#endif
	out = user->x - (++x);
	if (out <= 0)
		return 0;
	move_term(user, user->y, x);
	clreol_term(user);
	return out;
}

/*
 * Kill current line.
 */
void
kill_term(user)
	register yuser *user;
{
	if (user->x > 0) {
		move_term(user, user->y, 0);
		clreol_term(user);
	}
}

/*
 * Expand a tab.  We use non-destructive tabs.
 */
void
tab_term(user)
	register yuser *user;
{
	int i;
	/* Find nearest tab and jump to it. */
	if (user->x < user->t_cols) {
		for (i = (user->x + 1); i <= user->t_cols; i++) {
			if (user->scr_tabs[i] == 1) {
				move_term(user, user->y, i);
				break;
			}
		}
	}
}

/*
 * Process a carriage return.
 */
void
cr_term(user)
	register yuser *user;
{
	move_term(user, user->y, 0);
	return;
}

/*
 * Process a line feed.
 */
void
lf_term(user)
	register yuser *user;
{
	register int new_y, next_y;

	new_y = user->y + 1;
	if (user->flags & FL_RAW) {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL)
				scroll_term(user);
		}
		move_term(user, new_y, user->x);
	} else {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL) {
				scroll_term(user);
				move_term(user, user->y, user->x);
				return;
			}
			new_y = 0;
		}
		next_y = new_y + 1;
		if (next_y >= user->rows)
			next_y = 0;
		if (next_y > 0 || !(user->flags & FL_SCROLL)) {
			move_term(user, next_y, user->x);
		}
		move_term(user, new_y, user->x);
	}
}

/*
 * Process a newline.
 */
void
newline_term(user)
	register yuser *user;
{
	register int new_y, next_y;

	new_y = user->y + 1;
	if (user->flags & FL_RAW) {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL)
				scroll_term(user);
		}
		move_term(user, new_y, 0);
	} else {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL) {
				scroll_term(user);
				move_term(user, user->y, 0);
				return;
			}
			new_y = 0;
		}
		next_y = new_y + 1;
		if (next_y >= user->rows)
			next_y = 0;
		if (next_y > 0 || !(user->flags & FL_SCROLL)) {
			move_term(user, next_y, 0);
			clreol_term(user);
		}
		move_term(user, new_y, 0);
		clreol_term(user);
	}
}

/*
 * Insert lines.
 */
void
add_line_term(user, num)
	register yuser *user;
	int num;
{
	register yachar *c;
	register int i;

	if (num == 1 && user->y == 0)
		rev_scroll_term(user);
	else {
		/* find number of remaining lines */

		i = user->rows - user->y - num;
		if (i <= 0) {
			i = user->x;
			move_term(user, user->y, 0);
			clreos_term(user);
			move_term(user, user->y, i);
			return;
		}
		/* swap the remaining lines to bottom */

		for (i--; i >= 0; i--) {
			c = user->scr[user->y + i];
			user->scr[user->y + i] = user->scr[user->y + i + num];
			user->scr[user->y + i + num] = c;
		}

		/* clear the added lines */

		for (num--; num >= 0; num--) {
			c = user->scr[user->y + num];
			for (i = 0; i < user->cols; i++)
#ifdef YTALK_COLOR
				*(c++) = yac(' ');
#else
				*(c++) = ' ';
#endif
		}
		redraw_term(user, user->y);
	}
}

/*
 * Delete lines.
 */
void
del_line_term(user, num)
	register yuser *user;
	int num;
{
	register yachar *c;
	register int i;

	if (num == 1 && user->y == 0)
		scroll_term(user);
	else {
		/* find number of remaining lines */

		i = user->rows - user->y - num;
		if (i <= 0) {
			i = user->x;
			move_term(user, user->y, 0);
			clreos_term(user);
			move_term(user, user->y, i);
			return;
		}
		/* swap the remaining lines to top */

		for (; i > 0; i--) {
			c = user->scr[user->rows - i];
			user->scr[user->rows - i] = user->scr[user->rows - i - num];
			user->scr[user->rows - i - num] = c;
		}

		/* clear the remaining bottom lines */

		for (; num > 0; num--) {
			c = user->scr[user->rows - num];
			for (i = 0; i < user->cols; i++)
#ifdef YTALK_COLOR
				*(c++) = yac(' ');
#else
				*(c++) = ' ';
#endif
		}
		redraw_term(user, user->y);
	}
}

static void
copy_text(fr, to, count)
	register yachar *fr, *to;
	register int count;
{
	if (to < fr) {
		for (; count > 0; count--)
			*(to++) = *(fr++);
	} else {
		fr += count;
		to += count;
		for (; count > 0; count--)
			*(--to) = *(--fr);
	}
}

/*
 * Add chars.
 */
void
add_char_term(user, num)
	register yuser *user;
	int num;
{
	register yachar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - num - 1;
#ifdef YTALK_COLOR
	while (i > 0 && c->l == ' ')
#else
	while (i > 0 && *c == ' ')
#endif
		c--, i--;
	if (i <= 0) {
		clreol_term(user);
		return;
	}
	/* transfer the chars and clear the remaining */

	c++;
	copy_text(c - i, c - i + num, i);
	for (c -= i; num > 0; num--) {
#ifdef YTALK_COLOR
		_addch_term(user, *(c++) = yac(' '));
#else
		*(c++) = ' ';
		_addch_term(user, ' ');
#endif
	}
	for (; i > 0; i--)
		_addch_term(user, *(c++));
	_move_term(user, user->y, user->x);
}

/*
 * Delete chars.
 */
void
del_char_term(user, num)
	register yuser *user;
	int num;
{
	register yachar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - 1;
#ifdef YTALK_COLOR
	while (i > 0 && c->l == ' ')
#else
	while (i > 0 && *c == ' ')
#endif
		c--, i--;
	if (i <= 0) {
		clreol_term(user);
		return;
	}
	/* transfer the chars and clear the remaining */

	c++;
	copy_text(c - i, c - i - num, i);
	for (c -= (i + num); i > 0; i--)
		_addch_term(user, *(c++));
	for (; num > 0; num--) {
#ifdef YTALK_COLOR
		_addch_term(user, *(c++) = yac(' '));
#else
		*(c++) = ' ';
		_addch_term(user, ' ');
#endif
	}
	_move_term(user, user->y, user->x);
}

/*
 * Redraw a user's window.
 */
void
redraw_term(user, y)
	register yuser *user;
	register int y;
{
	register int x;
	register yachar *c;

	for (; y < user->t_rows; y++) {
		_move_term(user, y, 0);
		_clreol_term(user);
		c = user->scr[y];
		for (x = 0; x < user->t_cols; x++, c++) {
			_addch_term(user, *c);
		}
	}

	/* redisplay any active menu */

	if (menu_ptr != NULL)
		update_menu();
	else
		_move_term(user, user->y, user->x);
}

/*
 * Return the first interesting row for a user with a window of the given
 * height and width.
 */
static int
first_interesting_row(user, height, width)
	yuser *user;
	int height, width;
{
	register int j, i;
	register yachar *c;

	if (height < user->t_rows) {
		j = (user->y + 1) - height;
		if (j < 0)
			j += user->t_rows;
	} else {
		j = user->y + 1;
		if (j >= user->t_rows)
			j = 0;
	}
	while (j != user->y) {
		i = (width > user->t_cols) ? user->t_cols : width;
		for (c = user->scr[j]; i > 0; i--, c++)
#ifdef YTALK_COLOR
			if (c->l != ' ')
#else
			if (*c != ' ')
#endif
				break;
		if (i > 0)
			break;
		if (++j >= user->t_rows)
			j = 0;
	}
	return j;
}

/*
 * Called when a user's window has been resized.
 */
void
resize_win(user, height, width)
	yuser *user;
	int height, width;
{
	register int j, i;
	register yachar *c, **new_scr;
	int new_y, y_pos;

	if (height == user->t_rows && width == user->t_cols)
		return;

	/* resize the user terminal buffer */

	new_y = -1;
	y_pos = 0;
	new_scr = (yachar **) get_mem(height * sizeof(yachar *));
	if (user->scr == NULL) {
		user->t_rows = user->rows = 0;
		user->t_cols = user->cols = 0;
	} else if (user->region_set) {
		/* save as many top lines as possible */

		for (j = 0; j < height && j < user->t_rows; j++)
			new_scr[j] = user->scr[j];
		new_y = j - 1;
		y_pos = user->y;
		for (; j < user->t_rows; j++)
			free_mem(user->scr[j]);
		free_mem(user->scr);
	} else {
		/* shift all recent lines to top of screen */

		j = first_interesting_row(user, height, width);
		for (i = 0; i < height; i++) {
			new_scr[++new_y] = user->scr[j];
			if (j == user->y)
				break;
			if (++j >= user->t_rows)
				j = 0;
		}
		for (i++; i < user->t_rows; i++) {
			if (++j >= user->t_rows)
				j = 0;
			free_mem(user->scr[j]);
		}
		y_pos = new_y;
		free_mem(user->scr);
	}
	user->scr = new_scr;

	/* fill in the missing portions */

	if (width > user->t_cols) {
		for (i = 0; i <= new_y; i++) {
			user->scr[i] = (yachar *) realloc_mem(user->scr[i], width * sizeof(yachar));
			for (j = user->t_cols; j < width; j++)
#ifdef YTALK_COLOR
				user->scr[i][j] = yac(' ');
#else
				user->scr[i][j] = ' ';
#endif
		}

		user->scr_tabs = realloc_mem(user->scr_tabs, width * sizeof(int));
		for (j = user->t_cols; j < width; j++) {
			if (j % 8 == 0)
				user->scr_tabs[j] = 1;
			else
				user->scr_tabs[j] = 0;
		}
		/* rightmost column is always last tab on line */
		user->scr_tabs[width - 1] = 1;
	}
	for (i = new_y + 1; i < height; i++) {
		c = user->scr[i] = (yachar *) get_mem(width * sizeof(yachar));
		for (j = 0; j < width; j++)
#ifdef YTALK_COLOR
			*(c++) = yac(' ');
#else
			*(c++) = ' ';
#endif
	}

	/* reset window values */

	user->t_rows = user->rows = height;
	user->t_cols = user->cols = width;
	user->sc_top = 0;
	user->sc_bot = height - 1;
	move_term(user, y_pos, user->x);
	send_winch(user);
	redraw_term(user, 0);
	flush_term(user);

	/* Restore raw mode if running a shell */
	if (running_process)
		set_raw_term();
}

/*
 * Draw a nice box.
 */
static void
draw_box(user, height, width, c)
	yuser *user;
	int height, width;
	char c;
{
	register int i;

	if (width < user->t_cols) {
		for (i = 0; i < height; i++) {
			move_term(user, i, width);
			addch_term(user, c);
			if (width + 1 < user->t_cols)
				clreol_term(user);
		}
	}
	if (height < user->t_rows) {
		move_term(user, height, 0);
		for (i = 0; i < width; i++)
			addch_term(user, c);
		if (width < user->t_cols)
			addch_term(user, c);
		if (width + 1 < user->t_cols)
			clreol_term(user);
		if (height + 1 < user->t_rows) {
			move_term(user, height + 1, 0);
			clreos_term(user);
		}
	}
}

/*
 * Set the virtual terminal size, ie: the display region.
 */
void
set_win_region(user, height, width)
	yuser *user;
	int height, width;
{
	register int x, y;
	int old_height, old_width;

	if (height < 2 || height > user->t_rows)
		height = user->t_rows;
	if (width < 2 || width > user->t_cols)
		width = user->t_cols;

	/*
	 * Don't check if they're already equal; always perform processing.
	 * Just because it's equal over here doesn't mean it's equal for all
	 * ytalk connections.  We still need to clear the screen.
	 */

	old_height = user->rows;
	old_width = user->cols;
	user->rows = user->t_rows;
	user->cols = user->t_cols;
	if (user->region_set) {
		x = user->x;
		y = user->y;
		if (width > old_width || height > old_height)
			draw_box(user, old_height, old_width, ' ');
	} else {
		x = y = 0;
		move_term(user, 0, 0);
		clreos_term(user);
		user->region_set = 1;
	}
	draw_box(user, height, width, '%');

	/* set the display region */

	user->rows = height;
	user->cols = width;
	user->sc_top = 0;
	user->sc_bot = height - 1;
	move_term(user, y, x);
	flush_term(user);

	if (user == me)
		send_region();
}

/*
 * Set the virtual terminal size, ie: the display region.
 */
void
end_win_region(user)
	yuser *user;
{
	int old_height, old_width;

	old_height = user->rows;
	old_width = user->cols;
	user->rows = user->t_rows;
	user->cols = user->t_cols;
	user->sc_top = 0;
	user->sc_bot = user->rows - 1;
	if (old_height < user->t_rows || old_width < user->t_cols)
		draw_box(user, old_height, old_width, ' ');
	user->region_set = 0;
	if (user == me)
		send_end_region();
}

/*
 * Set the scrolling region.
 */
void
set_scroll_region(user, top, bottom)
	yuser *user;
	int top, bottom;
{
	if (top < 0 || top >= user->rows || bottom >= user->rows || bottom < top
	    || (bottom <= 0 && top <= 0)) {
		user->sc_top = 0;
		user->sc_bot = user->rows - 1;
	} else {
		user->sc_top = top;
		user->sc_bot = bottom;
	}
}

/*
 * Send a message to the terminal.
 */
void
msg_term(user, str)
	yuser *user;
	char *str;
{
	int y;

	if ((y = user->y + 1) >= user->rows)
		y = 0;
	_move_term(user, y, 0);
#ifdef YTALK_COLOR
	_addch_termc(user, '[');
	while (*str)
		_addch_termc(user, *(str++));
	_addch_termc(user, ']');
#else
	_addch_term(user, '[');
	while (*str)
		_addch_term(user, *(str++));
	_addch_term(user, ']');
#endif
	_clreol_term(user);
	_move_term(user, user->y, user->x);
	_flush_term(user);
}


#ifdef YTALK_COLOR

/* Toggle attributes */
#define DO_ATTR(attr, val) \
    if (buf->a & attr) { \
		acur |= attr; \
		if (!(alast & attr)) { \
		    p += sprintf(esc + p, "%d;", val); \
		} \
    } else { \
		if (alast & attr) { \
		    p += sprintf(esc + p, "%d;", val+20); \
		} \
    }

/*
 * Spew terminal line contents to a file descriptor.
 */
void
spew_line(fd, buf, len)
	int fd, len;
	yachar *buf;
{
	int a, b, c, p;
	int alast, acur;
	char esc[30];
	if (len <= 0)
		return;
	a = b = c = -1;
	alast = acur = 0;
	for (; len; buf++, len--) {
		p = 2;
		if (a != buf->a) {
			acur = 0;
			if (!buf->a) {
				strcpy(esc + p, "0;");
				p += 2;
			} else {
				DO_ATTR(A_BOLD, 1);
				DO_ATTR(A_DIM, 2);
				DO_ATTR(A_UNDERLINE, 4);
				DO_ATTR(A_BLINK, 5);
				DO_ATTR(A_REVERSE, 7);
			}
			alast = acur;
		}
		if (b != buf->b)
			p += sprintf(esc + p, "%d;", 30 + buf->b);
		if (c != buf->c)
			p += sprintf(esc + p, "%d;", 40 + buf->c);

		if (p != 2) {
			a = buf->a;
			b = buf->b;
			c = buf->c;
			esc[0] = 27;
			esc[1] = '[';
			esc[p - 1] = 'm';
			write(fd, esc, p);
		}
		write(fd, &buf->l, 1);
	}
}

#endif

/*
 * Spew terminal contents to a file descriptor.
 */
void
spew_term(user, fd, rows, cols)
	yuser *user;
	int fd, rows, cols;
{
	register yachar *c, *e;
	register int len;
	int y;
	static char tmp[20];

	if (user->region_set) {
		y = 0;
		if (cols > user->cols)
			cols = user->cols;
		if (rows > user->rows)
			rows = user->rows;
		for (;;) {
			for (c = e = user->scr[y], len = cols; len > 0; len--, c++)
#ifdef YTALK_COLOR
				if (c->l != ' ')
#else
				if (*c != ' ')
#endif
					e = c + 1;
#ifdef YTALK_COLOR
			if (e != user->scr[y])
				spew_line(fd, user->scr[y], e - user->scr[y]);
#else
			if (e != user->scr[y])
				(void) write(fd, user->scr[y], e - user->scr[y]);
#endif
			if (++y >= rows)
				break;
			if (user->crlf)
				write(fd, "\r\n", 2);
			else
				write(fd, "\n", 1);
		}

		/* move the cursor to the correct place */

		(void) sprintf(tmp, "%c[%d;%dH", 27, user->y + 1, user->x + 1);
		(void) write(fd, tmp, strlen(tmp));
	} else {
		y = first_interesting_row(user, rows, cols);
		for (;;) {
			if (y == user->y) {
#ifdef YTALK_COLOR
				if (user->x > 0)
					spew_line(fd, user->scr[y], user->x);
#else
				if (user->x > 0)
					(void) write(fd, user->scr[y], user->x);
#endif
				break;
			}
			for (c = e = user->scr[y], len = user->t_cols; len > 0; len--, c++)
#ifdef YTALK_COLOR
				if (c->l != ' ')
#else
				if (*c != ' ')
#endif
					e = c + 1;
			if (e != user->scr[y])
#ifdef YTALK_COLOR
				spew_line(fd, user->scr[y], e - user->scr[y]);
#else
				(void) write(fd, user->scr[y], e - user->scr[y]);
#endif
			if (user->crlf)
				write(fd, "\r\n", 2);
			else
				write(fd, "\n", 1);
			if (++y >= user->t_rows)
				y = 0;
		}
	}
}

void
special_menu_term(user, y, x, section, len)
	yuser *user;
	int y, x;
	int section;
	int len;
{
	if (term_type == 1)
		special_menu_curses(user, y, x, section, len);
}

/*
 * Draw some raw characters to the screen without updating any buffers.
 * Whoever uses this should know what they're doing.  It should always be
 * followed by a redraw_term() before calling any of the normal term
 * functions again.
 *
 * If the given string is not as long as the given length, then the string is
 * repeated to fill the given length.
 *
 * This is an unadvertised function.
 */
void
raw_term(user, y, x, str, len)
	yuser *user;
	int y, x;
	ychar *str;
	int len;
{
	register ychar *c;
#ifdef YTALK_COLOR
	yachar ac;
	ac.l = 0;
	ac.a = raw_attr;
	ac.b = raw_color;
	ac.c = 0;
#endif

	if (y < 0 || y >= user->t_rows)
		return;
	if (x < 0 || x >= user->t_cols)
		return;
	_move_term(user, y, x);

	for (c = str; len > 0; len--, c++) {
		if (*c == '\0')
			c = str;
		if (!is_printable(*c))
			return;
#ifdef YTALK_COLOR
		ac.l = *c;
		_addch_term(user, ac);
#else
		_addch_term(user, *c);
#endif
	}
}

int
center(width, n)
	int width, n;
{
	if (n >= width)
		return 0;
	return (width - n) >> 1;
}

void
redraw_all_terms()
{
	register yuser *u;

	switch (term_type) {
	case 1:		/* curses */
		redisplay_curses();
		break;
	default:
		redraw_term(me, 0);
		flush_term(me);
		for (u = connect_list; u; u = u->next) {
			redraw_term(u, 0);
			flush_term(u);
		}
	}
}

void
set_raw_term()
{
	/* only some terminal systems need to do this */

	switch (term_type) {
	case 1:		/* curses */
		set_raw_curses();
		break;
	}
}

void
set_cooked_term()
{
	/* only some terminal systems need to do this */

	switch (term_type) {
	case 1:		/* curses */
		set_cooked_curses();
		break;
	}
}

int
term_does_asides()
{
	/* only some terminal systems can do this */

	switch (term_type) {
	case 2:		/* X11 */
		return 1;
	}
	return 0;
}
