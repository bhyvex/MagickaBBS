#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <stdarg.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

int mynode;
struct bbs_config conf;

struct user_record *gUser;
int gSocket;
int sshBBS;
int usertimeout;
int timeoutpaused;

char *ipaddress;

void dolog(char *fmt, ...) {
	char buffer[512];
	struct tm time_now;
	time_t timen;
	FILE *logfptr;

	if (conf.log_path == NULL) return;

	timen = time(NULL);

	localtime_r(&timen, &time_now);

	snprintf(buffer, 512, "%s/%04d%02d%02d.log", conf.log_path, time_now.tm_year + 1900, time_now.tm_mon + 1, time_now.tm_mday);
	logfptr = fopen(buffer, "a");
    if (!logfptr) {
		dolog("Error opening log file!");
		return;
	}
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buffer, 512, fmt, ap);
  va_end(ap);

  fprintf(logfptr, "%02d:%02d:%02d [%s] %s\n", time_now.tm_hour, time_now.tm_min, time_now.tm_sec, ipaddress, buffer);

	fclose(logfptr);
}

struct fido_addr *parse_fido_addr(const char *str) {
	struct fido_addr *ret = (struct fido_addr *)malloc(sizeof(struct fido_addr));
	int c;
	int state = 0;

	ret->zone = 0;
	ret->net = 0;
	ret->node = 0;
	ret->point = 0;

	for (c=0;c<strlen(str);c++) {
		switch(str[c]) {
			case ':':
				state = 1;
				break;
			case '/':
				state = 2;
				break;
			case '.':
				state = 3;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				{
					switch (state) {
						case 0:
							ret->zone = ret->zone * 10 + (str[c] - '0');
							break;
						case 1:
							ret->net = ret->net * 10 + (str[c] - '0');
							break;
						case 2:
							ret->node = ret->node * 10 + (str[c] - '0');
							break;
						case 3:
							ret->point = ret->point * 10 + (str[c] - '0');
							break;
					}
				}
				break;
			default:
				free(ret);
				return NULL;
		}
	}
	return ret;
}


void timer_handler(int signum) {
	if (signum == SIGALRM) {
		if (gUser != NULL) {
			gUser->timeleft--;

			if (gUser->timeleft <= 0) {
				s_printf("\r\n\r\nSorry, you're out of time today..\r\n");
				disconnect("Out of Time");
			}


		}
		if (timeoutpaused == 0) {
			usertimeout--;
		}
		if (usertimeout <= 0) {
			s_printf("\r\n\r\nTimeout waiting for input..\r\n");
			disconnect("Timeout");
		}
	}
}

void s_printf(char *fmt, ...) {
	char buffer[512];

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buffer, 512, fmt, ap);
  va_end(ap);

	s_putstring(buffer);
}

void s_putchar(char c) {
	if (sshBBS) {
		putchar(c);
	} else {
		write(gSocket, &c, 1);
	}
}

void s_putstring(char *c) {
	if (sshBBS) {
		printf("%s", c);
	} else {
		write(gSocket, c, strlen(c));
	}
}

void s_displayansi_p(char *file) {
	FILE *fptr;
	char c;

	fptr = fopen(file, "r");
	if (!fptr) {
		return;
	}
	c = fgetc(fptr);
	while (!feof(fptr) && c != 0x1a) {
		s_putchar(c);
		c = fgetc(fptr);
	}
	fclose(fptr);
}


void s_displayansi(char *file) {
	FILE *fptr;
	char c;

	char buffer[256];

	sprintf(buffer, "%s/%s.ans", conf.ansi_path, file);

	fptr = fopen(buffer, "r");
	if (!fptr) {
		return;
	}
	c = fgetc(fptr);
	while (!feof(fptr) && c != 0x1a) {
		s_putchar(c);
		c = fgetc(fptr);
	}
	fclose(fptr);
}

char s_getchar() {
	unsigned char c;
	int len;

	do {

		if (sshBBS) {
			c = getchar();
		} else {
			len = read(gSocket, &c, 1);

			if (len == 0) {
				disconnect("Socket Closed");
			}
		}

		if (!sshBBS) {
			while (c == 255) {
				len = read(gSocket, &c, 1);
				if (len == 0) {
					disconnect("Socket Closed");
				} else if (c == 255) {
					usertimeout = 10;
					return c;
				}
				len = read(gSocket, &c, 1);
				if (len == 0) {
					disconnect("Socket Closed");
				}
				len = read(gSocket, &c, 1);
				if (len == 0) {
					disconnect("Socket Closed");
				}
			}
		}


	/*	if (c == '\r') {
			if (len == 0) {
				disconnect("Socket Closed");
			}
		}*/
	} while (c == '\n');
	usertimeout = 10;

	return (char)c;
}

