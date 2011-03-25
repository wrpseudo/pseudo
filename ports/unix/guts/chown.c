/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_chown(const char *path, uid_t owner, gid_t group) {
 *	int rc = -1;
 */

	rc = wrap_fchownat(AT_FDCWD, path, owner, group, 0);

/*	return rc;
 * }
 */
