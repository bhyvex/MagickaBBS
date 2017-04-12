#define MENU_SUBMENU            1
#define MENU_LOGOFF             2
#define MENU_PREVMENU           3
#define MENU_AUTOMESSAGEWRITE   4
#define MENU_TEXTFILES          5
#define MENU_CHATSYSTEM         6
#define MENU_BBSLIST            7
#define MENU_LISTUSERS          8
#define MENU_BULLETINS          9
#define MENU_LAST10             10
#define MENU_SETTINGS           11
#define MENU_DOOR               12
#define MENU_MAILSCAN           13
#define MENU_READMAIL           14
#define MENU_POSTMESSAGE        15

struct menu_item {
    char hotkey;
    int command;
    char *data;
};

int menu_system(char *menufile) {
    FILE *fptr;
    char buffer[256];
    int menu_items = 0;
    struct menu_item **menu;
    char *lua_script;
    int do_lua_menu;
    char *ansi_file;
    fptr = fopen(menufile, "r");
    int i;
    int j;

    if (!fptr) {
        s_printf("Error opening menu file! %s\r\n", menufile);
        return 0;
    }

    lua_script = NULL;
    ansi_file = NULL;

    while (1) {


        fgets(buffer, 256, fptr);
        chomp(buffer);

        if (strncasecmp(buffer, "HOTKEY", 6) == 0) {
            menu_items++;
            if (menu_items == 1) {
                menu = (struct menu_item **)malloc(sizeof(struct menu_item *));
            } else {
                menu = (struct menu_item **)realloc(menu, sizeof(struct menu_item *) * (menu_items));
            }
            menu[menu_items-1] = (struct menu_item *)malloc(sizeof(struct menu_item));
            menu[menu_items-1]->hotkey = buffer[7];
            menu[menu_items-1]->data = NULL;
        } else if (strncasecmp(buffer, "COMMAND", 7) == 0 && menu_items > 0) {
            if (strncasemp(&buffer[8], "SUBMENU", 7) == 0) {
                menu[menu_items-1]->command = MENU_SUBMENU;
            } else if (strncasecmp(&buffer[8], "LOGOFF", 6) == 0) {
                menu[menu_items-1]->command = MENU_LOGOFF;
            } else if (strncasecmp(&buffer[8], "AUTOMESSAGE_WRITE", 17) == 0) {
                menu[menu_items-1]->command = MENU_AUTOMESSAGEWRITE;
            } else if (strncasecmp(&buffer[8], "TEXTFILES", 9) == 0) {
                menu[menu_items-1]->command = MENU_TEXTFILES;
            } else if (strncasecmp(&buffer[8], "CHATSYSTEM", 10) == 0) {
                menu[menu_items-1]->command = MENU_CHATSYSTEM;
            } else if (strncasecmp(&buffer[8], "BBSLIST", 7) == 0) {
                menu[menu_items-1]->command = MENU_BBSLIST;
            } else if (strncasecmp(&buffer[8], "LISTUSERS", 9) == 0) {
                menu[menu_items-1]->command = MENU_LISTUSERS;
            } else if (strncasecmp(&buffer[8], "BULLETINS", 9) == 0) {
                menu[menu_items-1]->command = MENU_BULLETINS;
            } else if (strncasecmp(&buffer[8], "LAST10CALLERS", 13) == 0) {
                menu[menu_items-1]->command = MENU_LAST10;
            } else if (strncasecmp(&buffer[8], "SETTINGS", 8) == 0) {
                menu[menu_items-1]->command = MENU_SETTINGS;
            } else if (strncasecmp(&buffer[8], "RUNDOOR", 7) == 0) {
                menu[menu_items-1]->command = MENU_DOOR;
            } else if (strncasecmp(&buffer[8], "MAILSCAN", 8) == 0) {
                menu[menu_items-1]->command = MENU_MAILSCAN;
            } else if (strncasecmp(&buffer[8], "READMAIL", 8) == 0) {
                menu[menu_items-1]->command = MENU_READMAIL;
            } else if (strncasecmp(&buffer[8], "POSTMESSAGE", 11) == 0) {
                menu[menu_items-1]->command = MENU_POSTMESSAGE;
            }
        } else if (strncasecmp(buffer, "DATA", 4) == 0) {
            menu[menu_items-1]->data = strdup(&buffer[5]);
        } else if (strncasecmp(buffer, "LUASCRIPT", 9) == 0) {
            if (lua_script != NULL) {
                free(lua_script);
            }
            lua_script = strdup(&buffer[10]);
        } else if (strncasecmp(buffer, "ANSIFILE", 8) == 0) {
            if (ansi_file != NULL) {
                free(ansi_file);
            }
            ansi_file = strdup(&buffer[9]);
        }
    }
    fclose(fptr);

    do_lua_menu = 0;

	if (conf.script_path != NULL && lua_script[0] != '/') {
		snprintf(buffer, 256 "%s/%s", conf.script_path, lua_script);
        do_lua_menu = 1;
    } else if (lua_script[0] == '/') {
        snprintf(buffer, 256 "%s", lua_script);
        do_lua_menu = 1;
    } 

    if (do_lua_menu)
		if (stat(buffer, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_loadfile(L, buffer);
			do_internal_menu = 0;
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_lua_menu = 0;
			}
		} else {
			do_lua_menu = 0;
		}
	}

	while (!doquit) {

		if (do_lua_menu == 0) {
            if (ansi_file != NULL) {
			    s_displayansi(ansi_file);
            }
			s_printf(get_string(142), user->timeleft);
			c = s_getc();
		} else {
			lua_getglobal(L, "menu");
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_lua_menu = 0;
				lua_close(L);
				continue;
			}
			lRet = (char *)lua_tostring(L, -1);
			lua_pop(L, 1);
			c = lRet[0];
		}

        for (i=0;i<menu_items;i++) {
            if (menu[i]->hotkey == c) {
                switch(menu[i]->command) {
                    case MENU_SUBMENU:
                        doquit = menu_system(menu[i]->data);
                        if (doquit == 1) {
                            // free menus
	                        if (do_lua_menu) {
		                        lua_close(L);
	                        }
                            if (lua_script != NULL) {
                                free(lua_script);
                            }
                            if (ansi_file != NULL) {
                                free(ansi_file);
                            }
                            for (i=0;i<menu_items;i++) {
                                if (menu[i]->data != NULL) {
                                    free(menu[i]->data)
                                }
                                free(menu[i]);
                            }
                            free(menu);
                            return doquit;
                        }
                        break;
                    case MENU_LOGOFF:
	                    if (do_lua_menu) {
		                    lua_close(L);
	                    }              
                        if (lua_script != NULL) {
                            free(lua_script);
                        }
                        if (ansi_file != NULL) {
                            free(ansi_file);
                        }
                        for (i=0;i<menu_items;i++) {
                            if (menu[i]->data != NULL) {
                                free(menu[i]->data)
                            }
                            free(menu[i]);
                        }
                        free(menu);                              
                        return 1;
                    case MENU_PREVMENU:
	                    if (do_lua_menu) {
		                    lua_close(L);
	                    }                    
                        if (lua_script != NULL) {
                            free(lua_script);
                        }
                        if (ansi_file != NULL) {
                            free(ansi_file);
                        }
                        for (i=0;i<menu_items;i++) {
                            if (menu[i]->data != NULL) {
                                free(menu[i]->data)
                            }
                            free(menu[i]);
                        }
                        free(menu);                                                      
                        return 0;
                    case MENU_AUTOMESSAGEWRITE:
                        automessage_write(user);
                        break;
                    case MENU_TEXTFILES:
                        display_textfiles();
                        break;
                    case MENU_CHATSYSTEM:
                        chat_system(user);
                        break;
                    case MENU_BBSLIST:
                        bbs_list(user);
                        break;
                    case MENU_LISTUSERS:
                        list_users(user);
                        break;
                    case MENU_BULLETINS:
                        display_bulletins();
                        break;
                    case MENU_LAST10:
                        display_last10_callers(user);
                        break;
                    case MENU_SETTINGS:
                        settings_menu(user);
                        break;
                    case MENU_DOOR:
				        {
					        for (j=0;j<conf.door_count;j++) {
						        if (strcasecmp(menu[i]->data, conf.doors[j]->name) == 0) {
							        dolog("%s launched door %s, on node %d", user->loginname, conf.doors[j]->name, mynode);
							        rundoor(user, conf.doors[j]->command, conf.doors[j]->stdio);
							        dolog("%s returned from door %s, on node %d", user->loginname, conf.doors[j]->name, mynode);
						        	break;
				        		}
		        			}
        				}
                        break;
                    case MENU_MAILSCAN:
                        mail_scan(user);
                        break;
                    case MENU_READMAIL:
                        read_mail(user);
                        break;
                    case MENU_POSTMESSAGE:
                        post_message(user);
                        break;
                }
                break;
            }
        }
    }

    // free menus;
    return doquit;
}