char s_getc() {
	char c = s_getchar();

	s_putchar(c);
	return (char)c;
}

void s_readstring(char *buffer, int max) {
	int i;
	char c;

	memset(buffer, 0, max);

	for (i=0;i<max;i++) {
		c = s_getchar();
		if ((c == '\b' || c == 127) && i > 0) {
			buffer[i-1] = '\0';
			i -= 2;
			s_printf("\e[D \e[D");
			continue;
		} else if (c == '\b' || c == 127) {
			i -= 1;
			continue;
		}

		if (c == '\n' || c == '\r') {
			return;
		}
		s_putchar(c);
		buffer[i] = c;
		buffer[i+1] = '\0';
	}
}

void s_readpass(char *buffer, int max) {
	int i;
	char c;

	for (i=0;i<max;i++) {
		c = s_getchar();

		if ((c == '\b' || c == 127) && i > 0) {
			buffer[i-1] = '\0';
			i-=2;
			s_printf("\e[D \e[D");
			continue;
		} else if (c == '\b' || c == 127) {
			i -= 1;
			continue;
		}

		if (c == '\n' || c == '\r') {
			return;
		}
		s_putchar('*');
		buffer[i] = c;
		buffer[i+1] = '\0';
	}
}

void disconnect(char *calledby) {
	char buffer[256];
	if (gUser != NULL) {
		save_user(gUser);
	}
	dolog("Node %d disconnected (%s)", mynode, calledby);
	sprintf(buffer, "%s/nodeinuse.%d", conf.bbs_path, mynode);
	remove(buffer);
	if (!sshBBS) {
		close(gSocket);
	}
	exit(0);
}

void record_last10_callers(struct user_record *user) {
	struct last10_callers new_entry;
		struct last10_callers callers[10];

	int i,j;
	FILE *fptr = fopen("last10.dat", "rb");

	if (fptr != NULL) {
		for (i=0;i<10;i++) {
			if (fread(&callers[i], sizeof(struct last10_callers), 1, fptr) < 1) {
				break;
			}
		}
		fclose(fptr);
	} else {
		i = 0;
	}

	if (strcasecmp(conf.sysop_name, user->loginname) != 0 ) {
		memset(&new_entry, 0, sizeof(struct last10_callers));
		strcpy(new_entry.name, user->loginname);
		strcpy(new_entry.location, user->location);
		new_entry.time = time(NULL);

		if (i == 10) {
			j = 1;
		} else {
			j = 0;
		}
		fptr = fopen("last10.dat", "wb");
		for (;j<i;j++) {
			fwrite(&callers[j], sizeof(struct last10_callers), 1, fptr);
		}
		fwrite(&new_entry, sizeof(struct last10_callers), 1, fptr);
		fclose(fptr);
	}
}

void display_last10_callers(struct user_record *user) {
	struct last10_callers callers[10];

	int i,z;
	struct tm l10_time;
	FILE *fptr = fopen("last10.dat", "rb");

	s_printf("\r\n\e[1;37mLast 10 callers:\r\n");
	s_printf("\e[1;30m-------------------------------------------------------------------------------\r\n");

	if (fptr != NULL) {

		for (i=0;i<10;i++) {
			if (fread(&callers[i], sizeof(struct last10_callers), 1, fptr) < 1) {
				break;
			}
		}

		fclose(fptr);
	} else {
		i = 0;
	}

	for (z=0;z<i;z++) {
		localtime_r(&callers[z].time, &l10_time);
		s_printf("\e[1;37m%-16s \e[1;36m%-32s \e[1;32m%02d:%02d %02d-%02d-%02d\e[0m\r\n", callers[z].name, callers[z].location, l10_time.tm_hour, l10_time.tm_min, l10_time.tm_mday, l10_time.tm_mon + 1, l10_time.tm_year - 100);
	}
	s_printf("\e[1;30m-------------------------------------------------------------------------------\e[0m\r\n");
	s_printf("Press any key to continue...\r\n");
	s_getc();
}

