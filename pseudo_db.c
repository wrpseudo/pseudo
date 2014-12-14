/*
 * pseudo_db.c, sqlite3 interface
 * 
 * Copyright (c) 2008-2010,2013 Wind River Systems, Inc.
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
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_db.h"

/* #define NPROFILE */

#ifdef NPROFILE
void xProfile(void * pArg, const char * pQuery, sqlite3_uint64 pTimeTaken)
{
       pseudo_diag("profile: %lld %s\n", pTimeTaken, pQuery);
}
#endif

struct log_history {
	int rc;
	unsigned long fields;
	sqlite3_stmt *stmt;
};

struct pdb_file_list {
	int rc;
	sqlite3_stmt *stmt;
};

static int file_db_dirty = 0;
#ifdef USE_MEMORY_DB
static sqlite3 *real_file_db = 0;
#endif
static sqlite3 *file_db = 0;
static sqlite3 *log_db = 0;

/* What's going on here, you might well ask?
 * This contains a template to build the database.  I suppose maybe it
 * should have been elegantly done as a big chunk of embedded SQL, but 
 * this looked like a good idea at the time.
 */
typedef struct { char *fmt; int arg; } id_row;

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
	{ "xattrs",
	  "id INTEGER PRIMARY KEY, "
	    "file_id INTEGER REFERENCES files(id) ON DELETE CASCADE, "
	    "name VARCHAR, "
	    "value VARCHAR",
	  NULL,
	  NULL },
	{ NULL, NULL, NULL, NULL },
}, log_tables[] = {
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
/*	{ "files__path", "files", "path" }, */
	{ "files__path_dev_ino", "files", "path, dev, ino" },
	{ "files__dev_ino", "files", "dev, ino" },
	{ "xattrs__file", "xattrs", "file_id" },
	{ NULL, NULL, NULL },
}, log_indexes[] = {
	{ NULL, NULL, NULL },
};

static char *file_pragmas[] = {
	"PRAGMA legacy_file_format = OFF;",
	"PRAGMA journal_mode = OFF;",
	/* the default page size produces painfully bad behavior
	 * for memory databases with some versions of sqlite.
	 */
	"PRAGMA page_size = 8192;",
	"PRAGMA locking_mode = EXCLUSIVE;",
	/* Setting this to NORMAL makes pseudo noticably slower
	 * than fakeroot, but is perhaps more secure.  However,
	 * note that sqlite always flushes to the OS; what is lacking
	 * in non-synchronous mode is waiting for the OS to
	 * confirm delivery to media, and also a bunch of cache
	 * flushing and reloading which we probably don't really
	 * need.
	 */
	"PRAGMA synchronous = OFF;",
	"PRAGMA foreign_keys = ON;",
	NULL
};

static char *log_pragmas[] = {
	"PRAGMA legacy_file_format = OFF;",
	"PRAGMA journal_mode = OFF;",
	"PRAGMA locking_mode = EXCLUSIVE;",
	"PRAGMA synchronous = OFF;",
	NULL
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
	{ "ALTER TABLE files ADD deleting INTEGER;" },
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
	/* track access types (read/write, etc) */
	{ "ALTER TABLE logs ADD access INTEGER;" },
	/* client program/path */
	{ "ALTER TABLE logs ADD program VARCHAR;" },
	/* message type (ping, op) */
	{ "ALTER TABLE logs ADD type INTEGER;" },
	{ NULL },
};

/* cleanup database before getting started
 *
 * On a large build, the logs database gets GIGANTIC...  And
 * we rarely-if-ever delete things from it.  So instead of
 * doing the vacuum operation on it at startup, which can impose
 * a several-minute delay, we do it only on deletions.
 *
 * There's no setup for log database right now.
 */
char *file_setups[] = {
	"VACUUM;",
	NULL,
};

struct database_info {
	char *pathname;
	struct sql_index *indexes;
	struct sql_table *tables;
	struct sql_migration *migrations;
	char **pragmas;
	char **setups;
	struct sqlite3 **db;
};

