/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static ssize_t
 * wrap_readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
 *	ssize_t rc = -1;
 */
	rc = real_readlinkat(dirfd, path, buf, bufsiz);

	if (rc > 0) {
		rc = pseudo_dechroot(buf, rc);
	}

/*	return rc;
 * }
 */
