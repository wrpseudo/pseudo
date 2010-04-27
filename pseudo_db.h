/*
 * pseudo_db.h, declarations and definitions for database use
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
typedef struct {
	time_t stamp;
	pseudo_msg_type_t type;
	op_id_t op;
	int access;
	unsigned long client;
	unsigned long fd;
	unsigned long long dev;
	unsigned long long ino;
	unsigned long mode;
	unsigned long gid;
	unsigned long uid;
	char *path;
	res_id_t result;
	sev_id_t severity;
	char *text;
	char *tag;
	char *program;
} log_entry;

extern int pdb_link_file(pseudo_msg_t *msg);
extern int pdb_unlink_file(pseudo_msg_t *msg);
extern int pdb_unlink_file_dev(pseudo_msg_t *msg);
extern int pdb_update_file(pseudo_msg_t *msg);
extern int pdb_update_file_path(pseudo_msg_t *msg);
extern int pdb_rename_file(const char *oldpath, pseudo_msg_t *msg);
extern int pdb_find_file_exact(pseudo_msg_t *msg);
extern int pdb_find_file_path(pseudo_msg_t *msg);
extern int pdb_find_file_dev(pseudo_msg_t *msg);
extern int pdb_find_file_ino(pseudo_msg_t *msg);
extern char *pdb_get_file_path(pseudo_msg_t *msg);

struct log_history;
typedef struct log_history *log_history;

union pseudo_query_data {
	unsigned long long ivalue;
	char *svalue;
};

typedef struct pseudo_query {
	enum pseudo_query_type type;
	enum pseudo_query_field field;
	union pseudo_query_data data;
	struct pseudo_query *next;
} pseudo_query_t;

extern int pdb_log_entry(log_entry *e);
extern int pdb_log_msg(sev_id_t severity, pseudo_msg_t *msg, const char *program, const char *tag, const char *text, ...);
extern int pdb_log_traits(pseudo_query_t *traits);

extern log_history pdb_history(pseudo_query_t *traits, unsigned long fields, int unique, int delete);
extern log_entry *pdb_history_entry(log_history h);
extern void pdb_history_free(log_history h);
extern void log_entry_free(log_entry *);
