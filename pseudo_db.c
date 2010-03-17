/*
 * pseudo_db.c, sqlite3 interface
 * 
 * Copyright (c) 2008-2010 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * version 2.1 along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_db.h"

struct log_history {
	int rc;
	unsigned long fields;
	sqlite3_stmt *stmt;
};

static sqlite3 *file_db = 0;
static sqlite3 *log_db = 0;

/* What's going on here, you might well ask?
 * This contains a template to build the database.  I suppose maybe it
 * should have been elegantly done as a big chunk of embedded SQL, but 
 * this looked like a good idea at the time.
 */
typedef struct { char *fmt; int arg; } id_row;

/* op_columns and op_rows are used to initialize the table of operations,
 * which exists so that the databases are self-consistent even if somehow
 * someone else's version of pseudo is out of sync.
 * The same applies to other tables like this.
 */
#define OP_ROW(id, name) { "%d, '" name "'", id }
char *op_columns = "id, name";
id_row op_rows[] = {
	OP_ROW(OP_UNKNOWN, "unknown"),
	OP_ROW(OP_NONE, "none"),
	OP_ROW(OP_CHDIR, "chdir"),
	OP_ROW(OP_CHMOD, "chmod"),
	OP_ROW(OP_CHOWN, "chown"),
	OP_ROW(OP_CLOSE, "close"),
	OP_ROW(OP_CREAT, "creat"),
	OP_ROW(OP_DUP, "dup"),
	OP_ROW(OP_FCHMOD, "fchmod"),
	OP_ROW(OP_FCHOWN, "fchown"),
	OP_ROW(OP_FSTAT, "fstat"),
	OP_ROW(OP_LINK, "link"),
	OP_ROW(OP_MKDIR, "mkdir"),
	OP_ROW(OP_MKNOD, "mknod"),
	OP_ROW(OP_OPEN, "open"),
	OP_ROW(OP_RENAME, "rename"),
	OP_ROW(OP_STAT, "stat"),
	OP_ROW(OP_UNLINK, "unlink"),
	OP_ROW(OP_SYMLINK, "symlink"),
	OP_ROW(OP_MAX, "max"),
	{ NULL, 0 }
};

/* same as for ops; defined so the values in the database are consistent */
#define SEV_ROW(id, name) { "%d, '" name "'", id }
char *sev_columns = "id, name";
id_row sev_rows[] = {
	SEV_ROW(SEVERITY_UNKNOWN, "unknown"),
	SEV_ROW(SEVERITY_NONE, "none"),
	SEV_ROW(SEVERITY_DEBUG, "debug"),
	SEV_ROW(SEVERITY_INFO, "info"),
	SEV_ROW(SEVERITY_WARN, "warn"),
	SEV_ROW(SEVERITY_ERROR, "error"),
	SEV_ROW(SEVERITY_CRITICAL, "critical"),
	SEV_ROW(SEVERITY_MAX, "max"),
	{ NULL, 0 }
};

/* same as for ops; defined so the values in the database are consistent */
#define RES_ROW(id, name, ok) { "%d, '" name "' , " #ok, id }
char *res_columns = "id, name, ok";
id_row res_rows[] = {
	RES_ROW(RESULT_UNKNOWN, "unknown", 0),
	RES_ROW(RESULT_NONE, "none", 0),
	RES_ROW(RESULT_SUCCEED, "succeed", 1),
	RES_ROW(RESULT_FAIL, "fail", 1),
	RES_ROW(RESULT_ERROR, "error", 0),
	RES_ROW(RESULT_MAX, "max", 0),
	{ NULL, 0 }
};

/* This seemed like a really good idea at the time.  The idea is that these
 * structures let me write semi-abstract code to "create a database" without
 * duplicating as much of the code.
 */
static struct sql_table {
	char *name;
	char *sql;
	char **names;
	id_row *values;
} file_tables[] = {
	{ "files",
	  "id INTEGER PRIMARY KEY, "
	    "path VARCHAR, "
	    "dev INTEGER, "
	    "ino INTEGER, "
	    "uid INTEGER, "
	    "gid INTEGER, "
	    "mode INTEGER, "
	    "rdev INTEGER",
	  NULL,
	  NULL },
	{ NULL, NULL, NULL, NULL },
}, log_tables[] = {
	{ "operations",
	  "id INTEGER PRIMARY KEY, name VARCHAR",
	  &op_columns,
	  op_rows }, 
	{ "results",
	  "id INTEGER PRIMARY KEY, name VARCHAR, ok INTEGER",
	  &res_columns,
	  res_rows }, 
	{ "severities",
	  "id INTEGER PRIMARY KEY, name VARCHAR",
	  &sev_columns,
	  sev_rows }, 
	{ "logs",
	  "id INTEGER PRIMARY KEY, "
	    "stamp INTEGER, "
	    "op INTEGER, "
	    "client INTEGER, "
	    "fd INTEGER, "
	    "dev INTEGER, "
	    "ino INTEGER, "
	    "mode INTEGER, "
	    "path VARCHAR, "
	    "result INTEGER, "
	    "severity INTEGER, "
	    "text VARCHAR ",
	  NULL,
	  NULL },
	{ NULL, NULL, NULL, NULL },
};

/* similarly, this creates indexes generically. */
static struct sql_index {
	char *name;
	char *table;
	char *keys;
} file_indexes[] = {
	{ "files__path", "files", "path" },
	{ "files__dev_ino", "files", "dev, ino" },
	{ NULL, NULL, NULL },
}, log_indexes[] = {
	{ NULL, NULL, NULL },
};

/* table migrations: */
/* If there is no migration table, we assume "version -1" -- the
 * version shipped with wrlinux 3.0, which had no version
 * number.  Otherwise, we check it for the highest version recorded.
 * We then perform, and then record, each migration in sequence.
 * The first migration is the migration to create the migrations
 * table; this way, it'll work on existing databases.  It'll also
 * work for new databases -- the migrations get performed in order
 * before the databases are considered to be set up.
 */

