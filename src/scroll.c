/* scroll.c - scrollback functions */
#include "header.h"

yuser *scuser;

void
scroll_up(user)
	yuser *user;
{
	u_short i;

	if (user->logbot == NULL)
		return;

	if (user->scroll == 0) {
		start_scroll_term(user);
	} else {
		if (user->sca == NULL)
			user->sca = user->logbot;
		for (i = 0; i < user->rows; i++)
			if (user->sca->prev != NULL)
				user->sca = user->sca->prev;
			else
				break;
	}

	user->scroll = 1;

	update_scroll_term(user);
}

void
scroll_down(user)
	yuser *user;
{
	u_short i;

	if (user->logbot == NULL)
		return;

	if (user->sca == NULL) {
		if (user->scroll) {
			user->scroll = 0;
			end_scroll_term(user);
			return;
		}
		user->sca = user->logbot;
	}
	for (i = 0; i < user->rows; i++)
		if (user->sca->next != NULL)
			user->sca = user->sca->next;
		else
			break;

	if (user->sca == user->logbot) {
		update_scroll_term(user);
		user->sca = NULL;
	} else
		update_scroll_term(user);
}
