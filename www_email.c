#if defined(ENABLE_WWW)

#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <stdlib.h>

#include "bbs.h"


extern struct bbs_config conf;

char *www_email_summary(struct user_record *user) {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *email_summary_sql = "SELECT sender,subject,seen,date FROM email WHERE recipient LIKE ?";
	struct tm msg_date;
	time_t date;
	char *from;
	char *subject;
	int seen;
	int msgid = 0;
	char *err_msg = 0;
	char *email_create_sql = "CREATE TABLE IF NOT EXISTS email ("
    					"id INTEGER PRIMARY KEY,"
						"sender TEXT COLLATE NOCASE,"
						"recipient TEXT COLLATE NOCASE,"
						"subject TEXT,"
						"body TEXT,"
						"date INTEGER,"
						"seen INTEGER);";
	
	
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);

	sprintf(buffer, "<div class=\"content-header\"><h2>Your Email</h2></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		free(page);
		return NULL;	
	}

	rc = sqlite3_exec(db, email_create_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) {
		sqlite3_free(err_msg);
		sqlite3_close(db);

		return NULL;
	}

	rc = sqlite3_prepare_v2(db, email_summary_sql, -1, &res, 0);

	if (rc == SQLITE_OK) {
		sqlite3_bind_text(res, 1, user->loginname, -1, 0);
	} else {
		sqlite3_finalize(res);
		sqlite3_close(db);
		free(page);
		return NULL;					
	}
	
	while (sqlite3_step(res) == SQLITE_ROW) {
		from = strdup((char *)sqlite3_column_text(res, 0));
		subject = strdup((char *)sqlite3_column_text(res, 1));
		seen = sqlite3_column_int(res, 2);
		date = (time_t)sqlite3_column_int(res, 3);
		localtime_r(&date, &msg_date);
		if (seen == 0) {
			sprintf(buffer, "<div class=\"email-summary\"><div class=\"email-id\">%d</div><div class=\"email-subject\"><a href=\"/email/%d\">%s</a></div><div class=\"email-from\">%s</div><div class=\"email-date\">%d:%2d %d-%d-%d</div></div>\n", msgid + 1, msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
		} else {
			sprintf(buffer, "<div class=\"email-summary-seen\"><div class=\"email-id\">%d</div><div class=\"email-subject\"><a href=\"/email/%d\">%s</a></div><div class=\"email-from\">%s</div><div class=\"email-date\">%d:%2d %d-%d-%d</div></div>\n", msgid + 1, msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
		}
		free(from);
		free(subject);
	}
	sqlite3_finalize(res);
	sqlite3_close(db);
	return page;
}

#endif
