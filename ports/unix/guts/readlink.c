/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static ssize_t
 * wrap_readlink(const char *path, char *buf, size_t bufsiz) {
 *	ssize_t rc = -1;
 */

	rc = wrap_readlinkat(AT_FDCWD, path, buf, bufsiz);

/*	return rc;
 * }
 */
