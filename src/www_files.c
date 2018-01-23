#if defined(ENABLE_WWW)

#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <libgen.h>
#include "bbs.h"
#include "../deps/hashids/hashids.h"

extern struct bbs_config conf;
extern struct user_record *gUser;

void www_expire_old_links() {
    char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    char sql[] = "delete from wwwhash where expire <= ?";
    char *ret;
    time_t now = time(NULL);

    snprintf(buffer, PATH_MAX, "%s/www_file_hashes.sq3", conf.bbs_path);

 	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        return;
    }
	sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
    	sqlite3_close(db);
		return;
    }
    sqlite3_bind_int(res, 1, now);
    sqlite3_step(res);
    sqlite3_finalize(res);
    sqlite3_close(db);   
}

int www_check_hash_expired(char *hash) {
    char buffer[PATH_MAX];
    sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    time_t now = time(NULL);
    char sql[] = "select id from wwwhash where hash = ? and expire > ?";
    snprintf(buffer, PATH_MAX, "%s/www_file_hashes.sq3", conf.bbs_path);
	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        return 1;
    }
	sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
    	sqlite3_close(db);
		return 0;
    }

    sqlite3_bind_text(res, 1, hash, -1, 0);
    sqlite3_bind_int(res, 2, now);

    if (sqlite3_step(res) == SQLITE_ROW) {
        sqlite3_finalize(res);
	    sqlite3_close(db);
        return 0;
    }
    sqlite3_finalize(res);
	sqlite3_close(db);
    return 1;
}

void www_add_hash_to_db(char *hash, time_t expiry) {
    char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    char csql[] = "create table if not exists wwwhash (id INTEGER PRIMARY KEY, hash TEXT, expiry INTEGER)";
    char chsql[] = "select id from wwwhash where hash = ?";
    char usql[] = "update wwwhash SET expiry = ? WHERE hash = ?";
    char isql[] = "insert into wwwhash (hash, expiry) values(?, ?)";
    
    char *ret;
    char *err_msg = 0;

    snprintf(buffer, PATH_MAX, "%s/www_file_hashes.sq3", conf.bbs_path);

	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        return;
    }
	sqlite3_busy_timeout(db, 5000);   

    rc = sqlite3_exec(db, csql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {

        dolog("SQL error: %s", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }

    // first check if hash is in database
    rc = sqlite3_prepare_v2(db, chsql, -1, &res, 0);
    if (rc != SQLITE_OK) {
    	sqlite3_close(db);
		return;
    }
    sqlite3_bind_text(res, 1, hash, -1, 0);
    rc = sqlite3_step(res);
    if (rc == SQLITE_ROW) {
        // if so, update hash
        sqlite3_finalize(res);
        rc = sqlite3_prepare_v2(db, usql, -1, &res, 0);
        if (rc != SQLITE_OK) {
    	    sqlite3_close(db);
		    return;
        }
        sqlite3_bind_int(res, 1, expiry);
        sqlite3_bind_text(res, 2, hash, -1, 0);
        sqlite3_step(res);
        sqlite3_finalize(res);
        sqlite3_close(db);

        return;
    }
    // if not add hash
    sqlite3_finalize(res);
    rc = sqlite3_prepare_v2(db, isql, -1, &res, 0);
    if (rc != SQLITE_OK) {
    	sqlite3_close(db);
		return;
    }
    sqlite3_bind_text(res, 1, hash, -1, 0);
    sqlite3_bind_int(res, 2, expiry);
    sqlite3_step(res);
    sqlite3_finalize(res);
    sqlite3_close(db);
}

char *www_decode_hash(char *hash) {
    long long unsigned numbers[4];
    int dir, sub, fid, uid;
    hashids_t *hashids = hashids_init(conf.bbs_name);
    char buffer[PATH_MAX];
	sqlite3 *db;
    sqlite3_stmt *res;
    int rc;
    char sql[] = "select filename from files where approved = 1 and id = ?";
    char *ret;

    if (www_check_hash_expired(hash)) {
        return NULL;
    }

    if (hashids_decode(hashids, hash, numbers) != 4) {
        hashids_free(hashids);
        return NULL;
    }
    hashids_free(hashids);

    uid = (int)numbers[0];
    dir = (int)numbers[1];
    sub = (int)numbers[2];
    fid = (int)numbers[3];

    if (dir >= conf.file_directory_count || sub >= conf.file_directories[dir]->file_sub_count) {
        return NULL;
    }

#if 0
    // TODO: check security level...

    if (conf.file_directories[dir]->sec_level < )
#endif
    // get filename from database
    snprintf(buffer, PATH_MAX, "%s/%s.sq3", conf.bbs_path, conf.file_directories[dir]->file_subs[sub]->database);
	rc = sqlite3_open(buffer, &db);
	if (rc != SQLITE_OK) {
        dolog("Cannot open database: %s", sqlite3_errmsg(db));
        return NULL;
    }
	sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
    	sqlite3_close(db);
		return NULL;
    }
    sqlite3_bind_int(res, 1, fid);
    if (sqlite3_step(res) == SQLITE_ROW) {
        ret = strdup(sqlite3_column_text(res, 0));
        sqlite3_finalize(res);
	    sqlite3_close(db);

        return ret;
    }
	sqlite3_finalize(res);
	sqlite3_close(db);
    return NULL;    
}

char *www_create_link(int dir, int sub, int fid) {
    char url[PATH_MAX];
    char *ret;
    char *hashid;
    int sizereq;
    time_t expiry;

    hashids_t *hashids = hashids_init(conf.bbs_name);

    sizereq = hashids_estimate_encoded_size_v(hashids, 4, gUser->id, dir, sub, fid);

    hashid = (char *)malloc(sizereq);

    if (hashids_encode_v(hashids, hashid, 4, gUser->id, dir, sub, fid) == 0) {
        hashids_free(hashids);
        free(hashid);
        return NULL;
    }

    hashids_free(hashids);

    snprintf(url, PATH_MAX, "%sfiles/%s", conf.www_url, hashid);

    // add link into hash database
    expiry = time(NULL) + 86400;
    www_add_hash_to_db(hashid, expiry);

    free(hashid);

    ret = strdup(url);

    return ret;
}

#endif
