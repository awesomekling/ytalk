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

/*
 * Add to linked list
 */
mem_list *
#ifdef YTALK_DEBUG
add_area(list, addr, size, line, file)
	int line;
	char *file;
#else
add_area(list, addr, size)
#endif
	mem_list *list;
	yaddr addr;
	int size;
{
	mem_list *entry;
	if (addr == 0)
		return list;
	entry = (mem_list *) malloc(sizeof(mem_list));
	entry->addr = addr;
	entry->size = size;
#ifdef YTALK_DEBUG
	entry->line = line;
	entry->file = file;
#endif
	entry->prev = NULL;
	if(list != NULL)
		list->prev = entry;
	entry->next = list;
	return entry;
}

mem_list *
find_area(list, addr)
	mem_list *list;
	yaddr addr;
{
	mem_list *it = list;
	while(it != NULL) {
		if(it->addr == addr)
			break;
		it = it->next;
	}
	return it;
}

/*
 * Delete from linked list. There are many -> in this funktion.. Try to stay concentrated.
 */
mem_list *
del_area(list, entry)
	mem_list *list;
	mem_list *entry;
{
	if(list == entry)
		list = entry->next;

	if(entry->next == NULL && entry->prev == NULL) {
		free(entry);
		return NULL;
	}

	if(entry->prev != NULL)
		entry->prev->next = entry->next;
	else
		entry->next->prev = NULL;

	if(entry->next != NULL)
		entry->next->prev = entry->prev;
	else
		entry->prev->next = NULL;

	free(entry);

	return list;
}

/*
 * Change stored size
 */
void
change_area(list, addr, new_addr, size)
	mem_list *list;
	yaddr addr, new_addr;
	int size;
{
	mem_list *it = list;
	while (it != NULL) {
		if (it->addr == addr) {
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

/*
 * Size to clear where the pointer points
 */
int
get_size(list, addr)
	mem_list *list;
	yaddr addr;
{
	mem_list *it = list;
	int size = -1;
	while (it != NULL) {
		if (it->addr == addr) {
			size = it->size;
			break;
		}
		it = it->next;
	}
	return size;
}

/*
 * Allocate memory.
 */
yaddr
#ifdef YTALK_DEBUG
real_get_mem(n, line, file)
	int line;
	char *file;
#else
get_mem(n)
#endif
	int n;
{
	yaddr out;
	if ((out = (yaddr) malloc((size_t) n)) == NULL) {
		show_error("malloc() failed");
		bail(YTE_NO_MEM);
	}
#ifdef YTALK_DEBUG
	glist = add_area(glist, out, n, line, file);
#else
	glist = add_area(glist, out, n);
#endif
	return out;
}

/*
 * Free and clear memory
 */
void
#ifdef YTALK_DEBUG
real_free_mem(addr, line, file)
	int line;
	char *file;
#else
free_mem(addr)
#endif
	yaddr addr;
{
	mem_list *entry;
	if((entry = find_area(glist, addr)) != NULL) {
		memset(entry->addr, '\0', (size_t) entry->size);
		free(entry->addr);
		glist = del_area(glist, entry);
	} else {
#ifdef YTALK_DEBUG
#ifdef HAVE_SNPRINTF
		snprintf(errstr, MAXERR, "Free failed: Not in allocation list: 0x%lx (%s:%d)", (long unsigned) addr, file, line);
#else
		sprintf(errstr, "Free failed: Not in allocation list: 0x%lx (%s:%d)", (long unsigned) addr, file, line);
#endif
		show_error(errstr);
		bad_free++;
#endif
	}
}

/*
 * Reallocate memory.
 */
yaddr
realloc_mem(p, n)
	yaddr p;
	int n;
{
	yaddr out;
	if (p == NULL) {
#ifdef YTALK_DEBUG
		realloc_null++;
#endif
		return get_mem(n);
	}
	if ((out = (yaddr) realloc(p, (size_t) n)) == NULL) {
		show_error("realloc() failed");
		bail(YTE_NO_MEM);
	}
	change_area(glist, p, out, n);
	return out;
}

/*
 * Clear all memory
 */
void
clear_all()
{
	mem_list *it;
#ifdef YTALK_DEBUG
	fprintf(stderr, "Clearing memory\n");
#endif
	while (glist != NULL) {
#ifdef YTALK_DEBUG
		fprintf(stderr, "0x%lx: %d\t(%s:%d)\n", (long unsigned) glist->addr, glist->size,
			glist->file, glist->line
		 );
#endif
		/* The next 5 rows are a simpler and faster version of free_mem() */
		it = glist;
		glist = glist->next;
		memset(it->addr, '\0', (size_t) it->size);
		free(it->addr);
		free(it);
#ifdef YTALK_DEBUG
		leaked++;
#endif
	}
#ifdef YTALK_DEBUG
	fprintf(stderr, "Statistics:\n");
	fprintf(stderr, "Bad free_mem() calls:    %u\n", bad_free);
	fprintf(stderr, "Bad realloc_mem() calls: %u\n", bad_realloc);
	fprintf(stderr, "realloc_mem(NULL) calls: %u\n", realloc_null);
	fprintf(stderr, "Leaked allocations:      %u\n", leaked);
#endif
}
