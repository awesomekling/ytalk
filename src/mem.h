/*
 * src/mem.h
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

#ifdef YTALK_DEBUG
typedef struct __mem_list {
	yaddr addr;
	int size;
	char *file;
	int line;
	struct __mem_list *prev;
	struct __mem_list *next;
} mem_list;

#define get_mem(size) (real_get_mem((size), __LINE__, __FILE__))
#define realloc_mem(addr, size) (real_realloc_mem((addr), (size), __LINE__, __FILE__))
#define free_mem(addr) (real_free_mem((addr), __LINE__, __FILE__))

extern yaddr real_get_mem(int, int, char *);
extern yaddr real_realloc_mem(yaddr, int, int, char *);
extern void real_free_mem(yaddr, int, char *);
extern void change_area(mem_list *, yaddr, yaddr, int);
extern int get_size(mem_list *, yaddr);
extern void clear_all(void);
extern mem_list *add_area(mem_list *, yaddr, int, int, char *);
extern mem_list *del_area(mem_list *, mem_list *);
#else

#define free_mem(addr) (free(addr))

extern yaddr get_mem(int);
extern yaddr realloc_mem(yaddr, int);

#endif /* YTALK_DEBUG */
