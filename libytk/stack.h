/* libytk/stack.h */

#ifndef __YTK_STACK_H__
#define __YTK_STACK_H__

#include "ytk.h"

#define ytk_is_empty_stack(s)		(((s) == NULL) || ((s)->top == NULL))

typedef struct __ytk_stack_item {
	ytk_thing *thing;
	struct __ytk_stack_item *prev;
	struct __ytk_stack_item *next;
} ytk_stack_item;

typedef struct __ytk_stack {
	ytk_stack_item *top;
} ytk_stack;

ytk_stack *ytk_new_stack(void);
void ytk_purge_stack(ytk_stack *);
void ytk_push(ytk_stack *, ytk_thing *);
ytk_thing *ytk_pop(ytk_stack *);
int ytk_handle_stack_input(ytk_stack *, int);
void ytk_winch_stack(ytk_stack *);
int ytk_on_stack(ytk_stack *, ytk_thing *);

#endif /* __YTK_STACK_H__ */
