/* mem.c */

#include "header.h"
#include "mem.h"

#ifdef YTALK_DEBUG
/* some statistical accumulators */
unsigned int bad_free = 0;
unsigned int bad_realloc = 0;
unsigned int realloc_null = 0;
unsigned int leaked = 0;
#endif

static mem_list *glist = NULL;

/* Add to linked list
 */
mem_list *
add_area(list, addr, size, line, file)
  mem_list *list;
  yaddr addr;
  int size, line;
  char *file;
{
    mem_list *entry;
    if(addr == 0)
	return list;
    entry = (mem_list *)malloc(sizeof(mem_list));
    entry->addr = addr;
    entry->size = size;
    entry->line = line;
    entry->file = file;
    entry->next = list;
    return entry;
}

/* Delete from linked list
 */
mem_list *
del_area(list, addr)
  mem_list *list;
  yaddr addr;
{
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
void
change_area(list, addr, new_addr, size)
  mem_list *list;
  yaddr addr, new_addr;
  int size;
{
    mem_list *it = list;
    while(it != NULL) {
	if(it->addr == addr) {
	    it->addr = new_addr;
	    it->size = size;
	    return;
	}
	it = it->next;
    }
#ifdef YTALK_DEBUG
    show_error("Reallocation failed: Not in allocation list");
    bad_realloc++;
#endif
}

/* Size to clear where the pointer points
 */
int
get_size(list, addr)
  mem_list *list;
  yaddr addr;
{
    mem_list *it = list;
    int size = -1;
    while(it != NULL) {
	if(it->addr == addr) {
	    size = it->size;
	    break;
	}
	it = it->next;
    }
    return size;
}

/* Allocate memory.
 */
yaddr
real_get_mem(n, line, file)
  int n, line;
  char *file;
{
    yaddr out;
    if((out = (yaddr)malloc((size_t)n)) == NULL) {
	show_error("malloc() failed");
	bail(YTE_NO_MEM);
    }
    glist = add_area(glist, out, n, line, file);
    return out;
}

/* Free and clear memory
 */
void
free_mem(addr)
  yaddr addr;
{
    int size;
    if((size = get_size(glist, addr)) > 0) {
	(void)memset(addr, '\0', (size_t)size);
	free(addr);
	glist = del_area(glist, addr);
    } else {
#ifdef YTALK_DEBUG
	show_error("Free failed: Not in allocation list");
	bad_free++;
#endif
    }
}

/* Reallocate memory.
 */
yaddr
realloc_mem(p, n)
  yaddr p;
  int n;
{
    yaddr out;
    if(p == NULL) {
#ifdef YTALK_DEBUG
	realloc_null++;
#endif
	return get_mem(n);
    }
    if((out = (yaddr)realloc(p, (size_t)n)) == NULL) {
	show_error("realloc() failed");
	bail(YTE_NO_MEM);
    }
    change_area(glist, p, out, n);
    return out;
}

/* Clear all memory
 */
void
clear_all()
{
#ifdef YTALK_DEBUG
    printf("Clearing memory\n");
#endif
    while(glist != NULL) {
#ifdef YTALK_DEBUG
	printf("0x%lx: %d\t(%s:%d)\n", glist->addr, glist->size,
				       glist->file, glist->line
	);
#endif
	free_mem(glist->addr);
#ifdef YTALK_DEBUG
	leaked++;
#endif
    }
#ifdef YTALK_DEBUG
    printf("Statistics:\n");
    printf("Bad free_mem() calls:    %u\n", bad_free);
    printf("Bad realloc_mem() calls: %u\n", bad_realloc);
    printf("realloc_mem(NULL) calls: %u\n", realloc_null);
    printf("Leaked allocations:      %u\n", leaked);
#endif
}
