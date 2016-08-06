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
		s_printf("\e[2J\e[1;32mYour Settings\r\n");
		s_printf("\e[1;30m-------------------------------------------------------------------------------\e[0m\r\n");
		s_printf("\e[0;36mP. \e[1;37mPassword (\e[1;33mNot Shown\e[1;37m)\r\n");
		s_printf("\e[0;36mL. \e[1;37mLocation (\e[1;33m%s\e[1;37m)\r\n", user->location);
		s_printf("\e[0;36mQ. \e[1;37mQuit to Main Menu\r\n");
		s_printf("\e[1;30m-------------------------------------------------------------------------------\e[0m\r\n");

		c = s_getc();

		switch(tolower(c)) {
			case 'p':
				{
					s_printf("\r\nEnter your current password: ");
					s_readpass(buffer, 16);
					hash = hash_sha256(buffer, user->salt);
					if (strcmp(hash, user->password) == 0) {
						s_printf("\r\nEnter your new password (8 chars min): ");
						s_readstring(buffer, 16);
						if (strlen(buffer) >= 8) {
							free(user->password);
							free(user->salt);

							gen_salt(&user->salt);
							user->password = hash_sha256(buffer, user->salt);

							save_user(user);
							s_printf("\r\nPassword Changed!\r\n");
						} else {
							s_printf("\r\nPassword too short!\r\n");
						}
					} else {
						s_printf("\r\nPassword Incorrect!\r\n");
					}
				}
				break;
			case 'l':
				{
					s_printf("\r\nEnter your new location: ");
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
