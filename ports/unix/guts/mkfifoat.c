/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_mkfifoat(int dirfd, const char *path, mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_mknodat(dirfd, path, (mode & 07777) | S_IFIFO, (dev_t) 0);

/*	return rc;
 * }
 */
