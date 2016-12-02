#include <sqlite3.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs.h"

extern struct bbs_config conf;

struct email_msg {
	int id;
	char *from;
	char *subject;
	int seen;
	time_t date;
	char *body;
};

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

void show_email(struct user_record *user, int msgno, int email_count, struct email_msg **emails) {
    char buffer[256];
    sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *dsql = "DELETE FROM email WHERE id=?";
	char *isql = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";
	char *ssql = "UPDATE email SET seen=1 WHERE id=?";

	int id;
	char *sender;
	char *subject;
	time_t date;
	struct tm msg_date;
	int z;
	int lines;
	char c;
	char *replybody;
	int chars;
	int start_line;
	int msg_line_count;
	char **msg_lines;
	int i, j;
	int position;
	int should_break;
	int quit = 0;

	while (!quit) {

		s_printf(get_string(57), emails[msgno]->from);
		s_printf(get_string(58), emails[msgno]->subject);
		localtime_r(&emails[msgno]->date, &msg_date);
		sprintf(buffer, "%s", asctime(&msg_date));
		buffer[strlen(buffer) - 1] = '\0';
		s_printf(get_string(59), buffer);
		s_printf(get_string(60));

		lines = 0;
		chars = 0;
        
        
        msg_line_count = 0;
        start_line = 0;
        
        // count the number of lines...
        for (z=0;z<strlen(emails[msgno]->body);z++) {
            if (emails[msgno]->body[z] == '\r' || chars == 79) {
                if (msg_line_count == 0) {
                    msg_lines = (char **)malloc(sizeof(char *));
                } else {
                    msg_lines = (char **)realloc(msg_lines, sizeof(char *) * (msg_line_count + 1));
                }
                
                msg_lines[msg_line_count] = (char *)malloc(sizeof(char) * (z - start_line + 1));
                
                if (z == start_line) {
                    msg_lines[msg_line_count][0] = '\0';
                } else {
                    strncpy(msg_lines[msg_line_count], &emails[msgno]->body[start_line], z - start_line);
                    msg_lines[msg_line_count][z-start_line] = '\0';
                }
                msg_line_count++;
                if (emails[msgno]->body[z] == '\r') {
                    start_line = z + 1;
                } else {
                    start_line = z;
                }
                chars = 0;
            } else {
                chars ++;
            }
        }
        
        lines = 0;
        
        position = 0;
        should_break = 0;
       
        while (!should_break) {
            s_printf("\e[5;1H\e[0J");
            for (z=position;z<msg_line_count;z++) {
                
                s_printf("%s\e[K\r\n", msg_lines[z]);
                
                if (z - position >= 17) {
                    break;
                }
            }
            s_printf(get_string(187));
            s_printf(get_string(191));
            c = s_getc();
            
            if (tolower(c) == 'r') {
                should_break = 1;
            } else if (tolower(c) == 'q') {
                should_break = 1;
                quit = 1;
			} else if (tolower(c) == 'd') {
				should_break = 1;
            } else if (c == '\e') {
                c = s_getc();
                if (c == 91) {
                    c = s_getc();
                    if (c == 65) {
                        position--;
                        if (position < 0) {
                            position = 0;
                        }
                    } else if (c == 66) {
                        position++;
                        if (position + 17 > msg_line_count) {
                            position--;
                        }
                    } else if (c == 67) {
						c = ' ';
						should_break = 1;
					} else if (c == 68) {
						c = 'b';
						should_break = 1;
					}
                }
            }  
        }

        for (i=0;i<msg_line_count;i++) {
            free(msg_lines[i]);
        }
        free(msg_lines);	
        msg_line_count = 0;
		
		sprintf(buffer, "%s/email.sq3", conf.bbs_path);

		rc = sqlite3_open(buffer, &db);
		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
			sqlite3_close(db);

			exit(1);
		}

		rc = sqlite3_prepare_v2(db, ssql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_int(res, 1, emails[msgno]->id);
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
			sqlite3_finalize(res);
			sqlite3_close(db);
			return;
		}
		sqlite3_step(res);

		sqlite3_finalize(res);
		sqlite3_close(db);
		

		if (tolower(c) == 'r') {
			if (emails[msgno]->subject != NULL) {
				if (strncasecmp(emails[msgno]->subject, "RE:", 3) != 0) {
					snprintf(buffer, 256, "RE: %s", emails[msgno]->subject);
				} else {
					snprintf(buffer, 256, "%s", emails[msgno]->subject);
				}
			}
			subject = (char *)malloc(strlen(buffer) + 1);
			strcpy(subject, buffer);

			replybody = external_editor(user, user->loginname, emails[msgno]->from, emails[msgno]->body, emails[msgno]->from, subject, 1);
			if (replybody != NULL) {
				 sprintf(buffer, "%s/email.sq3", conf.bbs_path);

				rc = sqlite3_open(buffer, &db);
				if (rc != SQLITE_OK) {
					dolog("Cannot open database: %s", sqlite3_errmsg(db));
					sqlite3_close(db);

					exit(1);
				}
				rc = sqlite3_prepare_v2(db, isql, -1, &res, 0);

				if (rc == SQLITE_OK) {
					sqlite3_bind_text(res, 1, user->loginname, -1, 0);
					sqlite3_bind_text(res, 2, emails[msgno]->from, -1, 0);
					sqlite3_bind_text(res, 3, subject, -1, 0);
					sqlite3_bind_text(res, 4, replybody, -1, 0);
					sqlite3_bind_int(res, 5, time(NULL));
				} else {
					dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
					sqlite3_finalize(res);
					sqlite3_close(db);
					free(replybody);
					free(subject);
					return;
				}
				sqlite3_step(res);

				sqlite3_finalize(res);
				sqlite3_close(db);
				free(replybody);
			}
			free(subject);
		} else if (tolower(c) == 'd') {
			sprintf(buffer, "%s/email.sq3", conf.bbs_path);

			rc = sqlite3_open(buffer, &db);
			if (rc != SQLITE_OK) {
				dolog("Cannot open database: %s", sqlite3_errmsg(db));
				sqlite3_close(db);

				exit(1);
			}
			rc = sqlite3_prepare_v2(db, dsql, -1, &res, 0);

			if (rc == SQLITE_OK) {
				sqlite3_bind_int(res, 1, emails[msgno]->id);
			} else {
				dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
				sqlite3_finalize(res);
				sqlite3_close(db);
				return;
			}
			sqlite3_step(res);
			
			sqlite3_finalize(res);
			sqlite3_close(db);
			quit = 1;
		} else if (tolower(c) == ' ') {
			msgno ++;
			if (msgno == email_count) {
				quit = 1;
			}
		} else if (tolower(c) == 'b') {
			msgno--;
			if (msgno < 0) {
				quit = 1;
			}
		}
	} 
}

