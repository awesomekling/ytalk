/*
 * src/mem.c
 * memory function wrappers and leak tracking
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

#ifdef YTALK_DEBUG
/* some statistical accumulators */
unsigned int bad_free = 0;
unsigned int bad_realloc = 0;
unsigned int realloc_null = 0;
unsigned int leaked = 0;

static mem_list *glist = NULL;

/*
 * Add to linked list
 */
mem_list *
add_area(mem_list *list, yaddr addr, int size, int line, char *file)
{
	mem_list *entry;
	if (!addr)
		return list;
	entry = malloc(sizeof(mem_list));
	entry->addr = addr;
	entry->size = size;
	entry->line = line;
	entry->file = file;
	entry->prev = NULL;
	if (list)
		list->prev = entry;
	entry->next = list;
	return entry;
}

/*
 * Delete from linked list. There are many -> in this funktion.. Try to stay concentrated.
 */
mem_list *
del_area(mem_list *list, mem_list *entry)
{
	if (list == entry)
		list = entry->next;

	if (!entry->next && !entry->prev) {
		free(entry);
		return NULL;
	}

	if (entry->prev)
		entry->prev->next = entry->next;
	else
		entry->next->prev = NULL;

	if (entry->next)
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
change_area(mem_list *list, yaddr addr, yaddr new_addr, int size)
{
	mem_list *it = list;
	while (it) {
		if (it->addr == addr) {
			it->addr = new_addr;
			it->size = size;
			return;
		}
		it = it->next;
	}
	show_error("Reallocation failed: Not in allocation list");
	bad_realloc++;
}

/*
 * Size to clear where the pointer points
 */
int
get_size(mem_list *list, yaddr addr)
{
	mem_list *it = list;
	int size = -1;
	while (it) {
		if (it->addr == addr) {
			size = it->size;
			break;
		}
		it = it->next;
	}
	return size;
}
#endif /* YTALK_DEBUG */

/*
 * Allocate memory.
 */
yaddr
#ifdef YTALK_DEBUG
real_get_mem(int n, int line, char *file)
#else
get_mem(int n)
#endif
{
	yaddr out;
	out = malloc((size_t) n);
	if (!out) {
		show_error("malloc() failed");
		bail(YTE_NO_MEM);
	}
#ifdef YTALK_DEBUG
	glist = add_area(glist, out, n, line, file);
#endif
	return out;
}

/*
 * Free and clear memory
 */
#ifdef YTALK_DEBUG
void
real_free_mem(yaddr addr, int line, char *file)
{
	mem_list *entry = glist;
	for (entry = glist; entry; entry = entry->next) {
		if (entry->addr == addr)
			break;
	}
	if (entry) {
		memset(entry->addr, '\0', (size_t) entry->size);
		free(entry->addr);
		glist = del_area(glist, entry);
	} else {
#  ifdef HAVE_SNPRINTF
		snprintf(errstr, MAXERR, "Free failed: Not in allocation list: 0x%lx (%s:%d)", (long unsigned) addr, file, line);
#  else
		sprintf(errstr, "Free failed: Not in allocation list: 0x%lx (%s:%d)", (long unsigned) addr, file, line);
#  endif
		show_error(errstr);
		bad_free++;
	}
}
#endif /* YTALK_DEBUG */

/*
 * Reallocate memory.
 */
yaddr
#ifdef YTALK_DEBUG
real_realloc_mem(yaddr p, int n, int line, char *file)
#else
realloc_mem(yaddr p, int n)
#endif /* YTALK_DEBUG */
{
	yaddr out;
	if (!p) {
#ifdef YTALK_DEBUG
		realloc_null++;
		return real_get_mem(n, line, file);
#else
		return get_mem(n);
#endif
	}
	out = realloc(p, (size_t) n);
	if (!out) {
		show_error("realloc() failed");
		bail(YTE_NO_MEM);
	}
#ifdef YTALK_DEBUG
	change_area(glist, p, out, n);
#endif
	return out;
}

#ifdef YTALK_DEBUG
/*
 * Clear all memory
 */
void
clear_all()
{
	mem_list *it;
	fprintf(stderr, "Freeing memory\n");
	while (glist) {
		fprintf(stderr, "0x%lx: %d\t(%s:%d)\n", (long unsigned) glist->addr, glist->size,
			glist->file, glist->line
		 );
		/* The next 5 rows are a simpler and faster version of free_mem() */
		it = glist;
		glist = glist->next;
		memset(it->addr, '\0', (size_t) it->size);
		free(it->addr);
		free(it);
		leaked++;
	}
	fprintf(stderr, "Statistics:\n");
	fprintf(stderr, "Bad free_mem() calls:    %u\n", bad_free);
	fprintf(stderr, "Bad realloc_mem() calls: %u\n", bad_realloc);
	fprintf(stderr, "realloc_mem(NULL) calls: %u\n", realloc_null);
	fprintf(stderr, "Leaked allocations:      %u\n", leaked);
}
#endif /* YTALK_DEBUG */
