#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "deps/jsmn/jsmn.h"
#include "bbs.h"

extern struct bbs_config conf;
extern int mynode;
extern int gSocket;
extern int sshBBS;

static char **screenbuffer;
static int chat_socket;
static int line_at;
static int row_at;
static char sbuf[512];
extern struct user_record gUser;

struct chat_msg {
    char nick[16];
    char bbstag[16];
    char msg[256];
};

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static char *encapsulate_quote(char *in) {
	char out[160];
	int i;
	int j = 0;
	for (i=0;i<strlen(in);i++) {
		if (in[j] == '\"' || in[j] == '\\') {
			out[i++] = '\\';
		}
		out[i] = in[j];
		out[i+1] = '\0';
		j++;
	}
	return strdup(out);
}

void scroll_up() {
	int y;

	for (y=1;y<23;y++) {
		strcpy(screenbuffer[y-1], screenbuffer[y]);

	}
	memset(screenbuffer[22], 0, 81);
	row_at = 0;
}

void raw(char *fmt, ...) {

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sbuf, 512, fmt, ap);
    va_end(ap);
    write(chat_socket, sbuf, strlen(sbuf));
}

int hostname_to_ip(char * hostname , char* ip) {
	struct hostent *he;
	struct in_addr **addr_list;
	int i;

	if ( (he = gethostbyname( hostname ) ) == NULL)
    {
		// get the host info
        return 1;
    }

	addr_list = (struct in_addr **) he->h_addr_list;

	for(i = 0; addr_list[i] != NULL; i++) {
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}

    return 1;
}
void append_screenbuffer(char *buffer) {
	int z;

	for (z=0;z<strlen(buffer);z++) {
		if (row_at == 80) {
			if (line_at == 22) {
				scroll_up();
				row_at = 0;
			} else {
				row_at = 0;
				line_at++;
			}
		}

		screenbuffer[line_at][row_at] = buffer[z];
		row_at++;
		screenbuffer[line_at][row_at] = '\0';
	}
	if (line_at == 22) {
		scroll_up();
	}
	if (line_at < 22) {
		line_at++;
	}

	row_at = 0;
}