void display_info() {
	struct utsname name;

	uname(&name);

	s_printf("\r\n\r\n\e[1;37mSystem Information\r\n");
	s_printf("\e[1;30m----------------------------------------------\r\n");
	s_printf("\e[1;32mBBS Name    : \e[1;37m%s\r\n", conf.bbs_name);
	s_printf("\e[1;32mSysOp Name  : \e[1;37m%s\r\n", conf.sysop_name);
	s_printf("\e[1;32mNode        : \e[1;37m%d\r\n", mynode);
	s_printf("\e[1;32mBBS Version : \e[1;37mMagicka %d.%d (%s)\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
	s_printf("\e[1;32mSystem      : \e[1;37m%s (%s)\r\n", name.sysname, name.machine);
	s_printf("\e[1;30m----------------------------------------------\e[0m\r\n");

	s_printf("Press any key to continue...\r\n");
	s_getc();
}

void automessage_write(struct user_record *user) {
	FILE *fptr;
	char automsg[450];
	char buffer[76];
	int i;
	struct tm timenow;
	time_t timen;

	memset(automsg, 0, 450);
	memset(buffer, 0, 76);

	if (user->sec_level >= conf.automsgwritelvl) {
		timen = time(NULL);
		localtime_r(&timen, &timenow);

		sprintf(automsg, "Automessage Posted by %s @ %s", user->loginname, asctime(&timenow));

		automsg[strlen(automsg) - 1] = '\r';
		automsg[strlen(automsg)] = '\n';
		s_printf("\r\nEnter your message (4 lines):\r\n");
		for (i=0;i<4;i++) {
			s_printf("\r\n%d: ", i);
			s_readstring(buffer, 75);
			strcat(automsg, buffer);
			strcat(automsg, "\r\n");
		}

		fptr = fopen("automessage.txt", "w");
		if (fptr) {
			fwrite(automsg, strlen(automsg), 1, fptr);
			fclose(fptr);
		} else {
			dolog("Unable to open automessage.txt for writing");
		}
	}
}

void automessage_display() {
	struct stat s;
	FILE *fptr;
	char buffer[90];
	int i;
	s_printf("\r\n\r\n");
	if (stat("automessage.txt", &s) == 0) {
		fptr = fopen("automessage.txt", "r");
		if (fptr) {
			for (i=0;i<5;i++) {
				memset(buffer, 0, 90);
				fgets(buffer, 88, fptr);
				buffer[strlen(buffer) - 1] = '\r';
				buffer[strlen(buffer)] = '\n';

				s_printf(buffer);
			}
			fclose(fptr);
		} else {
			dolog("Error opening automessage.txt");
		}
	} else {
		s_printf("No automessage!\r\n");
	}
	s_printf("\e[0mPress any key to continue...\r\n");
	s_getc();
}

void runbbs_real(int socket, char *ip, int ssh) {
	char buffer[256];
	char password[17];

	struct stat s;
	FILE *nodefile;
	int i;
	char iac_echo[] = {255, 251, 1, '\0'};
	char iac_sga[] = {255, 251, 3, '\0'};
	struct user_record *user;
	struct tm thetime;
	struct tm oldtime;
	time_t now;
	struct itimerval itime;
	struct sigaction sa;
	lua_State *L;
	int do_internal_login = 0;

	ipaddress = ip;

	if (!ssh) {
		write(socket, iac_echo, 3);
		write(socket, iac_sga, 3);
		gUser = NULL;
		sshBBS = 0;
	} else {
		sshBBS = 1;
	}



	gSocket = socket;

	s_printf("Magicka BBS v%d.%d (%s), Loading...\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);

	// find out which node we are
	mynode = 0;
	for (i=1;i<=conf.nodes;i++) {
		sprintf(buffer, "%s/nodeinuse.%d", conf.bbs_path, i);
		if (stat(buffer, &s) != 0) {
			mynode = i;
			nodefile = fopen(buffer, "w");
			if (!nodefile) {
				dolog("Error opening nodefile!");
				close(socket);
				exit(1);
			}

			fputs("UNKNOWN", nodefile);
			fclose(nodefile);

			break;
		}
	}

	if (mynode == 0) {
		s_printf("Sorry, all nodes are in use. Please try later\r\n");
		if (!ssh) {
			close(socket);
		}
		exit(1);
	}

	usertimeout = 10;
	timeoutpaused = 0;

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &timer_handler;
	sa.sa_flags = SA_RESTART;
	sigaction (SIGALRM, &sa, 0);

	itime.it_interval.tv_sec = 60;
	itime.it_interval.tv_usec = 0;
	itime.it_value.tv_sec = 60;
	itime.it_value.tv_usec = 0;

	setitimer (ITIMER_REAL, &itime, 0);

	s_displayansi("issue");

	if (!ssh) {
		s_printf("\e[0mEnter your Login Name or NEW to create an account\r\n");
		s_printf("Login:> ");

		s_readstring(buffer, 25);

		if (strcasecmp(buffer, "new") == 0) {
			user = new_user();
		} else {
			s_printf("\r\nPassword:> ");
			s_readpass(password, 16);
			user = check_user_pass(buffer, password);
			if (user == NULL) {
				s_printf("\r\nIncorrect Login.\r\n");
				disconnect("Incorrect Login");
			}

			gUser = user;

			for (i=1;i<=conf.nodes;i++) {
				sprintf(buffer, "%s/nodeinuse.%d", conf.bbs_path, i);
				if (stat(buffer, &s) == 0) {
					nodefile = fopen(buffer, "r");
					if (!nodefile) {
						dolog("Error opening nodefile!");
						disconnect("Error opening nodefile!");
					}
					fgets(buffer, 256, nodefile);

					if (strcasecmp(user->loginname, buffer) == 0) {
						fclose(nodefile);
						s_printf("\r\nYou are already logged in.\r\n");
						disconnect("Already Logged in");
					}
					fclose(nodefile);
				}
			}
		}
	} else {
		if (gUser != NULL) {
			user = gUser;
			s_printf("\e[0mWelcome back %s. Press enter to log in...\r\n", gUser->loginname);
			s_getc();
			for (i=1;i<=conf.nodes;i++) {
				sprintf(buffer, "%s/nodeinuse.%d", conf.bbs_path, i);
				if (stat(buffer, &s) == 0) {
					nodefile = fopen(buffer, "r");
					if (!nodefile) {
						dolog("Error opening nodefile!");
						disconnect("Error opening nodefile!");
					}
					fgets(buffer, 256, nodefile);

					if (strcasecmp(user->loginname, buffer) == 0) {
						fclose(nodefile);
						s_printf("\r\nYou are already logged in.\r\n");
						disconnect("Already Logged in");
					}
					fclose(nodefile);
				}
			}
		} else {
		 	gUser = new_user();
			user = gUser;
		}
	}
	sprintf(buffer, "%s/nodeinuse.%d", conf.bbs_path, mynode);
	nodefile = fopen(buffer, "w");
	if (!nodefile) {
		dolog("Error opening nodefile!");
		close(socket);
		exit(1);
	}

	fputs(user->loginname, nodefile);
	fclose(nodefile);



	// do post-login
	dolog("%s logged in, on node %d", user->loginname, mynode);
	// check time left
	now = time(NULL);
	localtime_r(&now, &thetime);
	localtime_r(&user->laston, &oldtime);

	if (thetime.tm_mday != oldtime.tm_mday || thetime.tm_mon != oldtime.tm_mon || thetime.tm_year != oldtime.tm_year) {
		user->timeleft = user->sec_info->timeperday;
		user->laston = now;
		save_user(user);
	}

	user->timeson++;


	if (conf.script_path != NULL) {
		sprintf(buffer, "%s/login_stanza.lua", conf.script_path);
		if (stat(buffer, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_dofile(L, buffer);
			lua_close(L);
			do_internal_login = 0;
		} else {
			do_internal_login = 1;
		}
	} else {
		do_internal_login = 1;
	}

	if (do_internal_login == 1) {
		// bulletins
		i = 0;
		sprintf(buffer, "%s/bulletin%d.ans", conf.ansi_path, i);

		while (stat(buffer, &s) == 0) {
			sprintf(buffer, "bulletin%d", i);
			s_displayansi(buffer);
			s_printf("\e[0mPress any key to continue...\r\n");
			s_getc();
			i++;
			sprintf(buffer, "%s/bulletin%d.ans", conf.ansi_path, i);
		}

		// external login cmd

		// display info
		display_info();

		display_last10_callers(user);

		// check email
		i = mail_getemailcount(user);
		if (i > 0) {
			s_printf("\r\nYou have %d e-mail(s) in your inbox.\r\n", i);
		} else {
			s_printf("\r\nYou have no e-mail.\r\n");
		}

		mail_scan(user);

		automessage_display();
	}
	record_last10_callers(user);
	// main menu
	main_menu(user);

	s_displayansi("goodbye");
	dolog("%s is logging out, on node %d", user->loginname, mynode);
	disconnect("Log out");
}

void runbbs(int socket, char *ip) {
	runbbs_real(socket, ip, 0);
}

void runbbs_ssh(char *ip) {
	runbbs_real(-1, ip, 1);
}
