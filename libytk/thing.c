/*
 * libytk/thing.c
 * The thing!
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
ytk_new_thing()
{
	ytk_thing *new;
	new = malloc(sizeof(ytk_thing));
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
		if (t->title != NULL)
			free(t->title);
		free(t);
	}
}