static char create_migration_table[] =
	"CREATE TABLE migrations ("
		"id INTEGER PRIMARY KEY, "
		"version INTEGER, "
		"stamp INTEGER, "
		"sql VARCHAR"
		");";
static char index_migration_table[] =
	"CREATE INDEX migration__version ON migrations (version)";

/* This used to be a { version, sql } pair, but version was always
 * the same as index into the table, so I removed it.
 * The first migration in each database is migration #0 -- the
 * creation of the migration table now being used for versioning.
 * The second is indexing on version -- sqlite3 can grab MAX(version)
 * faster if it's indexed.  (Indexing this table is very cheap, since
 * there are very few migrations and each one produces exactly
 * one insert.)
 */
static struct sql_migration {
	char *sql;
} file_migrations[] = {
	{ create_migration_table },
	{ index_migration_table },
	{ NULL },
}, log_migrations[] = {
	{ create_migration_table },
	{ index_migration_table },
	/* support for hostdeps merge -- this allows us to log "tags"
	 * along with events.
	 */
	{ "ALTER TABLE logs ADD tag VARCHAR;" },
	/* the logs table was defined so early I hadn't realized I cared
	 * about UID and GID.
	 */
	{ "ALTER TABLE logs ADD uid INTEGER;" },
	{ "ALTER TABLE logs ADD gid INTEGER;" },
	{ NULL },
};

/* pretty-print error along with the underlying SQL error. */
static void
dberr(sqlite3 *db, char *fmt, ...) {
	va_list ap;
	char debuff[8192];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(debuff, 8192, fmt, ap);
	va_end(ap);
	len = write(pseudo_util_debug_fd, debuff, len);
	if (db) {
		len = snprintf(debuff, 8192, ": %s\n", sqlite3_errmsg(db));
		len = write(pseudo_util_debug_fd, debuff, len);
	} else {
		len = write(pseudo_util_debug_fd, " (no db)\n", 9);
	}
}

/* those who enjoy children, sausages, and databases, should not watch
 * them being made.
 */
static int
make_tables(sqlite3 *db,
	struct sql_table *sql_tables,
	struct sql_index *sql_indexes,
	struct sql_migration *sql_migrations,
	char **existing, int rows) {

	static sqlite3_stmt *stmt;
	sqlite3_stmt *update_version = 0;
	struct sql_migration *m;
	int available_migrations;
	int version = -1;
	int i, j;
	char *sql;
	char *errmsg;
	int rc;
	int found = 0;

	for (i = 0; sql_tables[i].name; ++i) {
		found = 0;
		for (j = 1; j <= rows; ++j) {
			if (!strcmp(existing[j], sql_tables[i].name)) {
				found = 1;
				break;
			}
		}
		if (found)
			continue;
		/* now to create the table */
		sql = sqlite3_mprintf("CREATE TABLE %s ( %s );",
			sql_tables[i].name, sql_tables[i].sql);
		rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
		sqlite3_free(sql);
		if (rc) {
			dberr(db, "error trying to create %s", sql_tables[i].name);
			return 1;
		}
		if (sql_tables[i].values) {
			for (j = 0; sql_tables[i].values[j].fmt; ++j) {
				char buffer[256];
				sprintf(buffer, sql_tables[i].values[j].fmt, sql_tables[i].values[j].arg);
				sql = sqlite3_mprintf("INSERT INTO %s ( %s ) VALUES ( %s );",
					sql_tables[i].name,
					*sql_tables[i].names,
					buffer);
				rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
				sqlite3_free(sql);
				if (rc) {
					dberr(db, "error trying to populate %s",
						sql_tables[i].name);
					return 1;
				}
			}
		}
		for (j = 0; sql_indexes[j].name; ++j) {
			if (strcmp(sql_indexes[j].table, sql_tables[i].name))
				continue;
			sql = sqlite3_mprintf("CREATE INDEX %s ON %s ( %s );",
				sql_indexes[j].name,
				sql_indexes[j].table,
				sql_indexes[j].keys);
			rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
			sqlite3_free(sql);
			if (rc) {
				dberr(db, "error trying to index %s",
					sql_tables[i].name);
				return 1;
			}
		}
	}
	/* now, see about migrations */
	found = 0;
	for (j = 1; j <= rows; ++j) {
		if (!strcmp(existing[j], "migrations")) {
			found = 1;
			break;
		}
	}
	if (found) {
		sql = "SELECT MAX(version) FROM migrations;";
		rc = sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);
		if (rc) {
			dberr(db, "couldn't examine migrations table");
			return 1;
		}
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW) {
			version = (unsigned long) sqlite3_column_int64(stmt, 0);
			rc = sqlite3_step(stmt);
		} else {
			version = -1;
		}
		if (rc != SQLITE_DONE) {
			dberr(db, "not done after the single row we expected?", rc);
			return 1;
		}
		pseudo_debug(2, "existing database version: %d\n", version);
		rc = sqlite3_finalize(stmt);
		if (rc) {
			dberr(db, "couldn't finalize version check");
			return 1;
		}
	} else {
		pseudo_debug(2, "no existing database version\n");
		version = -1;
	}
	for (m = sql_migrations; m->sql; ++m)
		;
	available_migrations = m - sql_migrations;
	/* I am pretty sure this can never happen. */
	if (version < -1)
		version = -1;
	/* I hope this can never happen. */
	if (version >= available_migrations)
		version = available_migrations - 1;
	for (m = sql_migrations + (version + 1); m->sql; ++m) {
		int migration = (m - sql_migrations);
		pseudo_debug(3, "considering migration %d\n", migration);
		if (version >= migration)
			continue;
		pseudo_debug(2, "running migration %d\n", migration);
		rc = sqlite3_prepare_v2(db,
				m->sql,
				strlen(m->sql),
				&stmt, NULL);
		if (rc) {
			dberr(log_db, "couldn't prepare migration %d (%s)",
				migration, m->sql);
			return 1;
		}
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			dberr(file_db, "migration %d failed",
				migration);
			return 1;
		}
		sqlite3_finalize(stmt);
		/* this has to occur here, because the first migration
		 * executed CREATES the migration table, so you can't
		 * prepare this statement if you haven't already executed
		 * the first migration.
		 *
		 * Lesson learned:  Yes, it actually WILL be a sort of big
		 * deal to add versioning later.
		 */
		static char *update_sql =
			"INSERT INTO migrations ("
			"version, stamp, sql"
			") VALUES (?, ?, ?);";
		rc = sqlite3_prepare_v2(db,
				update_sql,
				strlen(update_sql),
				&update_version, NULL);
		if (rc) {
			dberr(db, "couldn't prepare statement to update migrations");
			return 1;
		}
		sqlite3_bind_int(update_version, 1, migration);
		sqlite3_bind_int(update_version, 2, time(NULL));
		sqlite3_bind_text(update_version, 3, m->sql, -1, SQLITE_STATIC);
		rc = sqlite3_step(update_version);
		if (rc != SQLITE_DONE) {
			dberr(db, "couldn't update migrations table (after migration to version %d)",
				migration);
			sqlite3_finalize(update_version);
			return 1;
		} else {
			pseudo_debug(3, "update of migrations (after %d) fine.\n",
				migration);
		}
		sqlite3_finalize(update_version);
		update_version = 0;
		version = migration;
	}

	return 0;
}