static struct database_info db_infos[] = {
	{
		"logs.db",
		log_indexes,
		log_tables,
		log_migrations,
		log_pragmas,
		NULL,
		&log_db
	},
	{
		"files.db",
		file_indexes,
		file_tables,
		file_migrations,
		file_pragmas,
		file_setups,
#ifdef USE_MEMORY_DB
		&real_file_db
	},
	{
		":memory:",
		file_indexes,
		file_tables,
		file_migrations,
		file_pragmas,
		file_setups,
#endif
		&file_db
	},
	{ 0, 0, 0, 0, 0, 0, 0 }
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

#ifdef USE_MEMORY_DB

static void
pdb_backup() {
        sqlite3_backup *pBackup;
        /* no point in doing this if we don't have a database to back up,
	 * or nothing's changed.
	 */
        if (!file_db || !real_file_db || !file_db_dirty)
                return;

	pBackup = sqlite3_backup_init(real_file_db, "main", file_db, "main");
	if (pBackup) {
		int rc;
		(void)sqlite3_backup_step(pBackup, -1);
		rc = sqlite3_backup_finish(pBackup);
		if (rc != SQLITE_OK) {
			dberr(real_file_db, "error during flush to disk");
		}
	}
	file_db_dirty = 0;
}

static void
pdb_restore() {
        sqlite3_backup *pBackup;
        /* no point in doing this if we don't have a database to back up */
        if (!file_db || !real_file_db)
                return;

	pBackup = sqlite3_backup_init(file_db, "main", real_file_db, "main");
	if (pBackup) {
		int rc;
		(void)sqlite3_backup_step(pBackup, -1);
		rc = sqlite3_backup_finish(pBackup);
		if (rc != SQLITE_OK) {
			dberr(file_db, "error during load from disk");
		}
	}
	file_db_dirty = 0;
}

int
pdb_maybe_backup(void) {
        static int occasional = 0;
        if (file_db && real_file_db) {
                occasional = (occasional + 1) % 10;
                if (occasional == 0) {
                        pdb_backup();
                        return 1;
                }
        }
        return 0;
}
#else /* USE_MEMORY_DB */
int
pdb_maybe_backup(void) {
	return 0;
}
#endif

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
		printf("considering table %s\n", sql_tables[i].name);
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
				snprintf(buffer, sizeof(buffer), sql_tables[i].values[j].fmt, sql_tables[i].values[j].arg);
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
		pseudo_debug(PDBGF_DB, "existing database version: %d\n", version);
		rc = sqlite3_finalize(stmt);
		if (rc) {
			dberr(db, "couldn't finalize version check");
			return 1;
		}
	} else {
		pseudo_debug(PDBGF_DB, "no existing database version\n");
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
		pseudo_debug(PDBGF_DB, "considering migration %d\n", migration);
		if (version >= migration)
			continue;
		pseudo_debug(PDBGF_DB, "running migration %d\n", migration);
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
			pseudo_debug(PDBGF_DB, "update of migrations (after %d) fine.\n",
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
	pseudo_debug(PDBGF_SERVER, "server exiting\n");
#ifdef USE_MEMORY_DB
        if (real_file_db) {
                pdb_backup();
                sqlite3_close(real_file_db);
	}
#endif
	if (file_db)
		sqlite3_close(file_db);
	if (log_db)
		sqlite3_close(log_db);
}

/* This function has been rewritten and I no longer hate it.  I just feel
 * like it's important to say that.
 */
static int
get_db(struct database_info *dbinfo) {
	int rc;
	int i;
	char *sql;
	char **results;
	int rows, columns;
	char *errmsg;
	static int registered_cleanup = 0;
	char *dbfile;
	sqlite3 *db;

	if (!dbinfo)
		return 1;
	if (!dbinfo->db)
		return 1;
	/* this database is perhaps already initialized? */
	if (*(dbinfo->db))
		return 0;

	dbfile = pseudo_localstatedir_path(dbinfo->pathname);
#ifdef USE_MEMORY_DB
        if (!strcmp(dbinfo->pathname, ":memory:")) {
                rc = sqlite3_open(dbinfo->pathname, &db);
        } else
#endif
                rc = sqlite3_open(dbfile, &db);
	free(dbfile);
	if (rc) {
		pseudo_diag("Failed: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		*(dbinfo->db) = NULL;
		return 1;
	}
	/* we store this in the database_info, but hereafter we'll just use
	 * the name db, because it is shorter.
	 */
	*dbinfo->db = db;
	if (!registered_cleanup) {
		atexit(cleanup_db);
		registered_cleanup = 1;
	}
	if (dbinfo->pragmas) {
		for (i = 0; dbinfo->pragmas[i]; ++i) {
			rc = sqlite3_exec(db, dbinfo->pragmas[i], NULL, NULL, &errmsg);
			pseudo_debug(PDBGF_SQL | PDBGF_VERBOSE, "executed pragma: '%s', rc %d.\n",
				dbinfo->pragmas[i], rc);
			if (rc) {
				dberr(db, dbinfo->pragmas[i]);
			}
		}
	}
	/* create database tables or die trying */
	sql =	"SELECT name FROM sqlite_master "
		"WHERE type = 'table' "
		"ORDER BY name;";
	rc = sqlite3_get_table(db, sql, &results, &rows, &columns, &errmsg);
	if (rc) {
		pseudo_diag("Failed: %s\n", errmsg);
	} else {
		rc = make_tables(db, dbinfo->tables, dbinfo->indexes, dbinfo->migrations, results, rows);
		sqlite3_free_table(results);
	}
	/* as of now, the only setup is a vacuum operation which we don't care about
	 * the results of.
	 */
	if (dbinfo->setups) {
		for (i = 0; dbinfo->setups[i]; ++i) {
			sqlite3_exec(db, dbinfo->setups[i], NULL, NULL, &errmsg);
		}
	}
	return rc;
}

static int
get_dbs(void) {
	int err = 0;
	int i;
#ifdef USE_MEMORY_DB
        int already_loaded = 0;
        if (file_db)
                already_loaded = 1;
#endif
	for (i = 0; db_infos[i].db; ++i) {
		if (get_db(&db_infos[i])) {
			pseudo_diag("Error getting '%s' database.\n",
				db_infos[i].pathname);
			err = 1;
		}
	}
#ifdef USE_MEMORY_DB
        if (!already_loaded && file_db)
                pdb_restore();
#endif
	return err;
}

/* put a prepared log entry into the database */
int
pdb_log_traits(pseudo_query_t *traits) {
	pseudo_query_t *trait;
	log_entry *e;
	int rc;

	if (!log_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 1;
	}
	e = calloc(sizeof(*e), 1);
	if (!e) {
		pseudo_diag("can't allocate space for log entry.");
		return 1;
	}
	for (trait = traits; trait; trait = trait->next) {
		switch (trait->field) {
		case PSQF_ACCESS:
			e->access = trait->data.ivalue;
			break;
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
		case PSQF_PROGRAM:
			e->program = trait->data.svalue ?
					strdup(trait->data.svalue) : NULL;
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
		case PSQF_TYPE:
			e->type = trait->data.ivalue;
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
		    "(stamp, op, access, client, dev, gid, ino, mode, path, result, severity, text, program, tag, type, uid)"
		    " VALUES "
		    "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	static sqlite3_stmt *insert;
	int field;
	int rc;

	if (!log_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 1;
	}

	if (!insert) {
		rc = sqlite3_prepare_v2(log_db, sql, strlen(sql), &insert, NULL);
		if (rc) {
			dberr(log_db, "couldn't prepare INSERT statement");
			return 1;
		}
	}

	field = 1;
	if (e) {
		if (e->stamp) {
			sqlite3_bind_int(insert, field++, e->stamp);
		} else {
			sqlite3_bind_int(insert, field++, (unsigned long) time(NULL));
		}
		sqlite3_bind_int(insert, field++, e->op);
		sqlite3_bind_int(insert, field++, e->access);
		sqlite3_bind_int(insert, field++, e->client);
		sqlite3_bind_int(insert, field++, e->dev);
		sqlite3_bind_int(insert, field++, e->gid);
		sqlite3_bind_int64(insert, field++, e->ino);
		sqlite3_bind_int(insert, field++, e->mode);
		if (e->path) {
			sqlite3_bind_text(insert, field++, e->path, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, field++);
		}
		sqlite3_bind_int(insert, field++, e->result);
		sqlite3_bind_int(insert, field++, e->severity);
		if (e->text) {
			sqlite3_bind_text(insert, field++, e->text, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, field++);
		}
		if (e->program) {
			sqlite3_bind_text(insert, field++, e->program, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, field++);
		}
		if (e->tag) {
			sqlite3_bind_text(insert, field++, e->tag, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, field++);
		}
		sqlite3_bind_int(insert, field++, e->type);
		sqlite3_bind_int(insert, field++, e->uid);
	} else {
		sqlite3_bind_int(insert, field++, (unsigned long) time(NULL));
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_null(insert, field++);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_null(insert, field++);
		sqlite3_bind_null(insert, field++);
		sqlite3_bind_null(insert, field++);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
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
pdb_log_msg(pseudo_sev_t severity, pseudo_msg_t *msg, const char *program, const char *tag, const char *text, ...) {
	char *sql = "INSERT INTO logs "
		    "(stamp, op, access, client, dev, gid, ino, mode, path, result, uid, severity, text, program, tag, type)"
		    " VALUES "
		    "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	static sqlite3_stmt *insert;
	char buffer[8192];
	int field;
	int rc;
	va_list ap;

	if (text) {
		va_start(ap, text);
		vsnprintf(buffer, 8192, text, ap);
		va_end(ap);
		text = buffer;
	}

	if (!log_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 1;
	}

	if (!insert) {
		rc = sqlite3_prepare_v2(log_db, sql, strlen(sql), &insert, NULL);
		if (rc) {
			dberr(log_db, "couldn't prepare INSERT statement");
			return 1;
		}
	}

	field = 1;
	sqlite3_bind_int(insert, field++, (unsigned long) time(NULL));
	if (msg) {
		sqlite3_bind_int(insert, field++, msg->op);
		sqlite3_bind_int(insert, field++, msg->access);
		sqlite3_bind_int(insert, field++, msg->client);
		sqlite3_bind_int(insert, field++, msg->dev);
		sqlite3_bind_int(insert, field++, msg->gid);
		sqlite3_bind_int64(insert, field++, msg->ino);
		sqlite3_bind_int(insert, field++, msg->mode);
		if (msg->pathlen) {
			sqlite3_bind_text(insert, field++, msg->path, -1, SQLITE_STATIC);
		} else {
			sqlite3_bind_null(insert, field++);
		}
		sqlite3_bind_int(insert, field++, msg->result);
		sqlite3_bind_int(insert, field++, msg->uid);
	} else {
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_null(insert, field++);
		sqlite3_bind_int(insert, field++, 0);
		sqlite3_bind_int(insert, field++, 0);
	}
	sqlite3_bind_int(insert, field++, severity);
	if (text) {
		sqlite3_bind_text(insert, field++, text, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(insert, field++);
	}
	if (program) {
		sqlite3_bind_text(insert, field++, program, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(insert, field++);
	}
	if (tag) {
		sqlite3_bind_text(insert, field++, tag, -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(insert, field++);
	}
	if (msg) {
		sqlite3_bind_int(insert, field++, msg->type);
	} else {
		sqlite3_bind_int(insert, field++, 0);
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
	if ((rc > 0) && ((size_t) rc >= (b->buflen - curlen))) {
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
		if ((rc > 0) && ((size_t) rc >= (b->buflen - curlen))) {
			pseudo_diag("tried to reallocate larger buffer, failed.  giving up.\n");
			return -1;
		}
	}
	if (rc >= 0)
		b->tail += rc;
	return rc;
}

sqlite3_stmt *
pdb_query(char *stmt_type, pseudo_query_t *traits, unsigned long fields, int unique, int want_results) {
	pseudo_query_t *trait;
	sqlite3_stmt *stmt;
	int done_any = 0;
	int field = 0;
	const char *order_by = "id";
	char *order_dir = "ASC";
	int rc;
	pseudo_query_field_t f;
	static buffer *sql;

	if (!log_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return NULL;
	}

	if (!stmt_type) {
		pseudo_diag("can't prepare a statement without a type.\n");
	}

	if (!sql) {
		sql = malloc(sizeof *sql);
		if (!sql) {
			pseudo_diag("can't allocate SQL buffer.\n");
			return NULL;
		}
		sql->buflen = 512;
		sql->data = malloc(sql->buflen);
		if (!sql->data) {
			pseudo_diag("can't allocate SQL text buffer.\n");
			free(sql);
			sql = 0;
			return NULL;
		}
	}
	sql->tail = sql->data;
	/* should be DELETE or SELECT */
	frag(sql, "%s ", stmt_type);

	if (want_results) {
		if (unique)
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
		case PSQF_PROGRAM:
		case PSQF_TEXT:
		case PSQF_TAG:
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
			case PSQT_LIKE:
			case PSQT_NOTLIKE:
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
			case PSQT_LIKE:
			case PSQT_NOTLIKE:
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
			case PSQT_EXACT:
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
			case PSQT_LIKE:
			case PSQT_NOTLIKE:
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
	if (want_results)
		frag(sql, "ORDER BY %s %s;", order_by, order_dir);
	pseudo_debug(PDBGF_SQL, "created SQL: <%s>\n", sql->data);

	/* second, prepare it */
	rc = sqlite3_prepare_v2(log_db, sql->data, strlen(sql->data), &stmt, NULL);
	if (rc) {
		dberr(log_db, "couldn't prepare %s statement", stmt_type);
		return NULL;
	}

	/* third, bind the fields */
	field = 1;
	for (trait = traits; trait; trait = trait->next) {
		switch (trait->field) {
		case PSQF_ORDER:
			/* this just creates a hunk of SQL above */
			break;
		case PSQF_PROGRAM:
		case PSQF_PATH:
		case PSQF_TAG:
		case PSQF_TEXT:
			sqlite3_bind_text(stmt, field++,
				trait->data.svalue, -1, SQLITE_STATIC);
			break;
		case PSQF_ACCESS:
		case PSQF_CLIENT:
		case PSQF_DEV:
		case PSQF_FD:
		case PSQF_FTYPE:
		case PSQF_INODE:
		case PSQF_GID:
		case PSQF_PERM:
		case PSQF_MODE:
		case PSQF_OP:
		case PSQF_RESULT:
		case PSQF_SEVERITY:
		case PSQF_STAMP:
		case PSQF_TYPE:
		case PSQF_UID:
			sqlite3_bind_int64(stmt, field++, trait->data.ivalue);
			break;
		default:
			pseudo_diag("Inexplicably invalid field type %d\n", trait->field);
			sqlite3_finalize(stmt);
			return NULL;
		}
	}
	return stmt;
}

int
pdb_delete(pseudo_query_t *traits, unsigned long fields) {
	sqlite3_stmt *stmt;

	stmt = pdb_query("DELETE", traits, fields, 0, 0);

	/* no need to return it, so... */
	if (stmt) {
		file_db_dirty = 1;
		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			dberr(log_db, "deletion failed");
			return -1;
		} else {
			pseudo_diag("Deleted records, vacuuming log database (may take a while).\n");
			/* we can't do anything about it if this fails... */
			sqlite3_exec(log_db, "VACUUM;", NULL, NULL, NULL);
		}
		sqlite3_finalize(stmt);
		return 0;
	}
	return -1;
}

log_history
pdb_history(pseudo_query_t *traits, unsigned long fields, int unique) {
	log_history h = NULL;
	sqlite3_stmt *stmt;

	stmt = pdb_query("SELECT", traits, fields, unique, 1);

	if (stmt) {
		/* fourth, return the statement, now ready to be stepped through */
		h = malloc(sizeof(*h));
		if (h) {
			h->rc = 0;
			h->fields = fields;
			h->stmt = stmt;
		} else {
			pseudo_diag("failed to allocate memory for log_history\n");
			sqlite3_finalize(stmt);
		}
		return h;
	} else {
		return NULL;
	}
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
		case PSQF_ACCESS:
			l->access = sqlite3_column_int64(h->stmt, column++);
			break;
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
		case PSQF_PROGRAM:
			s = sqlite3_column_text(h->stmt, column++);
			if (s)
				l->program = strdup((char *) s);
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
		case PSQF_TYPE:
			l->type = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_UID:
			l->uid = sqlite3_column_int64(h->stmt, column++);
			break;
		case PSQF_ORDER:
		case PSQF_FTYPE:
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
	free(e->program);
	free(e->tag);
	free(e);
}

/* Now for the actual file handling code! */

/* pdb_link_file:  Creates a new file from msg, using the provided path
 * or 'NAMELESS FILE'.
 */
int
pdb_link_file(pseudo_msg_t *msg, long long *row) {
	static sqlite3_stmt *insert;
	int rc;
	char *sql = "INSERT INTO files "
		    " ( path, dev, ino, uid, gid, mode, rdev, deleting ) "
		    " VALUES (?, ?, ?, ?, ?, ?, ?, 0);";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	sqlite3_bind_int64(insert, 3, msg->ino);
	sqlite3_bind_int(insert, 4, msg->uid);
	sqlite3_bind_int(insert, 5, msg->gid);
	sqlite3_bind_int(insert, 6, msg->mode);
	sqlite3_bind_int(insert, 7, msg->rdev);
	pseudo_debug(PDBGF_DB | PDBGF_FILE, "linking %s: dev %llu, ino %llu, mode %o, owner %d\n",
		(msg->pathlen ? msg->path : "<nil> (as NAMELESS FILE)"),
		(unsigned long long) msg->dev, (unsigned long long) msg->ino,
		(int) msg->mode, msg->uid);
	file_db_dirty = 1;
	rc = sqlite3_step(insert);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "insert may have failed (rc %d)", rc);
	}
	/* some users care what the row ID is */
	if (row) {
		*row = sqlite3_last_insert_rowid(file_db);
	}
	sqlite3_reset(insert);
	sqlite3_clear_bindings(insert);
	return rc != SQLITE_DONE;
}

/* pdb_unlink_file_dev:  Delete every instance of a dev/inode pair. */
int
pdb_unlink_file_dev(pseudo_msg_t *msg) {
	static sqlite3_stmt *sql_delete;
	int rc;
	char *sql = "DELETE FROM files WHERE dev = ? AND ino = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!sql_delete) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &sql_delete, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	sqlite3_bind_int(sql_delete, 1, msg->dev);
	sqlite3_bind_int64(sql_delete, 2, msg->ino);
	file_db_dirty = 1;
	rc = sqlite3_step(sql_delete);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete by inode may have failed");
	}
	sqlite3_reset(sql_delete);
	sqlite3_clear_bindings(sql_delete);
	return rc != SQLITE_DONE;
}

/* provide a path for a 'NAMELESS FILE' entry */
int
pdb_update_file_path(pseudo_msg_t *msg) {
	static sqlite3_stmt *update;
	int rc;
	char *sql = "UPDATE files SET path = ? "
		"WHERE path = 'NAMELESS FILE' and dev = ? AND ino = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
		pseudo_debug(PDBGF_DB, "can't update a file without a message or path.\n");
		return 1;
	}
	sqlite3_bind_text(update, 1, msg->path, -1, SQLITE_STATIC);
	sqlite3_bind_int(update, 2, msg->dev);
	sqlite3_bind_int64(update, 3, msg->ino);
	file_db_dirty = 1;
	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update path by inode may have failed");
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	return rc != SQLITE_DONE;
}

/* mark a file for pending deletion by a given client */
int
pdb_may_unlink_file(pseudo_msg_t *msg, int deleting) {
	static sqlite3_stmt *mark_file;
	int rc, exact;
	char *sql_mark_file = "UPDATE files SET deleting = ? WHERE path = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!mark_file) {
		rc = sqlite3_prepare_v2(file_db, sql_mark_file, strlen(sql_mark_file), &mark_file, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (msg->pathlen) {
		sqlite3_bind_int(mark_file, 1, deleting);
		sqlite3_bind_text(mark_file, 2, msg->path, -1, SQLITE_STATIC);
	} else {
		pseudo_debug(PDBGF_DB, "cannot mark a file for pending deletion without a path.");
		return 1;
	}
	file_db_dirty = 1;
	rc = sqlite3_step(mark_file);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "mark for deletion may have failed");
		return 1;
	}
	exact = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(exact %d) ", exact);
	sqlite3_reset(mark_file);
	sqlite3_clear_bindings(mark_file);
	/* indicate whether we marked something */
	if (exact > 0)
		return 0;
	else
		return 1;
}

/* unmark a file for pending deletion */
int
pdb_cancel_unlink_file(pseudo_msg_t *msg) {
	static sqlite3_stmt *mark_file;
	int rc, exact;
	char *sql_mark_file = "UPDATE files SET deleting = 0 WHERE path = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!mark_file) {
		rc = sqlite3_prepare_v2(file_db, sql_mark_file, strlen(sql_mark_file), &mark_file, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!msg) {
		return 1;
	}
	if (msg->pathlen) {
		sqlite3_bind_text(mark_file, 1, msg->path, -1, SQLITE_STATIC);
	} else {
		pseudo_debug(PDBGF_DB, "cannot unmark a file for pending deletion without a path.");
		return 1;
	}
	file_db_dirty = 1;
	rc = sqlite3_step(mark_file);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "unmark for deletion may have failed");
	}
	exact = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(exact %d) ", exact);
	sqlite3_reset(mark_file);
	sqlite3_clear_bindings(mark_file);
	return rc != SQLITE_DONE;
}

/* delete all files attached to a given cookie;
 * used for database fixup passes.
 */
int
pdb_did_unlink_files(int deleting) {
	static sqlite3_stmt *delete_exact;
	int rc, exact;
	char *sql_delete_exact = "DELETE FROM files WHERE deleting = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!delete_exact) {
		rc = sqlite3_prepare_v2(file_db, sql_delete_exact, strlen(sql_delete_exact), &delete_exact, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (deleting == 0) {
		pseudo_diag("did_unlink_files: deleting must be non-zero.\n");
		return 0;
	}
	sqlite3_bind_int(delete_exact, 1, deleting);
	file_db_dirty = 1;
	rc = sqlite3_step(delete_exact);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "cleanup of files marked for deletion may have failed");
	}
	exact = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(exact %d)\n", exact);
	sqlite3_reset(delete_exact);
	sqlite3_clear_bindings(delete_exact);
	return rc != SQLITE_DONE;
}

/* confirm deletion of a specific file by a given client */
int
pdb_did_unlink_file(char *path, int deleting) {
	static sqlite3_stmt *delete_exact;
	int rc, exact;
	char *sql_delete_exact = "DELETE FROM files WHERE path = ? AND deleting = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!delete_exact) {
		rc = sqlite3_prepare_v2(file_db, sql_delete_exact, strlen(sql_delete_exact), &delete_exact, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	if (!path) {
		pseudo_debug(PDBGF_DB, "cannot unlink a file without a path.");
		return 1;
	}
	sqlite3_bind_text(delete_exact, 1, path, -1, SQLITE_STATIC);
	sqlite3_bind_int(delete_exact, 2, deleting);
	file_db_dirty = 1;
	rc = sqlite3_step(delete_exact);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "cleanup of file marked for deletion may have failed");
	}
	exact = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(exact %d)\n", exact);
	sqlite3_reset(delete_exact);
	sqlite3_clear_bindings(delete_exact);
	return rc != SQLITE_DONE;
}

/* unlink a file, by path */
int
pdb_unlink_file(pseudo_msg_t *msg) {
	static sqlite3_stmt *delete_exact;
	int rc, exact;
	char *sql_delete_exact = "DELETE FROM files WHERE path = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!delete_exact) {
		rc = sqlite3_prepare_v2(file_db, sql_delete_exact, strlen(sql_delete_exact), &delete_exact, NULL);
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
	} else {
		pseudo_debug(PDBGF_DB, "cannot unlink a file without a path.");
		return 1;
	}
	file_db_dirty = 1;
	rc = sqlite3_step(delete_exact);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete exact by path may have failed");
	}
	exact = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(exact %d) ", exact);
	sqlite3_reset(delete_exact);
	sqlite3_clear_bindings(delete_exact);
	return rc != SQLITE_DONE;
}

