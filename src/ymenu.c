/*
 * src/ymenu.c
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

#include "ytk.h"
#include "ymenu.h"
#include "cwin.h"

static ytk_stack *menu_stack = NULL;
static ytk_thing *main_menu = NULL;
static ytk_thing *options_menu = NULL;
static ytk_thing *usermenu_menu = NULL;
static ytk_thing *userlist_menu = NULL;
static ytk_thing *error_box = NULL;

static ytk_thing *color_menu = NULL;

extern unsigned long int menu_colors;
extern unsigned long int menu_attr;

/* keypad escape sequences and their lengths */
static char *tcku, *tckd;
static int tckul, tckdl;

static yuser *menu_user;

static void do_hidething(ytk_thing *);
static void init_userlist();
static void init_usermenu(char);

static void
recolor_menus()
{
	ytk_set_colors(main_menu, menu_colors);
	ytk_set_attr(main_menu, menu_attr);
	ytk_set_colors(options_menu, menu_colors);
	ytk_set_attr(options_menu, menu_attr);
	ytk_set_colors(color_menu, menu_colors);
	ytk_set_attr(color_menu, menu_attr);
}


static void
do_runcmd(ytk_inputbox *b)
{
	/* XXX: This isn't the most beautiful way to do this, but since
	 * execute() locks for at least 1 full second, it's better to remove
	 * the menu before calling it.
	 *
	 * However, since hide_ymenu() deletes the shell command, we have
	 * to make a copy of it first.
	 */

	char *command = NULL;

	if (b->len > 0) {
		command = str_copy(b->data);
	}

	hide_ymenu();

	if (command != NULL) {
		execute(command);
		free_mem(command);
	}
}

static void
do_adduser(ytk_inputbox *b)
{
	if (b->len > 0)
		invite(b->data, 0);
	hide_ymenu();
}

static void
do_save_user_to_file(ytk_inputbox *b)
{
	char *filename = NULL;
	if (menu_user && b && b->len > 0)
		filename = str_copy(b->data);
	hide_ymenu();
	save_user_to_file(menu_user, filename);
	if (filename)
		free_mem(filename);
}

static void
do_hidething(ytk_thing *t)
{
	ytk_pop(menu_stack);
	if (t == main_menu)
		hide_ymenu();
	else if (t != options_menu && t != color_menu)
		ytk_delete_thing(t);
	refresh_curses();
	ytk_sync_display();
}

static void
esc_userlist(ytk_thing *t)
{
	(void) t;
	do_hidething(t);
}

static void
handle_main_menu(ytk_menu_item *i)
{
	ytk_thing *b;
	switch (i->hotkey) {
	case 'a':
		b = ytk_new_inputbox("Add user", 60, do_adduser);
		ytk_set_escape(b, do_hidething);
		ytk_set_colors(b, menu_colors);
		ytk_set_attr(b, menu_attr);
		ytk_push(menu_stack, b);
		break;
	case 'c':
		b = ytk_new_inputbox("Run command", 60, do_runcmd);
		ytk_set_escape(b, do_hidething);
		ytk_set_colors(b, menu_colors);
		ytk_set_attr(b, menu_attr);
		ytk_push(menu_stack, b);
		break;
	case 'o':
		ytk_push(menu_stack, options_menu);
		break;
	case 's':
		hide_ymenu();
		execute(NULL);
		break;
	case 'u':
		init_userlist();
		ytk_push(menu_stack, userlist_menu);
		break;
	case 'q':
		ytk_purge_stack(menu_stack);
		bail(YTE_SUCCESS);
		break;
	}
}

static void
handle_userlist_menu(ytk_menu_item *i)
{
	switch (i->hotkey) {
	case 'R':
		esc_userlist(userlist_menu);
		hide_ymenu();
		rering_all();
		break;
	case 'K':
		esc_userlist(userlist_menu);
		hide_ymenu();
		while (wait_list)
			free_user(wait_list);
		break;
	default:
		init_usermenu(i->hotkey);
		ytk_push(menu_stack, usermenu_menu);
		break;
	}
}

static void
do_reloadrc(ytk_menu_item *i)
{
	(void) i;
	hide_ymenu();
	read_ytalkrc();
	recolor_menus();
	redraw_all_terms();
}

