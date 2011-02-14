/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_open64(const char *path, int flags, ...mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_openat(AT_FDCWD, path, flags, mode | O_LARGEFILE);

/*	return rc;
 * }
 */
