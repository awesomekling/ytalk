/*
 * libytk/msgbox.h
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

#ifndef __YTK_MSGBOX_H__
#define __YTK_MSGBOX_H__

#include "ytk.h"

#define YTK_MSGBOX_ITEM_SEPARATOR(a)	((a) && !(a)->text)

#define ytk_next_msgbox_item(m, i)	(((i) == NULL) ? (m)->first : (i)->next)
#define ytk_add_msgbox_separator(a)	ytk_add_msgbox_item((a), NULL)

typedef struct __ytk_msgbox_item {
	struct __ytk_msgbox_item *prev;
	struct __ytk_msgbox_item *next;
	char *text;
} ytk_msgbox_item;

typedef struct {
	ytk_thing *base;
	ytk_msgbox_item *first;
	ytk_msgbox_item *last;
} ytk_msgbox;

ytk_thing *ytk_new_msgbox(char *);
ytk_msgbox_item *ytk_add_msgbox_item(ytk_msgbox *, char *);
void ytk_destroy_msgbox(ytk_msgbox *);
void ytk_handle_msgbox_input(ytk_msgbox *, int);
int ytk_msgbox_item_count(ytk_msgbox *);
int ytk_msgbox_width(ytk_msgbox *);

#endif /* __YTK_MSGBOX_H__ */