static void
handle_color(ytk_menu_item *i)
{
	char k = i->hotkey;
	char buf[8];
	int len;

	if (k >= '0' && k <= '7') {
		me->yac.foreground = k - '0';
		len = snprintf(buf, sizeof(buf), "%c[%dm", 27, (k - '0') + 30);
		send_users(me, (ychar *)buf, len, (ychar *)buf, len);
	}
	switch (k) {
	case 'b':
		if (i->value)
			me->yac.attributes |= YATTR_BOLD;
		else
			me->yac.attributes &= ~YATTR_BOLD;
		len = snprintf(buf, sizeof(buf), "\033[%d1m", (i->value ? 0 : 2));
		send_users(me, (ychar *)buf, len, (ychar *)buf, len);
		break;
	}
	hide_ymenu();
}

static void
set_flags(int togval, ylong mask)
{
	yuser *u;
	ylong new_flags;
	new_flags = (togval) ? (def_flags | mask) : (def_flags & ~mask);
	if (def_flags != new_flags) {
		for (u = user_list; u != NULL; u = u->unext)
			if (!(u->flags & FL_LOCKED))
				u->flags = new_flags;
	}
	def_flags = new_flags;
}

static void
handle_options_menu(ytk_menu_item *i) {
	switch (i->hotkey) {
	case 's': set_flags(i->value, FL_SCROLL); break;
	case 'w': set_flags(i->value, FL_WRAP); break;
	case 'i': set_flags(i->value, FL_IMPORT); break;
	case 'v': set_flags(i->value, FL_INVITE); break;
	case 'r': set_flags(i->value, FL_RING); break;
	case 'b': set_flags(i->value, FL_BEEP); break;
	case 'g': set_flags(i->value, FL_IGNBRK); break;
	case 'c': set_flags(i->value, FL_CAPS); break;
	case 'p': set_flags(i->value, FL_PROMPTRING); break;
	case 'q': set_flags(i->value, FL_PROMPTQUIT); break;
	}
}

void
init_ymenu()
{
	/* extract the termcap strings we want for keypad up/down */
	tcku = get_tcstr("ku");
	tckd = get_tcstr("kd");
	tckul = strlen(tcku);
	tckdl = strlen(tckd);

	/* build us a couple of menus */
	menu_stack = ytk_new_stack();

	main_menu = ytk_new_menu("Main Menu");
	ytk_add_menu_item(YTK_MENU(main_menu), "Add another user", 'a', handle_main_menu);
	ytk_add_menu_item(YTK_MENU(main_menu), "User list", 'u', handle_main_menu);
	ytk_add_menu_separator(YTK_MENU(main_menu));
	ytk_add_menu_item(YTK_MENU(main_menu), "Options", 'o', handle_main_menu);
	ytk_add_menu_item(YTK_MENU(main_menu), "Spawn shell", 's', handle_main_menu);
	ytk_add_menu_item(YTK_MENU(main_menu), "Run command", 'c', handle_main_menu);
	ytk_add_menu_separator(YTK_MENU(main_menu));
	ytk_add_menu_item(YTK_MENU(main_menu), "Quit", 'q', handle_main_menu);
	ytk_set_escape(main_menu, do_hidething);
	ytk_set_colors(main_menu, menu_colors);
	ytk_set_attr(main_menu, menu_attr);

	options_menu = ytk_new_menu("Options");
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Scrolling", 's', handle_options_menu, (def_flags & FL_SCROLL));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Word-wrap", 'w', handle_options_menu, (def_flags & FL_WRAP));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Auto-import", 'i', handle_options_menu, (def_flags & FL_IMPORT));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Auto-invite", 'v', handle_options_menu, (def_flags & FL_INVITE));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Reringing", 'r', handle_options_menu, (def_flags & FL_RING));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Beeps", 'b', handle_options_menu, (def_flags & FL_BEEP));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Ignore break", 'g', handle_options_menu, (def_flags & FL_IGNBRK));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Require caps", 'c', handle_options_menu, (def_flags & FL_CAPS));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Prompt rerings", 'p', handle_options_menu, (def_flags & FL_PROMPTRING));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Prompt to quit", 'q', handle_options_menu, (def_flags & FL_PROMPTQUIT));
	ytk_add_menu_separator(YTK_MENU(options_menu));
	ytk_add_menu_item(YTK_MENU(options_menu), "Reload ytalkrc", 'R', do_reloadrc);
	ytk_set_escape(options_menu, do_hidething);
	ytk_set_colors(options_menu, menu_colors);
	ytk_set_attr(options_menu, menu_attr);

	color_menu = ytk_new_menu("Color Menu");
	ytk_add_menu_item(YTK_MENU(color_menu), "Black", '0', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Red", '1', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Green", '2', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Yellow", '3', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Blue", '4', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Magenta", '5', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "Cyan", '6', handle_color);
	ytk_add_menu_item(YTK_MENU(color_menu), "White", '7', handle_color);
	ytk_add_menu_separator(YTK_MENU(color_menu));
	ytk_add_menu_toggle_item(YTK_MENU(color_menu), "Bold", 'b', handle_color, (me->yac.attributes & YATTR_BOLD));
	ytk_set_escape(color_menu, do_hidething);
	ytk_set_colors(color_menu, menu_colors);
	ytk_set_attr(color_menu, menu_attr);
}

