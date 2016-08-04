#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include "bbs.h"
#include "inih/ini.h"

extern struct bbs_config conf;

void sigterm_handler(int s)
{
	remove(conf.pid_file);
	exit(0);
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


static int door_config_handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct bbs_config *conf = (struct bbs_config *)user;
	int i;

	for (i=0;i<conf->door_count;i++) {
		if (strcasecmp(conf->doors[i]->name, section) == 0) {
			// found it
			if (strcasecmp(name, "key") == 0) {
				conf->doors[i]->key = value[0];
			} else if (strcasecmp(name, "command") == 0) {
				conf->doors[i]->command = strdup(value);
			} else if (strcasecmp(name, "stdio") == 0) {
				if (strcasecmp(value, "true") == 0) {
					conf->doors[i]->stdio = 1;
				} else {
					conf->doors[i]->stdio = 0;
				}
			}
			return 1;
		}
	}

	if (conf->door_count == 0) {
		conf->doors = (struct door_config **)malloc(sizeof(struct door_config *));
	} else {
		conf->doors = (struct door_config **)realloc(conf->doors, sizeof(struct door_config *) * (conf->door_count + 1));
	}

	conf->doors[conf->door_count] = (struct door_config *)malloc(sizeof(struct door_config));

	conf->doors[conf->door_count]->name = strdup(section);

	if (strcasecmp(name, "key") == 0) {
		conf->doors[conf->door_count]->key = value[0];
	} else if (strcasecmp(name, "command") == 0) {
		conf->doors[conf->door_count]->command = strdup(value);
	} else if (strcasecmp(name, "stdio") == 0) {
		if (strcasecmp(value, "true") == 0) {
			conf->doors[conf->door_count]->stdio = 1;
		} else {
			conf->doors[conf->door_count]->stdio = 0;
		}
	}
	conf->door_count++;

	return 1;
}

static int file_sub_handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct file_directory *fd = (struct file_directory *)user;
	int i;

	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "visible sec level")) {
			fd->sec_level = atoi(value);
		}
	} else {
		// check if it's partially filled in
		for (i=0;i<fd->file_sub_count;i++) {
			if (strcasecmp(fd->file_subs[i]->name, section) == 0) {
				if (strcasecmp(name, "upload sec level") == 0) {
					fd->file_subs[i]->upload_sec_level = atoi(value);
				} else if (strcasecmp(name, "download sec level") == 0) {
					fd->file_subs[i]->download_sec_level = atoi(value);
				} else if (strcasecmp(name, "database") == 0) {
					fd->file_subs[i]->database = strdup(value);
				} else if (strcasecmp(name, "upload path") == 0) {
					fd->file_subs[i]->upload_path = strdup(value);
				}
				return 1;
			}
		}
		if (fd->file_sub_count == 0) {
			fd->file_subs = (struct file_sub **)malloc(sizeof(struct file_sub *));
		} else {
			fd->file_subs = (struct file_sub **)realloc(fd->file_subs, sizeof(struct file_sub *) * (fd->file_sub_count + 1));
		}

		fd->file_subs[fd->file_sub_count] = (struct file_sub *)malloc(sizeof(struct file_sub));

		fd->file_subs[fd->file_sub_count]->name = strdup(section);
		if (strcasecmp(name, "upload sec level") == 0) {
			fd->file_subs[fd->file_sub_count]->upload_sec_level = atoi(value);
		} else if (strcasecmp(name, "download sec level") == 0) {
			fd->file_subs[fd->file_sub_count]->download_sec_level = atoi(value);
		} else if (strcasecmp(name, "database") == 0) {
			fd->file_subs[fd->file_sub_count]->database = strdup(value);
		} else if (strcasecmp(name, "upload path") == 0) {
			fd->file_subs[fd->file_sub_count]->upload_path = strdup(value);
		}
		fd->file_sub_count++;
	}
	return 1;
}


