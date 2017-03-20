#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../../deps/jsmn/jsmn.h"

struct chat_msg {
    char nick[16];
    char bbstag[16];
    char msg[256];
};

struct client {
    char bbstag[16];
    char nick[16];
    int fd;
};

struct client **clients;
int client_count = 0;

typedef enum { START, KEY, PRINT, SKIP, STOP } parse_state;

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

int main(int argc, char **argv) {
    int port;
    int server_socket;
    struct sockaddr_in server, client;
    fd_set master, read_fds;
    int fdmax;
    int c;
    int new_fd;
    struct chat_msg msg;
    int i, j, k;
    char buffer[1024];
    char buf[1024];
    jsmn_parser parser;
    jsmntok_t tokens[8];
    int r;
    int nbytes;
    if (argc < 2) {
        printf("Usage: magichat [port]\n");
        return 0;
    }

    port = atoi(argv[1]);

    if (port <= 1024 && port > 65535) {
        printf("Invalid port number, must be between 1024 - 65535\n");
        return 0;
    }

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		fprintf(stderr, "Couldn't create socket..\n");
		exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Bind Failed, Error\n");
		exit(1);
	}

	listen(server_socket, 3);
    FD_ZERO(&master);

    FD_SET(server_socket, &master);
    fdmax = server_socket;

    c = sizeof(struct sockaddr_in);


    while (1) {
        read_fds = master;
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(-1);
        }

        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_socket) {
                    new_fd = accept(server_socket, (struct sockaddr *)&client, (socklen_t *)&c);
					if (new_fd == -1) {
                        perror("accept");
                    } else {
                        if (client_count == 0) {
                            clients = (struct client **)malloc(sizeof(struct client *));
                        } else {
                            clients = (struct client **)realloc(clients, sizeof(struct client *) * (client_count + 1));
                        }

                        if (!clients) {
                            fprintf(stderr, "Out of memory!\n");
                            return -1;
                        }
                        
                        clients[client_count] = (struct client *)malloc(sizeof(struct client));

                        if (!clients[client_count]) {
                            fprintf(stderr, "Out of memory!\n");
                            return -1;                            
                        }

                        sprintf(clients[client_count]->bbstag, "UNKNOWN");
                        sprintf(clients[client_count]->nick, "UNKNOWN");
                        clients[client_count]->fd = new_fd;
                        
                        client_count++;

                        FD_SET(new_fd, &master); 
                        if (new_fd > fdmax) {
                            fdmax = new_fd;
                        }
                    }                    
                } else {
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        for (k=0;k<client_count;k++) {
                            if (clients[k]->fd == i) {
                                if (strcmp(clients[k]->nick, "UNKNOWN") != 0) {
                                    snprintf(buffer, 1024, "{\"bbs\": \"SYSTEM\", \"nick\": \"SYSTEM\", \"msg\": \"%s (%s) has left the chat\" }", clients[k]->nick, clients[k]->bbstag);
                                    for (j=0;j<=fdmax;j++) {
                                        if (FD_ISSET(j, &master)) {
                                            if (j != server_socket && j != clients[k]->fd) {
                                                if (send(j, buffer, strlen(buffer) + 1, 0) == -1) {
                                                    perror("send");
                                                }
                                            }
                                        }
                                    }
                                }
                                free(clients[k]);

                                for (j=k;j<client_count-1;j++) {
                                    clients[j] = clients[j+1];
                                }

                                client_count--;

                                if (client_count == 0) {
                                    free(clients);
                                } else {
                                    clients = realloc(clients, sizeof(struct client) * (client_count));
                                }
                            }
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client

                        jsmn_init(&parser);                        
                        r = jsmn_parse(&parser, buf, nbytes, tokens, sizeof(tokens)/sizeof(tokens[0]));
     
                        if (r < 0) {
                            continue;
                        }

                        if (r < 1 || tokens[0].type != JSMN_OBJECT) {
		                	continue;
                        }

                      	for (j = 1; j < r; j++) {
                    		if (jsoneq(buf, &tokens[j], "bbs") == 0) {
			                    sprintf(msg.bbstag, "%.*s", tokens[j+1].end-tokens[j+1].start, buf + tokens[j+1].start);
			                    j++;
                            }
                    		if (jsoneq(buf, &tokens[j], "nick") == 0) {
			                    sprintf(msg.nick, "%.*s", tokens[j+1].end-tokens[j+1].start, buf + tokens[j+1].start);
			                    j++;
                            }
                            if (jsoneq(buf, &tokens[j], "msg") == 0) {
			                    sprintf(msg.msg, "%.*s", tokens[j+1].end-tokens[j+1].start, buf + tokens[j+1].start);
			                    j++;
                            }                     
                        } 

                        if (strcmp(msg.msg, "LOGIN") == 0) {
                            for (j=0;j<client_count;j++) {
                                if (clients[j]->fd == i) {
                                    strncpy(clients[j]->bbstag, msg.bbstag, 16);
                                    strncpy(clients[j]->nick, msg.nick, 16);

                                    for(k = 0; k < client_count; k++) {
                                        if (i != clients[k]->fd && strcmp(clients[k]->nick, "UNKNOWN") != 0) {
                                            snprintf(buffer, 1024, "{\"bbs\": \"SYSTEM\", \"nick\": \"SYSTEM\", \"msg\": \"%s (%s) has joined the chat\" }", clients[j]->nick, clients[j]->bbstag);
                                            if (send(k, buffer, strlen(buffer) + 1, 0) == -1) {
                                                perror("send");
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        } else {
                            for (j=0;j<client_count;j++) {
                                if (clients[j]->fd == i) {
                                    if (strcmp(clients[j]->nick, "UNKNOWN") != 0) {
                                        for(k = 0; k < client_count; k++) {
                                            if (i != clients[k]->fd && strcmp(clients[k]->nick, "UNKNOWN") != 0) {

                                                if (send(k, buf, nbytes, 0) == -1) {
                                                    perror("send");
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}