/* registered with atexit */
static void
cleanup_db(void) {
	pseudo_debug(1, "server exiting\n");
	if (file_db)
		sqlite3_close(file_db);
	if (log_db)
		sqlite3_close(log_db);
}

/* I hate this function.
 * The need to separate logs and files into separate database (performance
 * and stability suffered when they were together) was discovered after
 * this was written, and it is still full of half-considered code and
 * unreasonable tests.  The test for whether db == &file_db is particularly
 * odious.
 *
 * The basic idea is to open the database, and make sure the tables exist
 * (using the make_tables function above).  Options are set to make sqlite
 * run reasonably efficiently.
 */
static int
get_db(sqlite3 **db) {
	int rc;
	char *sql;
	char **results;
	int rows, columns;
	char *errmsg;
	static int registered_cleanup = 0;
	char *dbfile;

	if (!db)
		return 1;
	if (*db)
		return 0;
	if (db == &file_db) {
		dbfile = pseudo_prefix_path(PSEUDO_DATA "files.db");
		rc = sqlite3_open(dbfile, db);
		free(dbfile);
	} else {
		dbfile = pseudo_prefix_path(PSEUDO_DATA "logs.db");
		rc = sqlite3_open(dbfile, db);
		free(dbfile);
	}
	if (rc) {
		pseudo_diag("Failed: %s\n", sqlite3_errmsg(*db));
		sqlite3_close(*db);
		*db = NULL;
		return 1;
	}
	if (!registered_cleanup) {
		atexit(cleanup_db);
		registered_cleanup = 1;
	}
	if (db == &file_db) {
		rc = sqlite3_exec(*db, "PRAGMA legacy_file_format = OFF;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "file_db legacy_file_format");
		}
		rc = sqlite3_exec(*db, "PRAGMA journal_mode = PERSIST;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "file_db journal_mode");
		}
		rc = sqlite3_exec(*db, "PRAGMA locking_mode = EXCLUSIVE;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "file_db locking_mode");
		}
		/* Setting this to NORMAL makes pseudo noticably slower
		 * than fakeroot, but is perhaps more secure.  However,
		 * note that sqlite always flushes to the OS; what is lacking
		 * in non-synchronous mode is waiting for the OS to
		 * confirm delivery to media, and also a bunch of cache
		 * flushing and reloading which we probably don't really
		 * need.
		 */
		rc = sqlite3_exec(*db, "PRAGMA synchronous = OFF;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "file_db synchronous");
		}
	} else if (db == &log_db) {
		rc = sqlite3_exec(*db, "PRAGMA legacy_file_format = OFF;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "log_db legacy_file_format");
		}
		rc = sqlite3_exec(*db, "PRAGMA journal_mode = OFF;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "log_db journal_mode");
		}
		rc = sqlite3_exec(*db, "PRAGMA locking_mode = EXCLUSIVE;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "log_db locking_mode");
		}
		rc = sqlite3_exec(*db, "PRAGMA synchronous = OFF;", NULL, NULL, &errmsg);
		if (rc) {
			dberr(*db, "log_db synchronous");
		}
	}
	/* create database tables or die trying */
	sql =	"SELECT name FROM sqlite_master "
		"WHERE type = 'table' "
		"ORDER BY name;";
	rc = sqlite3_get_table(*db, sql, &results, &rows, &columns, &errmsg);
	if (rc) {
		pseudo_diag("Failed: %s\n", errmsg);
	} else {
		if (db == &file_db) {
			rc = make_tables(*db, file_tables, file_indexes, file_migrations, results, rows);
		} else if (db == &log_db) {
			rc = make_tables(*db, log_tables, log_indexes, log_migrations, results, rows);
		}
		sqlite3_free_table(results);
	}
	/* cleanup database before getting started */
	sqlite3_exec(*db, "VACUUM;", NULL, NULL, &errmsg);
	return rc;
}

