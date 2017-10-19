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
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include "Xmodem/zmodem.h"
#include "bbs.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
extern struct bbs_config conf;
extern int gSocket;
extern int sshBBS;
extern int mynode;
extern int bbs_stdin;
extern int bbs_stdout;
extern int bbs_stderr;
extern time_t userlaston;
extern struct user_record *gUser;

struct file_entry {
	int dir;
	int sub;
	char *filename;
	char *description;
	int size;
	int dlcount;
	time_t uploaddate;
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

char *get_file_id_diz(char *filename) {
	char *description;
	char buffer[1024];
	struct stat s;
	int bpos;
	int i;
	FILE *fptr;
	int len;
	int ext;
	int arch;
	int stout;
	int stin;
	int sterr;

	ext = 0;
	arch = -1;
	
	for (i=strlen(filename)-1;i>=0;i--) {
		if (filename[i] == '.') {
			ext = i + 1;
			break;
		}
	}
	
	if (ext == 0) {
		return NULL;
	}
	
	for (i=0;i<conf.archiver_count;i++) {
		if (strcasecmp(&filename[ext], conf.archivers[i]->extension) == 0) {
			arch = i;
			break;
		}
	}
	
	if (arch == -1) {
		return NULL;
	}
	
	snprintf(buffer, 1024, "%s/node%d", conf.bbs_path, mynode);
	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}

	snprintf(buffer, 1024, "%s/node%d/temp", conf.bbs_path, mynode);
	if (stat(buffer, &s) == 0) {
		
		if (recursive_delete(buffer) != 0) {
			return NULL;
		}
	}
	
	mkdir(buffer, 0755);
	
	bpos = 0;
	for (i=0;i<strlen(conf.archivers[arch]->unpack);i++) {
		if (conf.archivers[arch]->unpack[i] == '*') {
			i++;
			if (conf.archivers[arch]->unpack[i] == 'a') {
				sprintf(&buffer[bpos], "%s", filename);
				bpos = strlen(buffer);
			} else if (conf.archivers[arch]->unpack[i] == 'd') {
				sprintf(&buffer[bpos], "%s/node%d/temp/", conf.bbs_path, mynode);
				bpos = strlen(buffer);				
			} else if (conf.archivers[arch]->unpack[i] == '*') {
				buffer[bpos++] = '*';
				buffer[bpos] = '\0';
			}
		} else {
			buffer[bpos++] = conf.archivers[arch]->unpack[i];
			buffer[bpos] = '\0';
		}
	}
	
	if (sshBBS) {
		stout = dup(STDOUT_FILENO);
		stin = dup(STDIN_FILENO);
		sterr = dup(STDERR_FILENO);
			
		dup2(bbs_stdout, STDOUT_FILENO);
		dup2(bbs_stderr, STDERR_FILENO);
		dup2(bbs_stdin, STDIN_FILENO);
	}
	system(buffer);
	
	if (sshBBS) {
	
		dup2(stout, STDOUT_FILENO);
		dup2(sterr, STDERR_FILENO);	
		dup2(stin, STDIN_FILENO);
		
		close(stin);
		close(stout);
		close(sterr);
	}
	
	snprintf(buffer, 1024, "%s/node%d/temp/FILE_ID.DIZ", conf.bbs_path, mynode);
	if (stat(buffer, &s) != 0) {
		snprintf(buffer, 1024, "%s/node%d/temp/file_id.diz", conf.bbs_path, mynode);
		if (stat(buffer, &s) != 0) {
			snprintf(buffer, 1024, "%s/node%d/temp", conf.bbs_path, mynode);
			recursive_delete(buffer);
			return NULL;
		}
	}
	
	description = (char *)malloc(s.st_size + 1);
	
	fptr = fopen(buffer, "rb");
	
	fread(description, 1, s.st_size, fptr);
	description[s.st_size] = '\0';
	fclose(fptr);
	
	bpos = 0;
	len = strlen(description);
	for (i=0;i<len;i++) {
		if (description[i] == '\r') {
			continue;
		} else {
			description[bpos++] = description[i];
			
		}
	}
	description[bpos] = '\0';
	
	snprintf(buffer, 1024, "%s/node%d/temp", conf.bbs_path, mynode);
	recursive_delete(buffer);
	
	return description;
}

