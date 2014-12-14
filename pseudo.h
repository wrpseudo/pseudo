/*
 * pseudo.h, shared definitions and structures for pseudo
 *
 * Copyright (c) 2008-2010, 2013 Wind River Systems, Inc.
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

/* List of magic initialization functions... */
extern void pseudo_init_wrappers(void);
extern void pseudo_init_util(void);
extern void pseudo_init_client(void);

void pseudo_dump_env(char **envp);
int pseudo_set_value(const char *key, const char *value);
char *pseudo_get_value(const char *key);
int pseudo_has_unload(char * const *envp);

#include "pseudo_tables.h"

extern void pseudo_debug_verbose(void);
extern void pseudo_debug_terse(void);
extern void pseudo_debug_set(char *);
extern void pseudo_debug_clear(char *);
extern void pseudo_debug_flags_finalize(void);
extern unsigned long pseudo_util_debug_flags;
extern int pseudo_util_debug_fd;
extern int pseudo_disabled;
extern int pseudo_allow_fsync;
extern int pseudo_diag(char *, ...) __attribute__ ((format (printf, 1, 2)));
#ifndef NDEBUG
#define pseudo_debug(x, ...) do { \
	if ((x) & PDBGF_VERBOSE) { \
		if ((pseudo_util_debug_flags & PDBGF_VERBOSE) && (pseudo_util_debug_flags & ((x) & ~PDBGF_VERBOSE))) { pseudo_diag(__VA_ARGS__); } \
	} else { \
		if (pseudo_util_debug_flags & (x)) { pseudo_diag(__VA_ARGS__); } \
	} \
} while (0) 
#define pseudo_debug_call(x, fn, ...) do { \
	if ((x) & PDBGF_VERBOSE) { \
		if ((pseudo_util_debug_flags & PDBGF_VERBOSE) && (pseudo_util_debug_flags & ((x) & ~PDBGF_VERBOSE))) { fn(__VA_ARGS__); } \
	} else { \
		if (pseudo_util_debug_flags & (x)) { fn(__VA_ARGS__); } \
	} \
} while (0) 
#else
/* this used to be a static inline function, but that meant that arguments
 * were still evaluated for side effects. We don't want that. The ...
 * is a C99ism, also supported by GNU C.
 */
#define pseudo_debug(...) 0
#define pseudo_debug_call(...) 0
#endif
extern void pseudo_dump_data(char *, const void *, size_t);
void pseudo_new_pid(void);
/* pseudo_fix_path resolves symlinks up to this depth */
#define PSEUDO_MAX_LINK_RECURSION 16
extern char *pseudo_fix_path(const char *, const char *, size_t, size_t, size_t *, int);
extern void pseudo_dropenv(void);
extern char **pseudo_dropenvp(char * const *);
extern void pseudo_setupenv(void);
extern char **pseudo_setupenvp(char * const *);
extern char *pseudo_prefix_path(char *);
extern char *pseudo_bindir_path(char *);
extern char *pseudo_libdir_path(char *);
extern char *pseudo_localstatedir_path(char *);
extern char *pseudo_get_prefix(char *);
extern char *pseudo_get_bindir(void);
extern char *pseudo_get_libdir(void);
extern char *pseudo_get_localstatedir(void);
extern int pseudo_logfile(char *defname);
extern ssize_t pseudo_sys_path_max(void);
extern ssize_t pseudo_path_max(void);
#define PSEUDO_PWD_MAX 4096
extern int pseudo_etc_file(const char *filename, char *realname, int flags, char *path[], int dircount);
extern void pseudo_stat32_from64(struct stat *, const struct stat64 *);
extern void pseudo_stat64_from32(struct stat64 *, const struct stat *);

extern char *pseudo_version;

#ifndef PSEUDO_BINDIR
 #define PSEUDO_BINDIR "bin"
#endif

#ifndef PSEUDO_LIBDIR
 #define PSEUDO_LIBDIR "lib"
#endif

#define STARTSWITH(x, y) (!memcmp((x), (y), sizeof(y) - 1))

#ifndef PSEUDO_LOCALSTATEDIR
 #define PSEUDO_LOCALSTATEDIR "var/pseudo"
#endif

#define PSEUDO_LOCKFILE "pseudo.lock"
#define PSEUDO_LOGFILE "pseudo.log"
#define PSEUDO_PIDFILE "pseudo.pid"
#define PSEUDO_SOCKET "pseudo.socket"

/* some systems might not have *at().  We like to define operations in
 * terms of each other, and for instance, open(...) is the same as
 * openat(AT_FDCWD, ...).  If no AT_FDCWD is provided, any value that can't
 * be a valid file descriptor will do.  Using -2 because -1 could be
 * mistaken for a failed syscall return.  AT_SYMLINK_NOFOLLOW has to be
 * non-zero; AT_SYMLINK_FOLLOW has to be non-zero and different.  Finally,
 * if this happened, we set our own flag we can use to indicate that dummy
 * implementations of the _at functions are needed.
 */
#ifndef AT_FDCWD
#define AT_FDCWD -2
#define AT_SYMLINK_NOFOLLOW 1
#define AT_SYMLINK_FOLLOW 2
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

/* Does link(2) let you create hard links to symlinks? Of course not. Who
 * would ever do that? Well, Linux did, and possibly as a result, linkat()
 * does by default too; if you are on a host with the historical Unix
 * behavior of following symlinks to find the link target, you will want
 * to set this to AT_SYMLINK_FOLLOW. Darwin does.
 */
#define PSEUDO_LINK_SYMLINK_BEHAVIOR 0

/* given n, pick a multiple of block enough bigger than n
 * to give us some breathing room.
 */
static inline size_t
round_up(size_t n, size_t block) {
	return block * (((n + block / 4) / block) + 1);
}

#include "pseudo_ports.h"