/* put a prepared log entry into the database */
int
pdb_log_traits(pseudo_query_t *traits) {
	pseudo_query_t *trait;
	log_entry *e;
	int rc;

	if (!log_db && get_db(&log_db)) {
		pseudo_diag("database error.\n");
		return 1;
	}
	e = calloc(sizeof(*e), 1);
	if (!e) {
		pseudo_diag("can't allocate space for log entry.");
		return 1;
	}
	for (trait = traits; trait; trait = trait->next) {
		switch (trait->field) {
		case PSQF_CLIENT:
			e->client = trait->data.ivalue;
			break;
		case PSQF_DEV:
			e->dev = trait->data.ivalue;
			break;
		case PSQF_FD: 
			e->fd = trait->data.ivalue;
			break;
		case PSQF_FTYPE:
			e->mode |= (trait->data.ivalue & S_IFMT);
			break;
		case PSQF_GID:
			e->gid = trait->data.ivalue;
			break;
		case PSQF_INODE:
			e->ino = trait->data.ivalue;
			break;
		case PSQF_MODE:
			e->mode = trait->data.ivalue;
			break;
		case PSQF_OP:
			e->op = trait->data.ivalue;
			break;
		case PSQF_PATH:
			e->path = trait->data.svalue ?
					strdup(trait->data.svalue) : NULL;
			break;
		case PSQF_PERM:
			e->mode |= (trait->data.ivalue & ~(S_IFMT) & 0177777);
			break;
		case PSQF_RESULT:
			e->result = trait->data.ivalue;
			break;
		case PSQF_SEVERITY:
			e->severity = trait->data.ivalue;
			break;
		case PSQF_STAMP:
			e->stamp = trait->data.ivalue;
			break;
		case PSQF_TAG:
			e->tag = trait->data.svalue ?
					strdup(trait->data.svalue) : NULL;
			break;
		case PSQF_TEXT:
			e->text = trait->data.svalue ?
					strdup(trait->data.svalue) : NULL;
			break;
		case PSQF_UID:
			e->uid = trait->data.ivalue;
			break;
		case PSQF_ID:
		case PSQF_ORDER:
		default:
			pseudo_diag("Invalid trait %s for log creation.\n",
				pseudo_query_field_name(trait->field));
			free(e);
			return 1;
			break;
		}
	}
	rc = pdb_log_entry(e);
	log_entry_free(e);
	return rc;
}

