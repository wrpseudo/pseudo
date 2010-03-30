/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_symlink(const char *oldname, const char *newpath) {
 *	int rc = -1;
 */

	rc = wrap_symlinkat(oldname, AT_FDCWD, newpath);

/*	return rc;
 * }
 */