static int mail_area_handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct mail_conference *mc = (struct mail_conference *)user;
	int i;

	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "visible sec level") == 0) {
			mc->sec_level = atoi(value);
		} else if (strcasecmp(name, "networked") == 0) {
			if (strcasecmp(value, "true") == 0) {
				mc->networked = 1;
			} else {
				mc->networked = 0;
			}
		} else if (strcasecmp(name, "real names") == 0) {
			if (strcasecmp(value, "true") == 0) {
				mc->realnames = 1;
			} else {
				mc->realnames = 0;
			}
		} else if (strcasecmp(name, "tagline") == 0) {
			mc->tagline = strdup(value);
		}
	} else if (strcasecmp(section, "network") == 0) {
		if (strcasecmp(name, "type") == 0) {
			if (strcasecmp(value, "fido") == 0) {
				mc->nettype = NETWORK_FIDO;
			} else if (strcasecmp(value, "wwiv") == 0) {
				mc->nettype = NETWORK_WWIV;
			}
		} else if (strcasecmp(name, "fido node") == 0) {
			mc->fidoaddr = parse_fido_addr(value);
		} else if (strcasecmp(name, "wwiv node") == 0) {
			mc->wwivnode = atoi(value);
		}
	} else {
		// check if it's partially filled in
		for (i=0;i<mc->mail_area_count;i++) {
			if (strcasecmp(mc->mail_areas[i]->name, section) == 0) {
				if (strcasecmp(name, "read sec level") == 0) {
					mc->mail_areas[i]->read_sec_level = atoi(value);
				} else if (strcasecmp(name, "write sec level") == 0) {
					mc->mail_areas[i]->write_sec_level = atoi(value);
				} else if (strcasecmp(name, "path") == 0) {
					mc->mail_areas[i]->path = strdup(value);
				} else if (strcasecmp(name, "type") == 0) {
					if (strcasecmp(value, "local") == 0) {
						mc->mail_areas[i]->type = TYPE_LOCAL_AREA;
					} else if (strcasecmp(value, "echo") == 0) {
						mc->mail_areas[i]->type = TYPE_ECHOMAIL_AREA;
					} else if (strcasecmp(value, "netmail") == 0) {
						mc->mail_areas[i]->type = TYPE_NETMAIL_AREA;
					}
				}
				return 1;
			}
		}
		if (mc->mail_area_count == 0) {
			mc->mail_areas = (struct mail_area **)malloc(sizeof(struct mail_area *));
		} else {
			mc->mail_areas = (struct mail_area **)realloc(mc->mail_areas, sizeof(struct mail_area *) * (mc->mail_area_count + 1));
		}

		mc->mail_areas[mc->mail_area_count] = (struct mail_area *)malloc(sizeof(struct mail_area));

		mc->mail_areas[mc->mail_area_count]->name = strdup(section);
		if (strcasecmp(name, "read sec level") == 0) {
			mc->mail_areas[mc->mail_area_count]->read_sec_level = atoi(value);
		} else if (strcasecmp(name, "write sec level") == 0) {
			mc->mail_areas[mc->mail_area_count]->write_sec_level = atoi(value);
		} else if (strcasecmp(name, "path") == 0) {
			mc->mail_areas[mc->mail_area_count]->path = strdup(value);
		} else if (strcasecmp(name, "type") == 0) {
			if (strcasecmp(value, "local") == 0) {
				mc->mail_areas[mc->mail_area_count]->type = TYPE_LOCAL_AREA;
			} else if (strcasecmp(value, "echo") == 0) {
				mc->mail_areas[mc->mail_area_count]->type = TYPE_ECHOMAIL_AREA;
			} else if (strcasecmp(value, "netmail") == 0) {
				mc->mail_areas[mc->mail_area_count]->type = TYPE_NETMAIL_AREA;
			}
		}
		mc->mail_area_count++;
	}
	return 1;
}

