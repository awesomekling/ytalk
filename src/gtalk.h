/* gtalk.h
 * some stuff to interface with GNU talk
 *
 */

#define GTALK_ESCAPE			0x03

#define GTALK_VERSION_MESSAGE	0x08

void gtalk_process(yuser *user, ychar data);

char *gtalk_parse_version(char *);
void gtalk_send_version(yuser *);
