typedef struct t_mem_list {
	yaddr addr;
	int size;
	struct t_mem_list *next;
} mem_list;

mem_list *add_area(mem_list *, yaddr, int);
mem_list *del_area(mem_list *, yaddr);
yaddr get_mem(int);
void free_mem(yaddr);
yaddr realloc_mem(yaddr, int);
void change_area(mem_list *, yaddr, yaddr, int);
int get_size(mem_list *, yaddr);
