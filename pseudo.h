/*
 * pseudo.h, shared definitions and structures for pseudo
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
#include <stdlib.h>
#include <fcntl.h>

typedef enum {
	OP_UNKNOWN = -1,
	OP_NONE = 0,
	OP_CHDIR,
	OP_CHMOD,
	OP_CHOWN,
	OP_CHROOT,
	OP_CLOSE,
	OP_CREAT,
	OP_DUP,
	OP_FCHMOD,
	OP_FCHOWN,
	OP_FSTAT,
	OP_LINK,
	OP_MKDIR,
	OP_MKNOD,
	OP_OPEN,
	OP_RENAME,
	OP_STAT,
	OP_UNLINK,
	/* added after the original release, so they have to go out of order
	 * to avoid breaking the operation numbers in old logs.
	 */
	OP_SYMLINK,
	OP_EXEC,
	OP_MAX
} op_id_t;
extern char *pseudo_op_name(op_id_t id);
extern op_id_t pseudo_op_id(char *name);

typedef enum {
	RESULT_UNKNOWN = -1,
	RESULT_NONE = 0,
	RESULT_SUCCEED,
	RESULT_FAIL,
	RESULT_ERROR,
	RESULT_MAX
} res_id_t;
extern char *pseudo_res_name(res_id_t id);
extern res_id_t pseudo_res_id(char *name);

typedef enum {
	SEVERITY_UNKNOWN = -1,
	SEVERITY_NONE = 0,
	SEVERITY_DEBUG,
	SEVERITY_INFO,
	SEVERITY_WARN,
	SEVERITY_ERROR,
	SEVERITY_CRITICAL,
	SEVERITY_MAX
} sev_id_t;
extern char *pseudo_sev_name(sev_id_t id);
extern sev_id_t pseudo_sev_id(char *name);

typedef enum pseudo_query_type {
	PSQT_UNKNOWN = -1,
	PSQT_NONE,
	PSQT_EXACT,	PSQT_LESS,	PSQT_GREATER,	PSQT_BITAND,
	PSQT_NOTEQUAL,	PSQT_LIKE,	PSQT_NOTLIKE,	PSQT_SQLPAT,
	PSQT_MAX
} pseudo_query_type_t;
extern char *pseudo_query_type_name(pseudo_query_type_t id);
extern char *pseudo_query_type_sql(pseudo_query_type_t id);
extern pseudo_query_type_t pseudo_query_type_id(char *name);

/* Note:  These are later used as bitwise masks into a value,
 * currently an unsigned long; if the number of these gets up
 * near 32, that may take rethinking.  The first thing to
 * go would probably be something special to do for FTYPE and
 * PERM because they aren't "real" database fields -- both
 * of them actually imply MODE.
 */
typedef enum pseudo_query_field {
	PSQF_UNKNOWN = -1,
	PSQF_NONE, /* so that these are always non-zero */
	PSQF_ACCESS,
	PSQF_CLIENT,	PSQF_DEV,	PSQF_FD, 	PSQF_FTYPE,
	PSQF_GID,	PSQF_ID,	PSQF_INODE,	PSQF_MODE,	
	PSQF_OP,	PSQF_ORDER,	PSQF_PATH,	PSQF_PERM,
	PSQF_RESULT,	PSQF_SEVERITY,	PSQF_STAMP,	PSQF_TAG,
	PSQF_TEXT,	PSQF_UID,
	PSQF_MAX
} pseudo_query_field_t;
extern char *pseudo_query_field_name(pseudo_query_field_t id);
extern pseudo_query_field_t pseudo_query_field_id(char *name);

extern void pseudo_debug_verbose(void);
extern void pseudo_debug_terse(void);
extern int pseudo_util_debug_fd;
#ifndef NDEBUG
extern int pseudo_debug_real(int, char *, ...) __attribute__ ((format (printf, 2, 3)));
#define pseudo_debug pseudo_debug_real
#else
/* oh no, mister compiler, please don't optimize me away! */
static inline void pseudo_debug(int level, char *text, ...) { }
#endif
extern int pseudo_diag(char *, ...) __attribute__ ((format (printf, 1, 2)));
void pseudo_new_pid(void);
/* pseudo_fix_path resolves symlinks up to this depth */
#define PSEUDO_MAX_LINK_RECURSION 16
extern char *pseudo_fix_path(const char *, const char *, size_t, size_t, size_t *, int);
extern void pseudo_dropenv(void);
extern void pseudo_setupenv(char *);
extern char *pseudo_prefix_path(char *);
extern char *pseudo_get_prefix(char *);
extern ssize_t pseudo_sys_path_max(void);
extern ssize_t pseudo_path_max(void);

extern char *pseudo_version;

#define PSEUDO_DATA "var/pseudo/"
#define PSEUDO_LOCKFILE PSEUDO_DATA "pseudo.lock"
#define PSEUDO_LOGFILE PSEUDO_DATA "pseudo.log"
#define PSEUDO_PIDFILE PSEUDO_DATA "pseudo.pid"
#define PSEUDO_SOCKET PSEUDO_DATA "pseudo.socket"

/* some systems might not have *at().  We like to define operations in
 * terms of each other, and for instance, open(...) is the same as
 * openat(AT_FDCWD, ...).  If no AT_FDCWD is provided, any value that can't
 * be a valid file descriptor will do.  Using -2 because -1 could be
 * mistaken for a failed syscall return.  Similarly, any value which isn't
 * zero will do to fake AT_SYMLINK_NOFOLLOW.  Finally, if this happened,
 * we set our own flag we can use to indicate that dummy implementations
 * of the _at functions are needed.
 */
#ifndef AT_FDCWD
#define AT_FDCWD -2
#define AT_SYMLINK_NOFOLLOW 1
#define PSEUDO_NO_REAL_AT_FUNCTIONS
#endif

/* Likewise, someone might not have O_LARGEFILE (the flag equivalent to
 * using open64()).  Since open64() is the same as O_LARGEFILE in flags,
 * we implement it that way... If the system has no O_LARGEFILE, we'll
 * just call open() with nothing special.
 */ 
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
