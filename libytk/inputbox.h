/* libytk/inputbox.h */

#ifndef __YTK_INPUTBOX_H__
#define __YTK_INPUTBOX_H__

#include "ytk.h"

typedef struct __ytk_inputbox {
	ytk_thing *base;
	char *data;
	char *buf;
	unsigned int size;
	unsigned int len;
	void (*callback) (struct __ytk_inputbox *);
} ytk_inputbox;

ytk_thing *ytk_new_inputbox(char *, int, void (*callback) (ytk_inputbox *));
void ytk_destroy_inputbox(ytk_inputbox *);
void ytk_handle_inputbox_input(ytk_inputbox *, char);

#endif /* __YTK_INPUTBOX_H__ */
