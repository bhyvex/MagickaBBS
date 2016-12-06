#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <string.h>
#include <poll.h>
#if defined(linux)
#  include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#  include <util.h>
#else
#  include <libutil.h>
#endif
#if defined(ENABLE_WWW)
#  include <microhttpd.h>
#endif
#include <termios.h>
#include "bbs.h"
#include "inih/ini.h"

extern struct bbs_config conf;
extern struct user_record *gUser;

int ssh_pid = -1;
int bbs_pid = 0;
int server_socket = -1;

#if defined(ENABLE_WWW)
struct MHD_Daemon *www_daemon;
#endif

void sigterm_handler(int s)
{
	if (ssh_pid != -1) {
		kill(ssh_pid, SIGTERM);
	}
	if (server_socket != -1) {
		close(server_socket);
	}
#if defined(ENABLE_WWW)
	if (www_daemon != NULL) {
		MHD_stop_daemon(www_daemon);
	}
#endif
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
		if (strcasecmp(name, "visible sec level") == 0) {
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
				} else if (strcasecmp(name, "qwk name") == 0) {
					mc->mail_areas[i]->qwkname = strdup(value);
					if (strlen(mc->mail_areas[i]->qwkname) > 8) {
						mc->mail_areas[i]->qwkname[8] = '\0';
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
		
		mc->mail_areas[mc->mail_area_count]->qwkname = NULL;
		
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
		} else if (strcasecmp(name, "qwk name") == 0) {
			mc->mail_areas[mc->mail_area_count]->qwkname = strdup(value);
			if (strlen(mc->mail_areas[mc->mail_area_count]->qwkname) > 8) {
				mc->mail_areas[mc->mail_area_count]->qwkname[8] = '\0';
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
		} else if (strcasecmp(name, "telnet port") == 0) {
			conf->telnet_port = atoi(value);
		} else if (strcasecmp(name, "enable ssh") == 0) {
			if (strcasecmp(value, "true") == 0) {
				conf->ssh_server = 1;
			} else {
				conf->ssh_server = 0;
			}
		} else if (strcasecmp(name, "enable www") == 0) {
			if (strcasecmp(value, "true") == 0) {
				conf->www_server = 1;
			} else {
				conf->www_server = 0;
			}
		} else if (strcasecmp(name, "www port") == 0) {
			conf->www_port = atoi(value);
		} else if (strcasecmp(name, "ssh port") == 0) {
			conf->ssh_port = atoi(value);
		} else if (strcasecmp(name, "ssh dsa key") == 0) {
			conf->ssh_dsa_key = strdup(value);
		} else if (strcasecmp(name, "ssh rsa key") == 0) {
			conf->ssh_rsa_key = strdup(value);
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
		} else if (strcasecmp(name, "qwk name") == 0) {
			conf->bwave_name = strdup(value);
			if (strlen(conf->bwave_name) > 8) {
				conf->bwave_name[8] = '\0';
			}
		} else if (strcasecmp(name, "main aka") == 0) {
			conf->main_aka = parse_fido_addr(value);
		} else if (strcasecmp(name, "qwk max messages") == 0) {
			conf->bwave_max_msgs = atoi(value);
		} else if (strcasecmp(name, "zip command") == 0) {
			conf->zip_cmd = strdup(value);
		} else if (strcasecmp(name, "unzip command") == 0) {
			conf->unzip_cmd = strdup(value);
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
		} else if (strcasecmp(name, "string file") == 0) {
			conf->string_file = strdup(value);
		} else if (strcasecmp(name, "www path") == 0) {
			conf->www_path = strdup(value);
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

int ssh_authenticate(ssh_session p_ssh_session) {
	ssh_message message;
	char *username;
	char *password;

	do {
		message = ssh_message_get(p_ssh_session);
		switch(ssh_message_type(message)) {
			case SSH_REQUEST_AUTH:
				switch(ssh_message_subtype(message)) {
					case SSH_AUTH_METHOD_PASSWORD:
						username = ssh_message_auth_user(message);
						password = ssh_message_auth_password(message);

						if (strcasecmp(username, "new") == 0 && strcasecmp(password, "new") == 0) {
							ssh_message_auth_reply_success(message, 0);
							ssh_message_free(message);
							gUser = NULL;
							return 1;
						}
						gUser = check_user_pass(username, password);
						if (gUser != NULL) {
							ssh_message_auth_reply_success(message, 0);
							ssh_message_free(message);
							return 1;
						}
						ssh_message_free(message);
						return 0;
					case SSH_AUTH_METHOD_NONE:
					default:
						ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE);
						ssh_message_reply_default(message);
						break;
				}
				break;
			default:
				ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE);
				ssh_message_reply_default(message);
				break;
		}
		ssh_message_free(message);
	} while(1);
}

char *ssh_getip(ssh_session session) {
  struct sockaddr_storage tmp;
  struct sockaddr_in *sock;
  unsigned int len = 100;
  char ip[100] = "\0";

  getpeername(ssh_get_fd(session), (struct sockaddr*)&tmp, &len);
  sock = (struct sockaddr_in *)&tmp;
  inet_ntop(AF_INET, &sock->sin_addr, ip, len);

	return strdup(ip);
}

static int ssh_copy_fd_to_chan(socket_t fd, int revents, void *userdata) {
  ssh_channel chan = (ssh_channel)userdata;
  char buf[2048];
  int sz = 0;

  if(!chan) {
    close(fd);
    return -1;
  }
  if(revents & POLLIN) {
    sz = read(fd, buf, 2048);
    if(sz > 0) {
      ssh_channel_write(chan, buf, sz);
    }
  }
  if(revents & POLLHUP) {
    ssh_channel_close(chan);
    sz = -1;
  }
  return sz;
}

static int ssh_copy_chan_to_fd(ssh_session session,
                                           ssh_channel channel,
                                           void *data,
                                           uint32_t len,
                                           int is_stderr,
                                           void *userdata) {
  int fd = *(int*)userdata;
  int sz;
  (void)session;
  (void)channel;
  (void)is_stderr;

  sz = write(fd, data, len);
  return sz;
}

static void ssh_chan_close(ssh_session session, ssh_channel channel, void *userdata) {
	int fd = *(int*)userdata;
	int status;
  (void)session;
  (void)channel;
	kill(bbs_pid, SIGTERM);
	waitpid(bbs_pid, &status, 0);
  close(fd);
}

struct ssh_channel_callbacks_struct ssh_cb = {
	.channel_data_function = ssh_copy_chan_to_fd,
  .channel_eof_function = ssh_chan_close,
  .channel_close_function = ssh_chan_close,
	.userdata = NULL
};

void serverssh(int port) {
	ssh_session p_ssh_session;
	ssh_bind p_ssh_bind;
	int err;
	int pid;
	int shell = 0;
	int fd;
	ssh_channel chan = 0;
	char *ip;
	ssh_event event;
	short events;
	ssh_message message;
	struct termios tios;


	err = ssh_init();
	if (err == -1) {
		fprintf(stderr, "Error starting SSH server.\n");
		exit(-1);
	}
	p_ssh_session = ssh_new();
	if (p_ssh_session == NULL) {
		fprintf(stderr, "Error starting SSH server.\n");
		exit(-1);
	}

	p_ssh_bind = ssh_bind_new();
	if (p_ssh_bind == NULL) {
		fprintf(stderr, "Error starting SSH server.\n");
		exit(-1);
	}

	ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_BINDPORT, &port);
	ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_DSAKEY, conf.ssh_dsa_key);
	ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_RSAKEY, conf.ssh_rsa_key);

	ssh_bind_listen(p_ssh_bind);

	while (1) {
		if (ssh_bind_accept(p_ssh_bind, p_ssh_session) == SSH_OK) {
			pid = fork();
			if (pid == 0) {
				if (ssh_handle_key_exchange(p_ssh_session)) {
					exit(-1);
				}
				if (ssh_authenticate(p_ssh_session) == 1) {
					do {
						message = ssh_message_get(p_ssh_session);
						if (message) {

							if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN && ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
								chan = ssh_message_channel_request_open_reply_accept(message);
								ssh_message_free(message);
								break;
							} else {
								ssh_message_reply_default(message);
								ssh_message_free(message);
							}
						} else {
							break;
						}
					} while(!chan);
					if (!chan) {
						fprintf(stderr, "Failed to get channel\n");
						ssh_finalize();
						exit(-1);
					}

					do {
						message = ssh_message_get(p_ssh_session);
						if (message) {
							if (ssh_message_type(message) == SSH_REQUEST_CHANNEL) {
								if (ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
									shell  = 1;
									ssh_message_channel_request_reply_success(message);
									ssh_message_free(message);
									break;
								} else if (ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_PTY) {
									ssh_message_channel_request_reply_success(message);
									ssh_message_free(message);
									continue;
								}
							}
						} else {
							break;
						}
					} while (!shell);

					if (!shell) {
						fprintf(stderr, "Failed to get shell\n");
						ssh_finalize();
						exit(-1);
					}

					ip = ssh_getip(p_ssh_session);



					bbs_pid = forkpty(&fd, NULL, NULL, NULL);
					if (bbs_pid == 0) {
						tcgetattr(STDIN_FILENO, &tios);
						tios.c_lflag &= ~(ICANON | ECHO | ECHONL);
						tios.c_iflag &= INLCR;
						tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios);
						runbbs_ssh(ip);
						exit(0);
					}
					free(ip);
					ssh_cb.userdata = &fd;
					ssh_callbacks_init(&ssh_cb);
					ssh_set_channel_callbacks(chan, &ssh_cb);

					events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;

					event = ssh_event_new();
					if(event == NULL) {
						ssh_finalize();
						exit(0);
					}
					if(ssh_event_add_fd(event, fd, events, ssh_copy_fd_to_chan, chan) != SSH_OK) {
						ssh_finalize();
						exit(0);
					}
					if(ssh_event_add_session(event, p_ssh_session) != SSH_OK) {
						ssh_finalize();
						exit(0);
					}

					do {
						ssh_event_dopoll(event, 1000);
					} while(!ssh_channel_is_closed(chan));

					ssh_event_remove_fd(event, fd);

					ssh_event_remove_session(event, p_ssh_session);

					ssh_event_free(event);
				}
				ssh_disconnect(p_ssh_session);
				ssh_finalize();

				exit(0);
			} else if (pid > 0) {

			} else {

			}
		}
	}
}

