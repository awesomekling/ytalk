/* rc.c -- read the .ytalkrc file */

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


#include "header.h"
#include "mem.h"
#include <pwd.h>
#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#include "cwin.h"

#define YTRC_ERROR(a)	fprintf(stderr, "(%s:%d) %s\n", fname, line, a)

#define IS_WHITE(c)	\
	((c)==' '  ||	\
	 (c)=='\t' ||	\
	 (c)=='\n' ||	\
	 (c)=='='  ||	\
	 (c)==','	\
	)

extern char *vhost;
extern char *gshell;

#ifdef YTALK_COLOR
extern unsigned long int ui_colors, ui_attr;
extern unsigned long int menu_colors, menu_attr;
#endif

static struct alias *alias0 = NULL;

typedef struct {
	char *option;
	long int flag;
} options;

#ifdef YTALK_COLOR
typedef struct {
	char *color;
	unsigned long int value;
} colors;
#endif

static options opts[] = {
	{"scrolling",		FL_SCROLL	},
	{"wordwrap",		FL_WRAP		},
	{"auto_import",		FL_IMPORT	},
	{"auto_invite",		FL_INVITE	},
	{"rering",		FL_RING		},
	{"prompt_rering",	FL_PROMPTRING	},
	{"beeps",		FL_BEEP		},
	{"ignore_break",	FL_IGNBRK	},
	{"require_caps",	FL_CAPS		},
	{"no_invite",		FL_NOAUTO	},
	{"prompt_quit",		FL_PROMPTQUIT	},
	{(char *)NULL,		0		}
};

#ifdef YTALK_COLOR
static colors cols[] = {
	{"black",		COLOR_BLACK	},
	{"red",			COLOR_RED	},
	{"green",		COLOR_GREEN	},
	{"yellow",		COLOR_YELLOW	},
	{"blue",		COLOR_BLUE	},
	{"magenta",		COLOR_MAGENTA	},
	{"cyan",		COLOR_CYAN	},
	{"white",		COLOR_WHITE	},
	{(char *)NULL,		0		}
};
#endif

/* ---- local functions ---- */

static char *
get_string(char **p)
{
	register char *c, *out;

	c = *p;
	while (IS_WHITE(*c))
		c++;
	if (*c == '\0')
		return NULL;
	out = c;
	while (*c != '\0') {
		if (*c == '\n') {
			*c = '\0';
			break;
		}
		c++;
	}
	*p = c;
	return out;
}

static char *
get_word(char **p)
{
	register char *c, *out;

	c = *p;
	while (IS_WHITE(*c))
		c++;
	if (*c == '\0')
		return NULL;
	out = c;
	while (*c && !IS_WHITE(*c))
		c++;
	if (*c)
		*(c++) = '\0';
	*p = c;
	return out;
}

#ifdef YTALK_COLOR
int
getcolor(char *color, long unsigned int *rc, long unsigned int *ra)
{
	int i = 0, found = 0;
	char *c = color;

	if (ra != NULL)
		*ra = 0;

	if ((strncmp(c, "bold-", 5) == 0) && strlen(c) > 5) {
		c += 5;
		if (ra != NULL)
			*ra = A_BOLD;
	}

	while (cols[i].color != NULL && !found) {
		if (strcmp(c, cols[i].color) == 0) {
			found = 1;
			*rc = cols[i].value;
		}
		i++;
	}

	return found;
}

/*
 * Sets colorvalues and returns a value from 0 to 3 depending on the status.
 * 0: success
 * 1: not enough params
 * 2: foreground color failed
 * 3: background color failed
 */
static int
setcolors(char *bg, char *fg, long unsigned int *ucolors, long unsigned int *fgattr)
{
	long unsigned int bgi, fgi;

	if (fg == NULL || bg == NULL)
		return 1;

	if (!getcolor(bg, &bgi, NULL))
		return 2;
	if (!getcolor(fg, &fgi, fgattr))
		return 3;

	*ucolors = bgi * 8 + fgi + 1;

	return 0;
}
#endif				/* YTALK_COLOR */

/*
 * Returns 1 for on,y,yes or 1 and 0 for off, n, no or 0.
 * else return -1
 */
static int
get_bool(char *value)
{
	char ok[8][4] = { "on", "ON", "On", "1", "y", "yes", "YES", "Yes" };
	char no[8][4] = { "off", "OFF", "Off", "0", "n", "no", "NO", "No" };
	int i;
	for (i = 0; i < 8; i++) {
		if (strcmp(ok[i], value) == 0) {
			return 1;
		} else if (strcmp(no[i], value) == 0) {
			return 0;
		}
	}
	return -1;
}

