#if defined(ENABLE_WWW)

#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "bbs.h"


extern struct bbs_config conf;

int www_email_delete(struct user_record *user, int id) {
	char buffer[256];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
    char *csql = "CREATE TABLE IF NOT EXISTS email ("
    					"id INTEGER PRIMARY KEY,"
						"sender TEXT COLLATE NOCASE,"
						"recipient TEXT COLLATE NOCASE,"
						"subject TEXT,"
						"body TEXT,"
						"date INTEGER,"
						"seen INTEGER);";
	char *dsql = "DELETE FROM email WHERE id=? AND recipient LIKE ?";
	char *err_msg = 0;
	
	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);

		return 0;
	}


	rc = sqlite3_exec(db, csql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) {
		sqlite3_free(err_msg);
		sqlite3_close(db);

		return 0;
	}

	rc = sqlite3_prepare_v2(db, dsql, -1, &res, 0);

	if (rc == SQLITE_OK) {
		sqlite3_bind_int(res, 1, id);
		sqlite3_bind_text(res, 2, user->loginname, -1, 0);
	} else {
		sqlite3_finalize(res);
		sqlite3_close(db);
		return 0;
	}
	sqlite3_step(res);

	sqlite3_finalize(res);
	sqlite3_close(db);
	return 1;
}

int www_send_email(struct user_record *user, char *recipient, char *subject, char *ibody) {
	char buffer[256];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
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
	char *body;
	struct utsname name;
	int i;
	int pos;
	
	if (check_user(recipient)) {
		return 0;
	}
	
	uname(&name);
	
	snprintf(buffer, 256, "\r--- MagickaBBS v%d.%d%s (%s/%s)\r * Origin: %s \r", VERSION_MAJOR, VERSION_MINOR, VERSION_STR, name.sysname, name.machine, conf.default_tagline);
	
	body = (char *)malloc(strlen(ibody) + strlen(buffer) + 1);
	memset(body, 0, strlen(ibody) + strlen(buffer) + 1);
	pos = 0;
	for (i = 0;i<strlen(ibody);i++) {
		if (ibody[i] != '\n') {
			body[pos] = ibody[i];
			pos++;
		}
	}
	
	strcat(body, buffer);
	
	sprintf(buffer, "%s/email.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);

		return 0;
	}


	rc = sqlite3_exec(db, csql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) {
		sqlite3_free(err_msg);
		sqlite3_close(db);

		return 0;
	}

	rc = sqlite3_prepare_v2(db, isql, -1, &res, 0);

	if (rc == SQLITE_OK) {
		sqlite3_bind_text(res, 1, user->loginname, -1, 0);
		sqlite3_bind_text(res, 2, recipient, -1, 0);
		sqlite3_bind_text(res, 3, subject, -1, 0);
		sqlite3_bind_text(res, 4, body, -1, 0);
		sqlite3_bind_int(res, 5, time(NULL));
	} else {
		sqlite3_finalize(res);
		sqlite3_close(db);
		return 0;
	}
	sqlite3_step(res);

	

	sqlite3_finalize(res);
	sqlite3_close(db);
	return 1;
}

char *www_new_email() {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);
	
	sprintf(buffer, "<div class=\"content-header\"><h2>New Email</h2></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<form action=\"/email/\" method=\"POST\" enctype=\"application/x-www-form-urlencoded\">\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "To : <input type=\"text\" name=\"recipient\" /><br />\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" /><br />\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<textarea name=\"body\" rows=25 cols=80></textarea>\n<br />");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<input type=\"submit\" name=\"submit\" value=\"Send\" />\n<br />");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);


	sprintf(buffer, "</form>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}	
	strcat(page, buffer);
	len += strlen(buffer);
	
	return page;	
}

