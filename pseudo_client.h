/*
 * pseudo_client.h, shared declarations for client
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
extern pseudo_msg_t *pseudo_client_op(op_id_t op, int flags, int fd, int dirfd, const char *path, const struct stat64 *buf, ...);
extern void pseudo_antimagic(void);
extern void pseudo_magic(void);
extern void pseudo_client_reset(void);
extern void pseudo_client_touchuid(void);
extern void pseudo_client_touchgid(void);
extern char *pseudo_client_fdpath(int fd);
extern int pseudo_client_shutdown(void);
extern int pseudo_fd(int fd, int how);
extern void pseudo_stat32_from64(struct stat *, struct stat64 *);
extern void pseudo_stat64_from32(struct stat64 *, struct stat *);
#define MOVE_FD	0
#define COPY_FD	1
#define PSEUDO_MIN_FD	20
extern int pseudo_euid;
extern int pseudo_fuid;
extern int pseudo_suid;
extern int pseudo_ruid;
extern int pseudo_egid;
extern int pseudo_sgid;
extern int pseudo_rgid;
extern int pseudo_fgid;
extern int pseudo_dir_fd;

/* Root can read, write, and execute files which have no read, write,
 * or execute permissions.
 *
 * A non-root user can't.
 *
 * When doing anything which actually writes to the filesystem, we add in
 * the user read/write/execute bits.  When storing to the database, though,
 * we mask out any such bits which weren't in the original mode.
 *
 * None of this will behave very sensibly if umask has 0700 bits in it;
 * this is a known limitation.
 */
#define PSEUDO_FS_MODE(mode) ((mode) | S_IRUSR | S_IWUSR | S_IXUSR)
#define PSEUDO_DB_MODE(fs_mode, user_mode) (((fs_mode) & ~0700) | ((user_mode & 0700)))

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