void list_emails(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *sql = "SELECT sender,subject,seen,date,body,id FROM email WHERE recipient LIKE ?";
	struct email_msg **emails;
	int email_count;
	int msgid;
	int msgtoread;
	struct tm msg_date;
	int redraw;
	int start;
	int position;
	int i;
	char c;
	int closed = 0;
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
	email_count = 0;
	while (sqlite3_step(res) == SQLITE_ROW) {
		if (email_count == 0) {
			emails = (struct email_msg **)malloc(sizeof(struct email_msg *));
		} else {
			emails = (struct email_msg **)realloc(emails, sizeof(struct email_msg *) * (email_count + 1));
		}
		
		emails[email_count] = (struct email_msg *)malloc(sizeof(struct email_msg));
		
		emails[email_count]->from = strdup((char *)sqlite3_column_text(res, 0));
		emails[email_count]->subject = strdup((char *)sqlite3_column_text(res, 1));
		emails[email_count]->seen = sqlite3_column_int(res, 2);
		emails[email_count]->date = (time_t)sqlite3_column_int(res, 3);
		emails[email_count]->body = strdup((char *)sqlite3_column_text(res, 4));
		emails[email_count]->id = sqlite3_column_int(res, 5);
		email_count++;
	}
	
	sqlite3_finalize(res);
	sqlite3_close(db);
	
	if (email_count == 0) {
		s_printf(get_string(194));
		return;
	}
	
	redraw = 1;
	start = 0;
	position = 0;
	while (!closed) {
		if (redraw) {
			s_printf(get_string(126));
			for (i=start;i<start + 22 && i<email_count;i++) {
				localtime_r((time_t *)&emails[i]->date, &msg_date);
				if (i == position) {
					if (!emails[i]->seen) {
						s_printf(get_string(192), i + 1, emails[i]->subject, emails[i]->from,msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
					} else {
						s_printf(get_string(193), i + 1, emails[i]->subject, emails[i]->from,msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
					}
				} else {
					if (!emails[i]->seen) {
						s_printf(get_string(64), i + 1, emails[i]->subject, emails[i]->from,msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
					} else {
						s_printf(get_string(65), i + 1, emails[i]->subject, emails[i]->from,msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
					}
				}
			}
			s_printf(get_string(190));
			redraw = 0;
		}
		c = s_getc();
		if (tolower(c) == 'q') {
			closed = 1;
		} else if (c == 27) {
			c = s_getc();
			if (c == 91) {
				c = s_getc();
				if (c == 66) {
					// down
					position++;
					if (position >= start + 22) {
						start += 22;
						if (start >= email_count) {
							start = email_count - 22;
						}
						redraw = 1;
					}
					if (position == email_count) {
						position--;
					} else if (!redraw) {
						s_printf("\e[%d;1H", position - start + 1);
						localtime_r((time_t *)&emails[position-1]->date, &msg_date);
						if (!emails[position - 1]->seen) {
							s_printf(get_string(64), position, emails[position-1]->subject, emails[position-1]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						} else {
							s_printf(get_string(65), position, emails[position-1]->subject, emails[position-1]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						}												
						s_printf("\e[%d;1H", position - start + 2);
						localtime_r((time_t *)&emails[position]->date, &msg_date);
						if (!emails[position]->seen) {
							s_printf(get_string(192), position + 1, emails[position]->subject, emails[position]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						} else {
							s_printf(get_string(193), position + 1, emails[position]->subject, emails[position]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						}												
					}
				} else if (c == 65) {
					// up
					position--;
					if (position < start) {
						start -=22;
						if (start < 0) {
							start = 0;
						}
						redraw = 1;
					}
					if (position <= 0) {
						start = 0;
						position = 0;
						redraw = 1;
					} else if (!redraw) {
						s_printf("\e[%d;1H", position - start + 3);
						localtime_r((time_t *)&emails[position + 1]->date, &msg_date);
						if (!emails[position + 1]->seen) {
							s_printf(get_string(64), position + 2, emails[position + 1]->subject, emails[position + 1]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						} else {
							s_printf(get_string(65), position + 2, emails[position + 1]->subject, emails[position + 1]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						}												
						s_printf("\e[%d;1H", position - start + 2);
						localtime_r((time_t *)&emails[position]->date, &msg_date);
						if (!emails[position]->seen) {
							s_printf(get_string(192), position + 1, emails[position]->subject, emails[position]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						} else {
							s_printf(get_string(193), position + 1, emails[position]->subject, emails[position]->from, msg_date.tm_hour, msg_date.tm_min, msg_date.tm_mday, msg_date.tm_mon + 1, msg_date.tm_year - 100);
						}												
												
					}											
				}
			}
		} else if (c == 13) {
			closed = 1;
			show_email(user, position, email_count, emails);
		}
	}	
	
	for (i=0;i<email_count;i++) {
		free(emails[i]->from);
		free(emails[i]->subject);
		free(emails[i]->body);
		free(emails[i]);
	}
	free(emails);
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
