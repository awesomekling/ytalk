/* rc.c -- read the .ytalkrc file */

/*			   NOTICE
 *
 * Copyright (c) 1990,1992,1993 Britt Yenne.  All rights reserved.
 * 
 * This software is provided AS-IS.  The author gives no warranty,
 * real or assumed, and takes no responsibility whatsoever for any 
 * use or misuse of this software, or any damage created by its use
 * or misuse.
 * 
 * This software may be freely copied and distributed provided that
 * no part of this NOTICE is deleted or edited in any manner.
 * 
 */

/* Mail comments or questions to ytalk@austin.eds.com */

#include "header.h"

#define IS_WHITE(c)	((c)==' ' || (c)=='\t' || (c)=='\n')

extern char *vhost;

static struct alias *alias0 = NULL;

/* ---- local functions ---- */

static char *
get_word(p)
  char **p;
{
    register char *c, *out;

    c = *p;
    while(IS_WHITE(*c))
	c++;
    if(*c == '\0')
	return NULL;
    out = c;
    while(*c && !IS_WHITE(*c))
	c++;
    if(*c)
	*(c++) = '\0';
    *p = c;
    return out;
}

static int
set_option(opt, value)
  char *opt, *value;
{
    ylong mask = 0;
    int set_it;

    if(strcmp(value, "true") == 0 || strcmp(value, "on") == 0)
	set_it = 1;
    else if(strcmp(value, "false") == 0 || strcmp(value, "off") == 0)
	set_it = 0;
    else
	return -1;
    
    if(strcmp(opt, "scroll") == 0
    || strcmp(opt, "scrolling") == 0
    || strcmp(opt, "sc") == 0)
	mask |= FL_SCROLL;

    else if(strcmp(opt, "wrap") == 0
    || strcmp(opt, "word-wrap") == 0
    || strcmp(opt, "wordwrap") == 0
    || strcmp(opt, "wrapping") == 0
    || strcmp(opt, "ww") == 0)
	mask |= FL_WRAP;

    else if(strcmp(opt, "import") == 0
    || strcmp(opt, "auto-import") == 0
    || strcmp(opt, "autoimport") == 0
    || strcmp(opt, "importing") == 0
    || strcmp(opt, "aip") == 0
    || strcmp(opt, "ai") == 0)
	mask |= FL_IMPORT;

    else if(strcmp(opt, "invite") == 0
    || strcmp(opt, "auto-invite") == 0
    || strcmp(opt, "autoinvite") == 0
    || strcmp(opt, "aiv") == 0
    || strcmp(opt, "av") == 0)
	mask |= FL_INVITE;

    else if(strcmp(opt, "ring") == 0
    || strcmp(opt, "rering") == 0
    || strcmp(opt, "r") == 0)
	mask |= FL_RING;

    else if(strcmp(opt, "prompt-ring") == 0
    || strcmp(opt, "prompt-rering") == 0
    || strcmp(opt, "promptring") == 0
    || strcmp(opt, "promptrering") == 0
    || strcmp(opt, "pr") == 0)
	mask |= FL_PROMPTRING;

    else if(strcmp(opt, "xwin") == 0
    || strcmp(opt, "xwindows") == 0
    || strcmp(opt, "XWindows") == 0
    || strcmp(opt, "Xwin") == 0
    || strcmp(opt, "x") == 0
    || strcmp(opt, "X") == 0)
	mask |= FL_XWIN;

    else if(strcmp(opt, "asides") == 0
    || strcmp(opt, "aside") == 0
    || strcmp(opt, "as") == 0)
	mask |= FL_ASIDE;
    
    else if(strcmp(opt, "caps") == 0
    || strcmp(opt, "CAPS") == 0
    || strcmp(opt, "ca") == 0
    || strcmp(opt, "CA") == 0)
	mask |= FL_CAPS;

    else if(strcmp(opt, "noinvite") == 0
    || strcmp(opt, "no-invite") == 0
    || strcmp(opt, "noinv") == 0
    || strcmp(opt, "ni") == 0)
	mask |= FL_NOAUTO;

    if(!mask)
	return -1;

    if(set_it)
	def_flags |= mask;
    else
	def_flags &= ~mask;

    return 0;
}

