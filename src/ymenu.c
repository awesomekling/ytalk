/*
 * ymenu.c
 *
 *
 */

#include "ytk.h"
#include "ymenu.h"
#include "cwin.h"

#ifdef YTALK_COLOR
extern int menu_colors;
extern int menu_attr;
#endif

ytk_stack *menu_stack = NULL;
ytk_thing *main_menu = NULL;
ytk_thing *options_menu = NULL;
ytk_thing *usermenu_menu = NULL;
ytk_thing *userlist_menu = NULL;
ytk_thing *error_box = NULL;
ytk_thing *message_box = NULL;

char ukey;

void do_hidething(ytk_thing *);

void
do_runcmd(ytk_inputbox *b)
{
	hide_ymenu();
	if (b->len > 0)
		execute(b->data);
	do_hidething(YTK_THING(b));
}

void
do_adduser(ytk_inputbox *b)
{
	hide_ymenu();
	if (b->len > 0)
		invite(b->data);
	do_hidething(YTK_THING(b));
}

void
do_hidething(ytk_thing *t)
{
	(void) ytk_pop(menu_stack);
	if (t == main_menu)
		hide_ymenu();
	else if (t != options_menu)
		ytk_delete_thing(t);
	refresh_curses();
	ytk_sync_display();
}

void
esc_userlist(ytk_thing *t)
{
	ytk_menu_item *i = NULL;
	while ((i = ytk_next_menu_item(YTK_MENU(t), i)) != NULL) {
		if (i->hotkey >= 'a' && i->hotkey <= 'z') {
			free_mem(i->text);
		}
	}
	do_hidething(t);
}

void
handle_runcmd()
{
	ytk_thing *c;
	c = ytk_new_inputbox("Run command", 60, do_runcmd);
	ytk_set_escape(c, do_hidething);
#ifdef YTALK_COLOR
	ytk_set_colors(c, menu_colors);
	ytk_set_attr(c, menu_attr);
#endif
	ytk_push(menu_stack, c);
}

void
handle_adduser()
{
	ytk_thing *a;
	a = ytk_new_inputbox("Add user", 60, do_adduser);
	ytk_set_escape(a, do_hidething);
#ifdef YTALK_COLOR
	ytk_set_colors(a, menu_colors);
	ytk_set_attr(a, menu_attr);
#endif
	ytk_push(menu_stack, a);
}

void
handle_shell(void *t)
{
	(void) t;
	hide_ymenu();
	execute(NULL);
}

void
handle_usermenu(void *i)
{
	init_usermenu(((ytk_menu_item *) i)->hotkey);
	ytk_push(menu_stack, usermenu_menu);
}

void
handle_userlist(void *i)
{
	(void) i;
	init_userlist();
	ytk_push(menu_stack, userlist_menu);
}

void
handle_quit(void *i)
{
	(void) i;
	if (!ytk_on_stack(menu_stack, options_menu))
		ytk_delete_thing(options_menu);
	ytk_purge_stack(menu_stack);
	bail(YTE_SUCCESS);
}

void
handle_rering_all(void *i)
{
	(void) i;
	esc_userlist(userlist_menu);
	hide_ymenu();
	rering_all();
}

void
handle_kill_unconnected(void *t)
{
	(void) t;
	esc_userlist(userlist_menu);
	hide_ymenu();
	while (wait_list)
		free_user(wait_list);
}

static void
set_flags(ylong new_flags)
{
	yuser *u;
	if (def_flags != new_flags) {
		for (u = user_list; u != NULL; u = u->unext)
			if (!(u->flags & FL_LOCKED))
				u->flags = new_flags;
	}
	def_flags = new_flags;
}

void
handle_options(void *i)
{
	(void) i;
	ytk_push(menu_stack, options_menu);
}

#define DEFAULT_TOGGLE(name, mask) \
void name(void *i) { \
	if (((ytk_menu_item *)i)->value) \
		set_flags(def_flags | mask); \
	else \
		set_flags(def_flags & ~mask); \
}

DEFAULT_TOGGLE(handle_scrolling, FL_SCROLL)
DEFAULT_TOGGLE(handle_wordwrap, FL_WRAP)
DEFAULT_TOGGLE(handle_autoimport, FL_IMPORT)
DEFAULT_TOGGLE(handle_autoinvite, FL_INVITE)
DEFAULT_TOGGLE(handle_rering, FL_RING)
DEFAULT_TOGGLE(handle_beeps, FL_BEEP)
DEFAULT_TOGGLE(handle_ignbrk, FL_IGNBRK)
DEFAULT_TOGGLE(handle_promptrering, FL_PROMPTRING)
DEFAULT_TOGGLE(handle_promptquit, FL_PROMPTQUIT)

