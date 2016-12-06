#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include "Xmodem/zmodem.h"
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
extern struct bbs_config conf;
extern int gSocket;
extern int sshBBS;

struct file_entry {
	char *filename;
	char *description;
	int size;
	int dlcount;
};

char **tagged_files;
int tagged_count = 0;

int ttySetRaw(int fd, struct termios *prevTermios) {
    struct termios t;

    if (tcgetattr(fd, &t) == -1)
        return -1;

    if (prevTermios != NULL)
        *prevTermios = t;

    t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);
    t.c_oflag &= ~OPOST;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
        return -1;

    return 0;
}

int ZXmitStr(u_char *str, int len, ZModem *info) {
	int i;

	for (i=0;i<len;i++) {
		if (str[i] == 255 && !sshBBS) {
			if (write(info->ofd, &str[i], 1) == 0) {
				return ZmErrSys;
			}
		}
		if (write(info->ofd, &str[i], 1) == 0) {
			return ZmErrSys;
		}
	}

	return 0;
}

void ZIFlush(ZModem *info) {
}

void ZOFlush(ZModem *info) {
}

int ZAttn(ZModem *info) {
	char *ptr ;

	if( info->attn == NULL )
		return 0 ;

	for(ptr = info->attn; *ptr != '\0'; ++ptr) {
		if( *ptr == ATTNBRK ) {

		} else if( *ptr == ATTNPSE ) {
			sleep(1);
		} else {
			write(info->ifd, ptr, 1) ;
		}
	}
	return 0;
}

void ZFlowControl(int onoff, ZModem *info) {
}

void ZStatus(int type, int value, char *status) {
}


char *upload_path;
char upload_filename[1024];

FILE *ZOpenFile(char *name, u_long crc, ZModem *info) {

	FILE *fptr;
	struct stat s;

	snprintf(upload_filename, 1023, "%s/%s", upload_path, basename(name));
	if (stat(upload_filename, &s) == 0) {
		return NULL;
	}

	fptr = fopen(upload_filename, "wb");

	return fptr;
}

int	ZWriteFile(u_char *buffer, int len, FILE *file, ZModem *info) {
	return fwrite(buffer, 1, len, file) == len ? 0 : ZmErrSys;
}

int	ZCloseFile(ZModem *info) {
	fclose(info->file);
	return 0;
}

void ZIdleStr(u_char *buffer, int len, ZModem *info) {
}

int doIO(ZModem *zm) {
	fd_set readfds;
	struct timeval timeout;
	u_char buffer[2048];
	u_char buffer2[1024];
	int	len;
	int pos;
	int done = 0;
	int	i;
	int j;

	while(!done) {
		FD_ZERO(&readfds);
		FD_SET(zm->ifd, &readfds) ;
		timeout.tv_sec = zm->timeout ;
		timeout.tv_usec = 0 ;
		i = select(zm->ifd+1, &readfds,NULL,NULL, &timeout) ;

		if( i==0 ) {
			done = ZmodemTimeout(zm) ;
		} else if (i > 0) {
			len = read(zm->ifd, buffer, 2048);
			if (len == 0) {
				disconnect("Socket closed");
			}

			pos = 0;
			for (j=0;j<len;j++) {
				if (buffer[j] == 255 && !sshBBS) {
					if (buffer[j+1] == 255) {
						buffer2[pos] = 255;
						pos++;
						j++;
					} else {
						j++;
						j++;
					}
				} else {
					buffer2[pos] = buffer[j];
					pos++;
				}
			}
			if (pos > 0) {
				done = ZmodemRcv(buffer2, pos, zm) ;
			}
		} else {
			// SIG INT catch
			if (errno != EINTR) {
				dolog("SELECT ERROR %s", strerror(errno));
			}
		}
	}
	return done;
}

