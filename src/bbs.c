#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <stdarg.h>
#include <fts.h>
#include <errno.h>
#include <sys/socket.h>
#include <iconv.h>
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"


int mynode = 0;
struct bbs_config conf;


struct user_record *gUser;
int gSocket;
int sshBBS;
int usertimeout;
int timeoutpaused;
time_t userlaston;

char *ipaddress = NULL;

void sigterm_handler2(int s)
{
	if (mynode != 0) {
		disconnect("Terminated.");
	}
	dolog("Terminated...");
	exit(0);
}

void sigint_handler(int s)
{
	// do nothing...
}
void broadcast(char *mess, ...) {
	char json[1024];
	char buffer[512];
    struct sockaddr_in s;
	int bcast_sock;
	int broadcastEnable=1;
	int ret;	
	
	
	
	if (conf.broadcast_enable && conf.broadcast_port > 1024 && conf.broadcast_port < 65536 && conf.broadcast_address != NULL) {
		bcast_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		ret=setsockopt(bcast_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
		
		if (ret) {
			dolog("broadcast: Couldn't set socket to broadcast mode");
			close(bcast_sock);
			return;
		}
		memset(&s, 0, sizeof(struct sockaddr_in));
		
		s.sin_family=AF_INET;
		s.sin_addr.s_addr = inet_addr(conf.broadcast_address);
		s.sin_port = htons((unsigned short)conf.broadcast_port);
		bind(bcast_sock, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
		
		va_list ap;
		va_start(ap, mess);
		vsnprintf(buffer, 512, mess, ap);
		va_end(ap);		
		
		snprintf(json, 1024, "{\"System\": \"%s\", \"Program\": \"MagickaBBS\", \"Message\": \"%s\"}", conf.bbs_name, buffer);

		ret = sendto(bcast_sock, json, strlen(json) + 1, 0, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
		
		if (ret < 0) {
			dolog("broadcast: Couldn't send broadcast");
		}
		close(bcast_sock);
	}
}

void dolog(char *fmt, ...) {
	char buffer[512];
	struct tm time_now;
	time_t timen;
	FILE *logfptr;
	int mypid = getpid();

	if (conf.log_path == NULL) return;

	timen = time(NULL);

	localtime_r(&timen, &time_now);

	snprintf(buffer, 512, "%s/%04d%02d%02d.log", conf.log_path, time_now.tm_year + 1900, time_now.tm_mon + 1, time_now.tm_mday);
	logfptr = fopen(buffer, "a");
    if (!logfptr) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, 512, fmt, ap);
	va_end(ap);

	fprintf(logfptr, "%02d:%02d:%02d [%d][%s] %s\n", time_now.tm_hour, time_now.tm_min, time_now.tm_sec, mypid, ipaddress, buffer);

	fclose(logfptr);
}

struct fido_addr *parse_fido_addr(const char *str) {
	if (str == NULL) {
		return NULL;
	}
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
				s_printf(get_string(0));
				disconnect("Out of Time");
			}


		}
		if (timeoutpaused == 0) {
			usertimeout--;
		}
		if (usertimeout <= 0) {
			s_printf(get_string(1));
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

int should_convert_utf8() {
	if (gUser != NULL) {
		return gUser->codepage;
	} 
	return conf.codepage;
}

void s_putchar(char c) {
	iconv_t ic;
	char *inbuf;
	char *outbuf;
	char *ptr1;
	char *ptr2;
	size_t inc;
	size_t ouc;
	size_t sz;

	if (!should_convert_utf8()) {
		if (sshBBS) {
			putchar(c);
		} else {
			write(gSocket, &c, 1);
		}
	} else {
		ic = iconv_open("UTF-8", "CP437");
		inbuf = (char *)malloc(4);
		outbuf = (char *)malloc(4);
		memset(outbuf, 0, 4);
		sprintf(inbuf, "%c", c);
		inc = 1;
		ouc = 4;
		ptr1 = outbuf;
		ptr2 = inbuf;
		sz = iconv(ic, &inbuf, &inc, &outbuf, &ouc);
		if (sshBBS) {
			fprintf(stdout, "%s", ptr1);
		} else {
			write(gSocket, ptr1, outbuf - ptr1);
		}
		iconv_close(ic);
		free(ptr1);
		free(ptr2);
	}
}

void s_putstring(char *c) {
	iconv_t ic;	
	char *inbuf;
	char *outbuf;
	size_t inc;
	size_t ouc;
	size_t sz;
	char *ptr1;
	char *ptr2;
	if (!should_convert_utf8()) {
		if (sshBBS) {
			fprintf(stdout, "%s", c);
		} else {
			write(gSocket, c, strlen(c));
		}
	} else {
		ic = iconv_open("UTF-8", "CP437");
		inc = strlen(c);
		inbuf = strdup(c);
		outbuf = (char *)malloc(inc * 4);
		memset(outbuf, 0, inc * 4);
		ptr1 = outbuf;
		ptr2 = inbuf;		
		ouc = inc * 4;
		sz = iconv(ic, &inbuf, &inc, &outbuf, &ouc);
		if (sshBBS) {
			fprintf(stdout, "%s", ptr1);
		} else {
			write(gSocket, ptr1, outbuf - ptr1);
		}
		iconv_close(ic);
		free(ptr1);
		free(ptr2);
	}
}

void s_displayansi_pause(char *file, int pause) {
	FILE *fptr;
	char c;
	char ch;
	int lines = 0;
	char lastch = 0;

	fptr = fopen(file, "r");
	if (!fptr) {
		return;
	}
	c = fgetc(fptr);
	while (!feof(fptr) && c != 0x1a) {
		if (c == '\n' && lastch != '\r') {
			s_putchar('\r');
		}
		s_putchar(c);

		lastch = c;
		
		if (pause) {
			if (c == '\n') {
				lines++;
				if (lines == 24) {
					s_printf(get_string(223));
					ch = s_getchar();
					s_printf("\r\n");
					switch(tolower(ch)) {
						case 'c':
							pause = 0;
							break;
						case 'n':
							fclose(fptr);
							return;
						default:
							break;
					}
					lines = 0;
				}
			}
		}
		c = fgetc(fptr);
	}
	fclose(fptr);
}

void s_displayansi_p(char *file) {
	s_displayansi_pause(file, 0);
}


void s_displayansi(char *file) {
	FILE *fptr;
	char c;

	char buffer[256];

	if (strchr(file, '/') == NULL) {
		sprintf(buffer, "%s/%s.ans", conf.ansi_path, file);
		s_displayansi_pause(buffer, 0);
	} else {
		s_displayansi_pause(file, 0);
	}
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
				if (c == 254 || c == 253 || c == 252 || c == 251) {
					len = read(gSocket, &c, 1);
					if (len == 0) {
						disconnect("Socket Closed");
					}
				} else if (c == 250) {
					do {
						len = read(gSocket, &c, 1);
						if (len == 0) {
							disconnect("Socket Closed");
						}
					} while(c != 240);
				} 
				
				len = read(gSocket, &c, 1);
				if (len == 0) {
					disconnect("Socket Closed");
				}
			}
		}
	} while (c == '\n' || c == '\0');
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
		} else if (c == 27) {
			c = s_getchar();
			if (c == 91) {
				c = s_getchar();
			}
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
		} else if (c == 27) {
			c = s_getchar();
			if (c == 91) {
				c = s_getchar();
			}
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

void exit_bbs() {
	char buffer[1024];	
	snprintf(buffer, 1024, "%s/nodeinuse.%d", conf.bbs_path, mynode);
	remove(buffer);
}

void disconnect(char *calledby) {

	if (gUser != NULL) {
		save_user(gUser);
	}
	dolog("Node %d disconnected (%s)", mynode, calledby);

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
	time_t l10_timet;

	s_printf(get_string(2));
	s_printf(get_string(3));

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
		l10_timet = callers[z].time;
		localtime_r(&l10_timet, &l10_time);
		if (conf.date_style == 1) {
			s_printf(get_string(4), callers[z].name, callers[z].location, l10_time.tm_hour, l10_time.tm_min, l10_time.tm_mon + 1, l10_time.tm_mday, l10_time.tm_year - 100);
		} else {
			s_printf(get_string(4), callers[z].name, callers[z].location, l10_time.tm_hour, l10_time.tm_min, l10_time.tm_mday, l10_time.tm_mon + 1, l10_time.tm_year - 100);
		}
	}
	s_printf(get_string(5));
	s_printf(get_string(6));
	s_getc();
}