/* Unlink all the contents of directory
 * SQLite performance limitations:
 * path LIKE foo '/%' -> can't use index
 * path = A OR path = B -> can't use index
 * Solution: 
 * 1.  From web http://web.utk.edu/~jplyon/sqlite/SQLite_optimization_FAQ.html
 *         Use > and < instead of a glob at the end.
 */
int
pdb_unlink_contents(pseudo_msg_t *msg) {
	static sqlite3_stmt *delete_sub;
	int rc, sub;
	char *sql_delete_sub = "DELETE FROM files WHERE "
				"(path > (? || '/') AND path < (? || '0'));";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
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
		sqlite3_bind_text(delete_sub, 1, msg->path, -1, SQLITE_STATIC);
		sqlite3_bind_text(delete_sub, 2, msg->path, -1, SQLITE_STATIC);
	} else {
		pseudo_debug(PDBGF_DB, "cannot unlink a file without a path.");
		return 1;
	}
	file_db_dirty = 1;
	rc = sqlite3_step(delete_sub);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete sub by path may have failed");
	}
	sub = sqlite3_changes(file_db);
	pseudo_debug(PDBGF_DB, "(sub %d) ", sub);
	sqlite3_reset(delete_sub);
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

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
		pseudo_debug(PDBGF_DB, "rename: No path provided (ino %llu)\n", (unsigned long long) msg->ino);
		return 1;
	}
	if (!oldpath) {
		pseudo_debug(PDBGF_DB, "rename: No old path for %s\n", msg->path);
		return 1;
	}
	pseudo_debug(PDBGF_DB, "rename: Changing %s to %s\n", oldpath, msg->path);
	rc = sqlite3_bind_text(update_exact, 1, msg->path, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_exact, 2, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 1, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 2, msg->path, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 3, oldpath, -1, SQLITE_STATIC);
	rc = sqlite3_bind_text(update_sub, 4, oldpath, -1, SQLITE_STATIC);

	rc = sqlite3_exec(file_db, "BEGIN;", NULL, NULL, NULL);

	file_db_dirty = 1;
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

	rc = sqlite3_exec(file_db, "END;", NULL, NULL, NULL);

	sqlite3_clear_bindings(update_exact);
	sqlite3_clear_bindings(update_sub);
	return rc != SQLITE_DONE;
}

