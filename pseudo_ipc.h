/*
 * pseudo_ipc.h, definitions and declarations for pseudo IPC code
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

/* The [] item at the end of the struct is a C99 feature, replacing the
 * old (and unportable) "struct hack".
 */
typedef struct {
	pseudo_msg_type_t type;
	pseudo_op_t op;
	pseudo_res_t result;
	int access;
	int client;
	int fd;
	dev_t dev;
	unsigned long long ino;
	uid_t uid;
	gid_t gid;
	unsigned long long mode;
	dev_t rdev;
	unsigned int pathlen;
	int nlink;
	int deleting;
	char path[];
} pseudo_msg_t;

enum {
	PSA_EXEC = 1,
	PSA_WRITE = (PSA_EXEC << 1),
	PSA_READ = (PSA_WRITE << 1),
	PSA_APPEND = (PSA_READ << 1),
} pseudo_access_t;

#define PSEUDO_ACCESS_MAP(mode, fcntl_access, pseudo_access) ((((mode) & O_ACCMODE) == (fcntl_access)) ? (pseudo_access) : (0))
#define PSEUDO_ACCESS_FLAG(mode, fcntl_access, pseudo_access) (((mode) & (fcntl_access)) ? (pseudo_access) : (0))
#define PSEUDO_ACCESS(mode) ( \
	PSEUDO_ACCESS_MAP(mode, O_RDONLY, PSA_READ) | \
	PSEUDO_ACCESS_MAP(mode, O_WRONLY, PSA_WRITE) | \
	PSEUDO_ACCESS_MAP(mode, O_RDWR, PSA_READ | PSA_WRITE) | \
	PSEUDO_ACCESS_FLAG(mode, O_APPEND, PSA_APPEND))
extern int pseudo_access_fopen(const char *);

#define PSEUDO_HEADER_SIZE (offsetof(pseudo_msg_t, path))

extern pseudo_msg_t *pseudo_msg_receive(int fd);
extern pseudo_msg_t *pseudo_msg_dup(pseudo_msg_t *);
extern pseudo_msg_t *pseudo_msg_dupheader(pseudo_msg_t *);
extern pseudo_msg_t *pseudo_msg_new(size_t, const char *);
extern int pseudo_msg_send(int fd, pseudo_msg_t *, size_t, const char *);

void pseudo_msg_stat(pseudo_msg_t *msg, const PSEUDO_STATBUF *buf);
void pseudo_stat_msg(PSEUDO_STATBUF *buf, const pseudo_msg_t *msg);
