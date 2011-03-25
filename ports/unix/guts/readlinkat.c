/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static ssize_t
 * wrap_readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
 *	ssize_t rc = -1;
 */
#ifdef PSEUDO_NO_REAL_AT_FUNCTIONS
	if (dirfd != AT_FDCWD) {
		errno = ENOSYS;
		return -1;
	}
	rc = real_readlink(path, buf, bufsiz);
#else
	rc = real_readlinkat(dirfd, path, buf, bufsiz);
#endif

	if (rc > 0) {
		rc = pseudo_dechroot(buf, rc);
	}

/*	return rc;
 * }
 */
