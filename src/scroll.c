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

yuser *scuser;

long int scrollback_lines = 100;

void
init_scroll(yuser *user)
{
	if (scrollback_lines > 0) {
		user->scrollback = (yachar **) get_mem(scrollback_lines * sizeof(yachar *));
		memset(user->scrollback, 0, scrollback_lines * sizeof(yachar *));
		user->scrollpos = 0;
	}
}

void
free_scroll(yuser *user)
{
	long int i;
	if (scrollback_lines > 0) {
		if (user->scrollback != NULL) {
			for (i = 0; (i < scrollback_lines) && (user->scrollback[i]); i++)
				free_mem(user->scrollback[i]);
			free_mem(user->scrollback);
		}
	}
}

void
scroll_up(yuser *user)
{
	if (scrollback_lines == 0 || user->scrollback[0] == NULL)
		return;

	if (user->scroll == 0)
		start_scroll_term(user);

	if (user->scrollpos >= 0) {
		if (user->scrollpos < (user->rows / 2))
			user->scrollpos = 0;
		else
			user->scrollpos -= (user->rows / 2);
		user->scroll = 1;
	} else
		user->scroll = 0;
	update_scroll_term(user);
}

void
scroll_down(yuser *user)
{
	long int i;
	if (scrollback_lines == 0 || user->scrollback[0] == NULL)
		return;

	if ((user->scrollpos > (scrollback_lines - ((int) user->rows / 2))) ||
		(user->scrollback[user->scrollpos] == NULL ))
	{
		if (!(user->scrollback[user->scrollpos] == NULL)) {
			for (i = 0; (i < scrollback_lines) && (user->scrollback[i] != NULL); i++);
			user->scrollpos = i - 1;
		}
		user->scroll = 0;
		end_scroll_term(user);
	} else {
		user->scrollpos += (user->rows / 2);
		if (user->scrollback[user->scrollpos] == NULL) {
			for (i = 0; (i < scrollback_lines) && (user->scrollback[i] != NULL); i++);
			user->scrollpos = i - 1;
			user->scroll = 0;
			end_scroll_term(user);
		} else
			user->scroll = 1;
	}
	update_scroll_term(user);
}
