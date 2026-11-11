/* Handle-returning helpers for sqlite3. sqlite3_open and sqlite3_prepare_v2
 * hand their handle back through an out-parameter, and a Komodo int cannot
 * become an extern type handle, so these two wrappers return it directly.
 * The prototypes are declared here so the shim needs no sqlite3.h. */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
int sqlite3_open(const char *filename, sqlite3 **db);
int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nbyte, sqlite3_stmt **stmt, const char **tail);

sqlite3 *km_sqlite3_open(const char *filename) {
    sqlite3 *db = 0;
    sqlite3_open(filename, &db);
    return db;
}

sqlite3_stmt *km_sqlite3_prepare(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = 0;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    return stmt;
}
