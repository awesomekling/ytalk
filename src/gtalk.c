/* gtalk.c
 * some stuff to interface with GNU talk
 *
 */

#include "header.h"
#include "cwin.h"
#include "mem.h"
#include "gtalk.h"
#include "ymenu.h"

void
gtalk_process(yuser *user, ychar data)
{
	char *msg;

	if (user->gt_len == (MAXBUF - 1))
		return;

	if (user->gt_type == 0) {
		user->gt_type = data;
		return;
	}

	if (data == user->KILL || data == '\n') {
		user->got_gt = 0;
		user->gt_buf[user->gt_len] = 0;
		switch (user->gt_type) {
		case GTALK_PERSONAL_NAME:
		case GTALK_IMPORT_REQUEST:
			break;
		case GTALK_VERSION_MESSAGE:
			if (user->gt.version != NULL)
				free_mem(user->gt.version);
			user->gt.version = gtalk_parse_version(user->gt_buf);
			retitle_all_terms();
			break;
		default:
			msg = get_mem(user->gt_len + 2);
#ifdef HAVE_SNPRINTF
			snprintf(msg, user->gt_len + 2, "%c%s", user->gt_type, user->gt_buf);
#else
			sprintf(msg, "%c%s", user->gt_type, user->gt_buf);
#endif
			show_message_ymenu(user->full_name, msg);
			free_mem(msg);
		}
		return;
	}

	user->gt_buf[user->gt_len++] = data;
}

char *
gtalk_parse_version(char *str)
{
	char *p, *e;
	p = strchr(str, ' ');
	if (p != NULL)
		p = strchr(p + 1, ' ');
	if (p != NULL) {
		p++;
		for (e = p; *e; e++)
			if (*e == 21 || *e == '\n') {
				*e = 0;
				break;
			}
		return str_copy(p);
	}
	return NULL;
}

void
gtalk_send_version(yuser *user)
{
	char *buf;
	int len;
	buf = get_mem(strlen(me->user_name) + strlen(PACKAGE_VERSION) + 32);
#ifdef HAVE_SNPRINTF
	len = snprintf(buf, strlen(me->user_name) + strlen(PACKAGE_VERSION) + 32,
#else
	len = sprintf(buf,
#endif
						"%c%c%s %d YTALK %s%c\n",
						GTALK_ESCAPE,
						GTALK_VERSION_MESSAGE,
						me->user_name,
						ntohs(user->sock.sin_port),
						PACKAGE_VERSION,
						me->KILL
	);
	write(user->fd, buf, len);
	free_mem(buf);
	return;
}

void
gtalk_send_message(yuser *user, char *msg)
{
	char *buf;
	int len;
	buf = get_mem(strlen(msg) + 3);
#ifdef HAVE_SNPRINTF
	len = snprintf(buf, strlen(msg) + 3, "%c%s\n", GTALK_ESCAPE, msg);
#else
	len = sprintf(buf, "%c%s\n", GTALK_ESCAPE, msg);
#endif
	write(user->fd, buf, len);
	free_mem(buf);
}