int do_download(struct user_record *user, char *file) {
	struct termios oldit;
	struct termios oldot;
	char download_command[1024];
	int i;
	int argc;
	int last_char_space;
	char **arguments;
	int bpos;
	int len;
	
	if (conf.protocols[user->defprotocol - 1]->internal_zmodem) {
		if (sshBBS) {
			ttySetRaw(STDIN_FILENO, &oldit);
			ttySetRaw(STDOUT_FILENO, &oldot);
		}		
		download_zmodem(user, file);
		if (sshBBS) {
			tcsetattr(STDIN_FILENO, TCSANOW, &oldit);
			tcsetattr(STDOUT_FILENO, TCSANOW, &oldot);
		}
		return 1;
	} else {
		bpos = 0;
		for (i=0;i<strlen(conf.protocols[user->defprotocol - 1]->download);i++) {
			if (conf.protocols[user->defprotocol - 1]->download[i] == '*') {
				i++;
				if (conf.protocols[user->defprotocol - 1]->download[i] == '*') {
					download_command[bpos++] = conf.protocols[user->defprotocol - 1]->download[i];
					download_command[bpos] = '\0';
					continue;
				} else if (conf.protocols[user->defprotocol - 1]->download[i] == 'f') {
					sprintf(&download_command[bpos], "%s", file);
					bpos = strlen(download_command);
	
					continue;
				} else if (conf.protocols[user->defprotocol - 1]->download[i] == 's') {
					if (!sshBBS) {
						sprintf(&download_command[bpos], "%d", gSocket);
						bpos = strlen(download_command);
					} else {
						s_printf(get_string(209), conf.protocols[user->defprotocol - 1]->name);
						return 0;
					}
				}
				
			} else {
				download_command[bpos++] = conf.protocols[user->defprotocol - 1]->download[i];
				download_command[bpos] = '\0';
			}
		}
		argc = 1;
		last_char_space = 0;
		for (i=0;i<strlen(download_command);i++) {
			if (download_command[i] == ' ') {
				if (!last_char_space) {
					argc++;
					last_char_space = 1;
				}
			} else {
				last_char_space = 0;
			}
		}
		bpos = 1;
		arguments = (char **)malloc(sizeof(char *) * (argc + 1));
		len = strlen(download_command);
		for (i=0;i<len;) {
			if (download_command[i] != ' ') {
				i++;
				continue;
			}
			
			download_command[i] = '\0';
			i++;
			
			while (download_command[i] == ' ')
				i++;
				
			arguments[bpos++] = &download_command[i];
		}
		arguments[bpos] = NULL;
		
		arguments[0] = download_command;

		runexternal(user, download_command, conf.protocols[user->defprotocol - 1]->stdio, arguments, conf.bbs_path, 1, NULL);
		
		free(arguments);		
	}
	return 1;
}