static void
read_rcfile(fname)
  char *fname;
{
    FILE *fp;
    char *buf, *ptr;
    char *w, *arg1, *arg2, *arg3, *at;
    int line_no, errline;
    struct alias *a;

    if((fp = fopen(fname, "r")) == NULL)
    {
	if(errno != ENOENT)
	    show_error(fname);
	return;
    }
    buf = get_mem(BUFSIZ);

    line_no = errline = 0;
    while(fgets(buf, BUFSIZ, fp) != NULL)
    {
	line_no++;
	ptr = buf;
	w = get_word(&ptr);
	if(w == NULL || *w == '#')
	    continue;
	
	if(strcmp(w, "readdress") == 0)
	{
	    arg1 = get_word(&ptr);
	    arg2 = get_word(&ptr);
	    arg3 = get_word(&ptr);
	    if(arg3 == NULL)
	    {
		errline = line_no;
		break;
	    }
	    readdress_host(arg1, arg2, arg3);
	}
	else if(strcmp(w, "set") == 0 || strcmp(w, "turn") == 0)
	{
	    arg1 = get_word(&ptr);
	    arg2 = get_word(&ptr);
	    if(arg2 == NULL)
	    {
		errline = line_no;
		break;
	    }
	    if(set_option(arg1, arg2) < 0)
	    {
		errline = line_no;
		break;
	    }
	}
	else if (strcmp(w, "localhost") == 0)
	{
	    arg1 = get_word(&ptr);
	    if(arg1 == NULL)
	    {
		errline = line_no;
		break;
	    }
	    if (vhost == NULL)
	    {
		vhost = malloc(1+strlen(arg1));
		if (vhost != NULL)
		    strcpy(vhost, arg1);
	    }
	}
	else if (strcmp(w, "alias") == 0)
	{
	    arg1 = get_word(&ptr);
	    arg2 = get_word(&ptr);
	    if(arg2 == NULL)
	    {
		errline = line_no;
		break;
	    }
	    a = get_mem(sizeof (struct alias));
	    at = strchr(arg1, '@');
	    if (at == arg1) {
	      a->type = ALIAS_AFTER;
	      strncpy(a->from, arg1+1, 255);
	      a->from[254] = 0;
	      strncpy(a->to, (*arg2=='@' ? arg2+1 : arg2), 255);
	      a->to[254] = 0;
	    } else if (at == arg1 + strlen(arg1) - 1) {
	      a->type = ALIAS_BEFORE;
	      *at = 0;
	      strncpy(a->from, arg1, 255);
	      a->from[254] = 0;
	      strncpy(a->to, arg2, 255);
	      a->to[254] = 0;
	      if ((at = strchr(a->to, '@')) != NULL)
		*at = 0;
	    } else {
	      a->type = ALIAS_ALL;
	      strncpy(a->from, arg1, 255);
	      a->from[254] = 0;
	      strncpy(a->to, arg2, 255);
	      a->to[254] = 0;
	    }
	    a->next = alias0;
	    alias0 = a;
	}
	else
	{
	    errline = line_no;
	    break;
	}
    }
    if(errline)
    {
	sprintf(errstr, "%s: syntax error at line %d", fname, errline);
	errno = 0;
	show_error(errstr);
    }

    free(buf);
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

  for (a=alias0; a; a=a->next)
    if (a->type == ALIAS_ALL && strcmp(uh, a->from) == 0)
      return a->to;

  strncpy(uh1, uh, 255);
  uh1[254] = 0;
  if ((at = strchr(uh1, '@')) != NULL)
    *at = 0;
  for (a=alias0; a; a=a->next) {
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
    for (a=alias0; a; a=a->next) {
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
    char *w;
    yuser *u;
    char fname[256];

    /* read the system ytalkrc file */

#ifdef SYSTEM_YTALKRC
    read_rcfile(SYSTEM_YTALKRC);
#endif

    /* read the user's ytalkrc file */

    if((w = (char *)getenv("HOME")) != NULL)
    {
	sprintf(fname, "%s/.ytalkrc", w);
	read_rcfile(fname);
    }

    /* set all default flags */

    for(u = user_list; u != NULL; u = u->unext)
	if(!(u->flags & FL_LOCKED))
	    u->flags = def_flags;
}
