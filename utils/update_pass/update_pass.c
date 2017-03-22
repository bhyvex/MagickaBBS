#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <ctype.h>
#include <openssl/sha.h>

char *hash_sha256(char *pass, char *salt) {
	char *buffer = (char *)malloc(strlen(pass) + strlen(salt) + 1);
	char *shash = (char *)malloc(66);
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!buffer) {
		printf("Out of memory!");
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
		printf("Out of memory..");
		exit(-1);
	}
	fptr = fopen("/dev/urandom", "rb");
	if (!fptr) {
		printf("Unable to open /dev/urandom!");
		exit(-1);
	}
	for (i=0;i<10;i++) {
		fread(&c, 1, 1, fptr);
		salt[i] = (char)((abs(c) % 93) + 33);
	}
	fclose(fptr);
	salt[10] = '\0';
}

int main(int argc, char **argv) {
	sqlite3 *db;
	sqlite3_stmt *res;
	sqlite3_stmt *res2;

	char *alter_table_sql = "ALTER TABLE users ADD COLUMN salt TEXT";
	char *select_sql = "SELECT Id,password FROM users";
	char *update_sql = "UPDATE users SET password=?, salt=? WHERE Id=?";
	char *err_msg = 0;
	int id;
	int rc;
	char *password;
	char *hash;
	char *salt;

 	rc = sqlite3_open(argv[1], &db);

	if (rc != SQLITE_OK) {
		printf("Error opening database\n");
		return -1;
	}
	sqlite3_busy_timeout(db, 5000);
	rc = sqlite3_exec(db, alter_table_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) {

			printf("SQL error: %s\n", err_msg);

			sqlite3_free(err_msg);
			sqlite3_close(db);

			return 1;
	}
	rc = sqlite3_prepare_v2(db, select_sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
			printf("Cannot prepare statement: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(1);
	}
	while (sqlite3_step(res) == SQLITE_ROW) {
		id = sqlite3_column_int(res, 0);
		password = strdup(sqlite3_column_text(res, 1));

		gen_salt(&salt);
		hash = hash_sha256(password, salt);

		rc = sqlite3_prepare_v2(db, update_sql, -1, &res2, 0);
		if (rc != SQLITE_OK) {
				printf("Cannot prepare statement: %s\n", sqlite3_errmsg(db));
				sqlite3_close(db);
				exit(1);
		}
		sqlite3_bind_text(res2, 1, hash, -1, 0);
		sqlite3_bind_text(res2, 2, salt, -1, 0);
		sqlite3_bind_int(res2, 3, id);

		rc = sqlite3_step(res2);

		if (rc != SQLITE_DONE) {
			printf("Error: %s\n", sqlite3_errmsg(db));
			exit(1);
		}
		sqlite3_finalize(res2);
	}

	printf("Converted!\n");
	sqlite3_finalize(res);
	sqlite3_close(db);
}