/* renumber device only.
 * this is used if the filesystem moves to a new device, without changing
 * inode allocations.
 */
int
pdb_renumber_all(dev_t from, dev_t to) {
	static sqlite3_stmt *update;
	int rc;
	char *sql = "UPDATE files "
		    " SET dev = ? "
		    " WHERE dev = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!update) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &update, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare UPDATE statement");
			return 1;
		}
	}
	rc = sqlite3_bind_int(update, 1, to);
	if (rc) {
		dberr(file_db, "error binding device numbers to update");
	}
	rc = sqlite3_bind_int(update, 2, from);
	if (rc) {
		dberr(file_db, "error binding device numbers to update");
	}

	file_db_dirty = 1;
	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update may have failed: rc %d", rc);
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	pseudo_debug(PDBGF_DB, "updating device dev %llu to %llu\n",
		(unsigned long long) from, (unsigned long long) to);
	return rc != SQLITE_DONE;
}

/* change dev/inode for a given path -- used only by RENAME for now.
 */
int
pdb_update_inode(pseudo_msg_t *msg) {
	static sqlite3_stmt *update;
	int rc;
	char *sql = "UPDATE files "
		    " SET dev = ?, ino = ? "
		    " WHERE path = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	if (!msg->pathlen) {
		pseudo_diag("Can't update the inode of a file without its path.\n");
		return 1;
	}
	sqlite3_bind_int(update, 1, msg->dev);
	sqlite3_bind_int64(update, 2, msg->ino);
	rc = sqlite3_bind_text(update, 3, msg->path, -1, SQLITE_STATIC);
	if (rc) {
		/* msg->path can never be null, and if msg didn't
		 * have a non-zero pathlen, we'd already have exited
		 * above
		 */
		dberr(file_db, "error binding %s to select", msg->path);
	}

	file_db_dirty = 1;
	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update may have failed: rc %d", rc);
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	pseudo_debug(PDBGF_DB, "updating path %s to dev %llu, ino %llu\n",
		msg->path,
		(unsigned long long) msg->dev, (unsigned long long) msg->ino);
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

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	sqlite3_bind_int64(update, 6, msg->ino);

	file_db_dirty = 1;
	rc = sqlite3_step(update);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "update may have failed: rc %d", rc);
	}
	sqlite3_reset(update);
	sqlite3_clear_bindings(update);
	pseudo_debug(PDBGF_DB, "updating dev %llu, ino %llu, new mode %o, owner %d\n",
		(unsigned long long) msg->dev, (unsigned long long) msg->ino,
		(int) msg->mode, msg->uid);
	return rc != SQLITE_DONE;
}