void upload_zmodem(struct user_record *user, char *upload_p) {
	ZModem zm;
	struct termios oldit;
	struct termios oldot;
	if (sshBBS) {
		ttySetRaw(STDIN_FILENO, &oldit);
		ttySetRaw(STDOUT_FILENO, &oldot);
	}

	upload_path = upload_p;

	zm.attn = NULL;
	zm.windowsize = 0;
	zm.bufsize = 0;

	if (!sshBBS) {
		zm.ifd = gSocket;
		zm.ofd = gSocket;
	} else {
		zm.ifd = STDIN_FILENO;
		zm.ofd = STDOUT_FILENO;
	}
	zm.zrinitflags = 0;
	zm.zsinitflags = 0;

	zm.packetsize = 1024;

	ZmodemRInit(&zm);

	doIO(&zm);
	if (sshBBS) {
		tcsetattr(STDIN_FILENO, TCSANOW, &oldit);
		tcsetattr(STDOUT_FILENO, TCSANOW, &oldot);
	}	
}

void upload(struct user_record *user) {
	char buffer[331];
	char buffer2[66];
	char buffer3[256];
	int i;
	char *create_sql = "CREATE TABLE IF NOT EXISTS files ("
						"Id INTEGER PRIMARY KEY,"
						"filename TEXT,"
						"description TEXT,"
						"size INTEGER,"
						"dlcount INTEGER,"
						"approved INTEGER);";
	char *sql = "INSERT INTO files (filename, description, size, dlcount, approved) VALUES(?, ?, ?, 0, 0)";
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  struct stat s;
  char *err_msg = NULL;

	upload_zmodem(user, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->upload_path);


	s_printf("\r\nPlease enter a description:\r\n");
	buffer[0] = '\0';
	for (i=0;i<5;i++) {
		s_printf("\r\n%d: ", i);
		s_readstring(buffer2, 65);
		if (strlen(buffer2) == 0) {
			break;
		}
		strcat(buffer, buffer2);
		strcat(buffer, "\n");
	}

	sprintf(buffer3, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

	rc = sqlite3_open(buffer3, &db);

	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    rc = sqlite3_exec(db, create_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        dolog("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
		stat(upload_filename, &s);

        sqlite3_bind_text(res, 1, upload_filename, -1, 0);
        sqlite3_bind_text(res, 2, buffer, -1, 0);
        sqlite3_bind_int(res, 3, s.st_size);
    } else {
      	dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		return;
    }

    rc = sqlite3_step(res);

    if (rc != SQLITE_DONE) {
	      dolog("execution failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(res);
		sqlite3_close(db);
		return;
    }
    sqlite3_finalize(res);
    sqlite3_close(db);
}

void download_zmodem(struct user_record *user, char *filename) {
	ZModem zm;
	int	done ;

	dolog("Attempting to upload %s", filename);

	zm.attn = NULL;
	zm.windowsize = 0;
	zm.bufsize = 0;

	if (!sshBBS) {
		zm.ifd = gSocket;
		zm.ofd = gSocket;
	} else {
		zm.ifd = STDIN_FILENO;
		zm.ofd = STDOUT_FILENO;
	}
	zm.zrinitflags = 0;
	zm.zsinitflags = 0;

	zm.packetsize = 1024;


	ZmodemTInit(&zm) ;
	done = doIO(&zm);
	if ( done != ZmDone ) {
		return;
	}

	done = ZmodemTFile(filename, basename(filename), ZCBIN,0,0,0,0,0, &zm) ;

	switch( done ) {
	  case 0:
	    break ;

	  case ZmErrCantOpen:
	    dolog("cannot open file \"%s\": %s\n", filename, strerror(errno)) ;
	    return;

	  case ZmFileTooLong:
	    dolog("filename \"%s\" too long, skipping...\n", filename) ;
	    return;

	  case ZmDone:
	    return;

	  default:
	    return;
	}

	if (!done) {
		done = doIO(&zm);
	}

	if ( done != ZmDone ) {
		return;
	}

	done = ZmodemTFinish(&zm);

	if (!done) {
		done = doIO(&zm);
	}
}

void download(struct user_record *user) {
	int i;
	char *ssql = "select dlcount from files where filename like ?";
	char *usql = "update files set dlcount=? where filename like ?";
	char buffer[256];
	int dloads;
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
	struct termios oldit;
	struct termios oldot;
	if (sshBBS) {
		ttySetRaw(STDIN_FILENO, &oldit);
		ttySetRaw(STDOUT_FILENO, &oldot);
	}
	for (i=0;i<tagged_count;i++) {

		download_zmodem(user, tagged_files[i]);

		sprintf(buffer, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

		rc = sqlite3_open(buffer, &db);

		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
		rc = sqlite3_prepare_v2(db, ssql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_text(res, 1, tagged_files[i], -1, 0);
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		}

		rc = sqlite3_step(res);

		if (rc != SQLITE_ROW) {
			dolog("Downloaded a file not in database!!!!!");
			sqlite3_finalize(res);
			sqlite3_close(db);
			exit(1);
		}

		dloads = sqlite3_column_int(res, 0);
		dloads++;
		sqlite3_finalize(res);

		rc = sqlite3_prepare_v2(db, usql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_int(res, 1, dloads);
			sqlite3_bind_text(res, 2, tagged_files[i], -1, 0);
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		}

		rc = sqlite3_step(res);

		sqlite3_finalize(res);
		sqlite3_close(db);
	}

	if (sshBBS) {
		tcsetattr(STDIN_FILENO, TCSANOW, &oldit);
		tcsetattr(STDOUT_FILENO, TCSANOW, &oldot);
	}

	for (i=0;i<tagged_count;i++) {
		free(tagged_files[i]);
	}
	free(tagged_files);
	tagged_count = 0;
}

void list_files(struct user_record *user) {
	char *sql = "select filename, description, size, dlcount from files where approved=1";
	char buffer[256];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
	int files_c;
	int file_size;
	char file_unit;
	int lines = 0;
	int i;
	int j;
	int z;
	int k;
	int match;

	struct file_entry **files_e;

	sprintf(buffer, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc != SQLITE_OK) {
        sqlite3_finalize(res);
		sqlite3_close(db);
		s_printf(get_string(68));
		return;
    }


    files_c = 0;

	while (sqlite3_step(res) == SQLITE_ROW) {
		if (files_c == 0) {
			files_e = (struct file_entry **)malloc(sizeof(struct file_entry *));
		} else {
			files_e = (struct file_entry **)realloc(files_e, sizeof(struct file_entry *) * (files_c + 1));
		}
		files_e[files_c] = (struct file_entry *)malloc(sizeof(struct file_entry));
		files_e[files_c]->filename = strdup((char *)sqlite3_column_text(res, 0));
		files_e[files_c]->description = strdup((char *)sqlite3_column_text(res, 1));
		files_e[files_c]->size = sqlite3_column_int(res, 2);
		files_e[files_c]->dlcount = sqlite3_column_int(res, 3);

		files_c++;
	}
	sqlite3_finalize(res);
	sqlite3_close(db);

	if (files_c == 0) {
		s_printf(get_string(68));
		return;
	}
	s_printf("\r\n");
	for (i=0;i<files_c;i++) {
		file_size = files_e[i]->size;
		if (file_size > 1024 * 1024 * 1024) {
			file_size = file_size / 1024 / 1024 / 1024;
			file_unit = 'G';
		} else if (file_size > 1024 * 1024) {
			file_size = file_size / 1024 / 1024;
			file_unit = 'M';
		} else if (file_size > 1024) {
			file_size = file_size / 1024;
			file_unit = 'K';
		} else {
			file_unit = 'b';
		}
		s_printf(get_string(69), i, files_e[i]->dlcount, file_size, file_unit, basename(files_e[i]->filename));
		lines+=3;
		for (j=0;j<strlen(files_e[i]->description);j++) {
			if (files_e[i]->description[j] == '\n') {
				s_printf("\r\n");
				lines++;
				if (lines >= 18) {
					lines = 0;
					while (1) {
						s_printf(get_string(70));
						s_readstring(buffer, 5);
						if (strlen(buffer) == 0) {
							s_printf("\r\n");
							break;
						} else if (tolower(buffer[0]) == 'q') {
							for (z=0;z<files_c;z++) {
								free(files_e[z]->filename);
								free(files_e[z]->description);
								free(files_e[z]);
							}
							free(files_e);
							s_printf("\r\n");
							return;
						}  else {
							z = atoi(buffer);
							if (z >= 0 && z < files_c) {
								if (conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->download_sec_level <= user->sec_level) {
									match = 0;
									for (k=0;k<tagged_count;k++) {
										if (strcmp(tagged_files[k], files_e[z]->filename) == 0) {
											match = 1;
											break;
										}
									}
									if (match == 0) {
										if (tagged_count == 0) {
											tagged_files = (char **)malloc(sizeof(char *));
										} else {
											tagged_files = (char **)realloc(tagged_files, sizeof(char *) * (tagged_count + 1));
										}
										tagged_files[tagged_count] = strdup(files_e[z]->filename);
										tagged_count++;
										s_printf(get_string(71), basename(files_e[z]->filename));
									} else {
										s_printf(get_string(72));
									}
								} else {
									s_printf(get_string(73));
								}
							}
						}
					}
				} else {
					s_printf(get_string(74));
				}
			} else {
				s_putchar(files_e[i]->description[j]);
			}
		}
		if (lines >= 18) {
			lines = 0;
			while (1) {
				s_printf(get_string(70));
				s_readstring(buffer, 5);
				if (strlen(buffer) == 0) {
					s_printf("\r\n");
					break;
				} else if (tolower(buffer[0]) == 'q') {
					for (z=0;z<files_c;z++) {
						free(files_e[z]->filename);
						free(files_e[z]->description);
						free(files_e[z]);
					}
					free(files_e);
					s_printf("\r\n");
					return;
				}  else {
					z = atoi(buffer);
					if (z >= 0 && z < files_c) {
						if (conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->download_sec_level <= user->sec_level) {
							match = 0;
							for (k=0;k<tagged_count;k++) {
								if (strcmp(tagged_files[k], files_e[z]->filename) == 0) {
									match = 1;
									break;
								}
							}
							if (match == 0) {
								if (tagged_count == 0) {
									tagged_files = (char **)malloc(sizeof(char *));
								} else {
									tagged_files = (char **)realloc(tagged_files, sizeof(char *) * (tagged_count + 1));
								}
								tagged_files[tagged_count] = strdup(files_e[z]->filename);
								tagged_count++;
								s_printf(get_string(71), basename(files_e[z]->filename));
							} else {
								s_printf(get_string(72));
							}
						} else {
							s_printf(get_string(73));
						}
					}
				}
			}
		}		
	}
	while (1) {
		s_printf(get_string(75));
		s_readstring(buffer, 5);
		if (strlen(buffer) == 0) {
			for (z=0;z<files_c;z++) {
				free(files_e[z]->filename);
				free(files_e[z]->description);
				free(files_e[z]);
			}
			free(files_e);
			s_printf("\r\n");
			return;
		} else {
			z = atoi(buffer);
			if (z >= 0 && z < files_c) {
				if (conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->download_sec_level <= user->sec_level) {
					match = 0;
					for (k=0;k<tagged_count;k++) {
						if (strcmp(tagged_files[k], files_e[z]->filename) == 0) {
							match = 1;
							break;
						}
					}
					if (match == 0) {
						if (tagged_count == 0) {
							tagged_files = (char **)malloc(sizeof(char *));
						} else {
							tagged_files = (char **)realloc(tagged_files, sizeof(char *) * (tagged_count + 1));
						}
						tagged_files[tagged_count] = strdup(files_e[z]->filename);
						tagged_count++;
						s_printf(get_string(71), basename(files_e[z]->filename));
					} else {
						s_printf(get_string(72));
					}
				} else {
					s_printf(get_string(73));
				}
			}
		}
	}
}

int file_menu(struct user_record *user) {
	int doquit = 0;
	int dofiles = 0;
	char c;
	int i;
	int j;
	char prompt[256];
	struct stat s;
	int do_internal_menu = 0;
	char *lRet;
	lua_State *L;
	int result;

	if (conf.script_path != NULL) {
		sprintf(prompt, "%s/filemenu.lua", conf.script_path);
		if (stat(prompt, &s) == 0) {
			L = luaL_newstate();
			luaL_openlibs(L);
			lua_push_cfunctions(L);
			luaL_loadfile(L, prompt);
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

	while (!dofiles) {
		if (do_internal_menu == 1) {
			s_displayansi("filemenu");

			s_printf(get_string(76), user->cur_file_dir, conf.file_directories[user->cur_file_dir]->name, user->cur_file_sub, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->name, user->timeleft);

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
			case 27:
				{
					c = s_getc();
					if (c == 91) {
						c = s_getc();
					}
				}
				break;			
			case 'i':
				{
					s_printf(get_string(77));
					for (i=0;i<conf.file_directory_count;i++) {
						if (conf.file_directories[i]->sec_level <= user->sec_level) {
							s_printf(get_string(78), i, conf.file_directories[i]->name);
						}
						if (i != 0 && i % 20 == 0) {
							s_printf(get_string(6));
							c = s_getc();
						}
					}
					s_printf(get_string(79));
					s_readstring(prompt, 5);
					if (tolower(prompt[0]) != 'q') {
						j = atoi(prompt);
						if (j < 0 || j >= conf.file_directory_count || conf.file_directories[j]->sec_level > user->sec_level) {
							s_printf(get_string(80));
						} else {
							s_printf("\r\n");
							user->cur_file_dir = j;
							user->cur_file_sub = 0;
						}
					}
				}
				break;
			case 's':
				{
					s_printf(get_string(81));
					for (i=0;i<conf.file_directories[user->cur_file_dir]->file_sub_count;i++) {
						s_printf("  %d. %s\r\n", i, conf.file_directories[user->cur_file_dir]->file_subs[i]->name);

						if (i != 0 && i % 20 == 0) {
							s_printf(get_string(6));
							c = s_getc();
						}
					}
					s_printf(get_string(82));
					s_readstring(prompt, 5);
					if (tolower(prompt[0]) != 'q') {
						j = atoi(prompt);
						if (j < 0 || j >= conf.file_directories[user->cur_file_dir]->file_sub_count) {
							s_printf(get_string(83));
						} else {
							s_printf("\r\n");
							user->cur_file_sub = j;
						}
					}
				}
				break;
			case 'l':
				list_files(user);
				break;
			case 'u':
				{
					if (user->sec_level >= conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->upload_sec_level) {
						upload(user);
					} else {
						s_printf(get_string(84));
					}
				}
				break;
			case 'd':
				download(user);
				break;
			case 'c':
				{
					// Clear tagged files
					if (tagged_count > 0) {
						for (i=0;i<tagged_count;i++) {
							free(tagged_files[i]);
						}
						free(tagged_files);
						tagged_count = 0;
					}
				}
				break;
			case '}':
				{

					for (i=user->cur_file_dir;i<conf.file_directory_count;i++) {
						if (i + 1 == conf.file_directory_count) {
							i = -1;
						}
						if (conf.file_directories[i+1]->sec_level <= user->sec_level) {
							user->cur_file_dir = i + 1;
							user->cur_file_sub = 0;
							break;
						}
					}
				}
				break;
			case '{':
				{
					for (i=user->cur_file_dir;i>=0;i--) {
						if (i - 1 == -1) {
							i = conf.file_directory_count;
						}
						if (conf.file_directories[i-1]->sec_level <= user->sec_level) {
							user->cur_file_dir = i - 1;
							user->cur_file_sub = 0;
							break;
						}
					}
				}
				break;
			case ']':
				{
					i=user->cur_file_sub;
					if (i + 1 == conf.file_directories[user->cur_file_dir]->file_sub_count) {
						i = -1;
					}
					user->cur_file_sub = i + 1;
				}
				break;
			case '[':
				{
					i=user->cur_file_sub;
					if (i - 1 == -1) {
						i = conf.file_directories[user->cur_file_dir]->file_sub_count;
					}
					user->cur_file_sub = i - 1;
				}
				break;
			case 'q':
				dofiles = 1;
				break;
			case 'g':
				{
					doquit = do_logout();
					dofiles = doquit;
				}
				break;
		}
	}
	if (do_internal_menu == 0) {
		lua_close(L);
	}
	return doquit;
}
