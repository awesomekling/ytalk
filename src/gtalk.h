/*
 * src/gtalk.c
 * some stuff to interface with GNU talk
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

#define GTALK_ESCAPE			0x03

#define GTALK_PERSONAL_NAME		0x03
#define GTALK_IMPORT_REQUEST	0x06
#define GTALK_VERSION_MESSAGE	0x08

void gtalk_process(yuser *user, ychar data);

char *gtalk_parse_import_request(char *);
char *gtalk_parse_version(char *);
void gtalk_send_version(yuser *);
void gtalk_send_message(yuser *, char *);