void
init_ymenu()
{
	menu_stack = ytk_new_stack();

	main_menu = ytk_new_menu("YTalk Main Menu");
	ytk_add_menu_item(YTK_MENU(main_menu), "Add another user", 'a', handle_adduser);
	ytk_add_menu_item(YTK_MENU(main_menu), "User list", 'u', handle_userlist);
	ytk_add_menu_separator(YTK_MENU(main_menu));
	ytk_add_menu_item(YTK_MENU(main_menu), "Options", 'o', handle_options);
	ytk_add_menu_item(YTK_MENU(main_menu), "Spawn shell", 's', handle_shell);
	ytk_add_menu_item(YTK_MENU(main_menu), "Run command", 'c', handle_runcmd);
	ytk_add_menu_separator(YTK_MENU(main_menu));
	ytk_add_menu_item(YTK_MENU(main_menu), "Quit", 'q', handle_quit);
	ytk_set_escape(main_menu, hide_ymenu);
#ifdef YTALK_COLOR
	ytk_set_colors(main_menu, menu_colors);
	ytk_set_attr(main_menu, menu_attr);
#endif

	options_menu = ytk_new_menu("Options");
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Scrolling", 's', handle_scrolling, (def_flags & FL_SCROLL));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Word-wrap", 'w', handle_wordwrap, (def_flags & FL_WRAP));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Auto-import", 'i', handle_autoimport, (def_flags & FL_IMPORT));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Auto-invite", 'v', handle_autoinvite, (def_flags & FL_INVITE));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Reringing", 'r', handle_rering, (def_flags & FL_RING));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Beeps", 'b', handle_beeps, (def_flags & FL_BEEP));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Ignore break", 'k', handle_ignbrk, (def_flags & FL_IGNBRK));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Prompt rerings", 'p', handle_promptrering, (def_flags & FL_PROMPTRING));
	ytk_add_menu_toggle_item(YTK_MENU(options_menu), "Prompt to quit", 'q', handle_promptquit, (def_flags & FL_PROMPTQUIT));
	ytk_set_escape(options_menu, do_hidething);
#ifdef YTALK_COLOR
	ytk_set_colors(options_menu, menu_colors);
	ytk_set_attr(options_menu, menu_attr);
#endif
}

void
handle_disconnect_user(void *t)
{
	yuser *u;
	(void) t;
	for (u = user_list; u; u = u->unext)
		if (u->key == ukey) {
			free_user(u);
			break;
		}
	(void) ytk_pop(menu_stack);
	hide_ymenu();
}

void
init_usermenu(char k)
{
	yuser *u;
	for (u = user_list; u; u = u->unext)
		if (u->key == k) {
			usermenu_menu = ytk_new_menu(u->full_name);
			ytk_add_menu_item(YTK_MENU(usermenu_menu), "Disconnect", 'd', handle_disconnect_user);
			ytk_add_menu_item(YTK_MENU(usermenu_menu), "Save to file", 's', handle_quit);
			ukey = k;
			ytk_set_escape(usermenu_menu, do_hidething);
#ifdef YTALK_COLOR
			ytk_set_colors(usermenu_menu, menu_colors);
			ytk_set_attr(usermenu_menu, menu_attr);
#endif
			break;
		}
}