/* create a log from a given log entry, with tag and text */
int
pdb_log_entry(log_entry *e) {
	char *sql = "INSERT INTO logs "
		    "(stamp, op, client, dev, gid, ino, mode, path, result, severity, text, tag, uid)"
		    " VALUES "
		    "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	static sqlite3_stmt *insert;
	int rc;

	if (!log_db && get_db(&log_db)) {
		pseudo_diag("database error.\n");
		return 1;
	}

	if (!insert) {
		rc = sqlite3_prepare_v2(log_db, sql, strlen(sql), &insert, NULL);
		if (rc) {
			dberr(log_db, "couldn't prepare INSERT statement");
			return 1;
		}
	}

	if (e) {
		if (e->stamp) {
			sqlite3_bind_int(insert, 1, e->stamp);
		} else {
			sqlite3_bind_int(insert, 1, (unsigned long) time(NULL));
		}
		sqlite3_bind_int(insert, 2, e->op);
		sqlite3_bind_int(insert, 3, e->client);
		sqlite3_bind_int(insert, 4, e->dev);
		sqlite3_bind_int(insert, 5, e->gid);
		sqlite3_bind_int(insert, 6, e->ino);
		sqlite3_bind_int(insert, 7, e->mode);
		if (e->path) {
			sqlite3_bind_text(insert, 8, e->path, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, 8);
		}
		sqlite3_bind_int(insert, 9, e->result);
		sqlite3_bind_int(insert, 10, e->severity);
		if (e->text) {
			sqlite3_bind_text(insert, 11, e->text, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, 11);
		}
		if (e->tag) {
			sqlite3_bind_text(insert, 12, e->tag, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, 12);
		}
		sqlite3_bind_int(insert, 13, e->uid);
	} else {
		sqlite3_bind_int(insert, 1, (unsigned long) time(NULL));
		sqlite3_bind_int(insert, 2, 0);
		sqlite3_bind_int(insert, 3, 0);
		sqlite3_bind_int(insert, 4, 0);
		sqlite3_bind_int(insert, 5, 0);
		sqlite3_bind_int(insert, 6, 0);
		sqlite3_bind_int(insert, 7, 0);
		sqlite3_bind_null(insert, 8);
		sqlite3_bind_int(insert, 9, 0);
		sqlite3_bind_int(insert, 10, 0);
		sqlite3_bind_null(insert, 11);
		sqlite3_bind_null(insert, 12);
		sqlite3_bind_int(insert, 13, 0);
	}

	rc = sqlite3_step(insert);
	if (rc != SQLITE_DONE) {
		dberr(log_db, "insert may have failed");
	}
	sqlite3_reset(insert);
	sqlite3_clear_bindings(insert);
	return rc != SQLITE_DONE;
}
/* create a log from a given message, with tag and text */
int
pdb_log_msg(sev_id_t severity, pseudo_msg_t *msg, const char *tag, const char *text, ...) {
	char *sql = "INSERT INTO logs "
		    "(stamp, op, client, dev, gid, ino, mode, path, result, severity, text, tag, uid)"
		    " VALUES "
		    "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	static sqlite3_stmt *insert;
	char buffer[8192];
	int rc;
	va_list ap;

	if (text) {
		va_start(ap, text);
		vsnprintf(buffer, 8192, text, ap);
		va_end(ap);
		text = buffer;
	}

	if (!log_db && get_db(&log_db)) {
		pseudo_diag("database error.\n");
		return 1;
	}

	if (!insert) {
		rc = sqlite3_prepare_v2(log_db, sql, strlen(sql), &insert, NULL);
		if (rc) {
			dberr(log_db, "couldn't prepare INSERT statement");
			return 1;
		}
	}

	if (msg) {
		sqlite3_bind_int(insert, 2, msg->op);
		sqlite3_bind_int(insert, 3, msg->client);
		sqlite3_bind_int(insert, 4, msg->dev);
		sqlite3_bind_int(insert, 5, msg->gid);
		sqlite3_bind_int(insert, 6, msg->ino);
		sqlite3_bind_int(insert, 7, msg->mode);
		if (msg->pathlen) {
			sqlite3_bind_text(insert, 8, msg->path, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, 8);
		}
		sqlite3_bind_int(insert, 9, msg->result);
		sqlite3_bind_int(insert, 13, msg->uid);
	} else {
		sqlite3_bind_int(insert, 2, 0);
		sqlite3_bind_int(insert, 3, 0);
		sqlite3_bind_int(insert, 4, 0);
		sqlite3_bind_int(insert, 5, 0);
		sqlite3_bind_int(insert, 6, 0);
		sqlite3_bind_int(insert, 7, 0);
		sqlite3_bind_null(insert, 8);
		sqlite3_bind_int(insert, 9, 0);
		sqlite3_bind_int(insert, 13, 0);
	}
	sqlite3_bind_int(insert, 1, (unsigned long) time(NULL));
	sqlite3_bind_int(insert, 10, severity);
	if (text) {
		sqlite3_bind_text(insert, 11, text, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(insert, 11);
	}
	if (tag) {
		sqlite3_bind_text(insert, 12, tag, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(insert, 12);
	}

	rc = sqlite3_step(insert);
	if (rc != SQLITE_DONE) {
		dberr(log_db, "insert may have failed");
	}
	sqlite3_reset(insert);
	sqlite3_clear_bindings(insert);
	return rc != SQLITE_DONE;
}

#define BFSZ 8192
typedef struct {
	size_t buflen;
	char *data;
	char *tail;
} buffer;

static int
frag(buffer *b, char *fmt, ...) {
	va_list ap;
	static size_t curlen;
	int rc;

	if (!b) {
		pseudo_diag("frag called without buffer.\n");
		return -1;
	}
	curlen = b->tail - b->data;
	va_start(ap, fmt);
	rc = vsnprintf(b->tail, b->buflen - curlen, fmt, ap);
	va_end(ap);
	if (rc >= (b->buflen - curlen)) {
		size_t newlen = b->buflen;
		while (newlen <= (rc + curlen))
			newlen *= 2;
		char *newbuf = malloc(newlen);
		if (!newbuf) {
			pseudo_diag("failed to allocate SQL buffer.\n");
			return -1;
		}
		memcpy(newbuf, b->data, curlen + 1);
		b->tail = newbuf + curlen;
		free(b->data);
		b->data = newbuf;
		b->buflen = newlen;
		/* try again */
		va_start(ap, fmt);
		rc = vsnprintf(b->tail, b->buflen - curlen, fmt, ap);
		va_end(ap);
		if (rc >= (b->buflen - curlen)) {
			pseudo_diag("tried to reallocate larger buffer, failed.  giving up.\n");
			return -1;
		}
	}
	if (rc >= 0)
		b->tail += rc;
	return rc;
}

log_history
pdb_history(pseudo_query_t *traits, unsigned long fields, int distinct) {
	log_history h = NULL;
	pseudo_query_t *trait;
	sqlite3_stmt *select;
	int done_any = 0;
	int field = 0;
	char *order_by = "id";
	char *order_dir = "ASC";
	int rc;
	pseudo_query_field_t f;
	static buffer *sql;

	/* this column arrangement is used by pdb_history_entry() */
	if (!sql) {
		sql = malloc(sizeof *sql);
		if (!sql) {
			pseudo_diag("can't allocate SQL buffer.\n");
			return 0;
		}
		sql->buflen = 512;
		sql->data = malloc(sql->buflen);
		if (!sql->data) {
			pseudo_diag("can't allocate SQL text buffer.\n");
			free(sql);
			return 0;
		}
	}
	sql->tail = sql->data;
	frag(sql, "SELECT ");

	if (!log_db && get_db(&log_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}

	if (distinct)
		frag(sql, "DISTINCT ");

	done_any = 0;
	for (f = PSQF_NONE + 1; f < PSQF_MAX; ++f) {
		if (fields & (1 << f)) {
			frag(sql, "%s%s",
				done_any ? ", " : "",
				pseudo_query_field_name(f));
			done_any = 1;
		}
	}

	frag(sql, " FROM logs ");
	/* first, build up an SQL string with the fields and operators */
	done_any = 0;
	for (trait = traits; trait; trait = trait->next) {
		if (trait->field != PSQF_ORDER) {
			if (done_any) {
				frag(sql, "AND ");
			} else {
				frag(sql, "WHERE ");
				done_any = 1;
			}
		}
		switch (trait->field) {
		case PSQF_TEXT: /* FALLTHROUGH */
		case PSQF_TAG: /* FALLTHROUGH */
		case PSQF_PATH:
			switch (trait->type) {
			case PSQT_LIKE:
				frag(sql, "%s %s ('%' || ? || '%')",
					pseudo_query_field_name(trait->field),
					pseudo_query_type_sql(trait->type));
				break;
			case PSQT_NOTLIKE:
			case PSQT_SQLPAT:
				frag(sql, "%s %s ?",
					pseudo_query_field_name(trait->field),
					pseudo_query_type_sql(trait->type));
				break;
			default:
				frag(sql, "%s %s ? ",
					pseudo_query_field_name(trait->field),
					pseudo_query_type_sql(trait->type));
				break;
			}
			break;
		case PSQF_PERM:
			switch (trait->type) {
			case PSQT_LIKE: /* FALLTHROUGH */
			case PSQT_NOTLIKE: /* FALLTHROUGH */
			case PSQT_SQLPAT:
				pseudo_diag("Error:  Can't use a LIKE match on non-text fields.\n");
				return 0;
				break;
			default:
				break;
			}
			/* mask out permission bits */
			frag(sql, "(%s & %d) %s ? ",
				"mode",
				~(S_IFMT) & 0177777,
				pseudo_query_type_sql(trait->type));
			break;
		case PSQF_FTYPE:
			switch (trait->type) {
			case PSQT_LIKE: /* FALLTHROUGH */
			case PSQT_NOTLIKE: /* FALLTHROUGH */
			case PSQT_SQLPAT:
				pseudo_diag("Error:  Can't use a LIKE match on non-text fields.\n");
				return 0;
				break;
			default:
				break;
			}
			/* mask out permission bits */
			frag(sql, "(%s & %d) %s ? ",
				"mode",
				S_IFMT,
				pseudo_query_type_sql(trait->type));
			break;
		case PSQF_ORDER:
			order_by = pseudo_query_field_name(trait->data.ivalue);
			switch (trait->type) {
			case PSQT_LESS:
				order_dir = "DESC";
				break;
			case PSQT_EXACT: /* FALLTHROUGH */
				/* this was already the default */
				break;
			case PSQT_GREATER:
				order_dir = "ASC";
				break;
			default:
				pseudo_diag("Ordering must be < or >.\n");
				return 0;
				break;
			}
			break;
		default:
			switch (trait->type) {
			case PSQT_LIKE: /* FALLTHROUGH */
			case PSQT_NOTLIKE: /* FALLTHROUGH */
			case PSQT_SQLPAT:
				pseudo_diag("Error:  Can't use a LIKE match on non-text fields.\n");
				return 0;
				break;
			default:
				frag(sql, "%s %s ? ",
					pseudo_query_field_name(trait->field),
					pseudo_query_type_sql(trait->type));
				break;
			}
			break;
		}
	}
	frag(sql, "ORDER BY %s %s;", order_by, order_dir);
	pseudo_debug(1, "created SQL: <%s>\n", sql->data);

	/* second, prepare it */
	rc = sqlite3_prepare_v2(log_db, sql->data, strlen(sql->data), &select, NULL);
	if (rc) {
		dberr(log_db, "couldn't prepare SELECT statement");
		return 0;
	}

	/* third, bind the fields */
	field = 1;
	for (trait = traits; trait; trait = trait->next) {
		switch (trait->field) {
		case PSQF_ORDER:
			/* this just creates a hunk of SQL above */
			break;
		case PSQF_PATH: /* FALLTHROUGH */
		case PSQF_TAG: /* FALLTHROUGH */
		case PSQF_TEXT:
			sqlite3_bind_text(select, field++,
				trait->data.svalue, -1, SQLITE_STATIC);
			break;
		case PSQF_FTYPE: /* FALLTHROUGH */
		case PSQF_CLIENT: /* FALLTHROUGH */
		case PSQF_DEV: /* FALLTHROUGH */
		case PSQF_FD: /* FALLTHROUGH */
		case PSQF_INODE: /* FALLTHROUGH */
		case PSQF_GID: /* FALLTHROUGH */
		case PSQF_PERM: /* FALLTHROUGH */
		case PSQF_MODE: /* FALLTHROUGH */
		case PSQF_OP: /* FALLTHROUGH */
		case PSQF_RESULT: /* FALLTHROUGH */
		case PSQF_SEVERITY: /* FALLTHROUGH */
		case PSQF_STAMP: /* FALLTHROUGH */
		case PSQF_UID: /* FALLTHROUGH */
			sqlite3_bind_int(select, field++, trait->data.ivalue);
			break;
		default:
			pseudo_diag("Inexplicably invalid field type %d\n", trait->field);
			sqlite3_finalize(select);
			return 0;
		}
	}

	/* fourth, return the statement, now ready to be stepped through */
	h = malloc(sizeof(*h));
	if (h) {
		h->rc = 0;
		h->fields = fields;
		h->stmt = select;
	} else {
		pseudo_diag("failed to allocate memory for log_history\n");
	}
	return h;
}

log_entry *
pdb_history_entry(log_history h) {
	log_entry *l;
	const unsigned char *s;
	int column;
	pseudo_query_field_t f;

	if (!h || !h->stmt)
		return 0;
	/* in case someone tries again after we're already done */
	if (h->rc == SQLITE_DONE) {
		return 0;
	}
	h->rc = sqlite3_step(h->stmt);
	if (h->rc == SQLITE_DONE) {
		return 0;
	} else if (h->rc != SQLITE_ROW) {
		dberr(log_db, "statement failed");
		return 0;
	}
	l = calloc(sizeof(log_entry), 1);
	if (!l) {
		pseudo_diag("couldn't allocate log entry.\n");
		return 0;
	}

	column = 0;
	for (f = PSQF_NONE + 1; f < PSQF_MAX; ++f) {
		if (!(h->fields & (1 << f)))
			continue;
		switch (f) {
		case PSQF_CLIENT:
			l->client = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_DEV:
			l->dev = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_FD: 
			l->fd = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_GID:
			l->gid = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_INODE:
			l->ino = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_MODE:
			l->mode = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_OP:
			l->op = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_PATH:
			s = sqlite3_column_text(h->stmt, column++);
			if (s)
				l->path = strdup((char *) s);
			break;
		case PSQF_RESULT:
			l->result = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_SEVERITY:
			l->severity = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_STAMP:
			l->stamp = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_TAG:
			s = sqlite3_column_text(h->stmt, column++);
			if (s)
				l->tag = strdup((char *) s);
			break;
		case PSQF_TEXT:
			s = sqlite3_column_text(h->stmt, column++);
			if (s)
				l->text = strdup((char *) s);
			break;
		case PSQF_UID:
			l->uid = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_ORDER: /* FALLTHROUGH */
		case PSQF_FTYPE: /* FALLTHROUGH */
		case PSQF_PERM:
			pseudo_diag("field %s should not be in the fields list.\n",
				pseudo_query_field_name(f));
			return 0;
			break;
		default:
			pseudo_diag("unknown field %d\n", f);
			return 0;
			break;
		}
	}

	return l;
}

void
pdb_history_free(log_history h) {
	if (!h)
		return;
	if (h->stmt) {
		sqlite3_reset(h->stmt);
		sqlite3_finalize(h->stmt);
	}
	free(h);
}

void
log_entry_free(log_entry *e) {
	if (!e)
		return;
	free(e->text);
	free(e->path);
	free(e->tag);
	free(e);
}

/* Now for the actual file handling code! */

/* pdb_link_file:  Creates a new file from msg, using the provided path
 * or 'NAMELESS FILE'.
 */
int
pdb_link_file(pseudo_msg_t *msg) {
	static sqlite3_stmt *insert;
	int rc;
	char *sql = "INSERT INTO files "
		    " ( path, dev, ino, uid, gid, mode, rdev ) "
		    " VALUES (?, ?, ?, ?, ?, ?, ?);";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!insert) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &insert, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare INSERT statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (msg->pathlen) {
		sqlite3_bind_text(insert, 1, msg->path, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_text(insert, 1, "NAMELESS FILE", -1, SQLITE_STATIC);
	}
	sqlite3_bind_int(insert, 2, msg->dev);
	sqlite3_bind_int(insert, 3, msg->ino);
	sqlite3_bind_int(insert, 4, msg->uid);
	sqlite3_bind_int(insert, 5, msg->gid);
	sqlite3_bind_int(insert, 6, msg->mode);
	sqlite3_bind_int(insert, 7, msg->rdev);
	pseudo_debug(2, "linking %s: dev %llu, ino %llu, mode %o, owner %d\n",
		(msg->pathlen ? msg->path : "<nil> (as NAMELESS FILE)"),
		(unsigned long long) msg->dev, (unsigned long long) msg->ino,
		(int) msg->mode, msg->uid);
	rc = sqlite3_step(insert);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "insert may have failed (rc %d)", rc);
	}
	sqlite3_reset(insert);
	sqlite3_clear_bindings(insert);
	return rc != SQLITE_DONE;
}

/* pdb_unlink_file_dev:  Delete every instance of a dev/inode pair. */
int
pdb_unlink_file_dev(pseudo_msg_t *msg) {
	static sqlite3_stmt *delete;
	int rc;
	char *sql = "DELETE FROM files WHERE dev = ? AND ino = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!delete) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &delete, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(delete, 1, msg->dev);
	sqlite3_bind_int(delete, 2, msg->ino);
	rc = sqlite3_step(delete);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete by inode may have failed");
	}
	sqlite3_reset(delete);
	sqlite3_clear_bindings(delete);
	return rc != SQLITE_DONE;
}

