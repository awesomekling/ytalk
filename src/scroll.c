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
#include "mem.h"

#include <assert.h>

yuser *scuser;						/* User currently being scrolled. */
long int scrollback_lines = 100;	/* Maximum number of scrollback lines. */

void
init_scroll(yuser *user)
{
	/* Initialize scrollback (if we're keeping more than 0 lines) */
	if (scrollback_lines > 0) {
		user->scrollback = (yachar **) get_mem(scrollback_lines * sizeof(yachar *));
		memset(user->scrollback, 0, scrollback_lines * sizeof(yachar *));
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
			free_mem(user->scrollback[i]);
		free_mem(user->scrollback);
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
	long int i;

	assert(user);

	/* If we're not using scrollback, or this user isn't being scrolled,
	 * we can return 100 right here. */
	if (!scrollback_lines || !scrolling(user))
		return 100;

	/* Count the number of scrollback entries. This is pretty expensive
	 * compared to adding a member to yuser. (FIXME) */
	for (i = 0; i < scrollback_lines && user->scrollback[i]; i++);

	/* If the scrollback buffer is empty, we're at 100%. */
	if (i == 0)
		return 100;

	/* Return the scrolled amount in percent. */
	return (int) ( (float) user->scrollpos / (float) i * 100 );
}
