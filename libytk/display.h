/*
 * libytk/display.h
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

#ifndef __YTK_DISPLAY_H__
#define __YTK_DISPLAY_H__

#include "ytk.h"

void ytk_display_thing(ytk_thing *);
void ytk_sync_display(void);
void ytk_display_stack(ytk_stack *);
void ytk_winch_thing(ytk_thing *);

#endif /* __YTK_DISPLAY_H__ */
