/* libytk/thing.c */

#include "ytk.h"

ytk_thing *
ytk_new_thing(void)
{
	ytk_thing *new;
	new = get_mem(sizeof(ytk_thing));
	new->type = YTK_T_NULL;
	new->object = NULL;
	new->win = NULL;
	new->escape = NULL;
	return new;
}

void
ytk_delete_thing(ytk_thing *t)
{
	if (t->win != NULL) {
		delwin(t->win);
		t->win = NULL;
	}
	if (t->object != NULL) {
		switch (t->type) {
		case YTK_T_MENU:
			ytk_destroy_menu(YTK_MENU(t));
			break;
		case YTK_T_INPUTBOX:
			ytk_destroy_inputbox(YTK_INPUTBOX(t));
			break;
		case YTK_T_MSGBOX:
			ytk_destroy_msgbox(YTK_MSGBOX(t));
			break;
		default:
#ifdef YTALK_DEBUG
			fprintf(stderr, "ytk_delete_thing(): Unknown thing type %u\n", t->type);
#endif
			return;
		}
		free_mem(t);
	}
}

int
ytk_visible(ytk_thing *t)
{
	if (t && t->win)
		return TRUE;
	else
		return FALSE;
}
