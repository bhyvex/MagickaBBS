#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <ctype.h>
#include "bbs.h"

extern struct bbs_config conf;
extern int mynode;

void add_bbs(struct user_record *user) {
	char *create_sql = "CREATE TABLE IF NOT EXISTS bbslist ("
						"id INTEGER PRIMARY KEY,"
						"bbsname TEXT,"
						"sysop TEXT,"
						"telnet TEXT,"
						"owner INTEGER);";

	char *insert_sql = "INSERT INTO bbslist (bbsname, sysop, telnet, owner) VALUES(?,?, ?, ?)";

	char bbsname[19];
	char sysop[17];
	char telnet[39];
	char buffer[256];
	char c;
	char *err_msg = 0;
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;

	s_printf(get_string(28));
	s_readstring(bbsname, 18);

	s_printf(get_string(29));
	s_readstring(sysop, 16);

	s_printf(get_string(30));
	s_readstring(telnet, 38);

	s_printf(get_string(31));
	s_printf(get_string(32));
	s_printf(get_string(33), bbsname);
	s_printf(get_string(34), sysop);
	s_printf(get_string(35), telnet);
	s_printf(get_string(36));
	s_printf(get_string(37));

	c = s_getc();
	if (tolower(c) == 'y') {
		sprintf(buffer, "%s/bbslist.sq3", conf.bbs_path);

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

			return;
		}

		rc = sqlite3_prepare_v2(db, insert_sql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_text(res, 1, bbsname, -1, 0);
			sqlite3_bind_text(res, 2, sysop, -1, 0);
			sqlite3_bind_text(res, 3, telnet, -1, 0);
			sqlite3_bind_int(res, 4, user->id);


		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
		}


		rc = sqlite3_step(res);

		if (rc != SQLITE_DONE) {

			dolog("execution failed: %s", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
		}
	    sqlite3_finalize(res);
		sqlite3_close(db);
		s_printf(get_string(38));
	} else {
		s_printf(get_string(39));
	}
}

void delete_bbs(struct user_record *user) {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT bbsname FROM bbslist WHERE id=? and owner=?";
  char *dsql = "DELETE FROM bbslist WHERE id=?";
  int i;
  char c;

  s_printf(get_string(40));
  s_readstring(buffer, 5);
  i = atoi(buffer);

	sprintf(buffer, "%s/bbslist.sq3", conf.bbs_path);

  rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
		return;
	}
  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc == SQLITE_OK) {
		sqlite3_bind_int(res, 1, i);
		sqlite3_bind_int(res, 2, user->id);
	} else {
        sqlite3_close(db);
        s_printf(get_string(41));
        return;
    }
    if (sqlite3_step(res) == SQLITE_ROW) {
		s_printf(get_string(42), sqlite3_column_text(res, 0));
		sqlite3_finalize(res);
		c = s_getc();
		if (tolower(c) == 'y') {
			rc = sqlite3_prepare_v2(db, dsql, -1, &res, 0);
			if (rc == SQLITE_OK) {
				sqlite3_bind_int(res, 1, i);
			} else {
				sqlite3_close(db);
				s_printf(get_string(41));
				return;
			}
			sqlite3_step(res);
			s_printf(get_string(43));
			sqlite3_finalize(res);
		} else {
			s_printf(get_string(39));
		}
	} else {
		sqlite3_finalize(res);
		s_printf(get_string(44));
	}
	sqlite3_close(db);
}

void list_bbses() {
	char buffer[256];
	sqlite3 *db;
  sqlite3_stmt *res;
  int rc;
  char *sql = "SELECT id,bbsname,sysop,telnet FROM bbslist";
  int i;

  sprintf(buffer, "%s/bbslist.sq3", conf.bbs_path);

  rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
        sqlite3_close(db);
        s_printf(get_string(41));
        return;
    }
    i = 0;
    s_printf(get_string(45));

    while (sqlite3_step(res) == SQLITE_ROW) {
		s_printf(get_string(46), sqlite3_column_int(res, 0), sqlite3_column_text(res, 1), sqlite3_column_text(res, 2), sqlite3_column_text(res, 3));
		i++;
		if (i == 20) {
			s_printf(get_string(6));
			s_getc();
			i = 0;
		}
	}

	s_printf(get_string(47));
  sqlite3_finalize(res);
  sqlite3_close(db);

  s_printf(get_string(6));
	s_getc();
}

void bbs_list(struct user_record *user) {
	int doquit = 0;
	char c;

	while(!doquit) {
		s_printf(get_string(48));

		c = s_getc();

		switch(tolower(c)) {
			case 'l':
				list_bbses();
				break;
			case 'a':
				add_bbs(user);
				break;
			case 'd':
				delete_bbs(user);
				break;
			case 'q':
				doquit = 1;
				break;
		}
	}
}