static int
new_alias(char *a1, char *a2)
{
	struct alias *a;
	char *at;

	/* if a1 is null a2 is too, and we need both */
	if (a2 == NULL)
		return 0;

	a = get_mem(sizeof(struct alias));
	at = strchr(a1, '@');
	if (at == a1) {
		a->type = ALIAS_AFTER;
		strncpy(a->from, a1 + 1, 255);
		a->from[254] = 0;
		strncpy(a->to, (*a2 == '@' ? a2 + 1 : a2), 255);
		a->to[254] = 0;
	} else if (at == a1 + strlen(a1) - 1) {
		a->type = ALIAS_BEFORE;
		*at = 0;
		strncpy(a->from, a1, 255);
		a->from[254] = 0;
		strncpy(a->to, a2, 255);
		a->to[254] = 0;
		if ((at = strchr(a->to, '@')) != NULL)
			*at = 0;
	} else {
		a->type = ALIAS_ALL;
		strncpy(a->from, a1, 255);
		a->from[254] = 0;
		strncpy(a->to, a2, 255);
		a->to[254] = 0;
	}
	a->next = alias0;
	alias0 = a;
	return 1; /* success */
}

static int
set_shell(char *shell)
{
	struct passwd *pw;

	if (shell == NULL)
		return 0;

	if (*shell == '~') {
		pw = getpwuid(myuid);
		if (pw != NULL) {
			gshell = (char *) get_mem(strlen(pw->pw_dir) + strlen(shell) + 1);
			shell++;
#ifdef HAVE_SNPRINTF
			snprintf(gshell, strlen(pw->pw_dir) + strlen(shell) + 1, "%s%s", pw->pw_dir, shell);
#else
			sprintf(gshell, "%s%s", pw->pw_dir, shell);
#endif
		} else {
			return 0;
		}
	} else {
		gshell = (char *) get_mem(strlen(shell) + 1);
#ifdef HAVE_SNPRINTF
		snprintf(gshell, strlen(shell) + 1, "%s", shell);
#else
		sprintf(gshell, "%s", shell);
#endif
	}
	return 1;
}

static void
read_rcfile(char *fname)
{
	FILE *fp;
	char buf[BUFSIZ];
	char *ptr, *cmd, *from, *to, *on, *tmp;
	int i, line, found;
#ifdef YTALK_COLOR
	char *fg, *bg;
#endif

	if ((fp = fopen(fname, "r")) == NULL)
		return;

	line = 0;
	while (fgets(buf, BUFSIZ, fp)) {
		ptr = buf;
		found = 0;
		i = 0;
		line++;

		if (*ptr == '#')
			continue;

		cmd = get_word(&ptr);

		if (cmd == NULL)
			continue;

		while (opts[i].option != NULL) {
			if (strcmp(cmd, opts[i].option) == 0) {
				found = 1;
				switch (get_bool(get_word(&ptr))) {
				case 1:
					def_flags |= opts[i].flag;
					break;
				case -1:
					YTRC_ERROR(_("Invalid bool option."));
					bail(YTE_INIT);
					break;
				case 0:
					def_flags &= ~opts[i].flag;
					break;
				}
			}
			i++;
		}
		if (!found) {
			if (strcmp(cmd, "alias") == 0) {
				found = 1;
				from = get_word(&ptr);
				to   = get_word(&ptr);
				if (!new_alias(from, to)) {
					YTRC_ERROR(_("Not enough parameters for alias."));
					bail(YTE_INIT);
				}
#ifdef YTALK_COLOR
			} else if (strcmp(cmd, "menu_colors") == 0) {
				bg = get_word(&ptr);
				fg = get_word(&ptr);
				if (setcolors(bg, fg, &menu_colors, &menu_attr) == 0) {
					found = 1;
				}
			} else if (strcmp(cmd, "ui_colors") == 0) {
				bg = get_word(&ptr);
				fg = get_word(&ptr);
				switch (setcolors(bg, fg, &ui_colors, &ui_attr)) {
				case 0:
					found = 1;
					break;
				case 1:
					YTRC_ERROR(_("You must specify both foreground and background."));
					bail(YTE_INIT);
					break;
				case 2:
					YTRC_ERROR(_("Invalid foreground color."));
					bail(YTE_INIT);
					break;
				case 3:
					YTRC_ERROR(_("Invalid background color."));
					bail(YTE_INIT);
					break;
				}
#endif /* YTALK_COLOR */
			} else if (strcmp(cmd, "readdress") == 0) {
				found = 1;
				from = get_word(&ptr);
				to   = get_word(&ptr);
				on   = get_word(&ptr);
				switch (readdress_host(from, to, on)) {
				case 0:
					found = 1;
					break;
				case 1:
					YTRC_ERROR(_("Cannot resolve \"from\" address."));
					bail(YTE_INIT);
					break;
				case 2:
					YTRC_ERROR(_("Cannot resolve \"to\" address."));
					bail(YTE_INIT);
					break;
				case 3:
					YTRC_ERROR(_("Cannot resolve \"on\" address."));
					bail(YTE_INIT);
					break;
				case 4:
					YTRC_ERROR(_("From and to are the same host."));
					bail(YTE_INIT);
					break;
				}
			} else if (strcmp(cmd, "localhost") == 0) {
				found = 1;
				if (vhost != NULL) {
					YTRC_ERROR(_("Virtual host already set."));
					bail(YTE_INIT);
				}
				tmp = get_word(&ptr);
				if (tmp == NULL) {
					YTRC_ERROR(_("Missing hostname."));
					bail(YTE_INIT);
				}
				vhost = (char *) get_mem(1 + strlen(tmp));
				strcpy(vhost, tmp);
			} else if (strcmp(cmd, "title_format") == 0) {
				found = 1;
				tmp = get_string(&ptr);
				title_format = (char *) get_mem(1 + strlen(tmp));
				strcpy(title_format, tmp);
			} else if (strcmp(cmd, "user_format") == 0) {
				found = 1;
				tmp = get_string(&ptr);
				user_format = (char *) get_mem(1 + strlen(tmp));
				strcpy(user_format, tmp);
			} else if (strcmp(cmd, "shell") == 0) {
				found = 1;
				tmp = get_word(&ptr);
				if (!set_shell(tmp)) {
					YTRC_ERROR(_("Shell cannot be empty."));
				}
			} else if (strcmp(cmd, "history_rows") == 0) {
				found = 1;
				tmp = get_word(&ptr);
				if (tmp != NULL)
					scrollback_lines = strtol(tmp, NULL, 10);
			} else {
				YTRC_ERROR(_("Unknown option."));
				bail(YTE_INIT);
			}
		}
	}

	fclose(fp);
}

