typedef struct t_mem_list {
	yaddr addr;
	int size;
	char *file;
	int line;
	struct t_mem_list *next;
} mem_list;

#define get_mem(size) (real_get_mem(size, __LINE__, __FILE__))

mem_list *	add_area	( /* mem_list*, yaddr, int, int, char* */ );
mem_list *	del_area	( /* mem_list*, yaddr */ );
yaddr		real_get_mem	( /* int, int, char* */);
void		free_mem	( /* yaddr */ );
yaddr		realloc_mem	( /* yaddr, int */ );
void		change_area	( /* mem_list*, yaddr, yaddr, int */ );
int		get_size	( /* mem_list*, yaddr */ );
void		clear_all	( );
