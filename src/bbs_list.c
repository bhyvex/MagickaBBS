#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <ctype.h>
#include "bbs.h"

extern struct bbs_config conf;
extern struct user_record *gUser;
struct bbs_list_entry_t {
    int id;
    char *bbsname;
    char *sysopname;
    char *telnet;
    int owner;
};

int add_bbs(struct bbs_list_entry_t *new_entry) {
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
	char buffer[PATH_MAX];
	char c;
	char *err_msg = 0;
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    int id;
    
    s_printf("\e[2J\e[1;1H");

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
		snprintf(buffer, PATH_MAX, "%s/bbslist.sq3", conf.bbs_path);

		rc = sqlite3_open(buffer, &db);
		
		if (rc != SQLITE_OK) {
			dolog("Cannot open database: %s", sqlite3_errmsg(db));
            return 0;
		}
		sqlite3_busy_timeout(db, 5000);
		rc = sqlite3_exec(db, create_sql, 0, 0, &err_msg);
		if (rc != SQLITE_OK ) {

			dolog("SQL error: %s", err_msg);

			sqlite3_free(err_msg);
			sqlite3_close(db);

			return 0;
		}

		rc = sqlite3_prepare_v2(db, insert_sql, -1, &res, 0);

		if (rc == SQLITE_OK) {
			sqlite3_bind_text(res, 1, bbsname, -1, 0);
			sqlite3_bind_text(res, 2, sysop, -1, 0);
			sqlite3_bind_text(res, 3, telnet, -1, 0);
			sqlite3_bind_int(res, 4, gUser->id);
		} else {
			dolog("Failed to execute statement: %s", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 0;
		}


		rc = sqlite3_step(res);

		if (rc != SQLITE_DONE) {

			dolog("execution failed: %s", sqlite3_errmsg(db));
            sqlite3_finalize(res);
			sqlite3_close(db);
			return 0;
		}
        id = sqlite3_last_insert_rowid(db);

	    sqlite3_finalize(res);
		sqlite3_close(db);
		s_printf(get_string(38));

        if (new_entry != NULL) {
            new_entry->id = id;
            new_entry->bbsname = strdup(bbsname);
            new_entry->sysopname = strdup(sysop);
            new_entry->telnet = strdup(telnet);
            new_entry->owner = gUser->id;
        }
        return 1;
	} else {
		s_printf(get_string(39));
        return 0;
	}
}

int delete_bbs(int id) {
	char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    char *sql = "SELECT bbsname FROM bbslist WHERE id=? and owner=?";
    char *dsql = "DELETE FROM bbslist WHERE id=?";
    char c;
    
    s_printf("\e[2J\e[1;1H");

    snprintf(buffer, PATH_MAX, "%s/bbslist.sq3", conf.bbs_path);

    rc = sqlite3_open(buffer, &db);
 
	if (rc != SQLITE_OK) {
		return 0;
	}
	sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc == SQLITE_OK) {
		sqlite3_bind_int(res, 1, id);
		sqlite3_bind_int(res, 2, gUser->id);
	} else {
        sqlite3_close(db);
        s_printf(get_string(41));
        return 0;
    }
    if (sqlite3_step(res) == SQLITE_ROW) {
		s_printf(get_string(42), sqlite3_column_text(res, 0));
		sqlite3_finalize(res);
		c = s_getc();
		if (tolower(c) == 'y') {
			rc = sqlite3_prepare_v2(db, dsql, -1, &res, 0);
			if (rc == SQLITE_OK) {
				sqlite3_bind_int(res, 1, id);
			} else {
				sqlite3_close(db);
				s_printf(get_string(41));
				return 0;
			}
			sqlite3_step(res);
			s_printf(get_string(43));
			sqlite3_finalize(res);
            sqlite3_close(db);
            return 1;
		} else {
			s_printf(get_string(39));
            sqlite3_close(db);
            return 0;
		}
	} else {
		sqlite3_finalize(res);
		s_printf(get_string(44));
        sqlite3_close(db);
        return 0;
	}
}

