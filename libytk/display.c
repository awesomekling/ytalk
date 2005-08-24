/*
 * libytk/display.c
 * Display routines for libytk
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

#include "ytk.h"

static int
ytk_create_window(ytk_thing *t)
{
	int rows = 0;
	int cols = 2;

	switch (t->type) {
	case YTK_T_MENU:
		rows += ytk_menu_item_count(YTK_MENU(t));
		cols += ytk_menu_width(YTK_MENU(t));
		break;
	case YTK_T_INPUTBOX:
		rows += 1;
		cols += YTK_INPUTBOX(t)->size;
		break;
	case YTK_T_MSGBOX:
		rows += ytk_msgbox_item_count(YTK_MSGBOX(t));
		cols += ytk_msgbox_width(YTK_MSGBOX(t));
		break;
	default:
#ifdef YTALK_DEBUG
		fprintf(stderr, "ytk_create_window(): Thing type %d does not support windows.\n", t->type);
#endif
		return FALSE;
	}

	if ((rows > LINES) || (cols > COLS))
		return FALSE;

	t->height = rows;
	t->width = cols;
	t->top = ((LINES / 2) - (rows / 2)) - 1;
	t->left = (COLS / 2) - (cols / 2);
	t->win = newwin(
		t->height + 2, t->width + 2,
		t->top, t->left
	);
	return TRUE;
}

void
ytk_winch_thing(ytk_thing *t)
{
	if (t->win != NULL)
		delwin(t->win);
	if (!ytk_create_window(t))
		t->win = NULL;
}

static void
ytk_display_inputbox(ytk_thing *t)
{
	snprintf(YTK_INPUTBOX(t)->buf, t->width + 1, " %-*s ",
		t->width - 2, YTK_INPUTBOX(t)->data);
	mvwaddstr(t->win, 1, 1, YTK_INPUTBOX(t)->buf);
	mvwaddch(t->win, 1, strlen(YTK_INPUTBOX(t)->data) + 2, ACS_CKBOARD);
}

static void
ytk_display_msgbox(ytk_thing *t)
{
	ytk_msgbox_item *it = NULL;
	char *linebuf;
	int y = 1;

	linebuf = get_mem(t->width * sizeof(char) + 1);

#ifdef YTALK_COLOR
	wattron(t->win, COLOR_PAIR(t->colors) | t->attr);
#endif

	while ((it = ytk_next_msgbox_item(YTK_MSGBOX(t), it))) {
		if (YTK_MSGBOX_ITEM_SEPARATOR(it)) {
			mvwaddch(t->win, y, 0, ACS_LTEE);
			whline(t->win, 0, t->width);
			mvwaddch(t->win, y, t->width + 1, ACS_RTEE);
			y++;
		} else {
			snprintf(linebuf, t->width + 1, " %-*s ", t->width - 2, it->text);
			mvwaddstr(t->win, y, 1, linebuf);
			y++;
		}
	}
	free_mem(linebuf);
}

static void
ytk_display_menu(ytk_thing *t)
{
	ytk_menu_item *it = NULL;
	char *linebuf;
	int y = 1;

	linebuf = get_mem(t->width * sizeof(char) + 1);
#ifdef YTALK_COLOR
	wattron(t->win, COLOR_PAIR(t->colors) | t->attr);
#endif

	while ((it = ytk_next_menu_item(YTK_MENU(t), it))) {
		if (YTK_MENU_ITEM_SEPARATOR(it)) {
			mvwaddch(t->win, y, 0, ACS_LTEE);
			whline(t->win, 0, t->width);
			mvwaddch(t->win, y, t->width + 1, ACS_RTEE);
			y++;
		} else {
			if (it->selected)
				wattron(t->win, A_REVERSE);
			if (YTK_MENU_ITEM_TOGGLE(it))
				snprintf(linebuf, t->width + 1, " [%c] %-*s ", (it->value) ? '*' : ' ', t->width - 6, it->text);
			else
				snprintf(linebuf, t->width + 1, " %-*s ", t->width - 2, it->text);
			if (it->hotkey)
				linebuf[t->width - 2] = it->hotkey;
			mvwaddstr(t->win, y, 1, linebuf);
			if (it->selected)
				wattroff(t->win, A_REVERSE);
			y++;
		}
	}
	free_mem(linebuf);
}

void
ytk_display_thing(ytk_thing *t)
{
	if (t->win == NULL)
		if (!ytk_create_window(t))
			return;

#ifdef YTALK_COLOR
	wattron(t->win, COLOR_PAIR(t->colors) | t->attr);

	wborder(t->win,
		ACS_VLINE | COLOR_PAIR(t->colors) | t->attr,
		ACS_VLINE | COLOR_PAIR(t->colors) | t->attr,
		ACS_HLINE | COLOR_PAIR(t->colors) | t->attr,
		ACS_HLINE | COLOR_PAIR(t->colors) | t->attr,
		ACS_ULCORNER | COLOR_PAIR(t->colors) | t->attr,
		ACS_URCORNER | COLOR_PAIR(t->colors) | t->attr,
		ACS_LLCORNER | COLOR_PAIR(t->colors) | t->attr,
		ACS_LRCORNER | COLOR_PAIR(t->colors) | t->attr
	);
#else
	wborder(t->win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
#endif

	if (t->title != NULL) {
		wattron(t->win, A_BOLD);
		mvwaddstr(t->win, 0, 1 + (t->width / 2) - (strlen(t->title) / 2), t->title);
		wattroff(t->win, A_BOLD);
	}

	switch (t->type) {
	case YTK_T_MENU:
		ytk_display_menu(t);
		break;
	case YTK_T_INPUTBOX:
		ytk_display_inputbox(t);
		break;
	case YTK_T_MSGBOX:
		ytk_display_msgbox(t);
		break;
	}

#ifdef YTALK_COLOR
	wattroff(t->win, COLOR_PAIR(t->colors) | t->attr);
#endif

	wnoutrefresh(t->win);
}

void
ytk_sync_display(void)
{
	move(LINES - 1, COLS - 1);
	wnoutrefresh(stdscr);
	doupdate();
}
