/* vt100.c - terminal emulation */
#include "header.h"

#ifdef YTALK_COLOR
#include <ncurses.h>
int attr_map[10] = {
	0, A_BOLD, A_DIM, 0, A_UNDERLINE, A_BLINK, 0, A_REVERSE, 0, 0
};
#endif

void
vt100_process(user, data)
	yuser *user;
	char data;
{
	unsigned int i;

	if (data >= '0' && data <= '9' && user->got_esc > 1) {
		user->av[user->ac] = (user->av[user->ac] * 10) + (data - '0');
		return;
	}
	switch (data) {
	case ';':		/* arg separator */
		if (user->ac < MAXARG - 1)
			user->av[++(user->ac)] = 0;
		break;
	case '[':
		user->got_esc = 2;
		break;
	case '?':
		if (user->got_esc == 2)
			user->got_esc = 3;
		else
			user->got_esc = 0;
		break;
#ifdef YTALK_COLOR
	case 'm':
		for (i = 0; i <= user->ac; i++) {
			if (user->av[i] == 0) {
				user->c_at = 0;
				user->c_bg = 0;
				user->c_fg = 7;
			} else if (user->av[i] <= 9)
				user->c_at |= attr_map[user->av[i]];
			else if (user->av[i] >= 21 && user->av[i] <= 29)
				user->c_at &= ~attr_map[user->av[i] - 20];
			else if (user->av[i] >= 30 && user->av[i] <= 37)
				user->c_fg = user->av[i] - 30;
			else if (user->av[i] >= 40 && user->av[i] <= 47)
				user->c_bg = user->av[i] - 40;
		}
		user->got_esc = 0;
		break;
#endif
	case '7':		/* save cursor and attributes */
		user->sc_at = user->c_at;
		user->sc_bg = user->c_bg;
		user->sc_fg = user->c_fg;
		user->sy = user->y;
		user->sx = user->x;
		user->got_esc = 0;
		break;
	case 's':		/* save cursor */
		if (user->got_esc == 2) {
			user->sy = user->y;
			user->sx = user->x;
		}
		user->got_esc = 0;
		break;
	case '8':		/* restore cursor and attributes */
		user->c_at = user->sc_at;
		user->c_fg = user->sc_fg;
		user->c_bg = user->sc_bg;
		move_term(user, user->sy, user->sx);
		user->got_esc = 0;
		break;
	case 'u':		/* restore cursor */
		if (user->got_esc == 2) {
			move_term(user, user->sy, user->sx);
		}
		user->got_esc = 0;
		break;
	case 'h':		/* set modes */
		switch (user->av[0]) {
		case 1:	/* keypad "application" mode */
			keypad_term(user, 1);
			break;
		}
		user->got_esc = 0;
		break;
	case 'l':		/* clear modes */
		switch (user->av[0]) {
		case 1:	/* keypad "normal" mode */
			keypad_term(user, 0);
			break;
		}
		user->got_esc = 0;
		break;
	case '=':
		keypad_term(user, 1);
		user->got_esc = 0;
		break;
	case '>':
		keypad_term(user, 0);
		user->got_esc = 0;
		break;
	case '@':
		if (user->got_esc == 2) {	/* add char */
			if (user->av[0] == 0)
				add_char_term(user, 1);
			else
				add_char_term(user, user->av[0]);
		}
		user->got_esc = 0;
		break;
	case '0':
		if (user->lparen) {
			user->lparen = 0;
		}
		user->got_esc = 0;
		break;
	case 'A':		/* move up */
		if (user->got_esc == 2) {
			if (user->av[0] == 0)
				move_term(user, user->y - 1, user->x);
			else if ((int) user->av[0] > user->y)
				move_term(user, 0, user->x);
			else
				move_term(user, user->y - user->av[0], user->x);
		}
		user->got_esc = 0;
		break;
	case 'B':		/* move down */
		if (user->lparen) {
			user->lparen = 0;
		} else {
			if (user->av[0] == 0)
				move_term(user, user->y + 1, user->x);
			else
				move_term(user, user->y + user->av[0], user->x);
		}
		user->got_esc = 0;
		break;
	case 'C':		/* move right */
		if (user->got_esc == 2) {
			if (user->av[0] == 0)
				move_term(user, user->y, user->x + 1);
			else
				move_term(user, user->y, user->x + user->av[0]);
		}
		user->got_esc = 0;
		break;
	case 'D':		/* move left */
		if (user->got_esc == 2) {
			if (user->av[0] == 0)
				move_term(user, user->y, user->x - 1);
			else if ((int) user->av[0] > user->x)
				move_term(user, user->y, 0);
			else
				move_term(user, user->y, user->x - user->av[0]);
		} else {
			scroll_term(user);
		}
		user->got_esc = 0;
		break;
	case 'H':		/* move */
		if (user->got_esc == 2) {
			if (user->av[0] > 0)
				user->av[0]--;
			if (user->av[1] > 0)
				user->av[1]--;
			move_term(user, user->av[0], user->av[1]);
		} else {
			user->scr_tabs[user->x] = 1;
		}
		user->got_esc = 0;
		break;
	case 'G':		/* move horizontally */
		if (user->got_esc == 2) {
			if (user->av[0] > 0)
				user->av[0]--;
			move_term(user, user->y, user->av[0]);
		}
		user->got_esc = 0;
		break;
	case 'g':
		if (user->got_esc == 2) {
			if (user->av[0] == 3) {	/* clear all tabs */
				for (i = 0; i < user->t_cols; i++)
					user->scr_tabs[i] = 0;
			} else {
				user->scr_tabs[user->x] = 0;	/* clear tab at x */
			}
		}
		user->got_esc = 0;
		break;
	case 'J':		/* clear to end of screen */
		if (user->got_esc == 2) {
			clreos_term(user);
		}
		user->got_esc = 0;
		break;
	case 'K':		/* clear to end of line */
		if (user->got_esc == 2) {
			clreol_term(user);
		}
		user->got_esc = 0;
		break;
	case 'L':
		if (user->got_esc == 2) {	/* add line */
			if (user->av[0] == 0)
				add_line_term(user, 1);
			else
				add_line_term(user, user->av[0]);
		}
		user->got_esc = 0;
		break;
	case 'M':
		if (user->got_esc == 2) {	/* delete line */
			if (user->av[0] == 0)
				del_line_term(user, 1);
			else
				del_line_term(user, user->av[0]);
		} else		/* reverse scroll */
			rev_scroll_term(user);
		user->got_esc = 0;
		break;
	case 'P':
		if (user->got_esc == 2) {	/* del char */
			if (user->av[0] == 0)
				del_char_term(user, 1);
			else
				del_char_term(user, user->av[0]);
		}
		user->got_esc = 0;
		break;
	case 'S':		/* forward scroll */
		scroll_term(user);
		user->got_esc = 0;
		break;
	case 'r':		/* set scroll region */
		if (user->got_esc == 2) {
			if (user->av[0] > 0)
				user->av[0]--;
			if (user->av[1] > 0)
				user->av[1]--;
			set_scroll_region(user, user->av[0], user->av[1]);
			move_term(user, 0, 0);
		}
		user->got_esc = 0;
		break;
	case '(':		/* skip over lparens for now */
		user->lparen = 1;
		break;
	default:
		user->got_esc = 0;
	}
}
