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

/* List of magic initialization functions... */
extern void pseudo_init_wrappers(void);
extern void pseudo_init_util(void);
extern void pseudo_init_client(void);

void pseudo_dump_env(char **envp);
int pseudo_set_value(const char *key, const char *value);
char *pseudo_get_value(const char *key);

#include "pseudo_tables.h"

extern void pseudo_debug_verbose(void);
extern void pseudo_debug_terse(void);
extern int pseudo_util_debug_fd;
extern int pseudo_disabled;
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
extern int pseudo_etc_file(const char *filename, char *realname, int flags, char **search, int dircount);
#define PSEUDO_ETC_FILE(name, realname, flags) pseudo_etc_file((name), (realname), (flags), (char *[]) { pseudo_chroot, pseudo_passwd }, 2)
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

#include "pseudo_ports.h"
