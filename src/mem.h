typedef struct t_mem_list {
	yaddr addr;
	int size;
#ifdef YTALK_DEBUG
	char *file;
	int line;
#endif
	struct t_mem_list *prev;
	struct t_mem_list *next;
} mem_list;

#ifdef YTALK_DEBUG
#define get_mem(size) (real_get_mem(size, __LINE__, __FILE__))
#define free_mem(addr) (real_free_mem(addr, __LINE__, __FILE__))
#endif

mem_list *add_area( /* mem_list*, yaddr, int, int, char* */ );
mem_list *del_area( /* mem_list*, mem_list* */ );
#ifdef YTALK_DEBUG
yaddr real_get_mem( /* int, int, char* */ );
void real_free_mem( /* yaddr, int, char* */ );
#else
yaddr get_mem( /* int */ );
void free_mem( /* yaddr */ );
#endif
yaddr realloc_mem( /* yaddr, int */ );
void change_area( /* mem_list*, yaddr, yaddr, int */ );
mem_list *find_area( /* yaddr */ );
int get_size( /* mem_list*, yaddr */ );
void clear_all();
