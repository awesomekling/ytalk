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

#define IS_WHITE(c)	((c)==' ' || (c)=='\t' || (c)=='\n' || (c)=='=')

extern char *vhost;

#ifdef YTALK_COLOR
extern int newui_colors;
extern int newui_attr;
#endif

static struct alias *alias0 = NULL;

typedef struct {
	char *option;
	ylong flag;
} options;

#ifdef YTALK_COLOR
typedef struct {
	char *color;
	short value;
} colors;
#endif

options opts[] = {
	{"scrolling",		FL_SCROLL	},
	{"wordwrap",		FL_WRAP		},
	{"auto_import",		FL_IMPORT	},
	{"auto_invite",		FL_INVITE	},
	{"rering",		FL_RING		},
	{"prompt_rering",	FL_PROMPTRING	},
	{"beeps",		FL_BEEP		},
	{"ignore_break",	FL_IGNBRK	},
	{"newui",		FL_NEWUI	},
	{"require_caps",	FL_CAPS		},
	{"no_invite", 		FL_NOAUTO	},
	{"prompt_quit",		FL_PROMPTQUIT	},
	{(char *)NULL,		0		}
};

#ifdef YTALK_COLOR
colors cols[] = {
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
get_word(p)
	char **p;
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
getcolor(color, rc, ra)
	char *color;
	int *rc, *ra;
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

	while(cols[i].color != NULL && !found) {
		if(strcmp(c, cols[i].color) == 0) {
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
setcolors(char *bg, char *fg, int *ucolors, int *fgattr)
{
	int bgi, fgi;

	if(fg == NULL || bg == NULL)
		return 1;

	if(!getcolor(bg, &bgi, NULL))
		return 2;
	if(!getcolor(fg, &fgi, fgattr))
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
get_bool(value)
	char *value;
{
	char ok[8][4] = { "on", "ON", "On", "1", "y", "yes", "YES", "Yes" };
	char no[8][4] = { "off", "OFF", "Off", "0", "n", "no", "NO", "No" };
	int i;
	for(i = 0; i < 8; i++) {
		if(strcmp(ok[i], value) == 0) {
			return 1;
		} else if(strcmp(no[i], value) == 0) {
			return 0;
		}
	}
	return -1;
}

static int
new_alias(a1, a2)
	char *a1, *a2;
{
	struct alias *a;
	char *at;

	/* if a1 is null a2 is too, and we need both */
	if(a2 == NULL)
		return 0;

	a = get_mem(sizeof(struct alias));
	at = strchr(a1, '@');
	if (at == a1) {
		a->type = ALIAS_AFTER;
		(void) strncpy(a->from, a1 + 1, 255);
		a->from[254] = 0;
		(void) strncpy(a->to, (*a2 == '@' ? a2 + 1 : a2), 255);
		a->to[254] = 0;
	} else if (at == a1 + strlen(a1) - 1) {
		a->type = ALIAS_BEFORE;
		*at = 0;
		(void) strncpy(a->from, a1, 255);
		a->from[254] = 0;
		(void) strncpy(a->to, a2, 255);
		a->to[254] = 0;
		if ((at = strchr(a->to, '@')) != NULL)
			*at = 0;
	} else {
		a->type = ALIAS_ALL;
		(void) strncpy(a->from, a1, 255);
		a->from[254] = 0;
		(void) strncpy(a->to, a2, 255);
		a->to[254] = 0;
	}
	a->next = alias0;
	alias0 = a;
	return 1; /* success */
}

static void
read_rcfile(fname)
	char *fname;
{
	FILE *fp;
	char buf[BUFSIZ];
	char *ptr, *cmd, *from, *to, *on, *bg, *fg;
	char *host;
	int i, line, found;

	if ((fp = fopen(fname, "r")) == NULL)
		return;
	
	line = 0;
	while(fgets(buf, BUFSIZ, fp)) {
		ptr = buf;
		found = 0;
		i = 0;
		line++;

		if(*ptr == '#')
			continue;

		cmd = get_word(&ptr);

		if(cmd == NULL)
			continue;

		while(opts[i].option != NULL) {
			if (strcmp(cmd, opts[i].option) == 0) {
				found = 1;
				switch(get_bool(get_word(&ptr))) {
				case 1:
					def_flags |= opts[i].flag;
					break;
				case -1:
					fprintf(stderr, "Invalid bool option on line %d in %s\n", line, fname);
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
			if(strcmp(cmd, "alias") == 0) {
				found = 1;
				from = get_word(&ptr);
				to   = get_word(&ptr);
				if(!new_alias(from, to)) {
					fprintf(stderr, "Not enough parameters for alias on line %d in %s\n", line, fname);
					bail(YTE_INIT);
				}
			} else if(strcmp(cmd, "ui_colors") == 0) {
				bg = get_word(&ptr);
				fg = get_word(&ptr);
				switch(setcolors(bg, fg, &newui_colors, &newui_attr)) {
				case 0:
					found = 1;
					break;
				case 1:
					fprintf(stderr, "You must specify both foreground and background on line %d in %s\n", line, fname);
					bail(YTE_INIT);
					break;
				case 2:
					fprintf(stderr, "Foreground color on line %d (%s) is invalid\n", line, fname);
					bail(YTE_INIT);
					break;
				case 3:
					fprintf(stderr, "Background color on line %d (%s) is invalid\n", line, fname);
					bail(YTE_INIT);
					break;
				}
			} else if(strcmp(cmd, "readdress") == 0) {
				from = get_word(&ptr);
				to   = get_word(&ptr);
				on   = get_word(&ptr);
				switch(readdress_host(from, to, on)) {
				case 0:
					found = 1;
					break;
				case 1:
					fprintf(stderr, "Can't resolve %s on line %d in %s\n", from, line, fname);
					bail(YTE_INIT);
					break;
				case 2:
					fprintf(stderr, "Can't resolve %s on line %d in %s\n", to, line, fname);
					bail(YTE_INIT);
					break;
				case 3:
					fprintf(stderr, "Can't resolve %s on line %d in %s\n", on, line, fname);
					bail(YTE_INIT);
					break;
				case 4:
					fprintf(stderr, "From and to are the same host on line %d in %s\n", line, fname);
					bail(YTE_INIT);
					break;
				}
			} else if(strcmp(cmd, "localhost") == 0) {
				if(vhost != NULL) {
					fprintf(stderr, "Virtualhost already set before line %d in %s\n", line, fname);
					bail(YTE_INIT);
				}
				host = get_word(&ptr);
				if(host == NULL) {
					fprintf(stderr, "Missing hostname on line %d in %s\n", line, fname);
					bail(YTE_INIT);
				}
				vhost = (char *)get_mem(1 + strlen(host));
				strcpy(vhost, host);
			} else {
				fprintf(stderr, "Unknown rc-directive on line %d in %s\n", line, fname);
				bail(YTE_INIT);
			}
		}
	}

	fclose(fp);
}

/* ---- global functions ---- */

char *
resolve_alias(uh)
	char *uh;
{
	struct alias *a;
	static char uh1[256], *at;
	int found = 0;

	for (a = alias0; a; a = a->next)
		if (a->type == ALIAS_ALL && strcmp(uh, a->from) == 0)
			return a->to;

	(void) strncpy(uh1, uh, 255);
	uh1[254] = 0;
	if ((at = strchr(uh1, '@')) != NULL)
		*at = 0;
	for (a = alias0; a; a = a->next) {
		if (a->type == ALIAS_BEFORE && strcmp(uh1, a->from) == 0) {
			found = 1;
			(void) strncpy(uh1, a->to, 255);
			uh1[254] = 0;
			if ((at = strchr(uh, '@')) != NULL)
				if (strlen(uh1) + strlen(at) < 256)
					(void) strcat(uh1, at);
			uh = uh1;
			break;
		}
	}
	if (!found) {
		(void) strncpy(uh1, uh, 255);
		uh1[254] = 0;
	}
	at = strchr(uh1, '@');
	if (at && at[1]) {
		at++;
		for (a = alias0; a; a = a->next) {
			if (a->type == ALIAS_AFTER && strcmp(at, a->from) == 0) {
				found = 1;
				if (strlen(a->to) + (at - uh1) < 255)
					(void) strcpy(at, a->to);
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

	pw = getpwuid(myuid);
	if (pw != NULL) {
		fname = get_mem((strlen(pw->pw_dir) + 10) * sizeof(char));
		(void) sprintf(fname, "%s/.ytalkrc", pw->pw_dir);
		read_rcfile(fname);
		free_mem(fname);
	}
	/* set all default flags */

	for (u = user_list; u != NULL; u = u->unext)
		if (!(u->flags & FL_LOCKED))
			u->flags = def_flags;
}
