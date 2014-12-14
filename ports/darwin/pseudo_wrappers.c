/*
 * pseudo_wrappers.c, darwin pseudo wrappers
 *
 * Copyright (c) 2008-2011 Wind River Systems, Inc.
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
/* we need XATTR_NOFOLLOW in scope */
#include <sys/xattr.h>

/* shared functionality for the xattr code */
/* Each of these functions is expecting to get an optional name, and
 * a populated statbuf to use for sending messages to the server.
 */

/* to avoid namespace pollution and such, we now duplicate the
 * basic functionality of a POSIX ACL list, as used by libacl or
 * the kernel. Documentation was obtained from the headers of libacl
 * and from a page or two of _The Linux Programming Interface_, by
 * Michael Kerrisk.
 */

typedef struct {
	uint16_t tag;
	uint16_t perm;
	uint32_t id;
} acl_entry;

typedef struct {
	uint32_t version;
	acl_entry entries[];
} acl_header;

enum acl_tags {
	ACL_UNDEFINED = 0x0,
	ACL_USER_OBJ = 0x1,
	ACL_USER = 0x2,
	ACL_GROUP_OBJ = 0x4,
	ACL_GROUP = 0x8,
	ACL_MASK = 0x10,
	ACL_OTHER = 0x20,
};

static const int endian_test = 1;
static const char *endian_tester = (char *) &endian_test;

static inline int
le16(int x16) {
	if (*endian_tester) {
		return x16;
	} else {
		return ((x16 & 0xff) << 8) | ((x16 & 0xff00) >> 8);
	}
}

static inline int
le32(int x32) {
	if (*endian_tester) {
		return x32;
	} else {
		return ((x32 & 0xff) << 24) | ((x32 & 0xff00) << 8) |
		       ((x32 & 0xff0000) >> 8) | ((x32 & 0xff000000) >> 24);
	}
}

/* set mode to match the contents of header. Return non-zero on error.
 * On a zero return, mode is a valid posix mode, and *extra is set to
 * 1 if any of the entries are not reflected by that mode. On a non-zero
 * return, no promises are made about *extra or *mode.
 */
static int
posix_permissions(const acl_header *header, int entries, int *extra, int *mode) {
	int acl_seen = 0;
	if (le32(header->version) != 2) {
		pseudo_diag("Fatal: ACL support no available for header version %d.\n",
			le32(header->version));
		return 1;
	}
	*mode = 0;
	*extra = 0;
	for (int i = 0; i < entries; ++i) {
		const acl_entry *e = &header->entries[i];
		int tag = le16(e->tag);
		int perm = le16(e->perm);
		acl_seen |= tag;
		switch (tag) {
		case ACL_USER_OBJ:
			*mode = *mode | (perm << 6);
			break;
		case ACL_GROUP_OBJ:
			*mode = *mode | (perm << 3);
			break;
		case ACL_OTHER:
			*mode = *mode | perm;
			break;
		case ACL_USER:
		case ACL_GROUP:
		case ACL_MASK:
			*extra = *extra + 1;
			break;
		default:
			pseudo_debug(PDBGF_XATTR, "Unknown tag in ACL: 0x%x.\n",
				tag);
			return 1;
		}
	}
	return 0;
}

#define RC_AND_BUF \
	int rc; \
	PSEUDO_STATBUF buf; \
	if (path) { \
		rc = base_lstat(path, &buf); \
	} else { \
		rc = base_fstat(fd, &buf); \
	} \
	if (rc == -1) { \
		return rc; \
	}
	
