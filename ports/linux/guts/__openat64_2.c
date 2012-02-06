/* 
 * Copyright (c) 2008-2010,2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap___openat64_2(int dirfd, const char *path, int flags) {
 *	int rc = -1;
 */

	rc = wrap_openat(dirfd, path, flags | O_LARGEFILE, 0);

/*	return rc;
 * }
 */