/* ---- global functions ---- */

char *
resolve_alias(char *uh)
{
	struct alias *a;
	static char uh1[256], *at;
	int found = 0;

	for (a = alias0; a; a = a->next)
		if (a->type == ALIAS_ALL && strcmp(uh, a->from) == 0)
			return a->to;

	strncpy(uh1, uh, 255);
	uh1[254] = 0;
	if ((at = strchr(uh1, '@')) != NULL)
		*at = 0;
	for (a = alias0; a; a = a->next) {
		if (a->type == ALIAS_BEFORE && strcmp(uh1, a->from) == 0) {
			found = 1;
			strncpy(uh1, a->to, 255);
			uh1[254] = 0;
			if ((at = strchr(uh, '@')) != NULL)
				if (strlen(uh1) + strlen(at) < 256)
					strcat(uh1, at);
			uh = uh1;
			break;
		}
	}
	if (!found) {
		strncpy(uh1, uh, 255);
		uh1[254] = 0;
	}
	at = strchr(uh1, '@');
	if (at && at[1]) {
		at++;
		for (a = alias0; a; a = a->next) {
			if (a->type == ALIAS_AFTER && strcmp(at, a->from) == 0) {
				found = 1;
				if (strlen(a->to) + (at - uh1) < 255)
					strcpy(at, a->to);
				break;
			}
		}
	}
	if (found)
		return uh1;
	else
		return uh;
}

void
read_ytalkrc(void)
{
	yuser *u;
	char *fname;
	struct passwd *pw;

	/* read the system ytalkrc file */

#ifdef SYSTEM_YTALKRC
	read_rcfile(SYSTEM_YTALKRC);
#endif

	/* read the user's ytalkrc file */

	pw = getpwuid(myuid);
	if (pw != NULL) {
		fname = get_mem((strlen(pw->pw_dir) + 10) * sizeof(char));
#ifdef HAVE_SNPRINTF
		snprintf(fname, strlen(pw->pw_dir) + 10, "%s/.ytalkrc", pw->pw_dir);
#else
		sprintf(fname, "%s/.ytalkrc", pw->pw_dir);
#endif
		read_rcfile(fname);
		free_mem(fname);
	}

	/* set all default flags */

	for (u = user_list; u != NULL; u = u->unext)
		if (!(u->flags & FL_LOCKED))
			u->flags = def_flags;
}
