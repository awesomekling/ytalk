/* ymenu.h */

void update_ymenu();
void show_ymenu();
void hide_ymenu();
void resize_ymenu();
void refresh_ymenu();

void init_ymenu();

void init_userlist();
void init_usermenu(char);

void redo_ymenu_userlist();

int show_message_ymenu(char *);
int show_error_ymenu(char *, char *);

int in_ymenu();
int yes_no(char *);
