/* mem.h */

#ifdef YTALK_DEBUG
typedef struct __mem_list {
	yaddr addr;
	int size;
	char *file;
	int line;
	struct __mem_list *prev;
	struct __mem_list *next;
} mem_list;

#define get_mem(size) (real_get_mem(size, __LINE__, __FILE__))
#define free_mem(addr) (real_free_mem(addr, __LINE__, __FILE__))

yaddr real_get_mem(int, int, char *);
void real_free_mem(yaddr, int, char *);
void change_area(mem_list *, yaddr, yaddr, int);
mem_list *find_area(mem_list *, yaddr);
int get_size(mem_list *, yaddr);
void clear_all(void);
mem_list *add_area(mem_list *, yaddr, int, int, char *);
mem_list *del_area(mem_list *, mem_list *);
#else
yaddr get_mem(int);
void free_mem(yaddr);
#endif /* YTALK_DEBUG */

yaddr realloc_mem(yaddr, int);
