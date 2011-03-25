/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_creat64(const char *path, ...mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_openat(AT_FDCWD, path, O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE, mode);

/*	return rc;
 * }
 */
