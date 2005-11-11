/*
 * libytk/ytk.h
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

#ifndef __YTK_H__
#define __YTK_H__

#include "header.h"
#include "mem.h"

#ifdef HAVE_NCURSES_H
# include <ncurses.h>
#else
# include <curses.h>
#endif

#define YTK_MENU(t)		((ytk_menu *)(t)->object)
#define YTK_INPUTBOX(t)	((ytk_inputbox *)(t)->object)
#define YTK_MSGBOX(t)	((ytk_msgbox *)(t)->object)
#define YTK_THING(o)	(o->base)

#define YTK_T_NULL		0
#define YTK_T_MENU		1
#define YTK_T_INPUTBOX	2
#define YTK_T_MSGBOX	3

#define YTK_KEYUP		'A'
#define YTK_KEYDOWN		'B'

#define ytk_set_escape(t, e)	((t)->escape = (e))
#define ytk_set_colors(t, c)	((t)->colors = (c))
#define ytk_set_attr(t, a)		((t)->attr = (a))

typedef struct __ytk_thing {
	void	*object;
	int	type;
	WINDOW	*win;
	int	height, width;
	int	top, left;
	char	*title;
	void	(*escape) (struct __ytk_thing *);
	long unsigned int colors;
	long unsigned int attr;
} ytk_thing;

ytk_thing *ytk_new_thing(void);
void ytk_delete_thing(ytk_thing *);

#include "stack.h"
#include "menu.h"
#include "inputbox.h"
#include "msgbox.h"
#include "display.h"

#endif /* __YTK_H__ */