static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
	struct bbs_config *conf = (struct bbs_config *)user;

	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "bbs name") == 0) {
			conf->bbs_name = strdup(value);
		} else if (strcasecmp(name, "sysop name") == 0) {
			conf->sysop_name = strdup(value);
		} else if (strcasecmp(name, "nodes") == 0) {
			conf->nodes = atoi(value);
		} else if (strcasecmp(name, "new user level") == 0) {
			conf->newuserlvl = atoi(value);
		} else if (strcasecmp(name, "irc server") == 0) {
			conf->irc_server = strdup(value);
		} else if (strcasecmp(name, "irc port") == 0) {
			conf->irc_port = atoi(value);
		} else if (strcasecmp(name, "irc channel") == 0) {
			conf->irc_channel = strdup(value);
		} else if (strcasecmp(name, "default tagline") == 0) {
			conf->default_tagline = strdup(value);
		} else if (strcasecmp(name, "external editor cmd") == 0) {
			conf->external_editor_cmd = strdup(value);
		} else if (strcasecmp(name, "external editor stdio") == 0) {
			if (strcasecmp(value, "true") == 0) {
				conf->external_editor_stdio = 1;
			} else {
				conf->external_editor_stdio = 0;
			}
		} else if (strcasecmp(name, "automessage write level") == 0) {
			conf->automsgwritelvl = atoi(value);
		} else if (strcasecmp(name, "fork") == 0)  {
			if (strcasecmp(value, "true") == 0) {
				conf->fork = 1;
			} else {
				conf->fork = 0;
			}
		}
	} else if (strcasecmp(section, "paths") == 0){
		if (strcasecmp(name, "ansi path") == 0) {
			conf->ansi_path = strdup(value);
		} else if (strcasecmp(name, "bbs path") == 0) {
			conf->bbs_path = strdup(value);
		} else if (strcasecmp(name, "log path") == 0) {
			conf->log_path = strdup(value);
		} else if (strcasecmp(name, "script path") == 0) {
			conf->script_path = strdup(value);
		} else if (strcasecmp(name, "echomail semaphore") == 0) {
			conf->echomail_sem = strdup(value);
		} else if (strcasecmp(name, "netmail semaphore") == 0) {
			conf->netmail_sem = strdup(value);
		} else if (strcasecmp(name, "pid file") == 0) {
			conf->pid_file = strdup(value);
		}
	} else if (strcasecmp(section, "mail conferences") == 0) {
		if (conf->mail_conference_count == 0) {
			conf->mail_conferences = (struct mail_conference **)malloc(sizeof(struct mail_conference *));
		} else {
			conf->mail_conferences = (struct mail_conference **)realloc(conf->mail_conferences, sizeof(struct mail_conference *) * (conf->mail_conference_count + 1));
		}

		conf->mail_conferences[conf->mail_conference_count] = (struct mail_conference *)malloc(sizeof(struct mail_conference));
		conf->mail_conferences[conf->mail_conference_count]->name = strdup(name);
		conf->mail_conferences[conf->mail_conference_count]->path = strdup(value);
		conf->mail_conferences[conf->mail_conference_count]->tagline = NULL;
		conf->mail_conferences[conf->mail_conference_count]->mail_area_count = 0;
		conf->mail_conferences[conf->mail_conference_count]->nettype = 0;
		conf->mail_conference_count++;
	} else if (strcasecmp(section, "file directories") == 0) {
		if (conf->file_directory_count == 0) {
			conf->file_directories = (struct file_directory **)malloc(sizeof(struct file_directory *));
		} else {
			conf->file_directories = (struct file_directory **)realloc(conf->file_directories, sizeof(struct file_directory *) * (conf->file_directory_count + 1));
		}

		conf->file_directories[conf->file_directory_count] = (struct file_directory *)malloc(sizeof(struct file_directory));
		conf->file_directories[conf->file_directory_count]->name = strdup(name);
		conf->file_directories[conf->file_directory_count]->path = strdup(value);
		conf->file_directories[conf->file_directory_count]->file_sub_count = 0;
		conf->file_directory_count++;
	} else if (strcasecmp(section, "text files") == 0) {
		if (conf->text_file_count == 0) {
			conf->text_files = (struct text_file **)malloc(sizeof(struct text_file *));
		} else {
			conf->text_files = (struct text_file **)realloc(conf->text_files, sizeof(struct text_file *) * (conf->text_file_count + 1));
		}

		conf->text_files[conf->text_file_count] = (struct text_file *)malloc(sizeof(struct text_file));
		conf->text_files[conf->text_file_count]->name = strdup(name);
		conf->text_files[conf->text_file_count]->path = strdup(value);
		conf->text_file_count++;

	}

	return 1;
}

