/* vt100.c - terminal emulation */
#include "header.h"

void
vt100_process(user, data)
  yuser *user;
  char data;
{
    int i;

    if(data >= '0' && data <= '9' && user->got_esc > 1)
    {
	user->av[user->ac] = (user->av[user->ac] * 10) + (data - '0');
	return;
    }
    switch(data)
    {
	case ';':	/* arg separator */
	    if(user->ac < MAXARG-1)
		user->av[++(user->ac)] = 0;
	    break;
	case '[':
	    user->got_esc = 2;
	    break;
	case '?':
	    if(user->got_esc == 2)
		user->got_esc = 3;
	    else
		user->got_esc = 0;
	    break;
	case '7':	/* save cursor and attributes */
	case 's':	/* save cursor */
	    user->sy = user->y;
	    user->sx = user->x;
	    user->got_esc = 0;
	    break;
	case '8':	/* restore cursor and attributes */
	case 'u':	/* restore cursor */
	    move_term(user, user->sy, user->sx);
	    user->got_esc = 0;
	    break;
	case 'h':	/* set modes */
	    switch(user->av[0]) {
		case 1:			/* keypad "application" mode */
		    keypad_term(user, 1);
	    break;
	    }
	    user->got_esc = 0;
	    break;
	case 'l':	/* clear modes */
	    switch(user->av[0]) {
		case 1:			/* keypad "normal" mode */
	    keypad_term(user, 0);
		    break;
	    }
	    user->got_esc = 0;
	    break;
	case '@':
	    if(user->got_esc == 2)	/* add char */
	    {
		if(user->av[0] == 0)
		    add_char_term(user, 1);
		else
		    add_char_term(user, user->av[0]);
	    }
	    user->got_esc = 0;
	    break;
	case 'A':	/* move up */
	    if(user->av[0] == 0)
		move_term(user, user->y - 1, user->x);
	    else if((int)user->av[0] > user->y)
		move_term(user, 0, user->x);
	    else
		move_term(user, user->y - user->av[0], user->x);
	    user->got_esc = 0;
	    break;
	case 'B':	/* move down */
	    if(user->av[0] == 0)
		move_term(user, user->y + 1, user->x);
	    else
		move_term(user, user->y + user->av[0], user->x);
	    user->got_esc = 0;
	    break;
	case 'C':	/* move right */
	    if(user->av[0] == 0)
		move_term(user, user->y, user->x + 1);
	    else
		move_term(user, user->y, user->x + user->av[0]);
	    user->got_esc = 0;
	    break;
	case 'D':	/* move left */
	    if(user->av[0] == 0)
		move_term(user, user->y, user->x - 1);
	    else if((int)user->av[0] > user->x)
		move_term(user, user->y, 0);
	    else
		move_term(user, user->y, user->x - user->av[0]);
	    user->got_esc = 0;
	    break;
	case 'H':	/* move */
	    if(user->got_esc == 2) {
		if(user->av[0] > 0)
		    user->av[0]--;
		if(user->av[1] > 0)
		    user->av[1]--;
		move_term(user, user->av[0], user->av[1]);
	    } else {
		user->scr_tabs[user->x] = 1;
	    }
	    user->got_esc = 0;
	    break;
	case 'g':
	    if(user->got_esc == 2) {
		if(user->av[0] == 3) {			/* clear all tabs */
		    for(i=0;i<user->t_cols;i++)
			user->scr_tabs[i] = 0;
		} else {
		    user->scr_tabs[user->x] = 0;	/* clear tab at x */
		}
	    }
	    break;
	case 'J':	/* clear to end of screen */
	    clreos_term(user);
	    user->got_esc = 0;
	    break;
	case 'K':	/* clear to end of line */
	    clreol_term(user);
	    user->got_esc = 0;
	    break;
	case 'L':
	    if(user->got_esc == 2)	/* add line */
	    {
		if(user->av[0] == 0)
		    add_line_term(user, 1);
		else
		    add_line_term(user, user->av[0]);
	    }
	    user->got_esc = 0;
	    break;
	case 'M':
	    if(user->got_esc == 2)	/* delete line */
	    {
		if(user->av[0] == 0)
		    del_line_term(user, 1);
		else
		    del_line_term(user, user->av[0]);
	    }
	    else			/* reverse scroll */
		rev_scroll_term(user);
	    user->got_esc = 0;
	    break;
	case 'P':
	    if(user->got_esc == 2)	/* del char */
	    {
		if(user->av[0] == 0)
		    del_char_term(user, 1);
		else
		    del_char_term(user, user->av[0]);
	    }
	    user->got_esc = 0;
	    break;
	case 'S':	/* forward scroll */
	    scroll_term(user);
	    user->got_esc = 0;
	    break;
	case 'r':	/* set scroll region */
	    if(user->av[0] > 0)
		user->av[0]--;
	    if(user->av[1] > 0)
		user->av[1]--;
	    set_scroll_region(user, user->av[0], user->av[1]);
	    move_term(user, 0, 0);
	    user->got_esc = 0;
	    break;
	default:
	    user->got_esc = 0;
    }
}
