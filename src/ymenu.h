/*
 * src/ymenu.h
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

void update_ymenu();
void show_ymenu();
void hide_ymenu();
void resize_ymenu();

void refresh_ymenu();
void __refresh_ymenu();

void init_ymenu();

void redo_ymenu_userlist();

int show_error_ymenu(char *, char *, char *);
void show_message_ymenu(char *, char *);

void show_colormenu();

int can_ymenu();
int in_ymenu();
int yes_no(char *);
