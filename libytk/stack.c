/* libytk/stack.c */

#include "ytk.h"

ytk_stack *
ytk_new_stack()
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
ytk_is_empty_stack(ytk_stack *st)
{
	if (st && st->top)
		return FALSE;
	else
		return TRUE;
}

int
ytk_on_stack(ytk_stack *st, ytk_thing *t)
{
	ytk_stack_item *si;
	if (st->top == NULL || t == NULL)
		return FALSE;
	si = st->top;
	while (si->prev != NULL) {
		if (si->thing == t)
			return TRUE;
		si = si->prev;
	}
	if (si->thing == t)
		return TRUE;
	return FALSE;
}

void
ytk_winch_stack(ytk_stack *st)
{
	ytk_stack_item *si;
	if (st->top == NULL)
		return;
	si = st->top;
	while (si->prev != NULL) {
		ytk_winch_thing(si->thing);
		si = si->prev;
	}
	ytk_winch_thing(si->thing);
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
	while (si->next != NULL) {
		ytk_display_thing(si->thing);
		si = si->next;
	}
	ytk_display_thing(si->thing);
}

void
ytk_purge_stack(ytk_stack *st)
{
	ytk_thing *t;
	while ((t = ytk_pop(st)))
		ytk_delete_thing(t);
	free_mem(st);
}

void
ytk_delete_stack(ytk_stack *st)
{
	for (;;) {
		if (ytk_pop(st) == NULL)
			break;
	}
	free_mem(st);
}

int
ytk_handle_stack_input(ytk_stack *st, int ch)
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
