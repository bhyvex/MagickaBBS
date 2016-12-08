#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs.h"

extern struct bbs_config conf;

void settings_menu(struct user_record *user) {
	char buffer[256];
	int dosettings = 0;
	char c;
	char *hash;
	int new_arc;
	int i;

	while (!dosettings) {
		s_printf(get_string(149));
		s_printf(get_string(150));
		s_printf(get_string(151));
		s_printf(get_string(152), user->location);
		s_printf(get_string(205), conf.archivers[user->defarchiver - 1]->name);
		s_printf(get_string(153));
		s_printf(get_string(154));

		c = s_getc();

		switch(tolower(c)) {
			case 27:
				{
					c = s_getc();
					if (c == 91) {
						c = s_getc();
					}
				}
				break;			
			case 'p':
				{
					s_printf(get_string(155));
					s_readpass(buffer, 16);
					hash = hash_sha256(buffer, user->salt);
					if (strcmp(hash, user->password) == 0) {
						s_printf(get_string(156));
						s_readstring(buffer, 16);
						if (strlen(buffer) >= 8) {
							free(user->password);
							free(user->salt);

							gen_salt(&user->salt);
							user->password = hash_sha256(buffer, user->salt);

							save_user(user);
							s_printf(get_string(157));
						} else {
							s_printf(get_string(158));
						}
					} else {
						s_printf(get_string(159));
					}
				}
				break;
			case 'l':
				{
					s_printf(get_string(160));
					s_readstring(buffer, 32);
					free(user->location);
					user->location = (char *)malloc(strlen(buffer) + 1);
					strcpy(user->location, buffer);
					save_user(user);
				}
				break;
			case 'a':
				{
					s_printf(get_string(206));
					
					for (i=0;i<conf.archiver_count;i++) {
						s_printf(get_string(207), i + 1, conf.archivers[i]->name);
					}
					
					s_printf(get_string(208));
					s_readstring(buffer, 5);
					new_arc = atoi(buffer);
					
					if (new_arc - 1 < 0 || new_arc > conf.archiver_count) {
						break;
					} else {
						user->defarchiver = new_arc;
						save_user(user);
					}
				}
				break;
			case 'q':
				dosettings = 1;
				break;
		}
	}
}
