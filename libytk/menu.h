/* libytk/menu.h */

#ifndef __YTK_MENU_H__
#define __YTK_MENU_H__

#include "ytk.h"

#define YTK_MENU_ITEM_NORMAL(a)		((a) && (a)->hotkey && (a)->text)
#define YTK_MENU_ITEM_TOGGLE(a)		(YTK_MENU_ITEM_NORMAL(a) && (a)->value != -1)
#define YTK_MENU_ITEM_SEPARATOR(a)	((a) && !(a)->text)

#define ytk_next_menu_item(m, i)	(((i) == NULL) ? (m)->first : (i)->next)
#define ytk_add_menu_separator(a)	ytk_add_menu_item((a), NULL, 0, NULL)

typedef struct __ytk_menu_item {
	struct __ytk_menu_item *prev;
	struct __ytk_menu_item *next;
	char *text;
	char hotkey;
	int selected;
	int value;
	void (*callback) (struct __ytk_menu_item *);
} ytk_menu_item;

typedef struct {
	ytk_thing *base;
	ytk_menu_item *first;
	ytk_menu_item *last;
} ytk_menu;

ytk_thing *ytk_new_menu(char *);
ytk_menu_item *ytk_add_menu_item(ytk_menu *, char *, char, void (*) (ytk_menu_item *));
ytk_menu_item *ytk_add_menu_toggle_item(ytk_menu *, char *, char, void (*) (ytk_menu_item *), int);
void ytk_destroy_menu(ytk_menu *);
void ytk_handle_menu_input(ytk_menu *, int);
int ytk_menu_item_count(ytk_menu *);
int ytk_menu_width(ytk_menu *);
ytk_menu_item *ytk_find_menu_item_with_hotkey(ytk_menu *, char);
void ytk_select_menu_item(ytk_menu *, ytk_menu_item *);
ytk_menu_item *ytk_find_selected_menu_item(ytk_menu *);
void ytk_step_up_menu(ytk_menu *);
void ytk_step_down_menu(ytk_menu *);

#endif /* __YTK_MENU_H__ */