/* find file using both path AND dev/inode as key */
int
pdb_find_file_exact(pseudo_msg_t *msg, long long *row) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE path = ? AND dev = ? AND ino = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	rc = sqlite3_bind_text(select, 1, msg->path, -1, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "error binding %s to select", msg->pathlen ? msg->path : "<nil>");
	}
	sqlite3_bind_int(select, 2, msg->dev);
	sqlite3_bind_int64(select, 3, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		if (row) {
			*row = sqlite3_column_int64(select, 0);
		}
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		msg->deleting = (int) sqlite3_column_int64(select, 8);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB, "find_exact: sqlite_done on first row\n");
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
pdb_find_file_path(pseudo_msg_t *msg, long long *row) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE path = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
		if (row) {
			*row = sqlite3_column_int64(select, 0);
		}
		msg->dev = sqlite3_column_int64(select, 2);
		msg->ino = sqlite3_column_int64(select, 3);
		msg->uid = sqlite3_column_int64(select, 4);
		msg->gid = sqlite3_column_int64(select, 5);
		msg->mode = sqlite3_column_int64(select, 6);
		msg->rdev = sqlite3_column_int64(select, 7);
		msg->deleting = (int) sqlite3_column_int64(select, 8);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB, "find_path: sqlite_done on first row\n");
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

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	sqlite3_bind_int64(select, 2, msg->ino);
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
		pseudo_debug(PDBGF_DB, "find_dev: sqlite_done on first row\n");
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
pdb_find_file_dev(pseudo_msg_t *msg, long long *row, char **path) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE dev = ? AND ino = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	sqlite3_bind_int64(select, 2, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		if (row) {
			*row = sqlite3_column_int64(select, 0);
		}
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		msg->deleting = (int) sqlite3_column_int64(select, 8);
		/* stash path */
		if (path) {
			*path = strdup((char *) sqlite3_column_text(select, 1));
			pseudo_debug(PDBGF_FILE, "find_file_dev: path %s\n",
				*path ? *path : "<nil>");
		}
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB, "find_dev: sqlite_done on first row\n");
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