static void
handle_disconnect_user(ytk_menu_item *i)
{
	(void) i;
	hide_ymenu();
	if (menu_user)
		free_user(menu_user);
}

static void
handle_save_to_file(ytk_menu_item *i)
{
	ytk_thing *b;
	(void) i;
	b = ytk_new_inputbox("Enter filename:", 60, do_save_user_to_file);
	ytk_set_escape(b, do_hidething);
	ytk_set_colors(b, menu_colors);
	ytk_set_attr(b, menu_attr);
	ytk_push(menu_stack, b);
}

static void
view_user_info(ytk_menu_item *item)
{
	static char client_type[128];
	static char gtalk_version[MAXBUF + 128];
	static char system_type[MAXBUF + 128];
	static char user_at_fqdn[128];
	static char edit_chars[40];
	ytk_thing *info_box;
	ychar c;
	char *p;
	int i;

	(void) item;

	if (!menu_user)
		return;

	info_box = ytk_new_msgbox("User information");

	snprintf(user_at_fqdn, sizeof(user_at_fqdn), "User:            %s", menu_user->full_name);
	ytk_add_msgbox_item(YTK_MSGBOX(info_box), user_at_fqdn);

	if (menu_user->edit[0] && menu_user->edit[1] && menu_user->edit[2]) {
		p = client_type;
		p += sprintf(p, "Client type:     ");
		if (menu_user->remote.vmajor > 2) {
			p += sprintf(p, "YTalk %d.%d",
				menu_user->remote.vmajor,
				menu_user->remote.vminor
			);
		} else if (menu_user->remote.vmajor == 2) {
			p += sprintf(p, "YTalk 2");
		} else if (menu_user->gt.version) {
			p += sprintf(p, "GNU Talk");
		} else {
			p += sprintf(p, "BSD Talk");
		}
		ytk_add_msgbox_item(YTK_MSGBOX(info_box), client_type);
	}

	if (menu_user->gt.version) {
		snprintf(gtalk_version, sizeof(gtalk_version), "Version info:    %s", menu_user->gt.version);
		ytk_add_msgbox_item(YTK_MSGBOX(info_box), gtalk_version);
	}
	if (menu_user->gt.system) {
		snprintf(system_type, sizeof(gtalk_version), "System type:     %s", menu_user->gt.system);
		ytk_add_msgbox_item(YTK_MSGBOX(info_box), system_type);
	}
	if (menu_user->edit[0] && menu_user->edit[1] && menu_user->edit[2]) {
		p = edit_chars;
		p += sprintf(edit_chars, "Edit characters: ");
		for (i = 0; i < 3; i++) {
			c = menu_user->edit[i];
			if (c < ' ') {
				p += sprintf(p, "^");
				c += '@';
			}
			p += sprintf(p, "%c ", c);
		}
		ytk_add_msgbox_item(YTK_MSGBOX(info_box), edit_chars);
	}
	ytk_set_escape(info_box, do_hidething);
	ytk_set_colors(info_box, menu_colors);
	ytk_set_attr(info_box, menu_attr);
	ytk_push(menu_stack, info_box);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

static void
init_usermenu(char k)
{
	yuser *user;
	for (user = user_list; user; user = user->unext)
		if (user->key == k)
			break;

	if (!user) {
		if (k != me->key)
			return;
		user = me;
	}

	usermenu_menu = ytk_new_menu(user->full_name);
	ytk_add_menu_item(YTK_MENU(usermenu_menu), "User information", 'i', view_user_info);
	if (user != me) {
		ytk_add_menu_item(YTK_MENU(usermenu_menu), "Disconnect", 'd', handle_disconnect_user);
	}
	ytk_add_menu_item(YTK_MENU(usermenu_menu), "Save to file", 's', handle_save_to_file);
	ytk_set_escape(usermenu_menu, do_hidething);
	ytk_set_colors(usermenu_menu, menu_colors);
	ytk_set_attr(usermenu_menu, menu_attr);
	menu_user = user;
}

static void
init_userlist()
{
	yuser *u;
	char buf[54], *p;
	char hk = 'a';
	userlist_menu = ytk_new_menu("User List");
	for (u = connect_list; u; u = u->next) {
		if (u != me) {
			p = buf
			 + sprintf(buf, "%-24.24s %03dx%03d [%03dx%03d] ",
			       u->full_name, u->remote.cols, u->remote.rows,
				   u->remote.my_cols, u->remote.my_rows
			 );
			if (u->remote.vmajor > 2)
				p += sprintf(p, "YTalk %d.%d", u->remote.vmajor, u->remote.vminor);
			else if (u->remote.vmajor == 2)
				p += sprintf(p, "YTalk 2.?");
			else if (u->gt.version != NULL)
				p += sprintf(p, "GNU talk");
			else
				p += sprintf(p, "BSD talk");
			u->key = hk++;
			ytk_add_menu_item(YTK_MENU(userlist_menu), buf, u->key, handle_userlist_menu);
		}
	}
	for (u = wait_list; u; u = u->next) {
		if (u != me) {
			p = buf
			 + sprintf(buf, "%-24.24s               %13.13s",
				   u->full_name, "<unconnected>"
			 );
			u->key = hk++;
			ytk_add_menu_item(YTK_MENU(userlist_menu), buf, u->key, handle_userlist_menu);
		}
	}
	ytk_add_menu_separator(YTK_MENU(userlist_menu));
	snprintf(buf, sizeof(buf), "Me (%.36s)", me->full_name);
	ytk_add_menu_item(YTK_MENU(userlist_menu), buf, me->key, handle_userlist_menu);
	ytk_add_menu_separator(YTK_MENU(userlist_menu));
	ytk_add_menu_item(YTK_MENU(userlist_menu), "Rering all unconnected", 'R', handle_userlist_menu);
	ytk_add_menu_item(YTK_MENU(userlist_menu), "Kill all unconnected", 'K', handle_userlist_menu);
	ytk_set_escape(userlist_menu, esc_userlist);
	ytk_set_colors(userlist_menu, menu_colors);
	ytk_set_attr(userlist_menu, menu_attr);
}

void
redo_ymenu_userlist()
{
	if (ytk_on_stack(menu_stack, userlist_menu) && !ytk_on_stack(menu_stack, usermenu_menu)) {
		ytk_pop(menu_stack);
		refresh_curses();
		if (userlist_menu != NULL) {
			ytk_delete_thing(userlist_menu);
			userlist_menu = NULL;
		}
		init_userlist();
		ytk_push(menu_stack, userlist_menu);
		ytk_display_stack(menu_stack);
		ytk_sync_display();
	}
}

void
hide_ymenu()
{
	ytk_thing *t;
	while ((t = ytk_pop(menu_stack))) {
		if (t != main_menu && t != options_menu && t != color_menu && t != userlist_menu) {
			ytk_delete_thing(t);
		} else if (t == userlist_menu) {
			esc_userlist(userlist_menu);
		} else {
			delwin(t->win);
			t->win = NULL;
		}
	}
	refresh_curses();
	ytk_sync_display();
}

void
show_ymenu()
{
	ytk_push(menu_stack, main_menu);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

void
show_colormenu()
{
	ytk_menu_item *it;

	it = ytk_find_menu_item_with_hotkey(YTK_MENU(color_menu), (me->yac.foreground + '0'));
	ytk_select_menu_item(YTK_MENU(color_menu), it);

	it = ytk_find_menu_item_with_hotkey(YTK_MENU(color_menu), 'b');
	it->value = (me->yac.attributes & YATTR_BOLD) != 0;

	ytk_push(menu_stack, color_menu);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

void
update_ymenu()
{
	char ch;
	int i;
	while (io_len > 0) {
		if (!ytk_is_empty_stack(menu_stack)) {
			if ((io_len >= tckul) && (memcmp(io_ptr, tcku, tckul) == 0)) {
				for (i = 0; i < tckul; i++)
					io_ptr++, io_len--;
				ytk_handle_stack_key(menu_stack, YTK_KEYUP);
				ytk_display_stack(menu_stack);
				ytk_sync_display();
				continue;
			} else if ((io_len >= tckdl) && (memcmp(io_ptr, tckd, tckdl) == 0)) {
				for (i = 0; i < tckdl; i++)
					io_ptr++, io_len--;
				ytk_handle_stack_key(menu_stack, YTK_KEYDOWN);
				ytk_display_stack(menu_stack);
				ytk_sync_display();
				continue;
			} else {
				ch = *(io_ptr++);
				io_len--;
				if (io_len > 0 && (ch == 27 || ch == ALTESC))
					for (; io_len > 0; io_ptr++, io_len--);
				ytk_handle_stack_input(menu_stack, ch);
			}
			ytk_display_stack(menu_stack);
			ytk_sync_display();
		} else {
			break;
		}
	}
}

int
in_ymenu()
{
	if (!ytk_is_empty_stack(menu_stack))
		return 1;
	else
		return 0;
}

int
can_ymenu()
{
	if (main_menu != NULL)
		return 1;
	else
		return 0;
}

void
resize_ymenu()
{
	ytk_winch_thing(main_menu);
	ytk_winch_thing(options_menu);
	ytk_winch_thing(color_menu);
	if (!ytk_is_empty_stack(menu_stack))
		ytk_winch_stack(menu_stack);
}

void
__refresh_ymenu()
{
	ytk_display_stack(menu_stack);
}

void
refresh_ymenu()
{
	__refresh_ymenu();
	ytk_sync_display();
}

static void
handle_error_box(ytk_thing *t)
{
	ytk_pop(menu_stack);
	ytk_delete_thing(t);
	error_box = NULL;
	refresh_curses();
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

int
show_error_ymenu(char *str, char *syserr, char *title)
{
	if (error_box == NULL)
		error_box = ytk_new_msgbox((title != NULL) ? title : "");
	else
		ytk_add_msgbox_separator(YTK_MSGBOX(error_box));
	ytk_add_msgbox_item(YTK_MSGBOX(error_box), str);
	if (syserr)
		ytk_add_msgbox_item(YTK_MSGBOX(error_box), syserr);
	ytk_winch_thing(error_box);
	if (!ytk_on_stack(menu_stack, error_box))
		ytk_push(menu_stack, error_box);
	ytk_set_escape(error_box, handle_error_box);
	ytk_set_colors(error_box, menu_colors);
	ytk_set_attr(error_box, menu_attr);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
	return 0;
}

int
yes_no(char *str)
{
	ytk_thing *yn;
	int out = 0;
	yn = ytk_new_msgbox(NULL);
	ytk_add_msgbox_item(YTK_MSGBOX(yn), str);
	ytk_set_colors(yn, menu_colors);
	ytk_set_attr(yn, menu_attr);
	ytk_push(menu_stack, yn);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
	do {
		update_ymenu();
		input_loop();
		for (; io_len > 0; io_len--, io_ptr++) {
			if (*io_ptr == 'Y' || (*io_ptr == 'y' && !(def_flags & FL_CAPS))) {
				out = 'y';
				break;
			}
			if (*io_ptr == 'N' || (*io_ptr == 'n' && !(def_flags & FL_CAPS)) || (*io_ptr == 27 || *io_ptr == ALTESC)) {
				out = 'n';
				break;
			}
		}
	} while (out == 0);
	ytk_delete_thing(yn);
	ytk_pop(menu_stack);
	if (ytk_is_empty_stack(menu_stack))
		hide_ymenu();
	else {
		refresh_curses();
		refresh_ymenu();
	}
	io_len = 0;
	return out;
}
