/* 
 * Copyright (c) 2008-2010,2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_openat64(int dirfd, const char *path, int flags, ...mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_openat(dirfd, path, flags | O_LARGEFILE, mode);

/*	return rc;
 * }
 */