char *www_email_display(struct user_record *user, int email) {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	struct tm msg_date;
	time_t date;
	char *from;
	char *subject;
	char *body;
	int id;
	int i;
	int chars;
	char *err_msg = 0;
	char *email_create_sql = "CREATE TABLE IF NOT EXISTS email ("
    					"id INTEGER PRIMARY KEY,"
						"sender TEXT COLLATE NOCASE,"
						"recipient TEXT COLLATE NOCASE,"
						"subject TEXT,"
						"body TEXT,"
						"date INTEGER,"
						"seen INTEGER);";
	char *email_show_sql = "SELECT id,sender,subject,body,date FROM email WHERE recipient LIKE ? LIMIT ?, 1";
	
	char *update_seen_sql = "UPDATE email SET seen=1 WHERE id=?";
	
	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);

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

	rc = sqlite3_prepare_v2(db, email_show_sql, -1, &res, 0);

	if (rc == SQLITE_OK) {
		sqlite3_bind_text(res, 1, user->loginname, -1, 0);
		sqlite3_bind_int(res, 2, email - 1);
	} else {
		sqlite3_finalize(res);
		sqlite3_close(db);
		free(page);
		return NULL;					
	}
	if (sqlite3_step(res) == SQLITE_ROW) {	
		id = sqlite3_column_int(res, 0);	
		from = strdup((char *)sqlite3_column_text(res, 1));
		subject = strdup((char *)sqlite3_column_text(res, 2));
		body = strdup((char *)sqlite3_column_text(res, 3));
		date = (time_t)sqlite3_column_int(res, 4);
		localtime_r(&date, &msg_date);
		
		sprintf(buffer, "<div class=\"content-header\"><h2>Your Email</h2></div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "<div class=\"email-view-header\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
	
		sprintf(buffer, "<div class=\"email-view-subject\">%s</div>\n", subject);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
	
		sprintf(buffer, "<div class=\"email-view-from\">From: %s</div>\n", from);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
	
		sprintf(buffer, "<div class=\"email-view-date\">Date: %.2d:%.2d %.2d-%.2d-%.2d</div>\n", msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		
		sprintf(buffer, "</div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		sprintf(buffer, "<div id=\"msgbody\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		for (i=0;i<strlen(body);i++) {
			if (body[i] == '\r') {
				sprintf(buffer, "<br />");
			} else {
				sprintf(buffer, "%c", body[i]);
			}
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);
		}
		sprintf(buffer, "</div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "<div class=\"email-reply-form\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		sprintf(buffer, "<h3>Reply</h3>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);
		
		sprintf(buffer, "<form action=\"/email/\" method=\"POST\" enctype=\"application/x-www-form-urlencoded\">\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "<input type=\"hidden\" name=\"recipient\" value=\"%s\" />\n", from);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		if (strncasecmp(subject, "re:", 3) == 0) {
			sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" value=\"%s\" /><br />\n", subject);
		} else {
			sprintf(buffer, "Subject : <input type=\"text\" name=\"subject\" value=\"RE: %s\" /><br />\n", subject);
		}
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "<textarea name=\"body\" rows=25 cols=80 id=\"replybody\">");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "%s said....\n\n", from);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "> ");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		chars = 0;
		
		for (i=0;i<strlen(body);i++) {
			if (body[i] == '\r') {
				sprintf(buffer, "\n> ");
				chars = 0;
			} else if (chars == 78) {
				sprintf(buffer, "\n> %c", body[i]);
				chars = 1;
			} else {
				sprintf(buffer, "%c", body[i]);
				chars ++;
			}
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}	
			strcat(page, buffer);
			len += strlen(buffer);			
		}
		
		sprintf(buffer, "</textarea>\n<br />");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "<input type=\"submit\" name=\"submit\" value=\"Reply\" />\n<br />");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);


		sprintf(buffer, "</form>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);

		sprintf(buffer, "</div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}	
		strcat(page, buffer);
		len += strlen(buffer);
		
		free(from);
		free(body);
		free(subject);
	
		sqlite3_finalize(res);
		
		rc = sqlite3_prepare_v2(db, update_seen_sql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_int(res, 1, id);
		} else {
			sqlite3_finalize(res);
			sqlite3_close(db);
			free(page);
			return NULL;					
		}
		
		sqlite3_step(res);
	} else {
		sprintf(buffer, "<div class=\"content-header\"><h2>No Such Email</h2></div>\n");
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);	
		len += strlen(buffer);	
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return page;
}

char *www_email_summary(struct user_record *user) {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *email_summary_sql = "SELECT id,sender,subject,seen,date FROM email WHERE recipient LIKE ?";
	struct tm msg_date;
	time_t date;
	char *from;
	char *subject;
	int seen;
	int id;
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
	len += strlen(buffer);
	
	sprintf(buffer, "<div class=\"button\"><a href=\"/email/new\">New Email</a></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
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
	
	sprintf(buffer, "<div class=\"div-table\">\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	while (sqlite3_step(res) == SQLITE_ROW) {
		id = sqlite3_column_int(res, 0);
		from = strdup((char *)sqlite3_column_text(res, 1));
		subject = strdup((char *)sqlite3_column_text(res, 2));
		seen = sqlite3_column_int(res, 3);
		date = (time_t)sqlite3_column_int(res, 4);
		localtime_r(&date, &msg_date);
		if (seen == 0) {
			sprintf(buffer, "<div class=\"email-summary\"><div class=\"email-id\">%d</div><div class=\"email-subject\"><a href=\"/email/%d\">%s</a></div><div class=\"email-from\">%s</div><div class=\"email-date\">%.2d:%.2d %.2d-%.2d-%.2d</div><div class=\"email-delete\"><a href=\"/email/delete/%d\"><img src=\"/static/delete.png\" alt=\"delete\" /></a></div></div>\n", msgid + 1, msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100, id);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
			len += strlen(buffer);
		} else {
			sprintf(buffer, "<div class=\"email-summary-seen\"><div class=\"email-id\">%d</div><div class=\"email-subject\"><a href=\"/email/%d\">%s</a></div><div class=\"email-from\">%s</div><div class=\"email-date\">%.2d:%.2d %.2d-%.2d-%.2d</div><div class=\"email-delete\"><a href=\"/email/delete/%d\"><img src=\"/static/delete.png\" alt=\"delete\" /></a></div></div>\n", msgid + 1, msgid + 1, subject, from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100, id);
			if (len + strlen(buffer) > max_len - 1) {
				max_len += 4096;
				page = (char *)realloc(page, max_len);
			}
			strcat(page, buffer);
			len += strlen(buffer);
		}
		free(from);
		free(subject);
		msgid++;
	}
	sprintf(buffer, "</div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
		
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	sqlite3_finalize(res);
	sqlite3_close(db);
	return page;
}

#endif
