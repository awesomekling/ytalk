/*
 * libytk/inputbox.h
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

#ifndef __YTK_INPUTBOX_H__
#define __YTK_INPUTBOX_H__

#include "ytk.h"

typedef struct __ytk_inputbox {
	ytk_thing *base;
	char *data;
	unsigned int size;
	unsigned int len;
	void (*callback) (struct __ytk_inputbox *);
} ytk_inputbox;

ytk_thing *ytk_new_inputbox(char *, int, void (*callback) (ytk_inputbox *));
void ytk_destroy_inputbox(ytk_inputbox *);
void ytk_handle_inputbox_input(ytk_inputbox *, ychar);

#endif /* __YTK_INPUTBOX_H__ */
