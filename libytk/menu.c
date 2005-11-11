/*
 * libytk/menu.c
 * The YTK menu
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
ytk_new_menu(char *title)
{
	ytk_thing *new_thing;
	ytk_menu *new_menu;
	new_thing = ytk_new_thing();
	new_menu = get_mem(sizeof(ytk_menu));
	new_menu->base = new_thing;
	new_thing->title = str_copy(title);
	new_menu->first = NULL;
	new_menu->last = NULL;
	new_thing->object = new_menu;
	new_thing->type = YTK_T_MENU;
	return new_thing;
}

void
ytk_destroy_menu(ytk_menu *m)
{
	ytk_menu_item *it, *itn;

	if (m->first == NULL) {
		free_mem(m);
		return;
	}
	it = m->first;
	while (it != NULL) {
		itn = it->next;
		if (it->text != NULL)
			free_mem(it->text);
		free_mem(it);
		it = itn;
	}
	free_mem(m);
}

ytk_menu_item *
ytk_find_menu_item_with_hotkey(ytk_menu *m, ychar k)
{
	ytk_menu_item *it;

	if (m->first == NULL)
		return NULL;

	it = m->first;
	while (it->hotkey != k) {
		if (it->next == NULL)
			return NULL;
		it = it->next;
	}
	return it;
}

ytk_menu_item *
ytk_find_selected_menu_item(ytk_menu *m)
{
	ytk_menu_item *it;

	if (m->first == NULL)
		return NULL;

	it = m->first;
	while (!it->selected) {
		if (it->next == NULL)
			return NULL;
		it = it->next;
	}
	return it;
}

void
ytk_select_menu_item(ytk_menu *m, ytk_menu_item *i)
{
	ytk_menu_item *it;
	if ((it = ytk_find_selected_menu_item(m))) {
		it->selected = false;
	}
	i->selected = true;
}

void
ytk_step_up_menu(ytk_menu *m)
{
	ytk_menu_item *it, *p;
	it = ytk_find_selected_menu_item(m);
	if (it == NULL || it == m->first || it->prev == NULL)
		return;
	p = it;
	while (YTK_MENU_ITEM_SEPARATOR(it->prev))
		it = it->prev;
	if (it->prev != NULL) {
		it->prev->selected = true;
		p->selected = false;
	}
}

void
ytk_step_down_menu(ytk_menu *m)
{
	ytk_menu_item *it, *p;
	it = ytk_find_selected_menu_item(m);
	if (it == NULL || it->next == NULL)
		return;
	p = it;
	while (YTK_MENU_ITEM_SEPARATOR(it->next))
		it = it->next;
	if (it->next != NULL) {
		it->next->selected = true;
		p->selected = false;
	}
}

ytk_menu_item *
ytk_add_menu_item(ytk_menu *m, char *text, char hotkey, void (*callback) (ytk_menu_item *))
{
	ytk_menu_item *new;
	new = get_mem(sizeof(ytk_menu_item));
	new->text = str_copy(text);
	new->hotkey = hotkey;
	new->callback = callback;
	new->selected = false;
	new->value = -1;
	new->next = NULL;
	new->prev = NULL;

	if (m->last == NULL) {
		new->selected = true;
		m->first = new;
	} else {
		m->last->next = new;
		m->last->next->prev = m->last;
	}
	m->last = new;
	return new;
}

ytk_menu_item *
ytk_add_menu_toggle_item(ytk_menu *m, char *t, char k, void (*c) (ytk_menu_item *), int v)
{
	ytk_menu_item *new;
	new = ytk_add_menu_item(m, t, k, c);
	new->value = (v != 0);
	return new;
}

int
ytk_menu_item_count(ytk_menu *m)
{
	ytk_menu_item *it;
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
ytk_menu_width(ytk_menu *m)
{
	ytk_menu_item *it;
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
			if (it->hotkey)
				l += 6;
			if (it->value != -1)
				l += 4;
			if (l > s)
				s = l;
		}
		it = it->next;
	}
	return s;
}

void
ytk_handle_menu_key(ytk_menu *m, int key)
{
	switch (key) {
	case YTK_KEYUP:
		ytk_step_up_menu(m);
		break;
	case YTK_KEYDOWN:
		ytk_step_down_menu(m);
		break;
	}
}

void
ytk_handle_menu_input(ytk_menu *m, ychar ch)
{
	ytk_menu_item *it;
	switch (ch) {
	case 'k':
		ytk_step_up_menu(m);
		break;
	case 'j':
		ytk_step_down_menu(m);
		break;
	case ' ':
	case '\r':
	case '\n':
		it = ytk_find_selected_menu_item(m);
		if (it != NULL) {
			if (YTK_MENU_ITEM_TOGGLE(it))
				it->value = !(it->value);
			if (it->callback != NULL)
				it->callback(it);
		}
		break;
	case ALTESC:
	case 27:
		if (m->base->escape != NULL)
			m->base->escape(m->base);
		break;
	default:
		if (is_printable(ch)) {
			it = ytk_find_menu_item_with_hotkey(m, ch);
			if (it != NULL) {
				ytk_select_menu_item(m, it);
				if (YTK_MENU_ITEM_TOGGLE(it))
					it->value = !(it->value);
				if (it->callback != NULL)
					it->callback(it);
			}
		}
	}
}
