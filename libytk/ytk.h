/* ytk.h
 * the ytalk thingkit
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

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#define YTK_MENU(w)		((ytk_menu *)w->object)
#define YTK_INPUTBOX(w)	((ytk_inputbox *)w->object)
#define YTK_MSGBOX(w)	((ytk_msgbox *)w->object)
#define YTK_THING(o)	(o->base)

#define YTK_T_NULL		0
#define YTK_T_MENU		1
#define YTK_T_INPUTBOX	2
#define YTK_T_MSGBOX	3

#define YTK_WINDOW_VPADDING 0
#define YTK_WINDOW_HPADDING 2

typedef struct __ytk_thing {
	void	*object;
	ylong	type;
	WINDOW	*win;
	ylong	height, width;
	ylong	top, left;
	char	*title;
	void	(*escape) (struct __ytk_thing *);
	int		colors;
	int		attr;
} ytk_thing;

ytk_thing *ytk_new_thing();
void ytk_delete_thing(ytk_thing *);
void ytk_set_escape(ytk_thing *, void (*) (ytk_thing *));
void ytk_set_colors(ytk_thing *, int);
void ytk_set_attr(ytk_thing *, int);

#include "stack.h"
#include "menu.h"
#include "inputbox.h"
#include "msgbox.h"
#include "display.h"

#endif /* __YTK_H__ */