int
pdb_get_xattr(long long file_id, char **value, size_t *len) {
	static sqlite3_stmt *select;
	int rc;
	char *response;
	size_t length;
	char *sql = "SELECT value FROM xattrs WHERE file_id = ? AND name = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	pseudo_debug(PDBGF_XATTR, "requested xattr named '%s' for file %lld\n", *value, file_id);
	sqlite3_bind_int(select, 1, file_id);
	rc = sqlite3_bind_text(select, 2, *value, -1, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "couldn't bind xattr name to SELECT.");
		return 1;
	}
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		response = (char *) sqlite3_column_text(select, 0);
		length = sqlite3_column_bytes(select, 0);
		pseudo_debug(PDBGF_XATTR, "got %d-byte results: '%s'\n",
			(int) length, response);
		if (response && length >= 1) {
			/* not a strdup because the values can contain
			 * arbitrary bytes.
			 */
			*value = malloc(length);
			memcpy(*value, response, length);
			*len = length;
			rc = 0;
		} else {
			*value = NULL;
			*len = 0;
			rc = 1;
		}
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB, "find_exact: sqlite_done on first row\n");
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

int
pdb_list_xattr(long long file_id, char **value, size_t *len) {
	static sqlite3_stmt *select;
	size_t allocated = 0;
	size_t used = 0;
	char *buffer = 0;
	int rc;
	char *sql = "SELECT name FROM xattrs WHERE file_id = ?;";

	/* if we don't have a record of the file, it must not have
	 * any extended attributes...
	 */
	if (file_id == -1) {
		*value = NULL;
		*len = 0;
		return 0;
	}

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	sqlite3_bind_int(select, 1, file_id);
	do {
		rc = sqlite3_step(select);
		if (rc == SQLITE_ROW) {
			char *value = (char *) sqlite3_column_text(select, 0);
			size_t len = sqlite3_column_bytes(select, 0);
			if (!buffer) {
				allocated = round_up(len, 256);
				buffer = malloc(allocated);
			}
			if (used + len + 2 > allocated) {
				size_t new_allocated = round_up(used + len + 2, 256);
				char *new_buffer = malloc(new_allocated);
				memcpy(new_buffer, buffer, used);
				free(buffer);
				allocated = new_allocated;
				buffer = new_buffer;
			}
			memcpy(buffer + used, value, len);
			buffer[used + len] = '\0';
			used = used + len + 1;
		} else if (rc == SQLITE_DONE) {
			*value = buffer;
			*len = used;
		} else {
			dberr(file_db, "non-row response from select?");
			free(buffer);
			*value = NULL;
			*len = 0;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	return rc != SQLITE_DONE;
}

int
pdb_remove_xattr(long long file_id, char *value, size_t len) {
	static sqlite3_stmt *delete;
	int rc;
	char *sql = "DELETE FROM xattrs WHERE file_id = ? AND name = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!delete) {
		rc = sqlite3_prepare_v2(file_db, sql, strlen(sql), &delete, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare DELETE statement");
			return 1;
		}
	}
	sqlite3_bind_int(delete, 1, file_id);
	rc = sqlite3_bind_text(delete, 2, value, len, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "couldn't bind xattr name to DELETE.");
		return 1;
	}
	file_db_dirty = 1;
	rc = sqlite3_step(delete);
	if (rc != SQLITE_DONE) {
		dberr(file_db, "delete xattr may have failed");
	}
	sqlite3_reset(delete);
	sqlite3_clear_bindings(delete);
	return rc != SQLITE_DONE;
}

