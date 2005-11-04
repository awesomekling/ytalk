/*
 * src/term.c
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
#include "mem.h"
#include "ymenu.h"
#include "cwin.h"
#include <termcap.h>

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif

#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#else
#  ifdef HAVE_SGTTY
#    include <sgtty.h>
#    ifdef hpux
#      include <sys/bsdtty.h>
#    endif
#    define USE_SGTTY
#  endif
#endif

static int term_type = 0;
static yachar emptyc;

char *bottom_msg = NULL;
ylong bottom_time = 0;

static char *tcbp = NULL;
static char *tcap = NULL, *tcapp;

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
init_sgtty(void)
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
}
#else
static void
init_termios(void)
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
}
#endif

#ifdef YTALK_COLOR
static void
user_yac(yuser *user, char data, yachar *ac)
{
	ac->attributes = user->yac.attributes;
	ac->background = user->yac.background;
	ac->foreground = user->yac.foreground;
	ac->data = data;
	ac->alternate_charset = user->altchar ^ user->csx;
}
#endif

static void
init_termcap(void)
{
	char *term, *tcks;
	int success;
	tcbp = get_mem(4096);
	tcapp = tcap = get_mem(4096);
	if ((term = getenv("TERM")) == NULL) {
		fprintf(stderr, "TERM not set.\n");
		exit(YTE_INIT);
	}
	success = tgetent(tcbp, term);
	if (success < 0) {
		fprintf(stderr, "Couldn't access termcap database.\n");
		exit(YTE_INIT);
	} else if (success == 0) {
		fprintf(stderr, "Unknown terminal type: %s\n", term);
		exit(YTE_INIT);
	}
	/* activate keypad capabilities */
	tcks = get_tcstr("ks");
	if (tcks != NULL)
		printf("%s", tcks);
}

static void
end_termcap(void)
{
	char *tcke;
	tcke = get_tcstr("ke");
	if (tcke != NULL)
		printf("%s", tcke);
	if (tcap != NULL)
		free_mem(tcap);
	if (tcbp != NULL)
		free_mem(tcbp);
}

char *
get_tcstr(char *id)
{
	return tgetstr(id, &tcapp);
}

/*
 * Initialize terminal and input characteristics.
 */
void
init_term(void)
{

#ifdef YTALK_COLOR
	emptyc.data = ' ';
	emptyc.attributes = 0;
	emptyc.background = 0;
	emptyc.foreground = 7;
#else
	emptyc = ' ';
#endif /* YTALK_COLOR */

#ifdef USE_SGTTY
	init_sgtty();
#else
	init_termios();
#endif

	/* we want to look for some magic strings in the tc library */
	init_termcap();

	/* it's all about curses from here on */
	init_curses();
	term_type = 1;

	/* set me up a terminal */

	if (open_term(me) < 0) {
		end_term();
		show_error("init_term: open_term() failed");
		bail(YTE_INIT);
	}
}

/*
 * Set terminal size.
 */
void
set_terminal_size(int fd, int rows, int cols)
{
#ifdef TIOCSWINSZ
	struct winsize winsize;

	winsize.ws_row = rows;
	winsize.ws_col = cols;
	ioctl(fd, TIOCSWINSZ, &winsize);
#endif
}

/*
 * Set terminal and input characteristics for slave terminals.
 */
void
set_terminal_flags(int fd)
{
#ifdef USE_SGTTY
	ioctl(fd, TIOCSETD, &line_discipline);
	ioctl(fd, TIOCLSET, &local_mode);
	ioctl(fd, TIOCSETP, &sgttyb);
	ioctl(fd, TIOCSETC, &tchars);
	ioctl(fd, TIOCSLTC, &ltchars);
#else
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
		show_error("tcsetattr failed");
#endif
}

int
what_term(void)
{
	return term_type;
}

/*
 * Change terminal keypad mode (only for me, only with curses)
 */
void
keypad_term(yuser *user, int bf)
{
	if (user != me)
		return;
	keypad_curses(bf);
}

/*
 * Abort all terminal processing.
 */
void
end_term(void)
{
	if (term_type == 1) {
		end_curses();
		end_termcap();
	}
	term_type = 0;
}

/*
 * Open a new user window.
 */
