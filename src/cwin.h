/*
 * src/cwin.h
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

#define set_raw_curses()	raw()
#define win_size(n)		((n) == 0 ? 0 : (LINES - 1) / (n))

extern void init_curses(void);
extern void end_curses(void);
extern int open_curses(yuser *);
extern void close_curses(yuser *);
extern void addch_curses(yuser *, yachar);
extern void move_curses(yuser *, int, int);
extern void clreol_curses(yuser *);
extern void clreos_curses(yuser *);
extern void scroll_curses(yuser *);
extern void keypad_curses(int);
extern void flush_curses(yuser *);
extern void redisplay_curses(void);
extern void refresh_curses(void);
extern void retitle_all_curses(void);
extern void set_cooked_curses(void);
extern void start_scroll_curses(yuser *);
extern void end_scroll_curses(yuser *);
extern void update_scroll_curses(yuser *);
extern void __update_scroll_curses(yuser *);
extern void update_message_curses(void);

#ifndef getyx
#define getyx(w,y,x)	y = w->_cury, x = w->_curx
#endif

/* EOF */
