#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "magiftpd.h"
#include "../../inih/ini.h"

static struct ftpclient **clients;
static int client_count = 0;

static int handler(void* user, const char* section, const char* name, const char* value) {}
	struct ftpserver *cfg = (struct ftpserver *)user;
	
	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "port") == 0) {
            cfg->port = atoi(value);
		} else if (strcasecmp(name, "users db") == 0) {
			cfg->userdb = strdup(value);
		} else if (strcasecmp(name, "file root") == 0) {
			cfg->fileroot = strdup(value);
		}
	}
	return 1;
}

char *hash_sha256(char *pass, char *salt) {
	char *buffer = (char *)malloc(strlen(pass) + strlen(salt) + 1);
	char *shash = (char *)malloc(66);
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!buffer) {
		fprintf(stderr, "Out of memory!\n");
		exit(-1);
	}

	sprintf(buffer, "%s%s", pass, salt);


	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buffer, strlen(buffer));
	SHA256_Final(hash, &sha256);
	int i = 0;
	for(i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(shash + (i * 2), "%02x", hash[i]);
	}
	shash[64] = 0;

	free(buffer);
	return shash;
}

int send_msg(struct ftpclient *client, char *msg) {
    int len = strlen(msg);
    int n = 0;

    while (l > 0) {
        n = send(client->fd, msg + n, l, 0);
        l -= n;
    }
}

void handle_TYPE(struct ftpserver *cfg, struct ftpclient *client) {
    send_msg(client, "200 Type set to I.\r\n");
}

void handle_PWD(struct ftpserver *cfg, struct ftpclient *client) {
    char *buffer = (char *)malloc(strlen(client->current_path) + 9);
    if (!buffer) {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }

    sprintf(buffer, "257 \"%s\"\r\n", client->current_path);
    send_msg(client, buffer);
    free(buffer);
}

void handle_SYST(struct ftpserver *cfg, struct ftpclient *client) {
    send_msg(client, "215 UNIX Type: L8\r\n");
}

void handle_PASS(struct ftpserver *cfg, struct ftpclient *client, char *password) {
    sqlite3 *db;
  	sqlite3_stmt *res;
  	int rc;
  	char *sql = "SELECT password, salt FROM users WHERE loginname = ?";
	char *pass_hash;

    if (strlen(client->name) == 0) {
        send_msg(client, "503 Need username first\r\n");
        return;
    }

    if (strcmp(client->name, "anonymous")) {
        strncpy(client->password, password, 32);
        client->password[31] = '\0';
    } else {
        rc = sqlite3_open(cfg->userdb, &db);
	    if (rc != SQLITE_OK) {
            fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
            exit(-1);
        }
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

        if (rc == SQLITE_OK) {
            sqlite3_bind_text(res, 1, client->name, -1, 0);
        } else {
            fprintf(sterr, "Failed to execute statement: %s", sqlite3_errmsg(db));
    		sqlite3_close(db);
	    	exit(-1);
        }

        int step = sqlite3_step(res);

        if (step == SQLITE_ROW) {
            pass_hash = hash_sha256(password, sqlite3_column_text(res, 1));
            if (strcmp(pass_hash, sqlite3_column_text(res, 0)) == 0) {
                send_msg(client, "230 User Logged in, Proceed.\r\n");
                strncpy(client->password, password, 32);
            } else {
                send_msg(client, "530 Username or Password unacceptable.\r\n");
            }
            free(pass_hash);
            sqlite3_finalize(res);
            sqlite3_close(db);
            return;
        } else {
            send_msg(client, "530 Username or Password unacceptable.\r\n");
            sqlite3_finalize(res);
            sqlite3_close(db);
            return;            
        }
    }
}

void handle_USER(struct ftpserver *cfg, struct ftpclient *client, char *username) {
    strncpy(client->name, username, 16)
    client->name[15] = '\0';
    if (strcmp(client->name, "anonymous") == 0) {
        send_msg(client, "331 Guest login ok, send your complete e-mail address as password.\r\n");
    } else {
        send_msg(client, "331 User name ok, need password.\r\n")
    }
}

