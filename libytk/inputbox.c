/* ytk/inputbox.c
 *
 *
 */

#include "ytk.h"

ytk_thing *
ytk_new_inputbox(char *title, int size, void (*callback)(ytk_inputbox *))
{
	ytk_thing *new_thing;
	ytk_inputbox *new_inputbox;
	new_thing = ytk_new_thing();
	new_inputbox = get_mem(sizeof(ytk_inputbox));
	new_inputbox->base = new_thing;
	new_thing->title = title;
	new_inputbox->data = get_mem((size + 1) * sizeof(char));
	new_inputbox->buf = get_mem((size + 1) * sizeof(char));
	new_inputbox->data[0] = '\0';
	new_inputbox->buf[0] = '\0';
	new_inputbox->size = size;
	new_inputbox->len = 0;
	new_inputbox->callback = callback;
	new_thing->object = new_inputbox;
	new_thing->type = YTK_T_INPUTBOX;
	return new_thing;
}

void
ytk_destroy_inputbox(ytk_inputbox *b)
{
	free_mem(b->data);
	free_mem(b->buf);
	free_mem(b);
}

void
ytk_handle_inputbox_input(ytk_inputbox *b, int ch)
{
	switch (ch) {
	case '\r':
	case '\n':
		if (b->callback != NULL)
			b->callback((void *) b);
		break;
	case 8:
		if (b->len > 0) {
			b->len--;
			b->data[b->len] = 0;
		}
		break;
	case 21:
		b->data[0] = '\0';
		b->len = 0;
		break;
	case 27:
		if (b->base->escape != NULL)
			b->base->escape(b->base);
		break;
	default:
		if (ch >= 32)
			if (b->len < (b->size)) {
				b->data[b->len++] = ch;
				b->data[b->len] = '\0';
			}
	}
}
