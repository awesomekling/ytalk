/* ytk_display.c
 * the ytalk thingkit
 *
 */

#include "ytk.h"

int
ytk_create_window(ytk_thing *t)
{
	ylong rows = YTK_WINDOW_VPADDING;
	ylong cols = YTK_WINDOW_HPADDING;

	switch (t->type) {
	case YTK_T_MENU:
		rows += ytk_menu_item_count(YTK_MENU(t));
		cols += ytk_menu_width(YTK_MENU(t));
		break;
	case YTK_T_INPUTBOX:
		rows += 1;
		cols += YTK_INPUTBOX(t)->size;
		break;
	case YTK_T_MSGBOX:
		rows += ytk_msgbox_item_count(YTK_MSGBOX(t));
		cols += ytk_msgbox_width(YTK_MSGBOX(t));
		break;
	default:
		fprintf(stderr, "ytk_create_window(): Thing type %d does not support windows.\n", t->type);
		return FALSE;
	}
	t->height = rows;
	t->width = cols;
	t->top = (LINES / 2) - (rows / 2);
	t->left = (COLS / 2) - (cols / 2);
	t->win = newwin(
		t->height + 2, t->width + 2,
		t->top, t->left
	);
	return TRUE;
}

void
ytk_winch_thing(ytk_thing *w)
{
	if (w->win) {
		delwin(w->win);
		w->win = NULL;		/* Not really necessary. */
	}
	if (!ytk_create_window(w))
		bail(YTE_ERROR);
}

void
ytk_display_inputbox(ytk_thing *t)
{
	memset(YTK_INPUTBOX(t)->buf, ' ', t->width - (YTK_WINDOW_HPADDING / 2));
	YTK_INPUTBOX(t)->buf[t->width - (YTK_WINDOW_HPADDING / 2)] = 0;

	wattroff(t->win, A_REVERSE);
	mvwaddstr(t->win, 1, 1 + (YTK_WINDOW_HPADDING / 2), YTK_INPUTBOX(t)->buf);
	mvwaddstr(t->win, 1, 1 + (YTK_WINDOW_HPADDING / 2), YTK_INPUTBOX(t)->data);
	waddch(t->win, ACS_CKBOARD);
}

void
ytk_display_msgbox(ytk_thing *t)
{
	ytk_msgbox_item *it = NULL;
	char *padbuf;
	ylong y;

	padbuf = get_mem(t->width * sizeof(char) + 1);
	memset(padbuf, ' ', t->width);
	padbuf[t->width] = 0;

	y = 1;
	while ((it = ytk_next_msgbox_item(YTK_MSGBOX(t), it))) {
		if(YTK_MSGBOX_ITEM_SEPARATOR(it)) {
			mvwaddch(t->win, y, 0, ACS_LTEE);
			whline(t->win, 0, t->width);
			mvwaddch(t->win, y, t->width + 1, ACS_RTEE);
			y++;
		} else {
			mvwaddstr(t->win, y, 1, padbuf);
			mvwaddstr(t->win, y, 1 + (YTK_WINDOW_HPADDING / 2), it->text);
			y++;
		}
	}
	free_mem(padbuf);
}

void
ytk_display_menu(ytk_thing *w)
{
	ytk_menu_item *it = NULL;
	char *padbuf;
	ylong y;

	padbuf = get_mem(w->width * sizeof(char) + 1);
	memset(padbuf, ' ', w->width);
	padbuf[w->width] = 0;

	y = 1;
	while ((it = ytk_next_menu_item(YTK_MENU(w), it))) {
		if(YTK_MENU_ITEM_SEPARATOR(it)) {
			mvwaddch(w->win, y, 0, ACS_LTEE);
			whline(w->win, 0, w->width);
			mvwaddch(w->win, y, w->width + 1, ACS_RTEE);
			y++;
		} else {
			if(it->selected)
				wattroff(w->win, A_REVERSE);
			mvwaddstr(w->win, y, 1, padbuf);
			if (YTK_MENU_ITEM_TOGGLE(it)) {
				mvwaddch(w->win, y, 1 + (YTK_WINDOW_HPADDING / 2), '[');
				if (it->value)
					waddch(w->win, ACS_DIAMOND);
				else
					waddch(w->win, ' ');
				waddch(w->win, ']');
				waddch(w->win, ' ');
				waddstr(w->win, it->text);
			} else {
				mvwaddstr(w->win, y, 1 + (YTK_WINDOW_HPADDING / 2), it->text);
			}
			if (it->hotkey)
				mvwaddch(w->win, y, w->width - (YTK_WINDOW_HPADDING / 2), it->hotkey);
			if(it->selected)
				wattron(w->win, A_REVERSE);
			y++;
		}
	}
	free_mem(padbuf);
}

void
ytk_display_thing(ytk_thing *t)
{
	if (!t->win)
		if(!ytk_create_window(t))
			bail(YTE_ERROR);

	wattron(t->win, A_REVERSE);

	box(t->win, 0, 0);

	wattron(t->win, A_BOLD);
	mvwaddstr(t->win, 0, (t->width / 2) - (strlen(t->title)) / 2, t->title);
	wattroff(t->win, A_BOLD);

	switch (t->type) {
	case YTK_T_MENU:
		ytk_display_menu(t);
		break;
	case YTK_T_INPUTBOX:
		ytk_display_inputbox(t);
		break;
	case YTK_T_MSGBOX:
		ytk_display_msgbox(t);
		break;
	}

	wnoutrefresh(t->win);
}

void
ytk_sync_display()
{
	move(LINES - 1, COLS - 1);
	wnoutrefresh(stdscr);
	doupdate();
}
