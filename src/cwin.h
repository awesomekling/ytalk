/* cwin.h -- curses interface (cwin.c) */

/*
 * NOTICE
 *
 * Copyright (c) 1990,1992,1993 Britt Yenne.  All rights reserved.
 *
 * This software is provided AS-IS.  The author gives no warranty, real or
 * assumed, and takes no responsibility whatsoever for any use or misuse of
 * this software, or any damage created by its use or misuse.
 *
 * This software may be freely copied and distributed provided that no part of
 * this NOTICE is deleted or edited in any manner.
 *
 */


extern void init_curses(void);
extern void end_curses(void);
extern int open_curses(yuser *);
extern void close_curses(yuser *);
extern void addch_curses(yuser *, yachar);
extern void move_curses(yuser *, int, int);
extern void clreol_curses(yuser *);
extern void clreos_curses(yuser *);
extern void scroll_curses(yuser *);
extern void keypad_curses(yuser *, int);
extern void flush_curses(yuser *);
extern void redisplay_curses(void);
extern void __redisplay_curses(void);	/* redisplay, but do not sync with
					 * remote screen */
extern void refresh_curses(void);
extern void retitle_all_curses(void);
extern void set_raw_curses(void);
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