/* provide a path for a 'NAMELESS FILE' entry */
int
pdb_update_file_path(pseudo_msg_t *msg) {
	static sqlite3_stmt *update;
	int rc;
	char *sql = "UPDATE files SET path = ? "
		"WHERE dev = ? AND ino = ? AND path = 'NAMELESS FILE';";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!update) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &update, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare UPDATE statement");
			return 1;
		}
	}
	if (!msg || !msg->pathlen) {
		pseudo_debug(1, "can't update a file without a message or path.\n");
		return 1;
	}
	sqlite3_bind_text(update, 1, msg->path, -1, SQLITE_STATIC);
	sqlite3_bind_int(update, 2, msg->dev);
	sqlite3_bind_int(update, 3, msg->ino);
	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update path by inode may have failed");
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	return rc != SQLITE_DONE;
}

/* unlink a file, by path */
/* SQLite limitations:
 * path LIKE foo '/%' -> can't use index
 * path = A OR path = B -> can't use index
 * Solution: 
 * 1.  Use two separate queries, so there's no OR.
 * 2.  (From the SQLite page):  Use > and < instead of a glob at the end.
 */
int
pdb_unlink_file(pseudo_msg_t *msg) {
	static sqlite3_stmt *delete_exact, *delete_sub;
	int rc;
	char *sql_delete_exact = "DELETE FROM files WHERE path = ?;";
	char *sql_delete_sub = "DELETE FROM files WHERE "
				"(path > (? || '/') AND path < (? || '0'));";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!delete_exact) {
		rc = sqlite3_prepare_v2(file_db, sql_delete_exact, strlen(sql_delete_exact), &delete_exact, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!delete_sub) {
		rc = sqlite3_prepare_v2(file_db, sql_delete_sub, strlen(sql_delete_sub), &delete_sub, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (msg->pathlen) {
		sqlite3_bind_text(delete_exact, 1, msg->path, -1, SQLITE_STATIC);
		sqlite3_bind_text(delete_sub, 1, msg->path, -1, SQLITE_STATIC);
		sqlite3_bind_text(delete_sub, 2, msg->path, -1, SQLITE_STATIC);
	} else {
		pseudo_debug(1, "cannot unlink a file without a path.");
		return 1;
	}
	rc = sqlite3_step(delete_exact);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete exact by path may have failed");
	}
	rc = sqlite3_step(delete_sub);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete sub by path may have failed");
	}
	sqlite3_reset(delete_exact);
	sqlite3_reset(delete_sub);
	sqlite3_clear_bindings(delete_exact);
	sqlite3_clear_bindings(delete_sub);
	return rc != SQLITE_DONE;
}