int
open_term(yuser *user)
{
	if (open_curses(user) != 0)
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
close_term(yuser *user)
{
	register int i;

	if (user->scr) {
		close_curses(user);
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

/*
 * Place a character.
 */
void
addch_term(yuser *user, ychar c)
{
	yachar ac;
#ifdef YTALK_COLOR
	user_yac(user, c, &ac);
#else
	ac = c;
#endif
	if (is_printable(c)) {
		addch_curses(user, ac);
		user->scr[user->y][user->x] = ac;
		if (++(user->x) >= user->cols) {
			user->bump = 1;
			user->x = user->cols - 1;
			if (user->cols < user->t_cols)
				move_curses(user, user->y, user->x);
		}
	}
}

/*
 * Move the cursor.
 */
void
move_term(yuser *user, int y, int x)
{
	if (y < 0 || y > user->sc_bot)
		y = user->sc_bot;
	if (x < 0 || x >= user->cols) {
		user->bump = 1;
		x = user->cols - 1;
	} else {
		user->bump = 0;
		user->onend = 0;
	}
	move_curses(user, y, x);
	user->y = y;
	user->x = x;
}

/*
 * Fill terminal region with char
 */
void
fill_term(yuser *user, int y1, int x1, int y2, int x2, ychar c)
{
	int y, x;
	yachar ac;
#ifdef YTALK_COLOR
	user_yac(user, c, &ac);
#else
	ac = c;
#endif
	for (y = y1; y <= y2; y++)
		for (x = x1; x <= x2; x++)
			user->scr[y][x] = ac;
	return;
}

/*
 * Clear to EOL.
 */
void
clreol_term(yuser *user)
{
	register int j;
	register yachar *c;
	yachar nc;

#ifdef YTALK_COLOR
	user_yac(user, ' ', &nc);
#else
	nc = ' ';
#endif

	if (user->cols < user->t_cols) {
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++)
			addch_curses(user, *(c++) = nc);
		move_term(user, user->y, user->x);
	} else {
		clreol_curses(user);
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++)
			*(c++) = nc;
	}
}

/*
 * Clear to EOS.
 */
void
clreos_term(yuser *user)
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
		clreos_curses(user);
		j = user->x;
		for (i = user->y; i < user->rows; i++) {
			c = user->scr[i] + j;
			for (; j < user->cols; j++)
				*(c++) = emptyc;
			j = 0;
		}
	}
}

/*
 * Scroll window.
 */
void
scroll_term(yuser *user)
{
	register int i;
	register yachar *c;
	long int y;
	int sy, sx;

	if (user->sc_bot > user->sc_top) {
		c = user->scr[user->sc_top];

		if (user->sc_top == 0 && scrollback_lines > 0) {
			if (user->scrollback[scrollback_lines - 1] == NULL) {
				for (y = 0; ((y < scrollback_lines) && (user->scrollback[y] != NULL)) ; y++);
				if (y < (scrollback_lines - 1))
					user->scrollback[y + 1] = NULL;
			} else {
				free_mem(user->scrollback[0]);
				for (y = 0; y < scrollback_lines - 1; y++) {
					user->scrollback[y] = user->scrollback[y + 1];
				}
				y = scrollback_lines - 1;
			}
			user->scrollback[y] = get_mem((user->cols + 1) * sizeof(yachar));
			memcpy(user->scrollback[y], user->scr[0], (user->cols * sizeof(yachar)));
#ifdef YTALK_COLOR
			user->scrollback[y][user->cols].data = '\0';
#else
			user->scrollback[y][user->cols] = '\0';
#endif
			if (!scrolling(user))
				user->scrollpos = y;
			else
				if (user->scrollback[scrollback_lines - 1]) {
					update_scroll_term(user);
					retitle_all_terms();
				}
		}
		for (i = user->sc_top; i < user->sc_bot; i++)
			user->scr[i] = user->scr[i + 1];
		user->scr[user->sc_bot] = c;
		for (i = 0; i < user->cols; i++)
			*(c++) = emptyc;
		if (user->rows == user->t_rows
		    && user->cols == user->t_cols
		    && user->sc_top == 0
		    && user->sc_bot == user->rows - 1)
			scroll_curses(user);
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
rev_scroll_term(yuser *user)
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
			*(c++) = emptyc;
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
 * Rub one character.
 */
void
rub_term(yuser *user)
{
	if (user->x > 0) {
		if (user->x == user->cols - 1)
			user->onend = 0;
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
void
word_term(yuser *user)
{
	register int x;

#ifdef YTALK_COLOR
	for (x = user->x - 1; x >= 0 && user->scr[user->y][x].data == ' '; x--)
		continue;
	for (; x >= 0 && user->scr[user->y][x].data != ' '; x--)
		continue;
#else
	for (x = user->x - 1; x >= 0 && user->scr[user->y][x] == ' '; x--)
		continue;
	for (; x >= 0 && user->scr[user->y][x] != ' '; x--)
		continue;
#endif
	if ((user->x - (++x)) <= 0)
		return;
	move_term(user, user->y, x);
	clreol_term(user);
	return;
}

/*
 * Kill current line.
 */
void
kill_term(yuser *user)
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
tab_term(yuser *user)
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
 * Process a line feed.
 */
void
lf_term(yuser *user)
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
			clreol_term(user);
		}
		move_term(user, new_y, user->x);
	}
}

/*
 * Process a newline.
 */
void
newline_term(yuser *user)
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
add_line_term(yuser *user, int num)
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
				*(c++) = emptyc;
		}
		redraw_term(user, user->y);
	}
}

