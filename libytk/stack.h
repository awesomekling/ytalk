/*
 * libytk/stack.h
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

#ifndef __YTK_STACK_H__
#define __YTK_STACK_H__

#include "ytk.h"

#define ytk_is_empty_stack(s)		(((s) == NULL) || ((s)->top == NULL))

typedef struct __ytk_stack_item {
	ytk_thing *thing;
	struct __ytk_stack_item *prev;
	struct __ytk_stack_item *next;
} ytk_stack_item;

typedef struct __ytk_stack {
	ytk_stack_item *top;
} ytk_stack;

ytk_stack *ytk_new_stack(void);
void ytk_purge_stack(ytk_stack *);
void ytk_push(ytk_stack *, ytk_thing *);
ytk_thing *ytk_pop(ytk_stack *);
int ytk_handle_stack_input(ytk_stack *, char);
int ytk_handle_stack_key(ytk_stack *, int);
void ytk_winch_stack(ytk_stack *);
int ytk_on_stack(ytk_stack *, ytk_thing *);

#endif /* __YTK_STACK_H__ */