/* rename a file.
 * If there are any other files with paths that are rooted in "file", then
 * file must really be a directory, and they should be renamed.
 * 
 * This is tricky:
 * You have to rename everything starting with "path/", but also "path" itself
 * with no slash.  Luckily for us, SQL can replace the old path with the
 * new path.
 */
int
pdb_rename_file(const char *oldpath, pseudo_msg_t *msg) {
	static sqlite3_stmt *update_exact, *update_sub;
	int rc;
	char *sql_update_exact = "UPDATE files SET path = ? WHERE path = ?;";
	char *sql_update_sub = "UPDATE files SET path = replace(path, ?, ?) "
			       "WHERE (path > (? || '/') AND path < (? || '0'));";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!update_exact) {
		rc = sqlite3_prepare_v2(file_db, sql_update_exact, strlen(sql_update_exact), &update_exact, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare UPDATE statement");
			return 1;
		}
	}
	if (!update_sub) {
		rc = sqlite3_prepare_v2(file_db, sql_update_sub, strlen(sql_update_sub), &update_sub, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare UPDATE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (!msg->pathlen) {
		pseudo_debug(1, "rename: No path provided (ino %llu)\n", (unsigned long long) msg->ino);
		return 1;
	}
	if (!oldpath) {
		pseudo_debug(1, "rename: No old path for %s\n", msg->path);
		return 1;
	}
	pseudo_debug(2, "rename: Changing %s to %s\n", oldpath, msg->path);
	rc = sqlite3_bind_text(update_exact, 1, msg->path, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_exact, 2, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 1, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 2, msg->path, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 3, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 4, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_step(update_exact);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update exact may have failed: rc %d", rc);
	}
	rc = sqlite3_step(update_sub);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update sub may have failed: rc %d", rc);
	}
	sqlite3_reset(update_exact);
	sqlite3_reset(update_sub);
	sqlite3_clear_bindings(update_exact);
	sqlite3_clear_bindings(update_sub);
	return rc != SQLITE_DONE;
}