void server(int port) {
	struct sigaction sa;
	struct sigaction st;
	struct sigaction sq;
	int client_sock, c;
	int pid;
	struct sockaddr_in server, client;
#if defined(ENABLE_WWW)
	www_daemon = NULL;
#endif

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			perror("sigaction - sigchld");
			remove(conf.pid_file);
			exit(1);
	}

	st.sa_handler = sigterm_handler;
	sigemptyset(&st.sa_mask);
	st.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &st, NULL) == -1) {
			perror("sigaction - sigterm");
			remove(conf.pid_file);
			exit(1);
	}

	sq.sa_handler = sigterm_handler;
	sigemptyset(&sq.sa_mask);
	sq.sa_flags = SA_SIGINFO;
	if (sigaction(SIGQUIT, &sq, NULL) == -1) {
			perror("sigaction - sigquit");
			remove(conf.pid_file);
			exit(1);
	}

	if (conf.ssh_server) {
		// fork ssh server
		ssh_pid = fork();

		if (ssh_pid == 0) {
			ssh_pid = -1;
			serverssh(conf.ssh_port);
			exit(0);
		}
		if (ssh_pid < 0) {
			fprintf(stderr, "Error forking ssh server.");
		}
	}

#if defined(ENABLE_WWW) 
	if (conf.www_server && conf.www_path != NULL) {
		www_init();
		www_daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, conf.www_port, NULL, NULL, &www_handler, NULL, MHD_OPTION_NOTIFY_COMPLETED, &www_request_completed, NULL, MHD_OPTION_URI_LOG_CALLBACK, &www_logger, NULL, MHD_OPTION_END);
	}
