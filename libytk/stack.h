/* ytk/stack.h
 *
 *
 */

#ifndef __YTK_STACK_H__
#define __YTK_STACK_H__

#include "ytk.h"

typedef struct __ytk_stack_item {
	ytk_thing *thing;
	struct __ytk_stack_item *prev;
	struct __ytk_stack_item *next;
} ytk_stack_item;

typedef struct __ytk_stack {
	ytk_stack_item *top;
} ytk_stack;

ytk_stack *ytk_new_stack();
void ytk_delete_stack(ytk_stack *);
void ytk_push_thing(ytk_stack *, ytk_thing *);
ytk_thing *ytk_pop_thing(ytk_stack *);
int ytk_handle_stack_input(ytk_stack *, int);
void ytk_winch_stack(ytk_stack *);

#endif /* __YTK_STACK_H__ */