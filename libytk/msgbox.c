/* libytk/msgbox.c */

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
	while (it->next != NULL) {
		itn = it->next;
		free_mem(it);
		it = itn;
	}
	free_mem(it);
	free_mem(m);
}

ytk_msgbox_item *
ytk_find_last_msgbox_item(ytk_msgbox *m)
{
	ytk_msgbox_item *it;
	if (m->first == NULL)
		return NULL;
	it = m->first;
	while (it->next != NULL)
		it = it->next;
	return it;
}

ytk_msgbox_item *
ytk_add_msgbox_item(ytk_msgbox *m, char *text)
{
	ytk_msgbox_item *new, *it;
	new = get_mem(sizeof(ytk_msgbox_item));
	new->text = text;
	new->next = NULL;
	new->prev = NULL;

	it = ytk_find_last_msgbox_item(m);
	if (it == NULL) {
		m->first = new;
	} else {
		it->next = new;
		it->next->prev = it;
	}
	return new;
}

ylong
ytk_msgbox_item_count(ytk_msgbox *m)
{
	ytk_msgbox_item *it;
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
ytk_msgbox_width(ytk_msgbox *m)
{
	ytk_msgbox_item *it;
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
			if (l > s)
				s = l;
		}
		it = it->next;
	}
	return s;
}

ytk_msgbox_item *
ytk_next_msgbox_item(ytk_msgbox *m, ytk_msgbox_item *i)
{
	if (i == NULL)
		return m->first;
	return i->next;
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
