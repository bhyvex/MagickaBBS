#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "bbs.h"

extern struct bbs_config conf;

char *nl_get_bbsname(struct fido_addr *addr, char *domain) {
	sqlite3 *db;
    sqlite3_stmt *res;
	int rc;
	char buffer[PATH_MAX];
	char *ret;
	char *sql_buf = "SELECT bbsname FROM nodelist WHERE nodeno=? AND domain=?";
	
	snprintf(buffer, PATH_MAX, "%s/nodelists.sq3", conf.bbs_path);
	
	rc = sqlite3_open(buffer, &db);
		
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        return strdup("Unknown");
    }
	sqlite3_busy_timeout(db, 5000);
	
	rc = sqlite3_prepare_v2(db, sql_buf, -1, &res, 0);

	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return 0;
	}

    if (addr->point == 0) {
        snprintf(buffer, PATH_MAX, "%d:%d/%d", addr->zone, addr->net, addr->node);
    } else {
        // no support for point addresses yet.
        return strdup("Unknown");
    }

    sqlite3_bind_text(res, 1, buffer, -1, 0);
    sqlite3_bind_text(res, 2, domain, -1, 0);
        
	if (sqlite3_step(res) != SQLITE_ROW) {
		sqlite3_finalize(res);
		sqlite3_close(db);
		return strdup("Unknown");
	} 
    ret = strdup(sqlite3_column_text(res, 0));

	sqlite3_finalize(res);
	sqlite3_close(db);
	return ret;
}