void chat_system(struct user_record *user) {
	struct sockaddr_in servaddr;
	fd_set fds;
	int t;
	int ret;
	char inputbuffer[80];
	int inputbuffer_at = 0;
	int len;
	char c;
	char buffer2[256];
	char buffer[513];
	char outputbuffer[513];
	char readbuffer[1024];
	int buffer_at = 0;
	int do_update = 1;
	int i;
	int j;
	int chat_in;
    jsmn_parser parser;
    jsmntok_t tokens[8];
    int r;
	struct chat_msg msg;
	char *input_b;

	if (sshBBS) {
		chat_in = STDIN_FILENO;
	} else {
		chat_in = gSocket;
	}

	memset(inputbuffer, 0, 80);
	if (conf.mgchat_server == NULL && conf.mgchat_bbstag != NULL) {
		s_putstring(get_string(49));
		return;
	}



	row_at = 0;
	line_at = 0;
	s_putstring("\e[2J\e[23;1H");
	s_putstring(get_string(50));
	s_putstring("\e[24;1H");

    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(conf.mgchat_port);


    if ( (chat_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return;
    }
    if (inet_pton(AF_INET, conf.mgchat_server, &servaddr.sin_addr) != 0) {
        hostname_to_ip(conf.mgchat_server, buffer);
        if (!inet_pton(AF_INET, buffer, &servaddr.sin_addr)) {
			return;
		}
    }
    if (connect(chat_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0 ) {
        return;
    }

	memset(buffer, 0, 513);

	screenbuffer = (char **)malloc(sizeof(char *) * 23);
    for (i=0;i<23;i++) {
		screenbuffer[i] = (char *)malloc(81);
		memset(screenbuffer[i], 0, 81);
	}

	raw("{ \"bbs\": \"%s\", \"nick\": \"%s\", \"msg\": \"LOGIN\" }", conf.mgchat_bbstag, user->loginname);

	while (1) {
		FD_ZERO(&fds);
		FD_SET(chat_in, &fds);
		FD_SET(chat_socket, &fds);

		if (chat_socket > chat_in) {
			t = chat_socket + 1;
		} else {
			t = chat_in + 1;
		}

		ret = select(t, &fds, NULL, NULL, NULL);

		if (ret > 0) {
			if (FD_ISSET(chat_in, &fds)) {
				len = read(chat_in, &c, 1);
				if (len == 0) {
					close(chat_socket);
					disconnect("Socket closed");
				}

				if (c == '\r') {
					if (inputbuffer[0] == '/') {
						if (strcasecmp(&inputbuffer[1], "quit") == 0) {
							close(chat_socket);
							for (i=0;i<22;i++) {
								free(screenbuffer[i]);
							}
							free(screenbuffer);
							return;
						}
					} else {
						input_b = encapsulate_quote(inputbuffer);
						raw("{ \"bbs\": \"%s\", \"nick\": \"%s\", \"msg\": \"%s\" }", conf.mgchat_bbstag, user->loginname, input_b);
						free(input_b);
						sprintf(buffer2, "%s: %s", user->loginname, inputbuffer);
						append_screenbuffer(buffer2);
						do_update = 1;
					}
					memset(inputbuffer, 0, 80);
					inputbuffer_at = 0;
				} else if (c != '\n') {
					if (c == '\b' || c == 127) {
						if (inputbuffer_at > 0) {
							inputbuffer_at--;
							inputbuffer[inputbuffer_at] = '\0';
							do_update = 2;
						}
					} else if (inputbuffer_at < 79) {
						inputbuffer[inputbuffer_at++] = c;
						do_update = 2;
					}
				}
			}
			if (FD_ISSET(chat_socket, &fds)) {
				len = read(chat_socket, readbuffer, 1024);
				if (len == 0) {
					s_putstring("\r\n\r\n\r\nLost connection to chat server!\r\n");
					for (i=0;i<22;i++) {
						free(screenbuffer[i]);
					}
					free(screenbuffer);
					return;
				}

				// json parse
				jsmn_init(&parser);				
                // we got some data from a client
                r = jsmn_parse(&parser, readbuffer, len, tokens, sizeof(tokens)/sizeof(tokens[0]));
     
                if ((r < 0) ||  (r < 1 || tokens[0].type != JSMN_OBJECT)) {
					// invalid json
				} else {
					for (j = 1; j < r; j++) {
                    	if (jsoneq(readbuffer, &tokens[j], "bbs") == 0) {
			            	sprintf(msg.bbstag, "%.*s", tokens[j+1].end-tokens[j+1].start, readbuffer + tokens[j+1].start);
			            	j++;
                        }
                    	if (jsoneq(readbuffer, &tokens[j], "nick") == 0) {
			                sprintf(msg.nick, "%.*s", tokens[j+1].end-tokens[j+1].start, readbuffer + tokens[j+1].start);
			                j++;
                        }
                        if (jsoneq(readbuffer, &tokens[j], "msg") == 0) {
			                sprintf(msg.msg, "%.*s", tokens[j+1].end-tokens[j+1].start, readbuffer + tokens[j+1].start);
			                j++;
                        }                     
                    } 
				}
				// set outputbuffer
				if (strcmp(msg.bbstag, "SYSTEM") == 0 && strcmp(msg.nick, "SYSTEM") == 0) {
					snprintf(outputbuffer, 512, ">> %s", msg.msg);
				} else {
					snprintf(outputbuffer, 512, "(%s)[%s]: %s", msg.bbstag, msg.nick, msg.msg);
				}
				// screen_append output buffer
				append_screenbuffer(outputbuffer);
				do_update = 1;


				memset(buffer, 0, 513);
				buffer_at = 0;
			}
		}
		if (do_update == 1) {
			s_putstring("\e[2J\e[1;1H");
			for (i=0;i<=line_at;i++) {
				s_printf("%s\r\n", screenbuffer[i]);
			}
			for (i=line_at+1;i<22;i++) {
				s_putstring("\r\n");
			}
			s_putstring(get_string(50));
			if (inputbuffer_at > 0) {
				s_putstring(inputbuffer);
			}
			do_update = 0;
		} else if (do_update == 2) {
			s_printf("\e[24;1f%s\e[K", inputbuffer);
		}
	}
}
