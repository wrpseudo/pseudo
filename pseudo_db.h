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
	pseudo_op_t op;
	int access;
	unsigned long client;
	unsigned long fd;
	unsigned long long dev;
	unsigned long long ino;
	unsigned long mode;
	unsigned long gid;
	unsigned long uid;
	char *path;
	pseudo_res_t result;
	pseudo_sev_t severity;
	char *text;
	char *tag;
	char *program;
} log_entry;

extern int pdb_maybe_backup(void);
extern int pdb_cancel_unlink_file(pseudo_msg_t *msg);
extern int pdb_did_unlink_file(char *path, int deleting);
extern int pdb_did_unlink_files(int deleting);
extern int pdb_link_file(pseudo_msg_t *msg, long long *row);
extern int pdb_may_unlink_file(pseudo_msg_t *msg, int deleting);
extern int pdb_unlink_file(pseudo_msg_t *msg);
extern int pdb_unlink_file_dev(pseudo_msg_t *msg);
extern int pdb_update_file(pseudo_msg_t *msg);
extern int pdb_update_file_path(pseudo_msg_t *msg);
extern int pdb_update_inode(pseudo_msg_t *msg);
extern int pdb_unlink_contents(pseudo_msg_t *msg);
extern int pdb_rename_file(const char *oldpath, pseudo_msg_t *msg);
extern int pdb_renumber_all(dev_t from, dev_t to);
extern int pdb_find_file_exact(pseudo_msg_t *msg, long long *row);
extern int pdb_find_file_path(pseudo_msg_t *msg, long long *row);
extern int pdb_find_file_dev(pseudo_msg_t *msg, long long *row, char **path);
extern int pdb_find_file_ino(pseudo_msg_t *msg, long long *row);
extern char *pdb_get_file_path(pseudo_msg_t *msg);
extern int pdb_get_xattr(long long file_id, char **value, size_t *len);
extern int pdb_list_xattr(long long file_id, char **value, size_t *len);
extern int pdb_remove_xattr(long long file_id, char *value, size_t len);
extern int pdb_set_xattr(long long file_id, char *value, size_t len, int flags);

struct log_history;
typedef struct log_history *log_history;

union pseudo_query_data {
	unsigned long long ivalue;
	char *svalue;
};

typedef struct pseudo_query {
	pseudo_query_type_t type;
	pseudo_query_field_t field;
	union pseudo_query_data data;
	struct pseudo_query *next;
} pseudo_query_t;

extern int pdb_log_entry(log_entry *e);
extern int pdb_log_msg(pseudo_sev_t severity, pseudo_msg_t *msg, const char *program, const char *tag, const char *text, ...);
extern int pdb_log_traits(pseudo_query_t *traits);

struct pdb_file_list;
typedef struct pdb_file_list *pdb_file_list;

extern pdb_file_list pdb_files(void);
extern pseudo_msg_t *pdb_file(pdb_file_list);
extern void pdb_files_done(pdb_file_list);

extern int pdb_delete(pseudo_query_t *traits, unsigned long fields);
extern log_history pdb_history(pseudo_query_t *traits, unsigned long fields, int unique);
extern log_entry *pdb_history_entry(log_history h);
extern void pdb_history_free(log_history h);
extern void log_entry_free(log_entry *);
