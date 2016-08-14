#include <sqlite3.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs.h"

extern struct bbs_config conf;

void send_email(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *recipient;
  char *subject;
  char *msg;
  char *csql = "CREATE TABLE IF NOT EXISTS email ("
    					"id INTEGER PRIMARY KEY,"
						"sender TEXT COLLATE NOCASE,"
						"recipient TEXT COLLATE NOCASE,"
						"subject TEXT,"
						"body TEXT,"
						"date INTEGER,"
						"seen INTEGER);";
  char *isql = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";
  char *err_msg = 0;

	s_printf(get_string(54));
	s_readstring(buffer, 16);

	if (strlen(buffer) == 0) {
		s_printf(get_string(39));
		return;
	}
	if (check_user(buffer)) {
		s_printf(get_string(55));
		return;
	}

	recipient = strdup(buffer);
	s_printf(get_string(56));
	s_readstring(buffer, 25);
	if (strlen(buffer) == 0) {
		free(recipient);
		s_printf(get_string(39));
		return;
	}
	subject = strdup(buffer);

	// post a message
	msg = external_editor(user, user->loginname, recipient, NULL, NULL, subject, 1);

	if (msg != NULL) {
		sprintf(buffer, "%s/email.sq3", conf.bbs_path);

		rc = sqlite3_open(buffer, &db);
		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
			sqlite3_close(db);

			exit(1);
		}


		rc = sqlite3_exec(db, csql, 0, 0, &err_msg);
		if (rc != SQLITE_OK ) {

			dolog("SQL error: %s", err_msg);

			sqlite3_free(err_msg);
			sqlite3_close(db);

			return;
		}

		rc = sqlite3_prepare_v2(db, isql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_text(res, 1, user->loginname, -1, 0);
			sqlite3_bind_text(res, 2, recipient, -1, 0);
			sqlite3_bind_text(res, 3, subject, -1, 0);
			sqlite3_bind_text(res, 4, msg, -1, 0);
			sqlite3_bind_int(res, 5, time(NULL));
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
			sqlite3_finalize(res);
			sqlite3_close(db);
			s_printf("\r\nNo such email\r\n");
			return;
		}
		sqlite3_step(res);

		sqlite3_finalize(res);
		sqlite3_close(db);
		free(msg);
	}
    free(subject);
    free(recipient);
}

void show_email(struct user_record *user, int msgno) {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT id,sender,subject,body,date FROM email WHERE recipient LIKE ? LIMIT ?, 1";
  char *dsql = "DELETE FROM email WHERE id=?";
  char *isql = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";
  char *ssql = "UPDATE email SET seen=1 WHERE id=?";
	int id;
	char *sender;
	char *subject;
	char *body;
	time_t date;
	struct tm msg_date;
	int z;
	int lines;
	char c;
	char *replybody;
	int chars;

	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
    dolog("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);

    exit(1);
  }
  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc == SQLITE_OK) {
    sqlite3_bind_text(res, 1, user->loginname, -1, 0);
    sqlite3_bind_int(res, 2, msgno);
  } else {
    dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		s_printf("\r\nNo such email\r\n");
		return;
  }
	if (sqlite3_step(res) == SQLITE_ROW) {
		id = sqlite3_column_int(res, 0);
		sender = strdup((char *)sqlite3_column_text(res, 1));
		subject = strdup((char *)sqlite3_column_text(res, 2));
		body = strdup((char *)sqlite3_column_text(res, 3));
		date = (time_t)sqlite3_column_int(res, 4);


		s_printf(get_string(57), sender);
		s_printf(get_string(58), subject);
		localtime_r(&date, &msg_date);
		sprintf(buffer, "%s", asctime(&msg_date));
		buffer[strlen(buffer) - 1] = '\0';
		s_printf(get_string(59), buffer);
		s_printf(get_string(60));

		lines = 0;
		chars = 0;

		for (z=0;z<strlen(body);z++) {
			if (body[z] == '\r' || chars == 79) {
				chars = 0;
				s_printf("\r\n");
				lines++;
				if (lines == 19) {
					s_printf(get_string(6));
					s_getc();
					lines = 0;
					s_printf("\e[5;1H\e[0J");
				}
			} else {
				s_putchar(body[z]);
				chars++;
			}
		}

		sqlite3_finalize(res);

		rc = sqlite3_prepare_v2(db, ssql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_int(res, 1, id);
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
			sqlite3_finalize(res);
			sqlite3_close(db);
			return;
		}
		sqlite3_step(res);

		s_printf(get_string(61));
		c = s_getc();
		if (tolower(c) == 'r') {
			if (subject != NULL) {
				if (strncasecmp(buffer, "RE:", 3) != 0) {
					snprintf(buffer, 256, "RE: %s", subject);
				} else {
					snprintf(buffer, 256, "%s", subject);
				}
				free(subject);
			}
			subject = (char *)malloc(strlen(buffer) + 1);
			strcpy(subject, buffer);

			replybody = external_editor(user, user->loginname, sender, body, sender, subject, 1);
			if (replybody != NULL) {
				rc = sqlite3_prepare_v2(db, isql, -1, &res, 0);

				if (rc == SQLITE_OK) {
					sqlite3_bind_text(res, 1, user->loginname, -1, 0);
					sqlite3_bind_text(res, 2, sender, -1, 0);
					sqlite3_bind_text(res, 3, subject, -1, 0);
					sqlite3_bind_text(res, 4, replybody, -1, 0);
					sqlite3_bind_int(res, 5, time(NULL));
				} else {
					dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
					sqlite3_finalize(res);
					sqlite3_close(db);
					s_printf("\r\nNo such email\r\n");
					return;
				}
				sqlite3_step(res);

				sqlite3_finalize(res);
				free(replybody);
			}
			free(sender);
			free(subject);
			free(body);
			sqlite3_close(db);
		} else if (tolower(c) == 'd') {
			free(sender);
			free(subject);
			free(body);

			rc = sqlite3_prepare_v2(db, dsql, -1, &res, 0);

			if (rc == SQLITE_OK) {
				sqlite3_bind_int(res, 1, id);
			} else {
				dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
				sqlite3_finalize(res);
				sqlite3_close(db);
				s_printf("\r\nNo such email\r\n");
				return;
			}
			sqlite3_step(res);

			sqlite3_finalize(res);
			sqlite3_close(db);
		}

	} else {
		dolog("Failed");
		sqlite3_finalize(res);
		sqlite3_close(db);
	}
}