void
init_userlist()
{
	yuser *u;
	char *buf, *p;
	char hk = 'a';
	userlist_menu = ytk_new_menu("User List");
	for (u = connect_list; u; u = u->next) {
		if (u != me) {
			buf = get_mem(54 * sizeof(char));
			p = buf
			 + sprintf(buf, "%-24.24s %03dx%03d [%03dx%03d] ",
			       u->full_name, u->remote.cols, u->remote.rows,
				   u->remote.my_cols, u->remote.my_rows
			 );
			if (u->remote.vmajor > 2)
				p += sprintf(p, "YTalk %d.%d", u->remote.vmajor, u->remote.vminor);
			else if (u->remote.vmajor == 2)
				p += sprintf(p, "YTalk 2.?");
			else
				p += sprintf(p, "UNIX talk");
			u->key = hk++;
			ytk_add_menu_item(YTK_MENU(userlist_menu), buf, u->key, handle_usermenu);
		}
	}
	for (u = wait_list; u; u = u->next) {
		if (u != me) {
			buf = get_mem(54 * sizeof(char));
			p = buf
			 + sprintf(buf, "%-24.24s               <unconnected>",
				   u->full_name
			 );
			u->key = hk++;
			ytk_add_menu_item(YTK_MENU(userlist_menu), buf, u->key, handle_usermenu);
		}
	}
	ytk_add_menu_separator(YTK_MENU(userlist_menu));
	ytk_add_menu_item(YTK_MENU(userlist_menu), "Rering all unconnected", 'R', handle_rering_all);
	ytk_add_menu_item(YTK_MENU(userlist_menu), "Kill all unconnected", 'K', handle_kill_unconnected);
	ytk_set_escape(userlist_menu, esc_userlist);
#ifdef YTALK_COLOR
	ytk_set_colors(userlist_menu, menu_colors);
	ytk_set_attr(userlist_menu, menu_attr);
#endif
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
		delwin(t->win);
		t->win = NULL;
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
update_ymenu()
{
	char ch;
	while (io_len > 0) {
		if (!ytk_is_empty_stack(menu_stack)) {
			ch = *(io_ptr++);
			io_len--;
			if (io_len > 0 && (ch == 27 || ch == ALTESC))
				for (; io_len > 0; io_ptr++, io_len--);
			ytk_handle_stack_input(menu_stack, ch);
			ytk_display_stack(menu_stack);
			ytk_sync_display();
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
	if (!ytk_is_empty_stack(menu_stack)) {
		ytk_winch_stack(menu_stack);
		ytk_sync_display();
	}
}

void
__refresh_ymenu()
{
	if (!ytk_is_empty_stack(menu_stack)) {
		ytk_display_stack(menu_stack);
	}
}

void
refresh_ymenu()
{
	if (!ytk_is_empty_stack(menu_stack)) {
		__refresh_ymenu();
		ytk_sync_display();
	}
}

void
handle_ebox(ytk_thing *t)
{
	(void) t;
	ytk_pop(menu_stack);
	ytk_delete_thing(error_box);
	error_box = NULL;
	refresh_curses();
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

int
show_error_ymenu(char *str, char *syserr)
{
	if (error_box == NULL)
		error_box = ytk_new_msgbox("YTalk Error");
	else
		ytk_add_msgbox_separator(YTK_MSGBOX(error_box));
	ytk_add_msgbox_item(YTK_MSGBOX(error_box), str);
	ytk_winch_thing(error_box);
	if (syserr && (strlen(syserr) > 0))
		ytk_add_msgbox_item(YTK_MSGBOX(error_box), syserr);
	if (!ytk_on_stack(menu_stack, error_box))
		ytk_push(menu_stack, error_box);
	ytk_set_escape(error_box, handle_ebox);
#ifdef YTALK_COLOR
	ytk_set_colors(error_box, menu_colors);
	ytk_set_attr(error_box, menu_attr);
#endif
	ytk_display_stack(menu_stack);
	ytk_sync_display();
	return 0;
}

void
handle_mbox(ytk_thing *t)
{
	(void) t;
	ytk_pop(menu_stack);
	ytk_delete_thing(message_box);
	message_box = NULL;
	refresh_curses();
	ytk_display_stack(menu_stack);
	ytk_sync_display();
}

int
show_message_ymenu(char *str)
{
	if (message_box == NULL)
		message_box = ytk_new_msgbox("YTalk Message");
	else
		ytk_add_msgbox_separator(YTK_MSGBOX(message_box));
	ytk_add_msgbox_item(YTK_MSGBOX(message_box), str);
	ytk_winch_thing(message_box);
	ytk_set_escape(message_box, handle_ebox);
#ifdef YTALK_COLOR
	ytk_set_colors(message_box, menu_colors);
	ytk_set_attr(message_box, menu_attr);
#endif
	if (!ytk_on_stack(menu_stack, message_box))
		ytk_push(menu_stack, message_box);
	ytk_display_stack(menu_stack);
	ytk_sync_display();
	return 0;
}

int
yes_no(char *str)
{
	ytk_thing *yn;
	int out = 0;
	yn = ytk_new_msgbox("");
	ytk_add_msgbox_item(YTK_MSGBOX(yn), str);
#ifdef YTALK_COLOR
	ytk_set_colors(yn, menu_colors);
	ytk_set_attr(yn, menu_attr);
#endif
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
	(void) ytk_pop(menu_stack);
	if (ytk_is_empty_stack(menu_stack))
		hide_ymenu();
	else {
		refresh_curses();
		refresh_ymenu();
	}
	io_len = 0;
	return out;
}
