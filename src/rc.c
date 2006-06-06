/*
 * src/rc.c
 * runtime configuration parser
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

#include "header.h"
#include "mem.h"
#include "cwin.h"

#include <pwd.h>

#define IS_WHITE(c)	\
	((c)==' '  ||	\
	 (c)=='\t' ||	\
	 (c)=='\n' ||	\
	 (c)=='='  ||	\
	 (c)==','	\
	)

extern char *vhost;

extern unsigned long int ui_colors, ui_attr;
extern unsigned long int menu_colors, menu_attr;

static struct alias *alias0 = NULL;

typedef struct {
	char *option;
	long int flag;
} options;

typedef struct {
	char *color;
	unsigned long int value;
} colors;

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

static char ebuf[MAXERR];

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

static int
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
		pw = getpwuid( getuid() );
		endpwent();
		if (pw != NULL) {
			gshell = (char *) realloc_mem(gshell, strlen(pw->pw_dir) + strlen(shell) + 1);
			shell++;
			snprintf(gshell, strlen(pw->pw_dir) + strlen(shell) + 1, "%s%s", pw->pw_dir, shell);
		} else {
			return 0;
		}
	} else {
		gshell = (char *) realloc_mem(gshell, strlen(shell) + 1);
		snprintf(gshell, strlen(shell) + 1, "%s", shell);
	}
	return 1;
}

static void
read_rcfile(char *fname)
{
	FILE *fp;
	char buf[BUFSIZ];
	char *ptr, *cmd, *from, *to, *tmp, *value;
	int i, line, found;
	char *fg, *bg;

	if ((fp = fopen(fname, "r")) == NULL)
		return;

	errno = 0;
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
				value = get_word(&ptr);
				switch (get_bool(value)) {
				case 1:
					def_flags |= opts[i].flag;
					break;
				case -1:
					snprintf(ebuf, MAXERR, "%s:%d: Invalid bool value '%s'", fname, line, value);
					show_error(ebuf);
					return;
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
					snprintf(ebuf, MAXERR, "%s:%d: Insufficient alias paramaters", fname, line);
					show_error(ebuf);
					return;
				}
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
					snprintf(ebuf, MAXERR, "%s:%d: You must specify both background and foreground colors", fname, line);
					show_error(ebuf);
					return;
				case 2:
					snprintf(ebuf, MAXERR, "%s:%d: Invalid foreground color '%s'", fname, line, fg);
					show_error(ebuf);
					return;
				case 3:
					snprintf(ebuf, MAXERR, "%s:%d: Invalid background color '%s'", fname, line, bg);
					show_error(ebuf);
					return;
				}
			} else if (strcmp(cmd, "localhost") == 0) {
				found = 1;
				if (vhost != NULL) {
					snprintf(ebuf, MAXERR, "%s:%d: Virtual host already set to '%s'", fname, line, vhost);
					show_error(ebuf);
					return;
				}
				tmp = get_word(&ptr);
				if (tmp == NULL) {
					snprintf(ebuf, MAXERR, "%s:%d: Missing hostname", fname, line);
					show_error(ebuf);
					return;
				}
				vhost = (char *) realloc_mem(vhost, 1 + strlen(tmp));
				strcpy(vhost, tmp);
			} else if (strcmp(cmd, "title_format") == 0) {
				found = 1;
				tmp = get_string(&ptr);
				if (!tmp) {
					snprintf(ebuf, MAXERR, "%s:%d: Missing format string for %s", fname, line, cmd);
					show_error(ebuf);
					return;
				}
				title_format = (char *) realloc_mem(title_format, 1 + strlen(tmp));
				strcpy(title_format, tmp);
			} else if (strcmp(cmd, "user_format") == 0) {
				found = 1;
				tmp = get_string(&ptr);
				if (!tmp) {
					snprintf(ebuf, MAXERR, "%s:%d: Missing format string for %s", fname, line, cmd);
					show_error(ebuf);
					return;
				}
				user_format = (char *) realloc_mem(user_format, 1 + strlen(tmp));
				strcpy(user_format, tmp);
			} else if (strcmp(cmd, "shell") == 0) {
				found = 1;
				tmp = get_word(&ptr);
				if (!set_shell(tmp)) {
					snprintf(ebuf, MAXERR, "%s:%d: Shell cannot be empty", fname, line);
					show_error(ebuf);
					return;
				}
			} else if (strcmp(cmd, "history_rows") == 0) {
				found = 1;
				tmp = get_word(&ptr);
				if (tmp != NULL)
					scrollback_lines = strtol(tmp, NULL, 10);
			} else {
				snprintf(ebuf, MAXERR, "%s:%d: Unknown option '%s'", fname, line, cmd);
				show_error(ebuf);
				return;
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
read_ytalkrc()
{
	yuser *u;
	char *fname;
	struct passwd *pw;

	/* read the system ytalkrc file */

#ifdef SYSTEM_YTALKRC
	read_rcfile(SYSTEM_YTALKRC);
#endif

	/* read the user's ytalkrc file */

	pw = getpwuid( getuid() );
	endpwent();
	if (pw != NULL) {
		fname = get_mem((strlen(pw->pw_dir) + 10) * sizeof(char));
		snprintf(fname, strlen(pw->pw_dir) + 10, "%s/.ytalkrc", pw->pw_dir);
		read_rcfile(fname);
		free_mem(fname);
	}

	/* set all default flags */

	for (u = user_list; u != NULL; u = u->unext)
		if (!(u->flags & FL_LOCKED))
			u->flags = def_flags;

}