/*
 * Delete lines.
 */
void
del_line_term(yuser *user, int num)
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
				*(c++) = emptyc;
		}
		redraw_term(user, user->y);
	}
}

static void
copy_text(yachar *fr, yachar *to, int count)
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
add_char_term(yuser *user, int num)
{
	register yachar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - num - 1;
#ifdef YTALK_COLOR
	while (i > 0 && c->data == ' ')
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
	for (c -= i; num > 0; num--)
		addch_curses(user, *(c++) = emptyc);
	for (; i > 0; i--)
		addch_curses(user, *(c++));
	move_curses(user, user->y, user->x);
}

/*
 * Delete chars.
 */
void
del_char_term(yuser *user, int num)
{
	register yachar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - 1;
#ifdef YTALK_COLOR
	while (i > 0 && c->data == ' ')
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
		addch_curses(user, *(c++));
	for (; num > 0; num--)
		addch_curses(user, *(c++) = emptyc);
	move_curses(user, user->y, user->x);
}

/*
 * Redraw a user's window.
 */
void
redraw_term(yuser *user, int y)
{
	register int x;
	register yachar *c;

	for (; y < user->t_rows; y++) {
		move_curses(user, y, 0);
		clreol_curses(user);
		c = user->scr[y];
		for (x = 0; x < user->t_cols; x++, c++) {
			addch_curses(user, *c);
		}
	}

	/* redisplay any active menu */

	if (in_ymenu())
		update_ymenu();
	move_curses(user, user->y, user->x);
}

/*
 * Return the first interesting row for a user with a window of the given
 * height and width.
 */
static int
first_interesting_row(yuser *user, int height, int width)
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
			if (c->data != ' ')
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
resize_win(yuser *user, int height, int width)
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
				user->scr[i][j] = emptyc;
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
			*(c++) = emptyc;
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
draw_box(yuser *user, int height, int width, char c)
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
set_win_region(yuser *user, int height, int width)
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
end_win_region(yuser *user)
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
set_scroll_region(yuser *user, int top, int bottom)
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
msg_term(char *str)
{
	bottom_msg = str;
	bottom_time = time(NULL);
	update_message_curses();
	return;
}


#ifndef YTALK_COLOR
void
spew_line(int fd, yachar *buf, int len)
{
	write(fd, (char *)buf, len);
}
#else
static void
spew_attrs(int fd, yachar *yac)
{
	char esc[30] = "\033[0m\033[";
	int p = 6;
	if (yac->attributes & YATTR_BOLD) {
		esc[p++] = '1';
		esc[p++] = ';';
	}
	if (yac->attributes & YATTR_DIM) {
		esc[p++] = '2';
		esc[p++] = ';';
	}
	if (yac->attributes & YATTR_UNDERLINE) {
		esc[p++] = '4';
		esc[p++] = ';';
	}
	if (yac->attributes & YATTR_BLINK) {
		esc[p++] = '5';
		esc[p++] = ';';
	}
	if (yac->attributes & YATTR_REVERSE) {
		esc[p++] = '7';
		esc[p++] = ';';
	}
	esc[p++] = '3';
	esc[p++] = '0' + yac->foreground;
	esc[p++] = ';';
	esc[p++] = '4';
	esc[p++] = '0' + yac->background;
	esc[p++] = 'm';
	/* Send our attributes */
	write(fd, esc, p);
}

