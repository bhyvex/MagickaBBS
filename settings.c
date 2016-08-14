#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs.h"

void settings_menu(struct user_record *user) {
	char buffer[256];
	int dosettings = 0;
	char c;
	char *hash;

	while (!dosettings) {
		s_printf(get_string(149));
		s_printf(get_string(150));
		s_printf(get_string(151));
		s_printf(get_string(152), user->location);
		s_printf(get_string(153));
		s_printf(get_string(154));

		c = s_getc();

		switch(tolower(c)) {
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
			case 'q':
				dosettings = 1;
				break;
		}
	}
}