int
pdb_set_xattr(long long file_id, char *value, size_t len, int flags) {
	static sqlite3_stmt *select, *update, *insert;
	int rc;
	long long existing_row = -1;
	char *select_sql = "SELECT id FROM xattrs WHERE file_id = ? AND name = ?;";
	char *insert_sql = "INSERT INTO xattrs "
		    " ( file_id, name, value ) "
		    " VALUES (?, ?, ?);";
	char *update_sql = "UPDATE xattrs SET value = ? WHERE id = ?;";
	char *vname = value;
	size_t vlen;

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}
	if (!select) {
		rc = sqlite3_prepare_v2(file_db, select_sql, strlen(select_sql), &select, NULL);
		if (rc) {
			dberr(file_db, "couldn't prepare SELECT statement");
			return 1;
		}
	}
	sqlite3_bind_int(select, 1, file_id);
	rc = sqlite3_bind_text(select, 2, value, -1, SQLITE_STATIC);
	if (rc) {
		dberr(file_db, "couldn't bind xattr name to SELECT.");
		return 1;
	}
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		existing_row = sqlite3_column_int64(select, 0);
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB | PDBGF_VERBOSE, "find_exact: sqlite_done on first row\n");
		existing_row = -1;
		break;
	default:
		dberr(file_db, "set_xattr: select returned neither a row nor done");
		rc = 1;
		break;
	}
	sqlite3_reset(select);
	sqlite3_clear_bindings(select);
	if (flags == XATTR_CREATE && existing_row != -1) {
		pseudo_debug(PDBGF_DB, "XATTR_CREATE with an existing row, failing.");
		return 1;
	}
	if (flags == XATTR_REPLACE && existing_row == -1) {
		pseudo_debug(PDBGF_DB, "XATTR_REPLACE with no existing row, failing.");
		return 1;
	}
	/* the material after the name is the value */
	vlen = strlen(value);
	len = len - (vlen + 1);
	value = value + vlen + 1;
	pseudo_debug(PDBGF_XATTR, "trying to set a value for %lld: name is '%s' [%d/%d bytes], value is '%s' [%d bytes]. Existing row %lld.\n",
		file_id, vname, (int) vlen, (int) (len + vlen + 1), value, (int) len, existing_row);
	if (existing_row != -1) {
		/* update */
		if (!update) {
			rc = sqlite3_prepare_v2(file_db, update_sql, strlen(update_sql), &update, NULL);
			if (rc) {
				dberr(file_db, "couldn't prepare UPDATE statement");
				return 1;
			}
		}
		rc = sqlite3_bind_text(update, 1, value, len, SQLITE_STATIC);
		if (rc) {
			dberr(file_db, "couldn't bind xattr value to UPDATE.");
			return 1;
		}
		sqlite3_bind_int(update, 2, existing_row);
		file_db_dirty = 1;
		rc = sqlite3_step(update);
		if (rc != SQLITE_DONE) {
			dberr(file_db, "update xattr may have failed");
		}
		sqlite3_reset(update);
		sqlite3_clear_bindings(update);
		return rc != SQLITE_DONE;
	} else {
		/* insert */
		if (!insert) {
			rc = sqlite3_prepare_v2(file_db, insert_sql, strlen(insert_sql), &insert, NULL);
			if (rc) {
				dberr(file_db, "couldn't prepare INSERT statement");
				return 1;
			}
		}
		pseudo_debug(PDBGF_XATTR, "insert should be getting ID %lld\n", file_id);
		sqlite3_bind_int64(insert, 1, file_id);
		rc = sqlite3_bind_text(insert, 2, vname, -1, SQLITE_STATIC);
		if (rc) {
			dberr(file_db, "couldn't bind xattr name to INSERT statement");
			return 1;
		}
		rc = sqlite3_bind_text(insert, 3, value, len, SQLITE_STATIC);
		if (rc) {
			dberr(file_db, "couldn't bind xattr value to INSERT statement");
			return 1;
		}
		file_db_dirty = 1;
		rc = sqlite3_step(insert);
		if (rc != SQLITE_DONE) {
			dberr(file_db, "insert xattr may have failed");
		}
		sqlite3_reset(insert);
		sqlite3_clear_bindings(insert);
		return rc != SQLITE_DONE;
	}
	return rc;
}

