/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lstat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = wrap___fxstatat(_STAT_VER, AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);

/*	return rc;
 * }
 */
