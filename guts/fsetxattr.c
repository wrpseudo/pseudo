/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags) {
 *	int rc = -1;
 */

	errno = ENOTSUP;
	rc = -1;

/*	return rc;
 * }
 */
