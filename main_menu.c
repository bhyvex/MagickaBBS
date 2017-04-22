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
extern struct user_record *gUser;
extern int mynode;

void display_bulletins() {
	int i;
	char buffer[PATH_MAX];
	struct stat s;
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

void active_nodes() {
	int i;
	struct stat s;
	char buffer[PATH_MAX];
	FILE *fptr;

	for (i=0;i<conf.nodes;i++) {
		snprintf(buffer, PATH_MAX, "%s/nodeinuse.%d", conf.bbs_path, i+1);
		if (stat(buffer, &s) == 0) {
			fptr = fopen(buffer, "r");
			if (fptr) {	
				fgets(buffer, PATH_MAX, fptr);
				fclose(fptr);
				chomp(buffer);
				s_printf(get_string(216), i+1, buffer);
			}
		}
	}
}

void send_node_msg() {
	char buffer[PATH_MAX];
	char msg[257];
	int nodetomsg = 0;
	struct stat s;
	FILE *fptr;

	s_printf(get_string(216));
	active_nodes();

	s_readstring(buffer, 4);
	nodetomsg = atoi(buffer);

	if (nodetomsg < 1 || nodetomsg > conf.nodes) {
		s_printf(get_string(218));
		return;
	}
	s_printf(get_string(219));

	s_readstring(msg, 256);

	snprintf(buffer, PATH_MAX, "%s/node%d", conf.bbs_path, nodetomsg);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}
	snprintf(buffer, PATH_MAX, "%s/node%d/nodemsg.txt", conf.bbs_path, nodetomsg);

	fptr = fopen(buffer, "a");
	if (fptr) {
		fprintf(fptr, get_string(220), gUser->loginname, mynode, msg);
		fclose(fptr);
	}
}

void display_textfiles() {
	int i;
	char buffer[5];

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