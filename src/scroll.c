/*
 * src/scroll.c
 * scrollback functions
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
#include "cwin.h"

#include <assert.h>

yuser *scuser;						/* User currently being scrolled. */
long int scrollback_lines = 100;	/* Maximum number of scrollback lines. */

void
init_scroll(yuser *user)
{
	/* Initialize scrollback (if we're keeping more than 0 lines) */
	if (scrollback_lines > 0) {
		user->scrollback = (yachar **) malloc((scrollback_lines+1) * sizeof(yachar *));
		memset(user->scrollback, 0, (scrollback_lines+1) * sizeof(yachar *));
		user->scrollpos = 0;
	}
}

void
free_scroll(yuser *user)
{
	/* Loop through the user's scrollback buffer and free each stored line. */
	long int i;
	if (scrollback_lines && user->scrollback) {
		for (i = 0; (i < scrollback_lines) && (user->scrollback[i]); i++)
			free(user->scrollback[i]);
		free(user->scrollback);
	}
}

void
scroll_up(yuser *user)
{
	/* Check that we're actually using scrollback and that at least one
	 * line has been logged. */
	if (!scrollback_lines || !user->scrollback[0])
		return;

	/* If we're not in scrolling mode, tell the terminal interface to
	 * get into character. */
	if (!scrolling(user))
		start_scroll_term(user);

	/* If we've scrolled below 0, we're done scrolling, so turn off the
	 * scroll view. */
	if (user->scrollpos < 0) {
		disable_scrolling(user);
	} else {
		/* If there is less than half a screen of scrollback data left,
		 * just hit the bottom. Otherwise, scroll up half a screen. */
		if (user->scrollpos < (user->rows / 2))
			user->scrollpos = 0;
		else
			user->scrollpos -= (user->rows / 2);
		/* Mark this user as scrolling. */
		enable_scrolling(user);
	}

	/* Update the terminal. */
	update_scroll_term(user);
}

void
scroll_down(yuser *user)
{
	/* Check that we're actually using scrollback and that at least one
	 * line has been logged. */
	if (!scrollback_lines || !user->scrollback[0])
		return;

	/* If we've got less than half a screen left to scroll down,
	 * scroll to last row and return to active screen. */
	if ((user->scrollpos > (scrollback_lines - ((int) user->rows / 2))) ||
		!user->scrollback[user->scrollpos])
	{
		/* If we're not already at rock bottom... */
		if (user->scrollback[user->scrollpos]) {
			/* Scroll to last line. */
			user->scrollpos = user->scrollend;
		}
		disable_scrolling(user);
	} else {
		/* Scroll down half a screen. */
		user->scrollpos += (user->rows / 2);
		/* If we hit the floor, return to active screen and turn
		 * off scrolling. */
		if (!user->scrollback[user->scrollpos]) {
			/* Scroll to last line. */
			user->scrollpos = user->scrollend;
			disable_scrolling(user);
		} else {
			enable_scrolling(user);
		}
	}

	/* If we're done scrolling this user, notify the terminal interface. */
	if (!scrolling(user))
		end_scroll_term(user);

	/* Update the terminal. */
	update_scroll_term(user);
}

int
scrolled_amount(yuser *user)
{
	double amount;
	int cutoff;

	assert(user);

	/* If we're not using scrollback, or this user isn't being scrolled,
	 * or this user hasn't got anything in the scrollback buffer yet,
	 * we can return 100 right here. */
	if (!scrollback_lines || !scrolling(user) || !user->scrollend)
		return 100;

	/* Do a little ceil() without ceil() since ceil() needs -lm */
	amount = (double) user->scrollpos / (double) user->scrollend * 100;
	cutoff = amount;

	if (amount != cutoff )
		cutoff++;

	/* Return the scrolled amount in percent. */
	return cutoff;
}

void
scroll_add_line(yuser *user, yachar *line)
{
	long int y;

	if (!scrollback_lines)
		return;

	if (user->scrollback[scrollback_lines - 1] == NULL) {
		/* There is unused room in this user's scrollback buffer. */
		y = user->scrollend;
		if (y < scrollback_lines - 1)
			user->scrollend++;
		user->scrollback[y] = malloc((user->cols + 1) * sizeof(yachar));
	} else {
		/* Displace the history buffer one step to make room. */
		yachar *oldest = user->scrollback[0];
		for (y = 0; y < scrollback_lines - 1; ++y) {
			user->scrollback[y] = user->scrollback[y + 1];
		}
		user->scrollback[y] = realloc(oldest, (user->cols + 1) * sizeof(yachar));
	}
	memcpy(user->scrollback[y], line, (user->cols * sizeof(yachar)));

	/* Null-terminate the string. */
	user->scrollback[y][user->cols].data = 0;

	if (!scrolling(user))
		user->scrollpos = y;
	else
		if (user->scrollback[scrollback_lines - 1]) {
			update_scroll_term(user);
			retitle_all_terms();
		}
}