#define DO_ATTR(attr, num) \
	if (buf->attributes & attr) { \
		if (!last.attributes & attr) { \
			esc[p++] = num; \
			esc[p++] = ';'; \
		} \
	} else { \
		if (last.attributes & attr) { \
			esc[p++] = '2'; \
			esc[p++] = num; \
			esc[p++] = ';'; \
		} \
	}

/*
 * Spew terminal line contents to a file descriptor.
 */
void
spew_line(int fd, yachar *buf, int len)
{
	yachar last;
	int v, p;
	char esc[30];
	if (len <= 0)
		return;
	v = 0;
	memcpy(&last, buf, sizeof(yachar));
	spew_attrs(fd, buf);
	for (; len; buf++, len--) {
		p = 2;

		if (last.attributes != buf->attributes) {
			if (!buf->attributes) {
				esc[p++] = '0';
				esc[p++] = ';';
			} else {
				DO_ATTR(YATTR_BOLD,      '1')
				DO_ATTR(YATTR_DIM,       '2')
				DO_ATTR(YATTR_UNDERLINE, '4')
				DO_ATTR(YATTR_BLINK,     '5')
				DO_ATTR(YATTR_REVERSE,   '7')
			}
		}
		if (last.foreground != buf->foreground) {
			esc[p++] = '3';
			esc[p++] = '0' + buf->foreground;
			esc[p++] = ';';
		}
		if (last.background != buf->background) {
			esc[p++] = '4';
			esc[p++] = '0' + buf->background;
			esc[p++] = ';';
		}
		if (p != 2) {
			esc[0] = 27;
			esc[1] = '[';
			esc[p - 1] = 'm';
			esc[p] = 0;
			write(fd, esc, p);
		}

		if (v != buf->alternate_charset) {
			/* Beware of the backwards logic ;-) */
			write(fd, v ? "\xF" : "\xE", 1);
			v = buf->alternate_charset;
		}

		write(fd, &buf->data, 1);

		memcpy(&last, buf, sizeof(yachar));
	}
}

#endif /* YTALK_COLOR */

/*
 * Spew terminal contents to a file descriptor.
 */
void
spew_term(yuser *user, int fd, int rows, int cols)
{
	register yachar *c, *e;
	register int len;
	int y;
	char buffer[20];

	if (user->region_set) {
		y = 0;
		if (cols > user->cols)
			cols = user->cols;
		if (rows > user->rows)
			rows = user->rows;
		for (;;) {
			for (c = e = user->scr[y], len = cols; len > 0; len--, c++)
#ifdef YTALK_COLOR
				if (c->data != ' ')
#else
				if (*c != ' ')
#endif
					e = c + 1;
#ifdef YTALK_COLOR
			if (e != user->scr[y])
				spew_line(fd, user->scr[y], e - user->scr[y]);
#else
			if (e != user->scr[y])
				write(fd, user->scr[y], e - user->scr[y]);
#endif
			if (++y >= rows)
				break;
			if (user->crlf)
				write(fd, "\r\n", 2);
			else
				write(fd, "\n", 1);
		}

		/* move the cursor to the correct place */

		len = snprintf(buffer, sizeof(buffer), "\033[%d;%dH", user->y + 1, user->x + 1);
		write(fd, buffer, len);

#ifdef YTALK_COLOR
		spew_attrs(fd, &user->yac);
		if (user->altchar)
			write(fd, "\033[(0", 4);
		if (user->csx)
			write(fd, &YT_ACS_ON, 1);
#endif

	} else {
		y = first_interesting_row(user, rows, cols);
		for (;;) {
			if (y == user->y) {
#ifdef YTALK_COLOR
				if (user->x > 0)
					spew_line(fd, user->scr[y], user->x);
#else
				if (user->x > 0)
					write(fd, user->scr[y], user->x);
#endif
				break;
			}
			for (c = e = user->scr[y], len = user->t_cols; len > 0; len--, c++)
#ifdef YTALK_COLOR
				if (c->data != ' ')
#else
				if (*c != ' ')
#endif
					e = c + 1;
			if (e != user->scr[y])
#ifdef YTALK_COLOR
				spew_line(fd, user->scr[y], e - user->scr[y]);
#else
				write(fd, user->scr[y], e - user->scr[y]);
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
