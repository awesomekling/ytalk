/*
 * libytk/stack.c
 * The YTK thing stack
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

ytk_stack *
ytk_new_stack(void)
{
	ytk_stack *new;
	new = get_mem(sizeof(ytk_stack));
	new->top = NULL;
	return new;
}

void
ytk_push(ytk_stack *st, ytk_thing *t)
{
	ytk_stack_item *new;
	new = get_mem(sizeof(ytk_stack_item));
	new->thing = t;
	new->next = NULL;
	new->prev = st->top;
	if (new->prev != NULL) {
		new->prev->next = new;
	}
	st->top = new;
}

ytk_thing *
ytk_pop(ytk_stack *st)
{
	ytk_stack_item *newtop;
	ytk_thing *ret;

	if (st->top == NULL)
		return NULL;

	ret = st->top->thing;

	newtop = st->top->prev;
	free_mem(st->top);
	st->top = newtop;

	if (st->top != NULL)
		st->top->next = NULL;

	return ret;
}

int
ytk_on_stack(ytk_stack *st, ytk_thing *t)
{
	ytk_stack_item *si;
	if (st->top == NULL || t == NULL)
		return FALSE;
	si = st->top;
	while (si != NULL) {
		if (si->thing == t)
			return TRUE;
		si = si->prev;
	}
	return FALSE;
}

void
ytk_winch_stack(ytk_stack *st)
{
	ytk_stack_item *si;
	if (st->top == NULL)
		return;
	si = st->top;
	while (si != NULL) {
		ytk_winch_thing(si->thing);
		si = si->prev;
	}
}

void
ytk_display_stack(ytk_stack *st)
{
	ytk_stack_item *si;
	if (st->top == NULL)
		return;
	si = st->top;
	while (si->prev != NULL) {
		si = si->prev;
	}
	while (si != NULL) {
		ytk_display_thing(si->thing);
		si = si->next;
	}
}

void
ytk_purge_stack(ytk_stack *st)
{
	ytk_thing *t;
	while ((t = ytk_pop(st)))
		ytk_delete_thing(t);
	free_mem(st);
}

int
ytk_handle_stack_input(ytk_stack *st, char ch)
{
	if (st->top == NULL)
		return FALSE;

	switch (st->top->thing->type) {
	case YTK_T_MENU:
		ytk_handle_menu_input(YTK_MENU(st->top->thing), ch);
		break;
	case YTK_T_INPUTBOX:
		ytk_handle_inputbox_input(YTK_INPUTBOX(st->top->thing), ch);
		break;
	case YTK_T_MSGBOX:
		ytk_handle_msgbox_input(YTK_MSGBOX(st->top->thing), ch);
		break;
#ifdef YTALK_DEBUG
	default:
		fprintf(stderr, "ytk_handle_stack_input(): Got input for unsupported thing type %d.\n", st->top->thing->type);
#endif
	}
	return TRUE;
}