void display_info() {
	struct utsname name;

	uname(&name);

	s_printf(get_string(7));
	s_printf(get_string(8));
	s_printf(get_string(9), conf.bbs_name);
	s_printf(get_string(10), conf.sysop_name);
	s_printf(get_string(11), mynode);
	s_printf(get_string(12), VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
	s_printf(get_string(13), name.sysname, name.machine);
	s_printf(get_string(14));

	s_printf(get_string(6));
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

		sprintf(automsg, get_string(15), user->loginname, asctime(&timenow));

		automsg[strlen(automsg) - 1] = '\r';
		automsg[strlen(automsg)] = '\n';
		s_printf(get_string(16));
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
		s_printf(get_string(17));
	}
	s_printf(get_string(6));
	s_getc();
}

void runbbs_real(int socket, char *ip, int ssh) {
	char buffer[1024];
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
	struct sigaction st;
	lua_State *L;
	int do_internal_login = 0;
	int usernotfound;
	int tries;
	
	atexit(exit_bbs);
	
	ipaddress = ip;

	if (!ssh) {
		gUser = NULL;
		sshBBS = 0;
		if (write(socket, iac_echo, 3) != 3) {
			dolog("Failed to send iac_echo");
			exit(0);			
		}
		if (write(socket, iac_sga, 3) != 3) {
			dolog("Failed to send iac_sga");
			exit(0);
		}
	} else {
		sshBBS = 1;
	}

	st.sa_handler = sigterm_handler2;
	sigemptyset(&st.sa_mask);
	st.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &st, NULL) == -1) {
		dolog("Failed to setup sigterm handler.");
		exit(1);
	}

	gSocket = socket;

	s_printf("Magicka BBS v%d.%d (%s), Loading...\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);

	// find out which node we are
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
		s_printf(get_string(18));
		if (!ssh) {
			close(socket);
		}
		exit(1);
	}

	dolog("Incoming connection on node %d", mynode);

	usertimeout = 10;
	timeoutpaused = 0;
	tries = 0;
	
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
tryagain:
		s_printf(get_string(19));
		s_printf(get_string(20));

		s_readstring(buffer, 25);

		usernotfound = 0;

		if (strcasecmp(buffer, "new") == 0) {
			usernotfound = 1;
		} else if (check_user(buffer)) {
			usernotfound = 1;
			s_printf(get_string(203));
		}
		
		if (usernotfound) {
			dolog("New user on node %d", mynode);
			user = new_user();
			gUser = user;
		} else {
			s_printf(get_string(21));
			s_readpass(password, 16);
			user = check_user_pass(buffer, password);
			if (user == NULL) {
				if (tries == 3) {
					s_printf(get_string(22));
					disconnect("Incorrect Login");
				} else {
					tries++;
					s_printf(get_string(22));
					goto tryagain;
				}
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
						s_printf(get_string(23));
						disconnect("Already Logged in");
					}
					fclose(nodefile);
				}
			}
		}
	} else {
		if (gUser != NULL) {
			user = gUser;
			s_printf(get_string(24), gUser->loginname);
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
						s_printf(get_string(23));
						disconnect("Already Logged in");
					}
					fclose(nodefile);
				}
			}
		} else {
			s_printf(get_string(25), conf.bbs_name);
			s_getc();
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

	sprintf(buffer, "%s/node%d/nodemsg.txt", conf.bbs_path, mynode);

	if (stat(buffer, &s) == 0) {
		unlink(buffer);
	}

	// do post-login
	dolog("%s logged in, on node %d", user->loginname, mynode);
	broadcast("%s logged in, on node %d", user->loginname, mynode);
	// check time left
	now = time(NULL);
	localtime_r(&now, &thetime);
	localtime_r(&user->laston, &oldtime);

	userlaston = user->laston;

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
		display_bulletins();

		// external login cmd

		// display info
		display_info();

		display_last10_callers(user);

		// check email
		i = mail_getemailcount(user);
		if (i > 0) {
			s_printf(get_string(26), i);
		} else {
			s_printf(get_string(27));
		}

		mail_scan(user);

		file_scan();

		automessage_display();
	}
	record_last10_callers(user);
	// main menu

	menu_system(conf.root_menu);
	
	do_logout();
	
	dolog("%s is logging out, on node %d", user->loginname, mynode);
	broadcast("%s is logging out, on node %d", user->loginname, mynode);
	disconnect("Log out");
}

