/* libytk/menu.c */

#include "ytk.h"

ytk_thing *
ytk_new_menu(char *title)
{
	ytk_thing *new_thing;
	ytk_menu *new_menu;
	new_thing = ytk_new_thing();
	new_menu = get_mem(sizeof(ytk_menu));
	new_menu->base = new_thing;
	new_thing->title = title;
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
	if (m->first == NULL)
		return;
	it = m->first;
	while (it->next != NULL) {
		itn = it->next;
		free_mem(it);
		it = itn;
	}
	free_mem(it);
	free_mem(m);
}

ytk_menu_item *
ytk_find_menu_item_with_hotkey(ytk_menu *m, char k)
{
	ytk_menu_item *it;

	if (m->first == NULL || m->first->next == NULL)
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

	if (m->first == NULL || m->first->next == NULL)
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
		it->selected = FALSE;
	}
	i->selected = TRUE;
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
		it->prev->selected = TRUE;
		p->selected = FALSE;
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
		it->next->selected = TRUE;
		p->selected = FALSE;
	}
}

ytk_menu_item *
ytk_add_menu_item(ytk_menu *m, char *text, char hotkey, void (*callback) (void *))
{
	ytk_menu_item *new;
	new = get_mem(sizeof(ytk_menu_item));
	new->text = text;
	new->hotkey = hotkey;
	new->callback = callback;
	new->selected = FALSE;
	new->value = -1;
	new->next = NULL;
	new->prev = NULL;

	if (m->last == NULL) {
		new->selected = TRUE;
		m->first = new;
	} else {
		m->last->next = new;
		m->last->next->prev = m->last;
	}
	m->last = new;
	return new;
}

ytk_menu_item *
ytk_add_menu_toggle_item(ytk_menu *m, char *t, char k, void (*c) (void *), int v)
{
	ytk_menu_item *new;
	new = ytk_add_menu_item(m, t, k, c);
	new->value = (v != 0);
	return new;
}

ylong
ytk_menu_item_count(ytk_menu *m)
{
	ytk_menu_item *it;
	ylong r = 0;
	if (m->first == NULL)
		return 0;
	it = m->first;
	while (it != NULL) {
		r++;
		it = it->next;
	}
	return r;
}

ylong
ytk_menu_width(ytk_menu *m)
{
	ytk_menu_item *it;
	ylong l, s = 0;
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

ytk_menu_item *
ytk_next_menu_item(ytk_menu *m, ytk_menu_item *i)
{
	if (i == NULL)
		return m->first;
	return i->next;
}

void
ytk_handle_menu_input(ytk_menu *m, int ch)
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
				it->callback((void *) it);
		}
		break;
	case ALTESC:
	case 27:
		if (m->base->escape != NULL)
			m->base->escape(m->base);
		break;
	default:
		if (ch > 32) {
			it = ytk_find_menu_item_with_hotkey(m, ch);
			if (it != NULL) {
				ytk_select_menu_item(m, it);
				if (YTK_MENU_ITEM_TOGGLE(it))
					it->value = !(it->value);
				if (it->callback != NULL)
					it->callback((void *) it);
			}
		}
	}
}