/* change uid/gid/mode/rdev in any existing entries matching a given
 * dev/inode pair.
 */
int
pdb_update_file(pseudo_msg_t *msg) {
	static sqlite3_stmt *update;
	int rc;
	char *sql = "UPDATE files "
		    " SET uid = ?, gid = ?, mode = ?, rdev = ? "
		    " WHERE dev = ? AND ino = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!update) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &update, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare UPDATE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(update, 1, msg->uid);
	sqlite3_bind_int(update, 2, msg->gid);
	sqlite3_bind_int(update, 3, msg->mode);
	sqlite3_bind_int(update, 4, msg->rdev);
	sqlite3_bind_int(update, 5, msg->dev);
	sqlite3_bind_int(update, 6, msg->ino);

	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update may have failed: rc %d", rc);
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	pseudo_debug(2, "updating dev %llu, ino %llu, new mode %o, owner %d\n",
		(unsigned long long) msg->dev, (unsigned long long) msg->ino,
		(int) msg->mode, msg->uid);
	return rc != SQLITE_DONE;
}

/* find file using both path AND dev/inode as key */
int
pdb_find_file_exact(pseudo_msg_t *msg) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE dev = ? AND ino = ? AND path = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(select, 1, msg->dev);
	sqlite3_bind_int(select, 2, msg->ino);
	rc = sqlite3_bind_text(select, 3, msg->path, -1, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "error binding %s to select", msg->pathlen ? msg->path : "<nil>");
	}
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(3, "find_exact: sqlite_done on first row\n");
		rc = 1;
		break;
	default:
		dberr(file_db, "find_exact: select returned neither a row nor done");
		rc = 1;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return rc;
}

/* find file using path as a key */
int
pdb_find_file_path(pseudo_msg_t *msg) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE path = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 1;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (!msg->pathlen) {
		return 1;
	}
	rc = sqlite3_bind_text(select, 1, msg->path, -1, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "error binding %s to select", msg->pathlen ? msg->path : "<nil>");
	}

	rc = sqlite3_column_count(select);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		msg->dev = (unsigned long) sqlite3_column_int64(select, 2);
		msg->ino = (unsigned long) sqlite3_column_int64(select, 3);
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(3, "find_path: sqlite_done on first row\n");
		rc = 1;
		break;
	default:
		dberr(file_db, "find_path: select returned neither a row nor done");
		rc = 1;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return rc;
}

/* find path for a file, given dev and inode as keys */
char *
pdb_get_file_path(pseudo_msg_t *msg) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT path FROM files WHERE dev = ? AND ino = ?;";
	char *response;

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 0;
		}
	}
	if (!msg) {
		return 0;
	}
	sqlite3_bind_int(select, 1, msg->dev);
	sqlite3_bind_int(select, 2, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		response = (char *) sqlite3_column_text(select, 0);
		if (response) {
			if (strcmp(response, "NAMELESS FILE")) {
				response = strdup(response);
			} else {
				response = 0;
			}
		}
		break;
	case SQLITE_DONE:
		pseudo_debug(3, "find_dev: sqlite_done on first row\n");
		response = 0;
		break;
	default:
		dberr(file_db, "find_dev: select returned neither a row nor done");
		response = 0;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return response;
}

/* find file using dev/inode as key */
int
pdb_find_file_dev(pseudo_msg_t *msg) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE dev = ? AND ino = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(select, 1, msg->dev);
	sqlite3_bind_int(select, 2, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(3, "find_dev: sqlite_done on first row\n");
		rc = 1;
		break;
	default:
		dberr(file_db, "find_dev: select returned neither a row nor done");
		rc = 1;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return rc;
}

/* find file using only inode as key.  Unused for now, planned to come
 * in for NFS usage.
 */
int
pdb_find_file_ino(pseudo_msg_t *msg) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE ino = ?;";

	if (!file_db && get_db(&file_db)) {
		pseudo_diag("database error.\n");
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(select, 1, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		msg->dev = (unsigned long) sqlite3_column_int64(select, 2);
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(3, "find_ino: sqlite_done on first row\n");
		rc = 1;
		break;
	default:
		dberr(file_db, "find_ino: select returned neither a row nor done");
		rc = 1;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return rc;
}