int do_upload(struct user_record *user, char *final_path) {
	char upload_path[1024];
	char upload_command[1024];
	char buffer3[256];
	int bpos;
	int i;
	int argc;
	int last_char_space;
	char **arguments;
	DIR *inb;
	struct dirent *dent;
	struct stat s;
	int len;
	
	if (conf.protocols[user->defprotocol - 1]->internal_zmodem) {
		upload_zmodem(user, final_path);
		return 1;
	} else {
		
		if (conf.protocols[user->defprotocol - 1]->upload_prompt) {
			s_printf(get_string(210));
			s_readstring(buffer3, 256);
			s_printf("\r\n");
		}
		bpos = 0;
		for (i=0;i<strlen(conf.protocols[user->defprotocol - 1]->upload);i++) {
			if (conf.protocols[user->defprotocol - 1]->upload[i] == '*') {
				i++;
				if (conf.protocols[user->defprotocol - 1]->upload[i] == '*') {
					upload_command[bpos++] = conf.protocols[user->defprotocol - 1]->upload[i];
					upload_command[bpos] = '\0';
					continue;
				} else if (conf.protocols[user->defprotocol - 1]->upload[i] == 'f') {
					if (conf.protocols[user->defprotocol - 1]->upload_prompt) {
						sprintf(&upload_command[bpos], "%s", buffer3);
						bpos = strlen(upload_command);
					}
					continue;
				} else if (conf.protocols[user->defprotocol - 1]->upload[i] == 's') {
					if (!sshBBS) {
						sprintf(&upload_command[bpos], "%d", gSocket);
						bpos = strlen(upload_command);
					} else {
						s_printf(get_string(209), conf.protocols[user->defprotocol - 1]->name);
						return 0;
					}
				}
				
			} else {
				upload_command[bpos++] = conf.protocols[user->defprotocol - 1]->upload[i];
				upload_command[bpos] = '\0';
			}
		}
		argc = 1;
		last_char_space = 0;
		for (i=0;i<strlen(upload_command);i++) {
			if (upload_command[i] == ' ') {
				if (!last_char_space) {
					argc++;
					last_char_space = 1;
				}
			} else {
				last_char_space = 0;
			}
		}
		bpos = 1;
		arguments = (char **)malloc(sizeof(char *) * (argc + 1));
		len = strlen(upload_command);
		for (i=0;i<len;) {
			if (upload_command[i] != ' ') {
				i++;
				continue;
			}
			
			upload_command[i] = '\0';
			i++;
			
			while (upload_command[i] == ' ')
				i++;
				
			arguments[bpos++] = &upload_command[i];
		}
		arguments[bpos] = NULL;
		
		arguments[0] = upload_command;
		
		snprintf(upload_path, 1024, "%s/node%d/upload/", conf.bbs_path, mynode);
		
		if (stat(upload_path, &s) == 0) {
			recursive_delete(upload_path);
		}
		
		mkdir(upload_path, 0755);
		
		runexternal(user, upload_command, conf.protocols[user->defprotocol - 1]->stdio, arguments, upload_path, 1, NULL);
		
		free(arguments);
		
		if (conf.protocols[user->defprotocol - 1]->upload_prompt) {
			snprintf(upload_command, 1024, "%s%s", upload_path, buffer3);
			if (stat(upload_command, &s) != 0) {
				recursive_delete(upload_path);
				return 0;
			}
			
			snprintf(upload_filename, 1024, "%s/%s", final_path, buffer3);
			if (stat(upload_filename, &s) == 0) {
				recursive_delete(upload_path);
				s_printf(get_string(214));
				return 0;
			}
			if (copy_file(upload_command, upload_filename) != 0) {
				recursive_delete(upload_path);
				return 0;				
			}
			
			recursive_delete(upload_path);
			return 1;
		} else {
			inb = opendir(upload_path);
			if (!inb) {
				return 0;
			}
			while ((dent = readdir(inb)) != NULL) {
				if (dent->d_type == DT_REG) {
					snprintf(upload_command, 1024, "%s%s", upload_path, dent->d_name);
					snprintf(upload_filename, 1024, "%s/%s", final_path, dent->d_name);
					
					if (stat(upload_filename, &s) == 0) {
						recursive_delete(upload_path);
						s_printf(get_string(214));
						closedir(inb);
						return 0;
					}
					
					if (copy_file(upload_command, upload_filename) != 0) {
						recursive_delete(upload_path);
						closedir(inb);
						return 0;				
					}
					closedir(inb);
					recursive_delete(upload_path);
					return 1;
				}
				
			}

			closedir(inb);
			return 0;
		}
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
						"uploaddate INTEGER,"
						"approved INTEGER);";
	char *sql = "INSERT INTO files (filename, description, size, dlcount, approved, uploaddate) VALUES(?, ?, ?, 0, 0, ?)";
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	struct stat s;
	char *err_msg = NULL;
	char *description;
	time_t curtime;
	
	if (!do_upload(user, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->upload_path)) {
		s_printf(get_string(211));
		return;
	}
	
	description = NULL;
	
	s_printf(get_string(198));
	description = get_file_id_diz(upload_filename);
	
	if (description == NULL) {
		s_printf(get_string(199));
		s_printf(get_string(200));
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

	} else {
		s_printf(get_string(201));
	}
	sprintf(buffer3, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

	rc = sqlite3_open(buffer3, &db);

	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
	sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_exec(db, create_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        dolog("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
		if (description != NULL) {
			free(description);
		}        
        return;
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
		stat(upload_filename, &s);

        sqlite3_bind_text(res, 1, upload_filename, -1, 0);
        if (description == NULL) {
			sqlite3_bind_text(res, 2, buffer, -1, 0);
		} else {
			sqlite3_bind_text(res, 2, description, -1, 0);
		}
        sqlite3_bind_int(res, 3, s.st_size);
        curtime = time(NULL);
        sqlite3_bind_int(res, 4, curtime);
    } else {
      	dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		if (description != NULL) {
			free(description);
		}
		return;
    }

    rc = sqlite3_step(res);

    if (rc != SQLITE_DONE) {
	      dolog("execution failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(res);
		sqlite3_close(db);
		if (description != NULL) {
			free(description);
		}		
		return;
    }
    sqlite3_finalize(res);
    sqlite3_close(db);
    if (description != NULL) {
		free(description);
	}
	
	s_printf(get_string(202));
	s_printf(get_string(6));
	s_getc();
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


	for (i=0;i<tagged_count;i++) {

		do_download(user, tagged_files[i]);

		sprintf(buffer, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

		rc = sqlite3_open(buffer, &db);

		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
		sqlite3_busy_timeout(db, 5000);
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



	for (i=0;i<tagged_count;i++) {
		free(tagged_files[i]);
	}
	free(tagged_files);
	tagged_count = 0;
}

void do_list_files(struct file_entry **files_e, int files_c) {
	int file_size;
	char file_unit;
	int lines = 0;
	int i;
	int j;
	int z;
	int k;
	int match;
	char buffer[6];
	
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
		if (files_e[i]->uploaddate > userlaston) {
			s_printf(get_string(231), i, files_e[i]->dlcount, file_size, file_unit, basename(files_e[i]->filename));
		} else {
			s_printf(get_string(69), i, files_e[i]->dlcount, file_size, file_unit, basename(files_e[i]->filename));
		}
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
								if (conf.file_directories[files_e[z]->dir]->file_subs[files_e[z]->sub]->download_sec_level <= gUser->sec_level) {
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
				if (strlen(&(files_e[i]->description[j])) > 1) {
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
						if (conf.file_directories[files_e[z]->dir]->file_subs[files_e[z]->sub]->download_sec_level <= gUser->sec_level) {
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
				if (conf.file_directories[files_e[z]->dir]->file_subs[files_e[z]->sub]->download_sec_level <= gUser->sec_level) {
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

void file_search() {
	char ch;
	int all = 0;
	int stype = 0;
	char buffer[PATH_MAX];
	char sqlbuffer[1024];
	char **searchterms;
	int searchterm_count = 0;
	char *ptr;
	int i;
	int j;
	int search_dir;
	int search_sub;
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
	int files_c;
	struct file_entry **files_e;
	
	if (!sqlite3_compileoption_used("SQLITE_ENABLE_FTS3")) {
		s_printf("\r\nSorry, search is unavailable. Please recompile sqlite3 with FTS3 support.\r\n");
		return;
	}
	
	s_printf("\r\nSearch by (F)ilename, (D)escription or (B)oth: ");
	ch = s_getc();
	
	switch(tolower(ch)) {
		case 'd':
			stype = 1;
			break;
		case 'b':
			stype = 2;
			break;
	}
	
	s_printf("\r\nSearch in (C)urrent area or (A)ll areas: ");
	
	ch = s_getc();
	if (tolower(ch) == 'a') {
		all = 1;
	}
	
	s_printf("\r\nKeywords: ");
	
	s_readstring(buffer, 128);
	
	if (strlen(buffer) == 0) {
		s_printf("\r\nPlease enter at least one keyword.\r\n");
		return;
	}
	ptr = strtok(buffer, " ");
	while (ptr != NULL) {
		if (searchterm_count == 0) {
			searchterms = (char **)malloc(sizeof(char *));
		} else {
			searchterms = (char **)realloc(searchterms, sizeof(char *) * (searchterm_count + 1));
		}
		
		searchterms[searchterm_count] = malloc(strlen(ptr) + 3);
		sprintf(searchterms[searchterm_count], "%%%s%%", ptr);
		searchterm_count++;
		ptr = strtok(NULL, " ");
	}
	if (stype == 0) {
		snprintf(sqlbuffer, 1024, "select filename, description, size, dlcount, uploaddate from files where approved=1 AND (filename LIKE ?");
		for (i=1; i < searchterm_count; i++) {
			strncat(sqlbuffer, " OR filename LIKE ?", 1024);
		}
		strncat(sqlbuffer, ")", 1024);
	}
	if (stype == 1) {
		snprintf(sqlbuffer, 1024, "select filename, description, size, dlcount, uploaddate from files where approved=1 AND (description LIKE ?");
		for (i=1; i < searchterm_count; i++) {
			strncat(sqlbuffer, " OR description LIKE ?", 1024);
		}
		strncat(sqlbuffer, ")", 1024);
	}
	if (stype == 2) {
		snprintf(sqlbuffer, 1024, "select filename, description, size, dlcount, uploaddate from files where approved=1 AND (filename LIKE ?");
		for (i=1; i < searchterm_count; i++) {
			strncat(sqlbuffer, " OR filename LIKE ?", 1024);
		}
		strncat(sqlbuffer, " OR description LIKE ?", 1024);
		for (i=1; i < searchterm_count; i++) {
			strncat(sqlbuffer, " OR description LIKE ?", 1024);
		}
		strncat(sqlbuffer, ")", 1024);
	}
	
	if (!all) {
		files_c = 0;		
		snprintf(buffer, PATH_MAX, "%s/%s.sq3", conf.bbs_path, conf.file_directories[gUser->cur_file_dir]->file_subs[gUser->cur_file_sub]->database);

		rc = sqlite3_open(buffer, &db);
		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
			sqlite3_close(db);

			exit(1);
		}
		sqlite3_busy_timeout(db, 5000);
		rc = sqlite3_prepare_v2(db, sqlbuffer, -1, &res, 0);

		if (rc != SQLITE_OK) {
			sqlite3_finalize(res);
			sqlite3_close(db);
			for (i=0;i<searchterm_count;i++) {
				free(searchterms[i]);
			}
			free(searchterms);
			return;
		}
		if (stype == 2) {
			for (j=0;j<2;j++) {
				for (i=0;i<searchterm_count;i++) {
					sqlite3_bind_text(res, j * searchterm_count + i + 1, searchterms[i], -1, 0);
				}
			}
		} else {
			for (i=0;i<searchterm_count;i++) {
				sqlite3_bind_text(res, i + 1, searchterms[i], -1, 0);
			}
		}

		

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
			files_e[files_c]->uploaddate = sqlite3_column_int(res, 4);
			files_e[files_c]->dir = gUser->cur_file_dir;
			files_e[files_c]->sub = gUser->cur_file_sub;
			files_c++;
		}

		sqlite3_finalize(res);
		sqlite3_close(db);

		if (files_c != 0) {
			do_list_files(files_e, files_c);
		}
	} else {
		files_c = 0;		
		for (search_dir = 0; search_dir < conf.file_directory_count; search_dir++) {
			if (conf.file_directories[search_dir]->sec_level > gUser->sec_level) {
				continue;
			}
			for (search_sub = 0; search_sub < conf.file_directories[search_dir]->file_sub_count; search_sub++) {
				if (conf.file_directories[search_dir]->file_subs[search_sub]->download_sec_level > gUser->sec_level) {
					continue;
				}
				snprintf(buffer, PATH_MAX, "%s/%s.sq3", conf.bbs_path, conf.file_directories[search_dir]->file_subs[search_sub]->database);

				rc = sqlite3_open(buffer, &db);
				if (rc != SQLITE_OK) {
					dolog("Cannot open database: %s", sqlite3_errmsg(db));
					sqlite3_close(db);
					exit(1);
				}
				sqlite3_busy_timeout(db, 5000);
				rc = sqlite3_prepare_v2(db, sqlbuffer, -1, &res, 0);

				if (rc != SQLITE_OK) {
					sqlite3_finalize(res);
					sqlite3_close(db);
					continue;
				}
				if (stype == 2) {
					for (j=0;j<2;j++) {
						for (i=0;i<searchterm_count;i++) {
							sqlite3_bind_text(res, j * searchterm_count + i + 1, searchterms[i], -1, 0);
						}
					}
				} else {
					for (i=0;i<searchterm_count;i++) {
						sqlite3_bind_text(res, i + 1, searchterms[i], -1, 0);
					}
				}

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
					files_e[files_c]->uploaddate = sqlite3_column_int(res, 4);
					files_e[files_c]->dir = gUser->cur_file_dir;
					files_e[files_c]->sub = gUser->cur_file_sub;
					files_c++;
				}
				sqlite3_finalize(res);
				sqlite3_close(db);				
			}
		}
		
		if (files_c != 0) {
			do_list_files(files_e, files_c);
		}
	}
	for (i=0;i<searchterm_count;i++) {
		free(searchterms[i]);
	}
	free(searchterms);
}

void list_files(struct user_record *user) {
	char *dsql = "select filename, description, size, dlcount, uploaddate from files where approved=1 ORDER BY uploaddate DESC";
	char *fsql = "select filename, description, size, dlcount, uploaddate from files where approved=1 ORDER BY filename";
	char *psql = "select filename, description, size, dlcount, uploaddate from files where approved=1 ORDER BY dlcount DESC";
	char *sql;
	char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
	int files_c;

	char ch;
	struct file_entry **files_e;

	s_printf(get_string(233));
	ch = s_getc();
	
	switch(tolower(ch)) {
		case 'u':
			sql = dsql;
			break;
		case 'p':
			sql = psql;
			break;
		default:
			sql = fsql;
			break;
		
	}
	s_printf("\r\n");
	snprintf(buffer, PATH_MAX, "%s/%s.sq3", conf.bbs_path, conf.file_directories[user->cur_file_dir]->file_subs[user->cur_file_sub]->database);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }
	sqlite3_busy_timeout(db, 5000);
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
		files_e[files_c]->uploaddate = sqlite3_column_int(res, 4);
		files_e[files_c]->dir = user->cur_file_dir;
		files_e[files_c]->sub = user->cur_file_sub;
		files_c++;
	}
	sqlite3_finalize(res);
	sqlite3_close(db);

	if (files_c == 0) {
		s_printf(get_string(68));
		return;
	}
	
	do_list_files(files_e, files_c);
}

void choose_subdir(struct user_record *user) {
	int i;
	char c;
	int j;
	char prompt[6];

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

void choose_directory(struct user_record *user) {
	int i;
	char c;
	int j;
	char prompt[6];
					
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

void clear_tagged_files() {
	int i;
	// Clear tagged files
	if (tagged_count > 0) {
		for (i=0;i<tagged_count;i++) {
			free(tagged_files[i]);
		}
		free(tagged_files);
		tagged_count = 0;
	}
}

void next_file_dir(struct user_record *user) {
	int i;
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

void prev_file_dir(struct user_record *user) {
	int i;
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

void next_file_sub(struct user_record *user) {
	int i;
	i=user->cur_file_sub;
	if (i + 1 == conf.file_directories[user->cur_file_dir]->file_sub_count) {
		i = -1;
	}
	user->cur_file_sub = i + 1;
}

void prev_file_sub(struct user_record *user) {
	int i;
	i=user->cur_file_sub;
	if (i - 1 == -1) {
		i = conf.file_directories[user->cur_file_dir]->file_sub_count;
	}
	user->cur_file_sub = i - 1;
}

void file_scan() {
	char c;
	int i;
	int j;
	char buffer[PATH_MAX];
	char sql[] = "SELECT COUNT(*) FROM files WHERE uploaddate > ? AND approved=1";
	int rc;
	sqlite3 *db;
    sqlite3_stmt *res;	
	int new_files;
	int lines = 0;
	
	s_printf(get_string(232));
	c = s_getc();
	
	if (tolower(c) == 'y') {
		for (i=0;i<conf.file_directory_count;i++) {
			if (conf.file_directories[i]->sec_level > gUser->sec_level) {
				continue;
			}
			s_printf(get_string(140), i, conf.file_directories[i]->name);
			lines += 2;
			if (lines == 22) {
				s_printf(get_string(6));
				s_getc();
				lines = 0;
			}				
			for (j=0;j<conf.file_directories[i]->file_sub_count;j++) {
				if (conf.file_directories[i]->file_subs[j]->download_sec_level > gUser->sec_level) {
					continue;
				}
				snprintf(buffer, PATH_MAX, "%s/%s.sq3", conf.bbs_path, conf.file_directories[i]->file_subs[j]->database);
				rc = sqlite3_open(buffer, &db);
				if (rc != SQLITE_OK) {
					dolog("Cannot open database: %s", sqlite3_errmsg(db));
					sqlite3_close(db);

					exit(1);
				}
				sqlite3_busy_timeout(db, 5000);
				rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

				if (rc != SQLITE_OK) {
					sqlite3_finalize(res);
					sqlite3_close(db);
					continue;
				}
				sqlite3_bind_int(res, 1, userlaston);
				
				if (sqlite3_step(res) != SQLITE_ERROR) {
					new_files = sqlite3_column_int(res, 0);
					if (new_files > 0) {
						s_printf(get_string(141), j, conf.file_directories[i]->file_subs[j]->name, new_files);
						lines++;
					}
				}
				sqlite3_finalize(res);
				sqlite3_close(db);
				
				if (lines == 22) {
					s_printf(get_string(6));
					s_getc();
					lines = 0;
				}				
			}
		}
		s_printf(get_string(6));
		s_getc();		
	}
}
