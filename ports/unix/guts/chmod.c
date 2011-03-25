/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_chmod(const char *path, mode_t mode) {
 *	int rc = -1;
 */

	rc = wrap_fchmodat(AT_FDCWD, path, mode, 0);

/*	return rc;
 * }
 */
