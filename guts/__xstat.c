/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap___xstat(int ver, const char *path, struct stat *buf) {
 *	int rc = -1;
 */

	rc = wrap___fxstatat(ver, AT_FDCWD, path, buf, 0);

/*	return rc;
 * }
 */