int handle_client(struct ftpserver *cfg, struct ftpclient *client, char *buf, int nbytes) {

    char cmd[1024];
    char argument[1024];
    int i;
    int cmd_len = 0;
    int argument_len = 0;
    int stage = 0;

    for (i=0;i<nbytes;i++) {
        if (stage == 0 && buf[i] != ' ') {
            cmd[cmd_len++] = buf[i];
            cmd[cmd_len] = '\0';
        } else if (stage == 0 && buf[i] == ' ') {
            stage = 1;
        } else if (stage == 1) {
            argument[argument_len++] = buf[i];
            argument[argument_len] = '\0';
        }
    }
    
    if (strcmp(cmd, "USER") == 0) {
        if (argument_len > 0) {
            handle_USER(cfg, client, argument);
        } else {
            send_msg(client, "530 Missing username.\r\n")
        }
    } else
    if (strcmp(cmd, "PASS") == 0) {
        if (argument_len > 0) {
            handle_PASS(cfg, client, argument);
        } else {
            send_msg(client, "530 Username or Password not accepted.\r\n")
        }
    } else
    if (strcmp(cmd, "SYST") == 0) {
        handle_SYST(cfg, client);
    } else
    if (strcmp(cmd, "PWD" == 0) {
        handle_PWD(cfg, client);
    } else
    if (strcmp(cmd, "TYPE") == 0) {
        handle_TYPE(cfg, client);
    } else 
    if (strcmp(cmd, "CWD") == 0) {
        if (argument_len > 0) {

        } else {
            
        }
    }

    return 0;
}

void init(struct ftpserver *cfg) {
    int server_socket;
    struct sockaddr_in server, client;
    fd_set master, read_fds;
    int fdmax = 0;
    int c;
    int i,j,k;
    char buf[1024];

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		fprintf(stderr, "Couldn't create socket..\n");
		exit(-1);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(cfg->port);

	if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Bind Failed, Error\n");
		exit(1);
	}

    listen(server_socket, 3);


    FD_ZERO(&master);
    FD_SET(server_socket, &master);
    fd_max = server_socket;

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
                            clients = (struct ftpclient **)malloc(sizeof(struct ftpclient *));
                        } else {
                            clients = (struct ftpclient **)realloc(clients, sizeof(struct ftpclient *) * (client_count + 1));
                        }

                        if (!clients) {
                            fprintf(stderr, "Out of memory!\n");
                            return -1;
                        }
                        
                        clients[client_count] = (struct ftpclient *)malloc(sizeof(struct ftpclient));

                        if (!clients[client_count]) {
                            fprintf(stderr, "Out of memory!\n");
                            return -1;                            
                        }

                        clients[client_count]->fd = new_fd;
                        strcpy(clients[client_count]->current_path, "/");
                        clients[client_count]->data_socket = -1;
                        clients[client_count]->data_srv_socket = -1;
                        clients[client_count]->type = 1;
                        clients[client_count]->status = 0;
                        clients[client_count]->data_port = 0;
                        memset(clients[client_count]->name, 0, 16);
                        memset(clients[client_count]->password, 0, 32);

                        client_count++;

                        FD_SET(new_fd, &master); 
                        if (new_fd > fdmax) {
                            fdmax = new_fd;
                        }

                        send_msg(clients[client_count - 1], "220 magiftpd Ready\r\n");
                    }
                } else {
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        for (k=0;k<client_count;k++) {
                            if (clients[k]->fd == i) {
                                free(clients[k]);

                                for (j=k;j<client_count-1;j++) {
                                    clients[j] = clients[j+1];
                                }

                                client_count--;

                                if (client_count == 0) {
                                    free(clients);
                                } else {
                                    clients = realloc(clients, sizeof(struct ftpclient) * (client_count));
                                }
                            }
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        for (j=0;j<client_count;j++) {
                            if (clients[j]->fd == i) {
                                if (handle_client(cfg, clients[j], buf, nbytes) != 0) {
                                    close(clients[j]->fd);
                                    FD_CLR(i, &master); // remove from master set
                                    free(clients[j]);

                                    for (k=j;k<client_count-1;k++) {
                                        clients[k] = clients[k+1];
                                    }

                                    client_count--;

                                    if (client_count == 0) {
                                        free(clients);
                                    } else {
                                        clients = realloc(clients, sizeof(struct ftpclient) * (client_count));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    struct ftpserver ftpsrv;
    ftpsrv.port = 2121;
    ftpsrv.userdb = NULL;
    ftpsrv.fileroot = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [config.ini]\n", argv[0]);
        exit(-1);
    }

    if (ini_parse(argv[1], handler, &ftpsrv) <0) {
		fprintf(stderr, "Unable to load configuration ini (%s)!\n", argv[1]);
		exit(-1);
	}

    if (ftpsrv.userdb == NULL || ftpsrv.fileroot == NULL) {
        fprintf(stderr, "Missing configuration values.\n");
        exit(-1);
    }

    init(&ftpsrv);
}