void bbs_list() {
	int i;
	int redraw = 1;
	int start = 0;
	int selected = 0;
	char c;
	char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    char *sql = "SELECT id,bbsname,sysop,telnet FROM bbslist";
    struct bbs_list_entry_t **entries;
    int entrycount;
    struct bbs_list_entry_t *newentry;

    while(1) {
        entrycount = 0;
        snprintf(buffer, PATH_MAX, "%s/bbslist.sq3", conf.bbs_path);

        rc = sqlite3_open(buffer, &db);

        if (rc != SQLITE_OK) {
            dolog("Cannot open database: %s", sqlite3_errmsg(db));
            return;
        }

        sqlite3_busy_timeout(db, 5000);
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);

        } else {
            while (sqlite3_step(res) == SQLITE_ROW) {
                if (entrycount == 0) {
                    entries = (struct bbs_list_entry_t **)malloc(sizeof(struct bbs_list_entry_t *));
                } else {
                    entries = (struct bbs_list_entry_t **)realloc(entries, sizeof(struct bbs_list_entry_t *) * (entrycount + 1));
                }
                entries[entrycount] = (struct bbs_list_entry_t *)malloc(sizeof(struct bbs_list_entry_t));

                entries[entrycount]->id = sqlite3_column_int(res, 0);
                entries[entrycount]->bbsname = strdup(sqlite3_column_text(res, 1));
                entries[entrycount]->sysopname = strdup(sqlite3_column_text(res, 2));
                entries[entrycount]->telnet = strdup(sqlite3_column_text(res, 3));
                entrycount++;
            }
            sqlite3_finalize(res);
            sqlite3_close(db);
        }

        if (entrycount > 0) {
            while (1) {
                if (redraw) {
                    s_printf("\e[2J\e[1;1H");
                    s_printf(get_string(270));
                    s_printf(get_string(271));
                    for (i=start;i<start+22 && i < entrycount;i++) {
                        if (i == selected) {
                            s_printf(get_string(269), i - start + 2, i, entries[i]->bbsname, entries[i]->sysopname, entries[i]->telnet);
                        } else {
                            s_printf(get_string(268), i - start + 2, i, entries[i]->bbsname, entries[i]->sysopname, entries[i]->telnet);
                        }
                    }
                    s_printf("\e[%d;5H", selected - start + 2);
                    redraw = 0;
                }
                c = s_getchar();
                if (tolower(c) == 'q') {
                    for (i=0;i<entrycount;i++) {
                        free(entries[i]->bbsname);
                        free(entries[i]->sysopname);
                        free(entries[i]->telnet);
                        free(entries[i]);
                    }
                    free(entries);
                    return;
                } else if (tolower(c) == 'a') {
                    newentry = (struct bbs_list_entry_t *)malloc(sizeof(struct bbs_list_entry_t));
                    if (add_bbs(newentry)) {
                        entries = (struct bbs_list_entry_t **)realloc(entries, sizeof(struct bbs_list_entry_t *) * (entrycount + 1));
                        entries[entrycount] = newentry;
                        entrycount++;
                        redraw = 1;
                    } else {
                        free(newentry);
                    }
                } else if (tolower(c) == 'd') {
                    if (delete_bbs(entries[selected]->id)) {
                        free(entries[selected]->bbsname);
                        free(entries[selected]->sysopname);
                        free(entries[selected]->telnet);
                        free(entries[selected]);

                        for (i=selected;i<entrycount - 1;i++) {
                            entries[selected] = entries[selected + 1];
                        }

                        entrycount--;
                        if (entrycount == 0) {
                            free(entries);
                            return;
                        }
                        entries = (struct bbs_list_entry_t **)realloc(entries, sizeof(struct bbs_list_entry_t *) * entrycount);

                        if (selected >= entrycount) {
                            selected = entrycount - 1;
                        }
                    }
                    redraw = 1;
                } else if (c == 27) {
                    c = s_getchar();
                    if (c == 91) {
                        c = s_getchar();
                        if (c == 66) {
                            // down
                            if (selected + 1 >= start + 22) {
                                start += 22;
                                if (start >= entrycount) {
                                    start = entrycount - 22;
                                }
                                redraw = 1;
                            }
                            selected++;
                            if (selected >= entrycount) {
                                selected = entrycount - 1;
                            } else {
                                if (!redraw) {		
                                    s_printf(get_string(268), selected - start + 1, selected - 1, entries[selected - 1]->bbsname, entries[selected - 1]->sysopname, entries[selected - 1]->telnet);
                                    s_printf(get_string(269), selected - start + 2, selected, entries[selected]->bbsname, entries[selected]->sysopname, entries[selected]->telnet);
                                    s_printf("\e[%d;4H", selected - start + 2);
                                }
                            }
                        } else if (c == 65) {
                            // up
                            if (selected - 1 < start) {
                                start -= 22;
                                if (start < 0) {
                                    start = 0;
                                }
                                redraw = 1;
                            }
                            selected--;
                            if (selected < 0) {
                                selected = 0;
                            } else {
                                if (!redraw) {	
                                    s_printf(get_string(269), selected - start + 2, selected, entries[selected]->bbsname, entries[selected]->sysopname, entries[selected]->telnet);
                                    s_printf(get_string(268), selected - start + 3, selected + 1, entries[selected + 1]->bbsname, entries[selected + 1]->sysopname, entries[selected + 1]->telnet);
                                    s_printf("\e[%d;4H", selected - start + 2);
                                }	
                            }
                        } else if (c == 75) {
                            // END KEY
                            selected = entrycount - 1;
                            start = entrycount - 22;
                            if (start < 0) {
                                start = 0;
                            }
                            redraw = 1;
                        } else if (c == 72) {
                            // HOME KEY
                            selected = 0;
                            start = 0;
                            redraw = 1;
                        } else if (c == 86 || c == '5') {
                            if (c == '5') {
                                s_getchar();
                            }
                            // PAGE UP
                            selected = selected - 22;
                            if (selected < 0) {
                                selected = 0;
                            }
                            start = selected;
                            redraw = 1;
                        } else if (c == 85 || c == '6') {
                            if (c == '6') {
                                s_getchar();
                            }
                            // PAGE DOWN
                            selected = selected + 22;
                            if (selected >= entrycount) {
                                selected = entrycount -1;
                            }
                            start = selected;
                            redraw = 1;
                        }
                    }
                } 
            }
        } else {
            // no entries
            s_printf("\e[2J\e[1;1H");
            s_printf(get_string(270));
            s_printf(get_string(271));
            s_printf(get_string(272));
            s_printf(get_string(273));

            while(1) {
                c = s_getchar();    
                
                if (tolower(c) == 'a') {
                    add_bbs(NULL);
                    break;
                } else if (tolower(c) == 'q') {
                    return;
                }
            }
        }
    }
}