void do_logout() {
	char buffer[256];
	struct stat s;
	lua_State *L;
	int ret = 0;
	char c;
	int result;
	int do_internal_logout = 1;
	
	if (conf.script_path != NULL) {
		sprintf(buffer, "%s/logout_stanza.lua", conf.script_path);
		if (stat(buffer, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_loadfile(L, buffer);
			result = lua_pcall(L, 0, 1, 0);
			if (result) {
				dolog("Failed to run script: %s", lua_tostring(L, -1));
				do_internal_logout = 1;
				lua_close(L);
			} else {
				do_internal_logout = 0;
			}
		}
	}

	if (do_internal_logout == 1) {
		s_displayansi("goodbye");
	} else {
		lua_getglobal(L, "logout");
		result = lua_pcall(L, 0, 0, 0);
		if (result) {
			dolog("Failed to run script: %s", lua_tostring(L, -1));
		}
		lua_close(L);
	}
}

void runbbs(int socket, char *ip) {
	runbbs_real(socket, ip, 0);
}

void runbbs_ssh(char *ip) {
	struct sigaction si;
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	si.sa_handler = sigint_handler;
	sigemptyset(&si.sa_mask);
	if (sigaction(SIGINT, &si, NULL) == -1) {
		dolog("Failed to setup sigint handler.");
		exit(1);
	}
	runbbs_real(-1, ip, 1);
}

int recursive_delete(const char *dir) {
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    char *files[] = { (char *) dir, NULL };

    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        dolog("%s: fts_open failed: %s", dir, strerror(errno));
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            dolog("%s: fts_read error: %s", curr->fts_accpath, strerror(curr->fts_errno));
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            break;

        case FTS_D:
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                dolog("%s: Failed to remove: %s", curr->fts_path, strerror(errno));
                ret = -1;
            }
            break;
        }
    }

finish:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret;
}

int copy_file(char *src, char *dest) {
	FILE *src_file;
	FILE *dest_file;

	char c;

	src_file = fopen(src, "rb");
	if (!src_file) {
		return -1;
	}
	dest_file = fopen(dest, "wb");
	if (!dest_file) {
		fclose(src_file);
		return -1;
	}

	while(1) {
		c = fgetc(src_file);
		if (!feof(src_file)) {
			fputc(c, dest_file);
		} else {
			break;
		}
	}

	fclose(src_file);
	fclose(dest_file);
	return 0;
}
