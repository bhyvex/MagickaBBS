#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <ctype.h>
#include <openssl/sha.h>
#include "bbs.h"
#include "inih/ini.h"

extern struct bbs_config conf;

char *hash_sha256(char *pass, char *salt) {
	char *buffer = (char *)malloc(strlen(pass) + strlen(salt) + 1);
	char *shash = (char *)malloc(66);
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!buffer) {
		dolog("Out of memory!");
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

void gen_salt(char **s) {
	FILE *fptr;
	int i;
	char c;
	*s = (char *)malloc(11);

	char *salt = *s;

	if (!salt) {
		dolog("Out of memory..");
		exit(-1);
	}
	fptr = fopen("/dev/urandom", "rb");
	if (!fptr) {
		dolog("Unable to open /dev/urandom!");
		exit(-1);
	}
	for (i=0;i<10;i++) {
		fread(&c, 1, 1, fptr);
		salt[i] = (char)((abs(c) % 93) + 33);
	}
	fclose(fptr);
	salt[10] = '\0';
}

static int secLevel(void* user, const char* section, const char* name,
                   const char* value)
{
	struct sec_level_t *conf = (struct sec_level_t *)user;

	if (strcasecmp(section, "main") == 0) {
		if (strcasecmp(name, "time per day") == 0) {
			conf->timeperday = atoi(value);
		}
	}
	return 1;
}

int save_user(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
    sqlite3_stmt *res;
	int rc;

	char *update_sql = "UPDATE users SET password=?, salt=?, firstname=?,"
					   "lastname=?, email=?, location=?, sec_level=?, last_on=?, time_left=?, cur_mail_conf=?, cur_mail_area=?, cur_file_dir=?, cur_file_sub=?, times_on=?, bwavepktno=?, archiver=?, protocol=? where loginname LIKE ?";

 	sprintf(buffer, "%s/users.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);

	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }

    rc = sqlite3_prepare_v2(db, update_sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(res, 1, user->password, -1, 0);
				sqlite3_bind_text(res, 2, user->salt, -1, 0);
        sqlite3_bind_text(res, 3, user->firstname, -1, 0);
        sqlite3_bind_text(res, 4, user->lastname, -1, 0);
        sqlite3_bind_text(res, 5, user->email, -1, 0);
        sqlite3_bind_text(res, 6, user->location, -1, 0);
        sqlite3_bind_int(res, 7, user->sec_level);
        sqlite3_bind_int(res, 8, user->laston);
        sqlite3_bind_int(res, 9, user->timeleft);
        sqlite3_bind_int(res, 10, user->cur_mail_conf);
        sqlite3_bind_int(res, 11, user->cur_mail_area);
        sqlite3_bind_int(res, 12, user->cur_file_dir);
        sqlite3_bind_int(res, 13, user->cur_file_sub);
        sqlite3_bind_int(res, 14, user->timeson);
        sqlite3_bind_int(res, 15, user->bwavepktno);
        sqlite3_bind_int(res, 16, user->defarchiver);
        sqlite3_bind_int(res, 17, user->defprotocol);
        sqlite3_bind_text(res, 18, user->loginname, -1, 0);
    } else {
        dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
    }


    rc = sqlite3_step(res);

    if (rc != SQLITE_DONE) {

        dolog("execution failed: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(1);
    }
	sqlite3_close(db);
	return 1;

}

int inst_user(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
    sqlite3_stmt *res;
	int rc;
    char *create_sql = "CREATE TABLE IF NOT EXISTS users ("
						"Id INTEGER PRIMARY KEY,"
						"loginname TEXT COLLATE NOCASE,"
						"password TEXT,"
						"salt TEXT,"
						"firstname TEXT,"
						"lastname TEXT,"
						"email TEXT,"
						"location TEXT,"
						"sec_level INTEGER,"
						"last_on INTEGER,"
						"time_left INTEGER,"
						"cur_mail_conf INTEGER,"
						"cur_mail_area INTEGER,"
						"cur_file_sub INTEGER,"
						"cur_file_dir INTEGER,"
						"times_on INTEGER,"
						"bwavepktno INTEGER,"
						"archiver INTEGER,"
						"protocol INTEGER);";

	char *insert_sql = "INSERT INTO users (loginname, password, salt, firstname,"
					   "lastname, email, location, sec_level, last_on, time_left, cur_mail_conf, cur_mail_area, cur_file_dir, cur_file_sub, times_on, bwavepktno, archiver, protocol) VALUES(?,?, ?,?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    char *err_msg = 0;

 	sprintf(buffer, "%s/users.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);

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

        return 1;
    }

    rc = sqlite3_prepare_v2(db, insert_sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(res, 1, user->loginname, -1, 0);
        sqlite3_bind_text(res, 2, user->password, -1, 0);
		sqlite3_bind_text(res, 3, user->salt, -1, 0);
        sqlite3_bind_text(res, 4, user->firstname, -1, 0);
        sqlite3_bind_text(res, 5, user->lastname, -1, 0);
        sqlite3_bind_text(res, 6, user->email, -1, 0);
        sqlite3_bind_text(res, 7, user->location, -1, 0);
        sqlite3_bind_int(res, 8, user->sec_level);
        sqlite3_bind_int(res, 9, user->laston);
        sqlite3_bind_int(res, 10, user->timeleft);
        sqlite3_bind_int(res, 11, user->cur_mail_conf);
        sqlite3_bind_int(res, 12, user->cur_mail_area);
        sqlite3_bind_int(res, 13, user->cur_file_dir);
        sqlite3_bind_int(res, 14, user->cur_file_sub);
        sqlite3_bind_int(res, 15, user->timeson);
		sqlite3_bind_int(res, 16, user->bwavepktno);
		sqlite3_bind_int(res, 17, user->defarchiver);
		sqlite3_bind_int(res, 18, user->defprotocol);
    } else {
        dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
    }


    rc = sqlite3_step(res);

    if (rc != SQLITE_DONE) {
        dolog("execution failed: %s", sqlite3_errmsg(db));
				sqlite3_close(db);
				exit(1);
    }

    user->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(res);
	sqlite3_close(db);
	return 1;
}

struct user_record *check_user_pass(char *loginname, char *password) {
	struct user_record *user;
	char buffer[1024];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT Id, loginname, password, salt, firstname,"
					   "lastname, email, location, sec_level, last_on, time_left, cur_mail_conf, cur_mail_area, cur_file_dir, cur_file_sub, times_on, bwavepktno, archiver, protocol FROM users WHERE loginname LIKE ?";
	char *pass_hash;

	sprintf(buffer, "%s/users.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(res, 1, loginname, -1, 0);
    } else {
        dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		return NULL;
    }

    int step = sqlite3_step(res);

    if (step == SQLITE_ROW) {
		user = (struct user_record *)malloc(sizeof(struct user_record));
		user->id = sqlite3_column_int(res, 0);
		user->loginname = strdup((char *)sqlite3_column_text(res, 1));
		user->password = strdup((char *)sqlite3_column_text(res, 2));
		user->salt = strdup((char *)sqlite3_column_text(res, 3));
		user->firstname = strdup((char *)sqlite3_column_text(res, 4));
		user->lastname = strdup((char *)sqlite3_column_text(res, 5));
		user->email = strdup((char *)sqlite3_column_text(res, 6));
		user->location = strdup((char *)sqlite3_column_text(res, 7));
		user->sec_level = sqlite3_column_int(res, 8);
		user->laston = (time_t)sqlite3_column_int(res, 9);
		user->timeleft = sqlite3_column_int(res, 10);
		user->cur_mail_conf = sqlite3_column_int(res, 11);
		user->cur_mail_area = sqlite3_column_int(res, 12);
		user->cur_file_dir = sqlite3_column_int(res, 13);
		user->cur_file_sub = sqlite3_column_int(res, 14);
		user->timeson = sqlite3_column_int(res, 15);
		user->bwavepktno = sqlite3_column_int(res, 16);
		user->defarchiver = sqlite3_column_int(res, 17);
		user->defprotocol = sqlite3_column_int(res, 18);
		pass_hash = hash_sha256(password, user->salt);

		if (strcmp(pass_hash, user->password) != 0) {
			free(user->loginname);
			free(user->firstname);
			free(user->lastname);
			free(user->email);
			free(user->location);
			free(user->salt);
			free(user);
			free(pass_hash);
		  sqlite3_finalize(res);
			sqlite3_close(db);
			return NULL;
		}
		free(pass_hash);
  	} else {
		sqlite3_finalize(res);
		sqlite3_close(db);
		return NULL;
	}

    sqlite3_finalize(res);
    sqlite3_close(db);

    user->sec_info = (struct sec_level_t *)malloc(sizeof(struct sec_level_t));

    snprintf(buffer, 1024, "%s/s%d.ini", conf.config_path, user->sec_level);
    if (ini_parse(buffer, secLevel, user->sec_info) <0) {
		dolog("Unable to load sec Level ini (%s)!", buffer);
		exit(-1);
	}

	if (user->cur_mail_conf > conf.mail_conference_count) {
		user->cur_mail_conf = 0;
	}
	if (user->cur_file_dir > conf.file_directory_count) {
		user->cur_file_dir = 0;
	}

	if (user->cur_mail_area > conf.mail_conferences[user->cur_mail_conf]->mail_area_count) {
		user->cur_mail_area = 0;
	}

	if (user->cur_file_sub > conf.file_directories[user->cur_file_dir]->file_sub_count) {
		user->cur_file_sub = 0;
	}


    return user;
}

void list_users(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    int i;

	char *sql = "SELECT loginname,location,times_on FROM users";

	sprintf(buffer, "%s/users.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);

	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
        dolog("Cannot prepare statement: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    s_printf(get_string(161));
    s_printf(get_string(162));
    s_printf(get_string(163));
    i = 0;
    while (sqlite3_step(res) == SQLITE_ROW) {
		s_printf(get_string(164), sqlite3_column_text(res, 0), sqlite3_column_text(res, 1), sqlite3_column_int(res, 2));

		i++;
		if (i == 20) {
			s_printf(get_string(6));
			s_getc();
			i = 0;
		}
	}
  s_printf(get_string(165));
  sqlite3_finalize(res);
  sqlite3_close(db);

  s_printf(get_string(6));
	s_getc();
}

int check_user(char *loginname) {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT * FROM users WHERE loginname = ?";

	sprintf(buffer, "%s/users.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);

	if (rc != SQLITE_OK) {
    dolog("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);

    exit(1);
  }
  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc == SQLITE_OK) {
      sqlite3_bind_text(res, 1, loginname, -1, 0);
  } else {
      dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
  }

  int step = sqlite3_step(res);

  if (step == SQLITE_ROW) {
		sqlite3_finalize(res);
		sqlite3_close(db);
		return 0;
  }

  sqlite3_finalize(res);
  sqlite3_close(db);
  return 1;
}

struct user_record *new_user() {
	char buffer[256];
	struct user_record *user;
	int done = 0;
	char c;
	int nameok = 0;
	int passok = 0;
	int i;

	user = (struct user_record *)malloc(sizeof(struct user_record));
	s_printf("\r\n\r\n");
	s_displayansi("newuser");

	do {
		passok = 0;
		nameok = 0;
		do {
			s_printf(get_string(166));
			s_readstring(buffer, 16);
			s_printf("\r\n");
			if (strlen(buffer) < 3) {
				s_printf(get_string(167));
				continue;
			}

			for (i=0;i<strlen(buffer);i++) {
				if (!(tolower(buffer[i]) >= 97 && tolower(buffer[i]) <= 122) && buffer[i] != 32) {
					s_printf(get_string(168));
					nameok = 1;
					break;
				}
			}
			if (nameok == 1) {
				nameok = 0;
				continue;
			}
			if (strcasecmp(buffer, "unknown") == 0) {
				s_printf(get_string(169));
				continue;
			}
			if (strcasecmp(buffer, "all") == 0) {
				s_printf(get_string(169));
				continue;
			}
			if (strcasecmp(buffer, "new") == 0) {
				s_printf(get_string(169));
				continue;
			}
			user->loginname = strdup(buffer);
			nameok = check_user(user->loginname);
			if (!nameok) {
				s_printf(get_string(170));
				free(user->loginname);
				memset(buffer, 0, 256);
			}
		} while (!nameok);
		s_printf(get_string(171));
		memset(buffer, 0, 256);
		s_readstring(buffer, 32);
		s_printf("\r\n");
		user->firstname = strdup(buffer);

		s_printf(get_string(172));
		memset(buffer, 0, 256);
		s_readstring(buffer, 32);
		s_printf("\r\n");
		user->lastname = strdup(buffer);

		s_printf(get_string(173));
		memset(buffer, 0, 256);
		s_readstring(buffer, 64);
		s_printf("\r\n");
		user->email = strdup(buffer);

		s_printf(get_string(174));
		memset(buffer, 0, 256);
		s_readstring(buffer, 32);
		s_printf("\r\n");
		user->location = strdup(buffer);

		do {
			s_printf(get_string(175));
			memset(buffer, 0, 256);
			s_readstring(buffer, 16);
			s_printf("\r\n");
			if (strlen(buffer) >= 8) {
				passok = 1;
			} else {
				s_printf(get_string(158));
			}
		} while (!passok);
		gen_salt(&user->salt);
		user->password = hash_sha256(buffer, user->salt);

		s_printf(get_string(176));
		s_printf(get_string(177));
		s_printf(get_string(178));
		s_printf(user->loginname);
		s_printf(get_string(179));
		s_printf(user->firstname);
		s_printf(get_string(180));
		s_printf(user->lastname);
		s_printf(get_string(181));
		s_printf(user->email);
		s_printf(get_string(182));
		s_printf(user->location);
		s_printf(get_string(183));
		s_printf(get_string(184));
		c = s_getchar();
		while (tolower(c) != 'y' && tolower(c) != 'n') {
			c = s_getchar();
		}

		if (tolower(c) == 'y') {
			done = 1;
		}
	} while (!done);
	user->sec_level = conf.newuserlvl;
	user->bwavepktno = 0;
	user->sec_info = (struct sec_level_t *)malloc(sizeof(struct sec_level_t));

	sprintf(buffer, "%s/config/s%d.ini", conf.bbs_path, user->sec_level);

	if (ini_parse(buffer, secLevel, user->sec_info) <0) {
		dolog("Unable to load sec Level ini (%s)!", buffer);
		exit(-1);
	}

	user->laston = time(NULL);
	user->timeleft = user->sec_info->timeperday;
	user->cur_file_dir = 0;
	user->cur_file_sub = 0;
	user->cur_mail_area = 0;
	user->cur_mail_conf = 0;
	user->timeson = 0;
	user->defprotocol = 1;
	user->defarchiver = 1;
	inst_user(user);

	return user;
}