void server(int port) {
	struct sigaction sa;
	struct sigaction st;
	int socket_desc, client_sock, c, *new_sock;
	int pid;
	struct sockaddr_in server, client;

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			perror("sigaction");
			exit(1);
	}

	st.sa_handler = sigterm_handler;
	sigemptyset(&st.sa_mask);
	if (sigaction(SIGTERM, &st, NULL) == -1) {
			perror("sigaction");
			exit(1);
	}

	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1) {
		fprintf(stderr, "Couldn't create socket..\n");
		exit(1);
	}


	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Bind Failed, Error\n");
		exit(1);
	}

	listen(socket_desc, 3);

	c = sizeof(struct sockaddr_in);

	while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c))) {
		if (client_sock == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				exit(-1);
			}
		}
		pid = fork();

		if (pid < 0) {
			perror("Error on fork\n");
			exit(1);
		}

		if (pid == 0) {
			close(socket_desc);
			runbbs(client_sock, strdup(inet_ntoa(client.sin_addr)));

			exit(0);
		} else {
			close(client_sock);
		}
	}
}

int main(int argc, char **argv) {
	int port;

	int i;
	int main_pid;
	FILE *fptr;

	if (argc < 3) {
		fprintf(stderr, "Usage ./magicka config/bbs.ini port\n");
		exit(1);
	}

	conf.mail_conference_count = 0;
	conf.door_count = 0;
	conf.file_directory_count = 0;
	conf.irc_server = NULL;
	conf.irc_port = 6667;
	conf.text_file_count = 0;
	conf.external_editor_cmd = NULL;
	conf.log_path = NULL;
	conf.script_path = NULL;
	conf.automsgwritelvl = 10;
	conf.echomail_sem = NULL;
	conf.netmail_sem = NULL;

	// Load BBS data
	if (ini_parse(argv[1], handler, &conf) <0) {
		fprintf(stderr, "Unable to load configuration ini (%s)!\n", argv[1]);
		exit(-1);
	}
	// Load mail Areas
	for (i=0;i<conf.mail_conference_count;i++) {
		if (ini_parse(conf.mail_conferences[i]->path, mail_area_handler, conf.mail_conferences[i]) <0) {
			fprintf(stderr, "Unable to load configuration ini (%s)!\n", conf.mail_conferences[i]->path);
			exit(-1);
		}
	}
	// Load file Subs
	for (i=0;i<conf.file_directory_count;i++) {
		if (ini_parse(conf.file_directories[i]->path, file_sub_handler, conf.file_directories[i]) <0) {
			fprintf(stderr, "Unable to load configuration ini (%s)!\n", conf.file_directories[i]->path);
			exit(-1);
		}
	}

	if (ini_parse("config/doors.ini", door_config_handler, &conf) <0) {
		fprintf(stderr, "Unable to load configuration ini (doors.ini)!\n");
		exit(-1);
	}

	port = atoi(argv[2]);

	if (conf.fork) {
		main_pid = fork();

		if (main_pid < 0) {
			fprintf(stderr, "Error forking.\n");
			exit(-1);
		} else
		if (main_pid > 0) {
			fptr = fopen(conf.pid_file, "w");
			if (!fptr) {
				fprintf(stderr, "Unable to open pid file for writing.\n");
			} else {
				fprintf(fptr, "%d", main_pid);
				fclose(fptr);
			}
		} else {
			server(port);
		}
	} else {
		server(port);
	}
}
