#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

extern struct bbs_config conf;

void main_menu(struct user_record *user) {
	int doquit = 0;
	char c;
	char buffer[256];
	int i;
	struct stat s;
	int do_internal_menu = 0;
	char *lRet;
	lua_State *L;
	int result;

	if (conf.script_path != NULL) {
		sprintf(buffer, "%s/mainmenu.lua", conf.script_path);
		if (stat(buffer, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_loadfile(L, buffer);
			do_internal_menu = 0;
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_internal_menu = 1;
			}
		} else {
			do_internal_menu = 1;
		}
	} else {
		do_internal_menu = 1;
	}

	while (!doquit) {

		if (do_internal_menu == 1) {
			s_displayansi("mainmenu");


			s_printf(get_string(142), user->timeleft);

			c = s_getc();
		} else {
			lua_getglobal(L, "menu");
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_internal_menu = 1;
				lua_close(L);
				continue;
			}
			lRet = (char *)lua_tostring(L, -1);
			lua_pop(L, 1);
			c = lRet[0];
		}

		switch(tolower(c)) {
			case 27:
				{
					c = s_getc();
					if (c == 91) {
						c = s_getc();
					}
				}
				break;			
			case 'o':
				{
					automessage_write(user);
				}
				break;
			case 'a':
				{
					if (conf.text_file_count > 0) {

						while(1) {
							s_printf(get_string(143));
							s_printf(get_string(144));

							for (i=0;i<conf.text_file_count;i++) {
								s_printf(get_string(145), i, conf.text_files[i]->name);
							}
							s_printf(get_string(146));
							s_printf(get_string(147));
							s_readstring(buffer, 4);
							if (tolower(buffer[0]) != 'q') {
								i = atoi(buffer);
								if (i >= 0 && i < conf.text_file_count) {
									s_printf("\r\n");
									s_displayansi_p(conf.text_files[i]->path);
									s_printf(get_string(6));
									s_getc();
									s_printf("\r\n");
								}
							} else {
								break;
							}
						}
					} else {
						s_printf(get_string(148));
						s_printf(get_string(6));
						s_getc();
					}
				}
				break;
			case 'c':
				{
					chat_system(user);
				}
				break;
			case 'l':
				{
					bbs_list(user);
				}
				break;
			case 'u':
				{
					list_users(user);
				}
				break;
			case 'b':
				{
					i = 0;
					sprintf(buffer, "%s/bulletin%d.ans", conf.ansi_path, i);

					while (stat(buffer, &s) == 0) {
						sprintf(buffer, "bulletin%d", i);
						s_displayansi(buffer);
						s_printf(get_string(6));
						s_getc();
						i++;
						sprintf(buffer, "%s/bulletin%d.ans", conf.ansi_path, i);
					}
				}
				break;
			case '1':
				{
					display_last10_callers(user);
				}
				break;
			case 'd':
				{
					doquit = door_menu(user);
				}
				break;
			case 'm':
				{
					doquit = mail_menu(user);
				}
				break;
			case 'g':
				{
					doquit = do_logout();
				}
				break;
			case 't':
				{
					doquit = file_menu(user);
				}
				break;
			case 's':
				{
					settings_menu(user);
				}
				break;
		}
	}
	if (do_internal_menu == 0) {
		lua_close(L);
	}
}
