#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#if defined(linux)
#  include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#  include <util.h>
#else
#  include <libutil.h>
#endif
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

extern struct bbs_config conf;
extern int mynode;
extern int gSocket;
extern int sshBBS;

int running_door_pid = 0;
int running_door = 0;

extern int timeoutpaused;

void doorchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    while(waitpid(-1, NULL, WNOHANG) > 0);

	running_door = 0;
}

int write_door32sys(struct user_record *user) {
	struct stat s;
	char buffer[256];
	FILE *fptr;
	char *ptr;
	int i;

	sprintf(buffer, "%s/node%d", conf.bbs_path, mynode);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}

	sprintf(buffer, "%s/node%d/door32.sys", conf.bbs_path, mynode);

	fptr = fopen(buffer, "w");

	if (!fptr) {
		dolog("Unable to open %s for writing!", buffer);
		return 1;
	}

	fprintf(fptr, "2\r\n"); // telnet type
	fprintf(fptr, "%d\r\n", gSocket); // socket
	fprintf(fptr, "38400\r\n"); // baudrate
	fprintf(fptr, "Magicka %d.%d\r\n", VERSION_MAJOR, VERSION_MINOR);
	fprintf(fptr, "%d\r\n", user->id);
	fprintf(fptr, "%s %s\r\n", user->firstname, user->lastname);
	fprintf(fptr, "%s\r\n", user->loginname);
	fprintf(fptr, "%d\r\n", user->sec_level);
	fprintf(fptr, "%d\r\n", user->timeleft);
	fprintf(fptr, "1\r\n"); // ansi emulation = 1
	fprintf(fptr, "%d\r\n", mynode);

	fclose(fptr);

	// create dorinfo1.def

	sprintf(buffer, "%s/node%d", conf.bbs_path, mynode);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}

	sprintf(buffer, "%s/node%d/dorinfo1.def", conf.bbs_path, mynode);

	fptr = fopen(buffer, "w");

	if (!fptr) {
		dolog("Unable to open %s for writing!", buffer);
		return 1;
	}

	strcpy(buffer, conf.sysop_name);

	ptr = NULL;

	for (i=0;i<strlen(buffer);i++) {
		if (buffer[i] == ' ') {
			ptr = &buffer[i+1];
			buffer[i] = '\0';
			break;
		}
	}

	fprintf(fptr, "%s\r\n", conf.bbs_name); // telnet type
	fprintf(fptr, "%s\r\n", buffer);
	if (ptr != NULL) {
		fprintf(fptr, "%s\r\n", ptr);
	} else {
		fprintf(fptr, "\r\n");
	}
	fprintf(fptr, "COM1\r\n"); // com port
	fprintf(fptr, "38400 BAUD,N,8,1\r\n");
	fprintf(fptr, "0\r\n");
	fprintf(fptr, "%s\r\n", user->firstname);
	fprintf(fptr, "%s\r\n", user->lastname);
	fprintf(fptr, "%s\r\n", user->location);
	fprintf(fptr, "1\r\n");
	fprintf(fptr, "30\r\n");
	fprintf(fptr, "%d\r\n", user->timeleft);
	fprintf(fptr, "-1\r\n");


	fclose(fptr);


	return 0;
}

void rundoor(struct user_record *user, char *cmd, int stdio) {
	char buffer[256];
	int pid;
	char *arguments[4];
	int ret;
	char c;
	int len;
	int master;
	int slave;
	fd_set fdset;
	int t;
	struct winsize ws;
	struct sigaction sa;
	int door_in;
	int door_out;

	timeoutpaused = 1;

	if (write_door32sys(user) != 0) {
		return;
	}

	if (stdio) {
		if (sshBBS) {
			door_in = STDIN_FILENO;
			door_out = STDOUT_FILENO;
		} else {
			door_in = gSocket;
			door_out = gSocket;
		}

		arguments[0] = strdup(cmd);
		sprintf(buffer, "%d", mynode);
		arguments[1] = strdup(buffer);
		sprintf(buffer, "%d", door_out);
		arguments[2] = strdup(buffer);
		arguments[3] = NULL;

		ws.ws_row = 24;
		ws.ws_col = 80;

		running_door = 1;

		if (openpty(&master, &slave, NULL, NULL, &ws) == 0) {
			sa.sa_handler = doorchld_handler;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = SA_RESTART | SA_SIGINFO;
			if (sigaction(SIGCHLD, &sa, NULL) == -1) {
				perror("sigaction");
				exit(1);
			}

			pid = fork();
			if (pid < 0) {
				return;
			} else if (pid == 0) {

				close(master);
				dup2(slave, 0);
				dup2(slave, 1);

				close(slave);

				setsid();

				ioctl(0, TIOCSCTTY, 1);

				execvp(cmd, arguments);
			} else {
				running_door_pid = pid;

				while(running_door != 0) {
					FD_ZERO(&fdset);
					FD_SET(master, &fdset);
					FD_SET(door_in, &fdset);
					if (master > door_in) {
						t = master + 1;
					} else {
						t = door_in + 1;
					}
					ret = select(t, &fdset, NULL, NULL, NULL);
					if (ret > 0) {
						if (FD_ISSET(door_in, &fdset)) {
							len = read(door_in, &c, 1);
							if (len == 0) {
								close(master);
								disconnect("Socket Closed");
								return;
							}
							if (c == '\n' || c == '\0') {
								continue;
							}
							write(master, &c, 1);
						} else if (FD_ISSET(master, &fdset)) {
							len = read(master, &c, 1);
							if (len == 0) {
								close(master);
								break;
							}
							write(door_out, &c, 1);
						}
					}
				}
			}
		}
		free(arguments[0]);
		free(arguments[1]);
		free(arguments[2]);
	} else {
		if (!sshBBS) {
			sprintf(buffer, "%s %d %d", cmd, mynode, gSocket);
			system(buffer);
		} else {
			s_printf(get_string(51));
		}
	}
	timeoutpaused = 0;
}

int door_menu(struct user_record *user) {
	int doquit = 0;
	int dodoors = 0;
	char buffer[256];
	int i;
	char c;
	struct stat s;
	int do_internal_menu = 0;
	char *lRet;
	lua_State *L;
	int result;

	if (conf.script_path != NULL) {
		sprintf(buffer, "%s/doors.lua", conf.script_path);
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

	while (!dodoors) {
		if (do_internal_menu == 1) {
			s_displayansi("doors");

			s_printf(get_string(52), user->timeleft);

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
			case 'q':
				dodoors = 1;
				break;
			case 'g':
				{
					doquit = do_logout();
					dodoors = doquit;
				}
				break;
			default:
				{
					for (i=0;i<conf.door_count;i++) {
						if (tolower(c) == tolower(conf.doors[i]->key)) {
							dolog("%s is launched door %s, on node %d", user->loginname, conf.doors[i]->name, mynode);
							rundoor(user, conf.doors[i]->command, conf.doors[i]->stdio);
							dolog("%s is returned from door %s, on node %d", user->loginname, conf.doors[i]->name, mynode);
							break;
						}
					}
				}
				break;
		}
	}
	if (do_internal_menu == 0) {
		lua_close(L);
	}
	return doquit;
}
