/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static long
 * wrap_pathconf(const char *path, int name) {
 *	long rc = -1;
 */

	rc = real_pathconf(path, name);

/*	return rc;
 * }
 */
