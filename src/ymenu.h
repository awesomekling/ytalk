/* ymenu.h */

void update_ymenu(void);
void show_ymenu(void);
void hide_ymenu(void);
void resize_ymenu(void);

void refresh_ymenu(void);
void __refresh_ymenu(void);

void init_ymenu(void);

void redo_ymenu_userlist(void);

int show_error_ymenu(char *, char *);

void show_colormenu(void);

int can_ymenu(void);
int in_ymenu(void);
int yes_no(char *);