void list_emails(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT sender,subject,seen,date FROM email WHERE recipient LIKE ?";
  char *subject;
  char *from;
  time_t date;
  int seen;
  int msgid;
	int msgtoread;
	struct tm msg_date;

	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
  	dolog("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);

    exit(1);
  }
  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc == SQLITE_OK) {
    sqlite3_bind_text(res, 1, user->loginname, -1, 0);
  } else {
    dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		s_printf(get_string(62));
		return;
  }

  msgid = 0;
	s_printf(get_string(63));
  while (sqlite3_step(res) == SQLITE_ROW) {
		from = strdup((char *)sqlite3_column_text(res, 0));
		subject = strdup((char *)sqlite3_column_text(res, 1));
		seen = sqlite3_column_int(res, 2);
		date = (time_t)sqlite3_column_int(res, 3);
		localtime_r(&date, &msg_date);
		if (seen == 0) {
			s_printf(get_string(64), msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
		} else {
			s_printf(get_string(65), msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
		}
		free(from);
		free(subject);

		if (msgid % 22 == 0 && msgid != 0) {
			s_printf(get_string(66));
			s_readstring(buffer, 5);
			if (strlen(buffer) > 0) {
				if (tolower(buffer[0]) == 'q') {
					sqlite3_finalize(res);
					sqlite3_close(db);
					return;
				} else {
					msgtoread = atoi(buffer) - 1;
					sqlite3_finalize(res);
					sqlite3_close(db);
					show_email(user, msgtoread);
					return;
				}
			}
			s_printf(get_string(63));
		}
		msgid++;
	}
	if (msgid == 0) {
		s_printf(get_string(62));
	} else {
		s_printf(get_string(67));
		s_readstring(buffer, 5);
		if (strlen(buffer) > 0) {
			msgtoread = atoi(buffer) - 1;
			sqlite3_finalize(res);
			sqlite3_close(db);
			show_email(user, msgtoread);
			return;
		}
	}
	sqlite3_finalize(res);
	sqlite3_close(db);
}


int mail_getemailcount(struct user_record *user) {
	char *sql = "SELECT COUNT(*) FROM email WHERE recipient LIKE ?";
	int count;
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;

	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
    dolog("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);

    exit(1);
  }
  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc == SQLITE_OK) {
    sqlite3_bind_text(res, 1, user->loginname, -1, 0);
  } else {
    dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(res);
		sqlite3_close(db);
		return 0;
  }

	count = 0;

  if (sqlite3_step(res) == SQLITE_ROW) {
		count = sqlite3_column_int(res, 0);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return count;
}
