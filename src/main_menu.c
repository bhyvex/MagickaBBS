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
		s_displayansi_pause(buffer, 1);
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

	s_printf(get_string(217));
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
	int redraw = 1;
	int start = 0;
	int selected = 0;
	char c;

	if (conf.text_file_count == 0) {
		s_printf("\e[2J\e[1;1H");
		s_printf(get_string(148));
		s_printf(get_string(185));
		s_getc();
		s_printf("\r\n");			
		return;
	}

	while (1) {
		if (redraw) {
			s_printf("\e[2J\e[1;1H");
			s_printf(get_string(143));
			s_printf(get_string(144));
			for (i=start;i<start+22 && i < conf.text_file_count;i++) {
				if (i == selected) {
					s_printf(get_string(249), i - start + 2, i, conf.text_files[i]->name);
				} else {
					s_printf(get_string(250), i - start + 2, i, conf.text_files[i]->name);
				}
			}
			s_printf("\e[%d;5H", selected - start + 2);
			redraw = 0;
		}
		c = s_getchar();
		if (tolower(c) == 'q') {
			break;
		} else if (c == 27) {
			c = s_getchar();
			if (c == 91) {
				c = s_getchar();
				if (c == 66) {
					// down
					if (selected + 1 >= start + 22) {
						start += 22;
						if (start >= conf.text_file_count) {
							start = conf.text_file_count - 22;
						}
						redraw = 1;
					}
					selected++;
					if (selected >= conf.text_file_count) {
						selected = conf.text_file_count - 1;
					} else {
						if (!redraw) {		
							s_printf(get_string(250), selected - start + 1, selected - 1, conf.text_files[selected - 1]->name);
							s_printf(get_string(249), selected - start + 2, selected, conf.text_files[selected]->name);
							s_printf("\e[%d;5H", selected - start + 2);
						}
					}
				} else if (c == 65) {
					// up
					if (selected - 1 < start) {
						start -= 22;
						if (start < 0) {
							start = 0;
						}
						redraw = 1;
					}
					selected--;
					if (selected < 0) {
						selected = 0;
					} else {
						if (!redraw) {	
							s_printf(get_string(249), selected - start + 2, selected, conf.text_files[selected]->name);
							s_printf(get_string(250), selected - start + 3, selected + 1, conf.text_files[selected + 1]->name);
							s_printf("\e[%d;5H", selected - start + 2);
						}	
					}
				} else if (c == 75) {
					// END KEY
					selected = conf.text_file_count - 1;
					start = conf.text_file_count - 22;
					if (start < 0) {
						start = 0;
					}
					redraw = 1;
				} else if (c == 72) {
					// HOME KEY
					selected = 0;
					start = 0;
					redraw = 1;
				} else if (c == 86 || c == '5') {
					if (c == '5') {
						s_getchar();
					}
					// PAGE UP
					selected = selected - 22;
					if (selected < 0) {
						selected = 0;
					}
					start = selected;
					redraw = 1;
				} else if (c == 85 || c == '6') {
					if (c == '6') {
						s_getchar();
					}
					// PAGE DOWN
					selected = selected + 22;
					if (selected >= conf.text_file_count) {
						selected = conf.text_file_count -1;
					}
					start = selected;
					redraw = 1;
				}
			}
		} else if (c == 13) {
			s_printf("\e[2J\e[1;1H");
			s_displayansi_p(conf.text_files[selected]->path);
			s_printf(get_string(185));
			s_getc();
			s_printf("\r\n");			
			redraw = 1;
		}
	}
}
