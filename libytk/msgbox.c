/*
 * libytk/msgbox.c
 * The YTK message box
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

ytk_thing *
ytk_new_msgbox(char *title)
{
	ytk_thing *new_thing;
	ytk_msgbox *new_msgbox;
	new_thing = ytk_new_thing();
	new_msgbox = get_mem(sizeof(ytk_msgbox));
	new_msgbox->base = new_thing;
	new_thing->title = title;
	new_msgbox->first = NULL;
	new_msgbox->last = NULL;
	new_thing->object = new_msgbox;
	new_thing->type = YTK_T_MSGBOX;
	return new_thing;
}

void
ytk_destroy_msgbox(ytk_msgbox *m)
{
	ytk_msgbox_item *it, *itn;
	if (m->first == NULL)
		return;
	it = m->first;
	while (it != NULL) {
		itn = it->next;
		free_mem(it);
		it = itn;
	}
	free_mem(m);
}

ytk_msgbox_item *
ytk_add_msgbox_item(ytk_msgbox *m, char *text)
{
	ytk_msgbox_item *new;
	new = get_mem(sizeof(ytk_msgbox_item));
	new->text = text;
	new->next = NULL;
	new->prev = NULL;

	if (m->last == NULL) {
		m->first = new;
	} else {
		m->last->next = new;
		m->last->next->prev = m->last;
	}
	m->last = new;
	return new;
}

int
ytk_msgbox_item_count(ytk_msgbox *m)
{
	ytk_msgbox_item *it;
	int r = 0;
	if (m->first == NULL)
		return 0;
	it = m->first;
	while (it != NULL) {
		r++;
		it = it->next;
	}
	return r;
}

int
ytk_msgbox_width(ytk_msgbox *m)
{
	ytk_msgbox_item *it;
	int l, s = 0;
	if (m->first == NULL) {
		if (m->base->title != NULL)
			return strlen(m->base->title);
		else
			return 0;
	}
	if (m->base->title != NULL)
		s = strlen(m->base->title);
	it = m->first;
	while (it != NULL) {
		if (it->text != NULL) {
			l = strlen(it->text);
			if (l > s)
				s = l;
		}
		it = it->next;
	}
	return s;
}

void
ytk_handle_msgbox_input(ytk_msgbox *m, int ch)
{
	switch (ch) {
	case ' ':
	case '\r':
	case '\n':
		if (m->base->escape != NULL)
			m->base->escape(m->base);
		break;
	case ALTESC:
	case 27:
		if (m->base->escape != NULL)
			m->base->escape(m->base);
		break;
	}
}