static ssize_t shared_getxattr(const char *path, int fd, const char *name, void *value, size_t size, u_int32_t position, int options) {
	RC_AND_BUF

	if (!strncmp(name, "com.apple.", 10)) {
		if (fd != -1) {
			return real_fgetxattr(fd, name, value, size, position, options);
		} else {
			return real_getxattr(path, name, value, size, position, options);
		}
	}

	pseudo_debug(PDBGF_XATTR, "getxattr(%s [fd %d], %s)\n",
		path ? path : "<no path>", fd, name);
	pseudo_msg_t *result = pseudo_client_op(OP_GET_XATTR, 0, fd, -1, path, &buf, name);
	if (result->result != RESULT_SUCCEED) {
		errno = ENOATTR;
		return -1;
	}

	if (value) {
		pseudo_debug(PDBGF_XATTR, "returned attributes: '%s' (%d bytes)\n",
			result->path, result->pathlen);
		if (size >= result->pathlen) {
			memcpy(value, result->path, result->pathlen);
		} else {
			memcpy(value, result->path, size);
			errno = ERANGE;
		}
	}
	return result->pathlen;
}

static int shared_setxattr(const char *path, int fd, const char *name, const void *value, size_t size, u_int32_t position, int options) {
	RC_AND_BUF
	pseudo_op_t op;

	pseudo_debug(PDBGF_XATTR, "setxattr(%s [fd %d], %s => '%.*s')\n",
		path ? path : "<no path>", fd, name, (int) size, (char *) value);

	if (!strncmp(name, "com.apple.", 10)) {
		if (fd != -1) {
			return real_fsetxattr(fd, name, value, size, position, options);
		} else {
			return real_setxattr(path, name, value, size, position, options);
		}
	}

	/* this may be a plain chmod */
	if (!strcmp(name, "system.posix_acl_access")) {
		int extra;
		int mode;
		int entries = (size - sizeof(acl_header)) / sizeof(acl_entry);
		if (!posix_permissions(value, entries, &extra, &mode)) {
			pseudo_debug(PDBGF_XATTR, "posix_acl_access translated to mode %04o. Remaining attribute(s): %d.\n",
				mode, extra);
			buf.st_mode = mode;
			/* we want to actually issue a corresponding chmod,
			 * as well, or else the file ends up 0600 on the
			 * host. Using the slightly-less-efficient wrap_chmod
			 * avoids possible misalignment.
			 */
			if (path) {
				wrap_chmod(path, mode);
			} else {
				wrap_fchmod(fd, mode);
			}
			/* we are sneaky, and do not actually record this using
			 * extended attributes. */
			if (!extra) {
				return 0;
			}
		}
	}

	if (options & XATTR_CREATE) {
		op = OP_CREATE_XATTR;
	} else if (options & XATTR_REPLACE) {
		op = OP_REPLACE_XATTR;
	} else {
		op = OP_SET_XATTR;
	}

	pseudo_msg_t *result = pseudo_client_op(op, 0, fd, -1, path, &buf, name, value, size);
	
	/* we automatically assume success */
	if (op == OP_SET_XATTR) {
		return 0;
	}

	/* CREATE/REPLACE operations can report failure */
	if (!result || result->result == RESULT_FAIL) {
		return -1;
	}

	return 0;
}

static ssize_t shared_listxattr(const char *path, int fd, char *list, size_t size, int options) {
	RC_AND_BUF
	char extra_name_buf[4096];
	ssize_t real_attr_len;
	ssize_t used = 0;
	if (fd != -1) {
		real_attr_len = real_flistxattr(fd, extra_name_buf, sizeof(extra_name_buf), options);
	} else {
		real_attr_len = real_listxattr(path, extra_name_buf, sizeof(extra_name_buf), options);
	}
	pseudo_debug(PDBGF_XATTR, "listxattr: %d bytes of FS xattr names, starting '%.*s'\n",
		(int) real_attr_len, (int) real_attr_len, extra_name_buf);

	/* we don't care why there aren't any */
	if (real_attr_len < 1) {
		real_attr_len = 0;
	}

	pseudo_msg_t *result = pseudo_client_op(OP_LIST_XATTR, 0, fd, -1, path, &buf);

	if (result->result != RESULT_SUCCEED && real_attr_len < 1) {
		pseudo_debug(PDBGF_XATTR, "listxattr: no success.\n");
		errno = ENOATTR;
		return -1;
	}

	if (list) {
		pseudo_debug(PDBGF_XATTR, "listxattr: %d bytes of names, starting '%.*s'\n",
			(int) result->pathlen, (int) result->pathlen, result->path);
		if (size >= result->pathlen) {
			memcpy(list, result->path, result->pathlen);
			used = result->pathlen;
		} else {
			memcpy(list, result->path, size);
			used = size;
			errno = ERANGE;
		}
		if (real_attr_len > 0) {
			if ((ssize_t) size >= used + real_attr_len) {
				memcpy(list + used, extra_name_buf, real_attr_len);
				used += real_attr_len;
			} else {
				memcpy(list + used, extra_name_buf, size - used);
				used = size;
				errno = ERANGE;
			}
			
		}
	} else {
		used = real_attr_len + result->pathlen;
	}
	return used;
}

