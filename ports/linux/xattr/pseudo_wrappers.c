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
	
static ssize_t shared_getxattr(const char *path, int fd, const char *name, void *value, size_t size) {
	RC_AND_BUF

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

static int shared_setxattr(const char *path, int fd, const char *name, const void *value, size_t size, int flags) {
	RC_AND_BUF
	pseudo_op_t op;

	pseudo_debug(PDBGF_XATTR, "setxattr(%s [fd %d], %s => '%.*s')\n",
		path ? path : "<no path>", fd, name, (int) size, (char *) value);

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

	switch (flags) {
	case XATTR_CREATE:
		op = OP_CREATE_XATTR;
		break;
	case XATTR_REPLACE:
		op = OP_REPLACE_XATTR;
		break;
	default:
		op = OP_SET_XATTR;
		break;
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

static ssize_t shared_listxattr(const char *path, int fd, char *list, size_t size) {
	RC_AND_BUF
	pseudo_msg_t *result = pseudo_client_op(OP_LIST_XATTR, 0, fd, -1, path, &buf);
	if (result->result != RESULT_SUCCEED) {
		pseudo_debug(PDBGF_XATTR, "listxattr: no success.\n");
		errno = ENOATTR;
		return -1;
	}
	if (list) {
		pseudo_debug(PDBGF_XATTR, "listxattr: %d bytes of names, starting '%.*s'\n",
			(int) result->pathlen, (int) result->pathlen, result->path);
		if (size >= result->pathlen) {
			memcpy(list, result->path, result->pathlen);
		} else {
			memcpy(list, result->path, size);
			errno = ERANGE;
		}
	}
	return result->pathlen;
}

static int shared_removexattr(const char *path, int fd, const char *name) {
	RC_AND_BUF
	pseudo_msg_t *result = pseudo_client_op(OP_REMOVE_XATTR, 0, fd, -1, path, &buf, name);

	if (result->result != RESULT_SUCCEED) {
		/* docs say ENOATTR, but I don't have one */
		errno = ENOENT;
		return -1;
	}
	return 0;
}

