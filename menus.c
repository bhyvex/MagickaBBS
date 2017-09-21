#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

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
#define MENU_CHOOSEMAILCONF     16
#define MENU_CHOOSEMAILAREA     17
#define MENU_SENDEMAIL          18
#define MENU_LISTEMAIL          19
#define MENU_NEXTMAILCONF       20
#define MENU_PREVMAILCONF       21
#define MENU_NEXTMAILAREA       22
#define MENU_PREVMAILAREA       23
#define MENU_BLUEWAVEDOWN       24
#define MENU_BLUEWAVEUP         25
#define MENU_CHOOSEFILEDIR      26
#define MENU_CHOOSEFILESUB      27
#define MENU_LISTFILES          28
#define MENU_UPLOAD             29
#define MENU_DOWNLOAD           30
#define MENU_CLEARTAGGEDFILES   31
#define MENU_NEXTFILEDIR        32
#define MENU_PREVFILEDIR        33
#define MENU_NEXTFILESUB        34
#define MENU_PREVFILESUB        35
#define MENU_LISTMESSAGES       36
#define MENU_DOSCRIPT           37
#define MENU_SENDNODEMSG        38
#define MENU_SUBUNSUBCONF		39

extern struct bbs_config conf;
extern struct user_record *gUser;
extern int mynode;

struct menu_item {
    char hotkey;
    int command;
    char *data;
    int seclevel;
};