static int shared_removexattr(const char *path, int fd, const char *name, int options) {
	RC_AND_BUF

	if (!strncmp(name, "com.apple.", 10)) {
		if (fd != -1) {
			return real_fremovexattr(fd, name, options);
		} else {
			return real_removexattr(path, name, options);
		}
	}

	pseudo_msg_t *result = pseudo_client_op(OP_REMOVE_XATTR, 0, fd, -1, path, &buf, name);

	if (result->result != RESULT_SUCCEED) {
		/* docs say ENOATTR, but I don't have one */
		errno = ENOENT;
		return -1;
	}
	return 0;
}


/* there's no fgetgrent_r or fgetpwent_r in Darwin */

#define PLENTY_LONG 2048
/* the original uid/gid code for Linux was written in terms of the
 * fget*ent_r() functions... which Darwin doesn't have.  But wait!  They're
 * actually pretty easy to implement.
 */
int
pseudo_fgetgrent_r(FILE *fp, struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	char linebuf[PLENTY_LONG] = { 0 };
	char *s, *t, *u;
	size_t max_members;
	char **members;
	size_t member = 0;
	long started_at = -1;
	gid_t gid;
	int error = ENOENT;
	size_t len;

	/* any early exit should set *gbufp to NULL */
	if (gbufp)
		*gbufp = NULL;

	if (!gbuf || !fp || !buf)
		goto error_out;

	if (fp == pseudo_host_etc_group_file) {
		struct group *g;
		pseudo_antimagic();
		g = getgrent();
		pseudo_magic();
		if (g) {
			char *s = linebuf;
			s += snprintf(linebuf, PLENTY_LONG,
				"%s:%s:%ld:",
				g->gr_name,
				g->gr_passwd,
				(long) g->gr_gid);
			if (g->gr_mem) {
				int i;
				for (i = 0; g->gr_mem[i]; ++i) {
					s += snprintf(s, 
						PLENTY_LONG - (s - linebuf),
						"%s,",
						g->gr_mem[i]);
				}
				if (s[-1] == ',')
					--s;
			}
			strcpy(s, "\n");
		} else {
			goto error_out;
		}
	} else {
		started_at = ftell(fp);
		if (started_at == -1) {
			goto error_out;
		}
		s = fgets(linebuf, PLENTY_LONG, fp);
		if (!s) {
			goto error_out;
		}
	}
	/* fgets will have stored a '\0' if there was no error; if there
	 * was an error, though, linebuf was initialized to all zeroes so
	 * the string is null-terminated anyway...
	 */
 	len = strlen(linebuf);
	if (len > buflen) {
		error = ERANGE;
		goto error_out;
	}
	memcpy(buf, linebuf, len);
	/* round up to 8, hope for the best? */
	len = len + 8 + (((unsigned long long) (buf + len)) % 8);
	members = (char **) (buf + len);
	if (len >= buflen) {
		error = ERANGE;
		goto error_out;
	}
	/* this is how many pointers we have room for... */
	max_members = (buflen - len) / sizeof(*members);

	t = buf;
	/* yes, I can assume that Darwin has strsep() */
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gbuf->gr_name = s;
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gbuf->gr_passwd = s;
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	gid = (gid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	gbuf->gr_gid = gid;

	/* now, s points to a comma-separated list of members, which we
	 * want to stash pointers to in 'members'.
	 */
	s = strsep(&t, ":");
	t = s;
	while ((s = strsep(&t, ",")) != NULL) {
		if (*s) {
			if (member + 1 > max_members) {
				errno = ERANGE;
				goto error_out;
			}
			members[member++] = s;
		}
	}
	if (member + 1 > max_members) {
		errno = ERANGE;
		goto error_out;
	}
	members[member++] = NULL;
	*gbufp = gbuf;
	return 0;

error_out:
	if (started_at != -1)
		fseek(fp, started_at, SEEK_SET);
	return error;
	return -1;
}

int
pseudo_fgetpwent_r(FILE *fp, struct passwd *pbuf, char *buf, size_t buflen, struct passwd **pbufp) {
	char linebuf[PLENTY_LONG] = { 0 };
	char *s, *t, *u;
	long started_at = -1;
	__darwin_time_t timestamp;
	uid_t uid;
	gid_t gid;
	int error = ENOENT;
	size_t len;

	/* any early exit should set *gbufp to NULL */
	if (pbufp)
		*pbufp = NULL;

	if (!pbuf || !fp || !buf)
		goto error_out;

	if (fp == pseudo_host_etc_passwd_file) {
		struct passwd *p;

		pseudo_antimagic();
		p = getpwent();
		pseudo_magic();
		if (p) {
			snprintf(linebuf, PLENTY_LONG,
				"%s:%s:%ld:%ld:%s:%ld:%ld:%s:%s:%s\n",
					p->pw_name,
					p->pw_passwd,
					(long) p->pw_uid,
					(long) p->pw_gid,
					p->pw_class,
					(long) p->pw_change,
					(long) p->pw_expire,
					p->pw_gecos,
					p->pw_dir,
					p->pw_shell);
		} else {
			goto error_out;
		}
	} else {
		started_at = ftell(fp);
		if (started_at == -1) {
			goto error_out;
		}
		s = fgets(linebuf, PLENTY_LONG, fp);
		if (!s) {
			goto error_out;
		}
	}
	/* fgets will have stored a '\0' if there was no error; if there
	 * was an error, though, linebuf was initialized to all zeroes so
	 * the string is null-terminated anyway...
	 */
 	len = strlen(linebuf);
	if (len > buflen) {
		error = ERANGE;
		goto error_out;
	}
	if (linebuf[len - 1] == '\n') {
		linebuf[len - 1] = '\0';
		--len;
	}
	memcpy(buf, linebuf, len);

	t = buf;
	/* yes, I can assume that Darwin has strsep() */
	s = strsep(&t, ":");
	if (!s) {
		goto error_out;
	}
	pbuf->pw_name = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_passwd = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	uid = (uid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_uid = uid;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	gid = (gid_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_gid = gid;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_class = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	timestamp = (__darwin_time_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_change = timestamp;

	timestamp = (__darwin_time_t) strtol(s, &u, 10);
	/* should be a null byte, otherwise we didn't get a valid number */
	if (*u)
		goto error_out;
	pbuf->pw_expire = timestamp;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_gecos = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_dir = s;

	s = strsep(&t, ":");
	if (!s)
		goto error_out;
	pbuf->pw_shell = s;

	*pbufp = pbuf;
	return 0;

error_out:
	if (started_at != -1)
		fseek(fp, started_at, SEEK_SET);
	return error;
	return -1;
}

int
pseudo_getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp) {
	/* note that we don't wrap fgetpwent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_pwd) {
		errno = ENOENT;
		return -1;
	}
	return pseudo_fgetpwent_r(pseudo_pwd, pwbuf, buf, buflen, pwbufp);
}

int 
pseudo_getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp) {
	/* note that we don't wrap fgetgrent_r, since there's no path
	 * references in it.
	 */
	if (!pseudo_grp) {
		errno = ENOENT;
		return -1;
	}
	return pseudo_fgetgrent_r(pseudo_grp, gbuf, buf, buflen, gbufp);
}