/* find file using only inode as key.  Unused for now, planned to come
 * in for NFS usage.
 */
int
pdb_find_file_ino(pseudo_msg_t *msg, long long *row) {
	static sqlite3_stmt *select;
	int rc;
	char *sql = "SELECT * FROM files WHERE ino = ?;";

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
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
	sqlite3_bind_int64(select, 1, msg->ino);
	rc = sqlite3_step(select);
	switch (rc) {
	case SQLITE_ROW:
		if (row) {
			*row = sqlite3_column_int64(select, 0);
		}
		msg->dev = (unsigned long) sqlite3_column_int64(select, 2);
		msg->uid = (unsigned long) sqlite3_column_int64(select, 4);
		msg->gid = (unsigned long) sqlite3_column_int64(select, 5);
		msg->mode = (unsigned long) sqlite3_column_int64(select, 6);
		msg->rdev = (unsigned long) sqlite3_column_int64(select, 7);
		msg->deleting = (int) sqlite3_column_int64(select, 8);
		rc = 0;
		break;
	case SQLITE_DONE:
		pseudo_debug(PDBGF_DB, "find_ino: sqlite_done on first row\n");
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

pdb_file_list
pdb_files(void) {
	pdb_file_list l;

	if (!file_db && get_dbs()) {
		pseudo_diag("%s: database error.\n", __func__);
		return 0;
	}

	l = malloc(sizeof(*l));
	if (!l)
		return NULL;

	l->rc = sqlite3_prepare_v2(file_db, "SELECT path, dev, ino, uid, gid, mode, rdev FROM files", -1, &l->stmt, NULL);
	if (l->rc) {
		dberr(file_db, "Couldn't start SELECT from files.\n");
		free(l);
		return NULL;
	}
	return l;
}

pseudo_msg_t *
pdb_file(pdb_file_list l) {
	const unsigned char *s;
	pseudo_msg_t *m;
	int column = 0;

	if (!l || !l->stmt)
		return 0;
	/* in case someone tries again after we're already done */
	if (l->rc == SQLITE_DONE) {
		return 0;
	}
	l->rc = sqlite3_step(l->stmt);
	if (l->rc == SQLITE_DONE) {
		return 0;
	} else if (l->rc != SQLITE_ROW) {
		dberr(log_db, "statement failed");
		return 0;
	}
	s = sqlite3_column_text(l->stmt, column++);
	m = pseudo_msg_new(0, (const char *) s);
	if (!m) {
		pseudo_diag("couldn't allocate file message.\n");
		return NULL;
	}
	pseudo_debug(PDBGF_DB, "pdb_file: '%s'\n", s ? (const char *) s : "<nil>");
	m->dev = sqlite3_column_int64(l->stmt, column++);
	m->ino = sqlite3_column_int64(l->stmt, column++);
	m->uid = sqlite3_column_int64(l->stmt, column++);
	m->gid = sqlite3_column_int64(l->stmt, column++);
	m->mode = sqlite3_column_int64(l->stmt, column++);
	m->rdev = sqlite3_column_int64(l->stmt, column++);
	return m;
}

void
pdb_files_done(pdb_file_list l) {
	if (!l)
		return;
	if (l->stmt) {
		sqlite3_reset(l->stmt);
		sqlite3_finalize(l->stmt);
	}
	free(l);
}
