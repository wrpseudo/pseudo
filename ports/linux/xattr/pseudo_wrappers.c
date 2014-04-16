/* shared functionality for the xattr code */
/* Each of these functions is expecting to get an optional name, and
 * a populated statbuf to use for sending messages to the server.
 */

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

	pseudo_debug(PDBGF_XATTR, "getxattr(%s/%d, %s)\n",
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

	char *combined;
	size_t nlen = strlen(name);
	size_t combined_len = nlen + size + 1;
	combined = malloc(combined_len + 1);
	memcpy(combined, name, nlen);
	combined[nlen] = '\0';
	memcpy(combined + nlen + 1, value, size);
	combined[combined_len] = '\0';

	pseudo_debug(PDBGF_XATTR, "setxattr(%s/%d, %s, %s => %s [%d])\n",
		path ? path : "<no path>", fd, name, (char *) value, combined + nlen + 1, (int) size);

	pseudo_op_t op;
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

	pseudo_msg_t *result = pseudo_client_op(op, 0, fd, -1, path, &buf, combined, combined_len);
	
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

