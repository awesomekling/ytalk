/* mem.c */

#include "header.h"
#include "mem.h"

static mem_list *glist = NULL;

/* Add to linked list
 */
mem_list *add_area(mem_list *list, yaddr addr, int size) {
	mem_list *entry;
	if(addr == 0)
		return list;
	entry = (mem_list *)malloc(sizeof(mem_list));
	entry->addr = addr;
	entry->size = size;
	entry->next = list;
	return entry;
}

/* Delete from linked list
 */
mem_list *del_area(mem_list *list, yaddr addr) {
	mem_list *it = list, *backup = list;
	if(it->addr == addr) {
		list = it->next;
		free(it);
		return list;
	}
	while(it != NULL) {
		if(it->addr == addr)
			break;
		backup = it;
		it = it->next;
	}
	backup->next = it->next;
	free(it);
	return list;
}
	
/* Change stored size
 */
void change_area(mem_list *list, yaddr addr, yaddr new_addr, int size) {
	mem_list *it = list;
	while(it != NULL) {
		if(it->addr == addr) {
			it->addr = new_addr;
			it->size = size;
			return;
		}
		it = it->next;
	}
	show_error("Realloc mem failed: Not in list");
}

/* Size to clear where the pointer points
 */
int get_size(mem_list *list, yaddr addr) {
	mem_list *it = list;
	while(it && it->addr != addr) {
		it = it->next;
	}
	return it->size;
}

/* Allocate memory.
 */
yaddr get_mem(int n) {
	yaddr out;
	if((out = (yaddr)malloc(n)) == NULL) {
		show_error("malloc() failed");
		bail(YTE_NO_MEM);
	}
	glist = add_area(glist, out, n);
	return out;
}

/* Free and clear memory
 */
void free_mem(yaddr addr) {
	memset(addr, '\0', get_size(glist, addr));
	free(addr);
	glist = del_area(glist, addr);
}

/* Reallocate memory.
 */
yaddr realloc_mem(yaddr p, int n) {
	yaddr out;
	if(p == NULL)
		return get_mem(n);
	if((out = (yaddr)realloc(p, n)) == NULL) {
		show_error("realloc() failed");
		bail(YTE_NO_MEM);
	}
	change_area(glist, p, out, n);
	return out;
}

/* Clear all memory
 */
void clear_all(mem_list *list) {
#ifdef YTALK_DEBUG
	printf("Clearing memory\n");
#endif
	while(list != NULL) {
#ifdef YTALK_DEBUG
		printf("%d: %d\n", list->addr, list->size);
#endif
		free_mem(list->addr);
		list = list->next;
	}
}