#endif

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		remove(conf.pid_file);
		fprintf(stderr, "Couldn't create socket..\n");
		exit(1);
	}


	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Bind Failed, Error\n");
		remove(conf.pid_file);
		exit(1);
	}

	listen(server_socket, 3);

	c = sizeof(struct sockaddr_in);

	while ((client_sock = accept(server_socket, (struct sockaddr *)&client, (socklen_t *)&c))) {
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
			close(server_socket);
			server_socket = -1;
			runbbs(client_sock, strdup(inet_ntoa(client.sin_addr)));

			exit(0);
		} else {
			close(client_sock);
		}
	}
}

int main(int argc, char **argv) {
	int i;
	int main_pid;
	FILE *fptr;
	struct stat s;

	if (argc < 2) {
		fprintf(stderr, "Usage ./magicka config/bbs.ini\n");
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
	conf.telnet_port = 0;
	conf.string_file = NULL;
	conf.www_path = NULL;
	
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

	load_strings();

	if (conf.fork) {
		if (stat(conf.pid_file, &s) == 0) {
			fprintf(stderr, "Magicka already running or stale pid file at: %s\n", conf.pid_file);
			exit(-1);
		}

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
			server(conf.telnet_port);
		}
	} else {
		server(conf.telnet_port);
	}
}
