/* vt100.c - terminal emulation */
#include "header.h"
#include "cwin.h"

#ifdef YTALK_COLOR
static unsigned long int attr_map[10] = {
	0, A_BOLD, A_DIM, 0, A_UNDERLINE, A_BLINK, 0, A_REVERSE, 0, 0
};
#endif

char YT_ACS_ON = '\xE';
char YT_ACS_OFF = '\xF';

void
vt100_process(yuser *user, char data)
{
	unsigned int i;

	if (data >= '0' && data <= '9') {
		if (user->vt.got_esc > 1) {
			user->vt.av[user->vt.ac] = (user->vt.av[user->vt.ac] * 10) + (int) (data - '0');
			return;
		}
		if (user->vt.hash == 1 && data == '8') {
			fill_term(user, 0, 0, user->rows - 1, user->cols - 1, 'E');
			redraw_term(user, 0);
			user->vt.got_esc = 0;
			return;
		}
	}
	switch (data) {
	case ';':		/* arg separator */
		if (user->vt.ac < MAXARG - 1)
			user->vt.av[++(user->vt.ac)] = 0;
		break;
	case '[':
		user->vt.got_esc = 2;
		break;
	case '?':
		if (user->vt.got_esc == 2)
			user->vt.got_esc = 3;
		else
			user->vt.got_esc = 0;
		break;
	case '#':
		user->vt.hash = 1;
		break;
#ifdef YTALK_COLOR
	case 'm':
		for (i = 0; i <= user->vt.ac; i++) {
			if (user->vt.av[i] == 0) {
				user->c_at = 0;
				user->c_bg = 0;
				user->c_fg = 7;
			} else if (user->vt.av[i] <= 9)
				user->c_at |= attr_map[user->vt.av[i]];
			else if (user->vt.av[i] >= 21 && user->vt.av[i] <= 29)
				user->c_at &= ~attr_map[user->vt.av[i] - 20];
			else if (user->vt.av[i] >= 30 && user->vt.av[i] <= 37)
				user->c_fg = user->vt.av[i] - 30;
			else if (user->vt.av[i] >= 40 && user->vt.av[i] <= 47)
				user->c_bg = user->vt.av[i] - 40;
		}
		user->vt.got_esc = 0;
		break;
#endif
	case 's':		/* save cursor */
		if (user->vt.got_esc == 2) {
			user->sy = user->y;
			user->sx = user->x;
		}
		user->vt.got_esc = 0;
		break;
	case 'u':		/* restore cursor */
		if (user->vt.got_esc == 2) {
			move_term(user, user->sy, user->sx);
		}
		user->vt.got_esc = 0;
		break;
	case 'c':
		write(user->fd, "\033[0c", 4);
		user->vt.got_esc = 0;
		break;
	case 'h':		/* set modes */
		switch (user->vt.av[0]) {
		case 1:	/* keypad "application" mode */
			keypad_term(user, 1);
			break;
		}
		user->vt.got_esc = 0;
		break;
	case 'l':		/* clear modes */
		switch (user->vt.av[0]) {
		case 1:	/* keypad "normal" mode */
			keypad_term(user, 0);
			break;
		}
		user->vt.got_esc = 0;
		break;
	case '=':
		keypad_term(user, 1);
		user->vt.got_esc = 0;
		break;
	case '>':
		keypad_term(user, 0);
		user->vt.got_esc = 0;
		break;
	case '@':
		if (user->vt.got_esc == 2) {	/* add char */
			if (user->vt.av[0] == 0)
				add_char_term(user, 1);
			else
				add_char_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'A':		/* move up */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] == 0)
				move_term(user, user->y - 1, user->x);
			else if ((int) user->vt.av[0] > user->y)
				move_term(user, 0, user->x);
			else
				move_term(user, user->y - user->vt.av[0], user->x);
		}
		user->vt.got_esc = 0;
		break;
	case 'B':		/* move down */
		if (user->vt.lparen == 1) {
			user->altchar = 0;
			user->vt.lparen = 0;
		} else {
			if (user->y != user->rows) {
				if (user->vt.av[0] == 0)
					move_term(user, user->y + 1, user->x);
				else
					move_term(user, user->y + user->vt.av[0], user->x);
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'C':		/* move right */
		if (user->vt.got_esc == 2) {
			if (user->x != user->cols) {
				if (user->vt.av[0] == 0)
					move_term(user, user->y, user->x + 1);
				else
					move_term(user, user->y, user->x + user->vt.av[0]);
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'D':		/* move left */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] == 0)
				move_term(user, user->y, user->x - 1);
			else if ((int) user->vt.av[0] > user->x)
				move_term(user, user->y, 0);
			else
				move_term(user, user->y, user->x - user->vt.av[0]);
		} else {
			if (user->y < user->sc_bot)
				user->y++;
			else
				scroll_term(user);
		}
		user->vt.got_esc = 0;
		break;
	case 'E':
		newline_term(user);
		user->vt.got_esc = 0;
		break;
	case 'f':		/* force cursor */
	case 'H':		/* move */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			if (user->vt.av[1] > 0)
				user->vt.av[1]--;
			move_term(user, user->vt.av[0], user->vt.av[1]);
		} else {
			if (data == 'H')	/* <esc>H is set tab */
				user->scr_tabs[user->x] = 1;
		}
		user->vt.got_esc = 0;
		break;
	case 'G':		/* move horizontally */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			move_term(user, user->y, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'g':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 3:		/* clear all tabs */
				for (i = 0; i < user->t_cols; i++)
					user->scr_tabs[i] = 0;
				break;
			case 0:		/* clear tab at x */
				user->scr_tabs[user->x] = 0;
				break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'J':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 0:		/* clear to end of screen */
				clreos_term(user);
				break;
			case 1:
				if (user->x > 0)
					fill_term(user, 0, 0, user->y - 1, user->cols - 1, ' ');
				fill_term(user, user->y, 0, user->y, user->x, ' ');
				redraw_term(user, 0);
				break;
			case 2:
				fill_term(user, 0, 0, user->rows - 1, user->cols - 1, ' ');
				redraw_term(user, 0);
				break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'K':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
				case 0:		/* clear to end of line */
					clreol_term(user);
					break;
				case 1:		/* clear to beginning of line */
					fill_term(user, user->y, 0, user->y, user->x, ' ');
					redraw_term(user, 0);
					break;
				case 2:		/* clear entire line */
					fill_term(user, user->y, 0, user->y, user->cols - 1, ' ');
					redraw_term(user, 0);
					break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'L':
		if (user->vt.got_esc == 2) {	/* add line */
			if (user->vt.av[0] == 0)
				add_line_term(user, 1);
			else
				add_line_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'M':
		if (user->vt.got_esc == 2) {	/* delete line */
			if (user->vt.av[0] == 0)
				del_line_term(user, 1);
			else
				del_line_term(user, user->vt.av[0]);
		} else		/* reverse scroll */
			if (user->y > user->sc_top)
				user->y--;
			else
				rev_scroll_term(user);
		user->vt.got_esc = 0;
		break;
	case 'P':
		if (user->vt.got_esc == 2) {	/* del char */
			if (user->vt.av[0] == 0)
				del_char_term(user, 1);
			else
				del_char_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'S':		/* forward scroll */
		scroll_term(user);
		user->vt.got_esc = 0;
		break;
	case 'r':		/* set scroll region */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			if (user->vt.av[1] > 0)
				user->vt.av[1]--;
			set_scroll_region(user, user->vt.av[0], user->vt.av[1]);
			move_term(user, 0, 0);
		}
		user->vt.got_esc = 0;
		break;
	case '(':
		user->vt.lparen = 1;
		break;
	case '0':
		if (user->vt.lparen == 1) {
			user->altchar = 1;
			user->vt.lparen = 0;
		}
		user->vt.got_esc = 0;
		break;
	case '7':		/* save cursor and attributes */
#ifdef YTALK_COLOR
		user->sc_at = user->c_at;
		user->sc_bg = user->c_bg;
		user->sc_fg = user->c_fg;
#endif
		user->sy = user->y;
		user->sx = user->x;
		user->vt.got_esc = 0;
		break;
	case '8':		/* restore cursor and attributes */
#ifdef YTALK_COLOR
		user->c_at = user->sc_at;
		user->c_fg = user->sc_fg;
		user->c_bg = user->sc_bg;
#endif
		move_term(user, user->sy, user->sx);
		user->vt.got_esc = 0;
		break;
	default:
		user->vt.got_esc = 0;
	}
}