int menu_system(char *menufile) {
    FILE *fptr;
    char buffer[PATH_MAX];
    int menu_items = 0;
    struct menu_item **menu;
    char *lua_script;
    int do_lua_menu;
    char *ansi_file;
    int i;
    int j;
    struct stat s;
    char *lRet;
	lua_State *L;
    int result;
    int doquit = 0;
    char c;
    int clearscreen = 0;

    if (menufile[0] == '/') {
        snprintf(buffer, PATH_MAX, "%s.mnu", menufile);
    } else {
        snprintf(buffer, PATH_MAX, "%s/%s.mnu", conf.menu_path, menufile);
    }
    fptr = fopen(buffer, "r");
    
    if (!fptr) {
        s_printf("Error opening menu file! %s\r\n", menufile);
        return 0;
    }

    lua_script = NULL;
    ansi_file = NULL;


    fgets(buffer, 256, fptr);
    while (!feof(fptr)) {


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
            menu[menu_items-1]->command = 0;
            menu[menu_items-1]->data = NULL;
            menu[menu_items-1]->seclevel = 0;
        } else if (strncasecmp(buffer, "COMMAND", 7) == 0 && menu_items > 0) {
            if (strncasecmp(&buffer[8], "SUBMENU", 7) == 0) {
                menu[menu_items-1]->command = MENU_SUBMENU;
            } else if (strncasecmp(&buffer[8], "LOGOFF", 6) == 0) {
                menu[menu_items-1]->command = MENU_LOGOFF;
            } else if (strncasecmp(&buffer[8], "PREVMENU", 8) == 0) {
                menu[menu_items-1]->command = MENU_PREVMENU;
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
            } else if (strncasecmp(&buffer[8], "CHOOSEMAILCONF", 14) == 0) {
                menu[menu_items-1]->command = MENU_CHOOSEMAILCONF;
            } else if (strncasecmp(&buffer[8], "CHOOSEMAILAREA", 14) == 0) {
                menu[menu_items-1]->command = MENU_CHOOSEMAILAREA;
            } else if (strncasecmp(&buffer[8], "SENDEMAIL", 9) == 0) {
                menu[menu_items-1]->command = MENU_SENDEMAIL;
            } else if (strncasecmp(&buffer[8], "LISTEMAIL", 9) == 0) {
                menu[menu_items-1]->command = MENU_LISTEMAIL;
            } else if (strncasecmp(&buffer[8], "NEXTMAILCONF", 12) == 0) {
                menu[menu_items-1]->command = MENU_NEXTMAILCONF;
            } else if (strncasecmp(&buffer[8], "PREVMAILCONF", 12) == 0) {
                menu[menu_items-1]->command = MENU_PREVMAILCONF;
            } else if (strncasecmp(&buffer[8], "NEXTMAILAREA", 12) == 0) {
                menu[menu_items-1]->command = MENU_NEXTMAILAREA;
            } else if (strncasecmp(&buffer[8], "PREVMAILAREA", 12) == 0) {
                menu[menu_items-1]->command = MENU_PREVMAILAREA;
            } else if (strncasecmp(&buffer[8], "BLUEWAVEDOWNLOAD", 16) == 0) {
                menu[menu_items-1]->command = MENU_BLUEWAVEDOWN;
            } else if (strncasecmp(&buffer[8], "BLUEWAVEUPLOAD", 14) == 0) {
                menu[menu_items-1]->command = MENU_BLUEWAVEUP;
            } else if (strncasecmp(&buffer[8], "CHOOSEFILEDIR", 13) == 0) {
                menu[menu_items-1]->command = MENU_CHOOSEFILEDIR;
            } else if (strncasecmp(&buffer[8], "CHOOSEFILESUB", 13) == 0) {
                menu[menu_items-1]->command = MENU_CHOOSEFILESUB;
            } else if (strncasecmp(&buffer[8], "LISTFILES", 9) == 0) {
                menu[menu_items-1]->command = MENU_LISTFILES;
            } else if (strncasecmp(&buffer[8], "UPLOAD", 6) == 0) {
                menu[menu_items-1]->command = MENU_UPLOAD;
            } else if (strncasecmp(&buffer[8], "DOWNLOAD", 8) == 0) {
                menu[menu_items-1]->command = MENU_DOWNLOAD;
            } else if (strncasecmp(&buffer[8], "CLEARTAGGED", 11) == 0) {
                menu[menu_items-1]->command = MENU_CLEARTAGGEDFILES;
            } else if (strncasecmp(&buffer[8], "NEXTFILEDIR", 11) == 0) {
                menu[menu_items-1]->command = MENU_NEXTFILEDIR;
            } else if (strncasecmp(&buffer[8], "PREVFILEDIR", 11) == 0) {
                menu[menu_items-1]->command = MENU_PREVFILEDIR;
            } else if (strncasecmp(&buffer[8], "NEXTFILESUB", 11) == 0) {
                menu[menu_items-1]->command = MENU_NEXTFILESUB;
            } else if (strncasecmp(&buffer[8], "PREVFILESUB", 11) == 0) {
                menu[menu_items-1]->command = MENU_PREVFILESUB;
            } else if (strncasecmp(&buffer[8], "LISTMESSAGES", 12) == 0) {
                menu[menu_items-1]->command = MENU_LISTMESSAGES;
            } else if (strncasecmp(&buffer[8], "DOSCRIPT", 8) == 0) {
                menu[menu_items-1]->command = MENU_DOSCRIPT;
            } else if (strncasecmp(&buffer[8], "SENDNODEMSG", 11) == 0) {
                menu[menu_items-1]->command = MENU_SENDNODEMSG;
            } else if (strncasecmp(&buffer[8], "SUBUNSUBCONF", 12) == 0) {
				menu[menu_items-1]->command = MENU_SUBUNSUBCONF;
			}
        } else if (strncasecmp(buffer, "SECLEVEL", 8) == 0) {
            menu[menu_items-1]->seclevel = atoi(&buffer[9]);
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
        } else if (strncasecmp(buffer, "CLEARSCREEN", 11) == 0) {
			clearscreen = 1;
		}

        fgets(buffer, 256, fptr);
    }
    fclose(fptr);

    do_lua_menu = 0;

    if (lua_script != NULL) {
        if (conf.script_path != NULL && lua_script[0] != '/') {
            snprintf(buffer, PATH_MAX, "%s/%s.lua", conf.script_path, lua_script);
            do_lua_menu = 1;
        } else if (lua_script[0] == '/') {
            snprintf(buffer, PATH_MAX, "%s.lua", lua_script);
            do_lua_menu = 1;
        } 

        if (do_lua_menu) {
            if (stat(buffer, &s) == 0) {
                L = luaL_newstate();
                luaL_openlibs(L);
                lua_push_cfunctions(L);
                luaL_loadfile(L, buffer);
                do_lua_menu = 1;
                result = lua_pcall(L, 0, 1, 0);
                if (result) {
                    dolog("Failed to run script: %s", lua_tostring(L, -1));
                    do_lua_menu = 0;
                }
            } else {
                do_lua_menu = 0;
            }
        }
    }


	while (!doquit) {
        if (gUser->nodemsgs) {
            snprintf(buffer, PATH_MAX, "%s/node%d/nodemsg.txt", conf.bbs_path, mynode);

            if (stat(buffer, &s) == 0) {
                fptr = fopen(buffer, "r");
                if (fptr) {
                    fgets(buffer, PATH_MAX, fptr);
                    while (!feof(fptr)) {
                        chomp(buffer);
                        s_printf("\r\n%s\r\n", buffer);
                        fgets(buffer, PATH_MAX, fptr);
                    }
                    fclose(fptr);
                    snprintf(buffer, PATH_MAX, "%s/node%d/nodemsg.txt", conf.bbs_path, mynode);
                    unlink(buffer);

                    s_printf(get_string(6));
                    c = s_getc();
                }
            }
        }
        
        if (clearscreen) {
			s_printf("\e[2J\e[1;1H");
		}
        
		if (do_lua_menu == 0) {
            if (ansi_file != NULL) {
			    s_displayansi(ansi_file);
            }
			s_printf(get_string(142), gUser->timeleft);
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
            if (tolower(menu[i]->hotkey) == tolower(c)) {
                if (menu[i]->seclevel <= gUser->sec_level) {
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
                                        free(menu[i]->data);
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
                                    free(menu[i]->data);
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
                                    free(menu[i]->data);
                                }
                                free(menu[i]);
                            }
                            free(menu);                                                      
                            return 0;
                        case MENU_AUTOMESSAGEWRITE:
                            automessage_write(gUser);
                            break;
                        case MENU_TEXTFILES:
                            display_textfiles();
                            break;
                        case MENU_CHATSYSTEM:
                            chat_system(gUser);
                            break;
                        case MENU_BBSLIST:
                            bbs_list(gUser);
                            break;
                        case MENU_LISTUSERS:
                            list_users(gUser);
                            break;
                        case MENU_BULLETINS:
                            display_bulletins();
                            break;
                        case MENU_LAST10:
                            display_last10_callers(gUser);
                            break;
                        case MENU_SETTINGS:
                            settings_menu(gUser);
                            break;
                        case MENU_DOOR:
                            {
                                for (j=0;j<conf.door_count;j++) {
                                    if (strcasecmp(menu[i]->data, conf.doors[j]->name) == 0) {
                                        dolog("%s launched door %s, on node %d", gUser->loginname, conf.doors[j]->name, mynode);
                                        rundoor(gUser, conf.doors[j]->command, conf.doors[j]->stdio, conf.doors[j]->codepage);
                                        dolog("%s returned from door %s, on node %d", gUser->loginname, conf.doors[j]->name, mynode);
                                        break;
                                    }
                                }
                            }
                            break;
                        case MENU_MAILSCAN:
                            mail_scan(gUser);
                            break;
                        case MENU_READMAIL:
                            read_mail(gUser);
                            break;
                        case MENU_POSTMESSAGE:
                            post_message(gUser);
                            break;
                        case MENU_CHOOSEMAILCONF:
                            choose_conference(gUser);
                            break;
                        case MENU_CHOOSEMAILAREA:
                            choose_area(gUser);
                            break;
                        case MENU_SENDEMAIL:
                            send_email(gUser);
                            break;
                        case MENU_LISTEMAIL:
                            list_emails(gUser);
                            break;
                        case MENU_NEXTMAILCONF:
                            next_mail_conf(gUser);
                            break;
                        case MENU_PREVMAILCONF:
                            prev_mail_conf(gUser);
                            break;
                        case MENU_NEXTMAILAREA:
                            next_mail_area(gUser);
                            break;
                        case MENU_PREVMAILAREA:
                            prev_mail_area(gUser);
                            break;
                        case MENU_BLUEWAVEDOWN:
                            bwave_create_packet();
                            break;
                        case MENU_BLUEWAVEUP:
                            bwave_upload_reply();
                            break;
                        case MENU_CHOOSEFILEDIR:
                            choose_directory(gUser);
                            break;
                        case MENU_CHOOSEFILESUB:
                            choose_subdir(gUser);
                            break;
                        case MENU_LISTFILES:
                            list_files(gUser);
                            break;
                        case MENU_UPLOAD:
                            if (gUser->sec_level >= conf.file_directories[gUser->cur_file_dir]->file_subs[gUser->cur_file_sub]->upload_sec_level) {
                                upload(gUser);
                            } else {
                                s_printf(get_string(84));
                            }
                            break;
                        case MENU_DOWNLOAD:
                            download(gUser);
                            break;
                        case MENU_CLEARTAGGEDFILES:
                            clear_tagged_files();
                            break;
                        case MENU_NEXTFILEDIR:
                            next_file_dir(gUser);
                            break;
                        case MENU_PREVFILEDIR:
                            prev_file_dir(gUser);
                            break;
                        case MENU_NEXTFILESUB:
                            next_file_sub(gUser);
                            break;
                        case MENU_PREVFILESUB:
                            prev_file_sub(gUser);
                            break; 
                        case MENU_LISTMESSAGES:
                            list_messages(gUser);
                            break;
                        case MENU_DOSCRIPT:
                            do_lua_script(menu[i]->data);
                            break;
                        case MENU_SENDNODEMSG:
                            send_node_msg();
                            break;
                        case MENU_SUBUNSUBCONF:
							msg_conf_sub_bases();
							break;
                        default:
                            break;     
                    }
                    break;
                }
            }
        }
    }

    // free menus;
    return doquit;
}
