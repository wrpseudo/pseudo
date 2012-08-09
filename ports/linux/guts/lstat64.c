/*
 * Copyright (c) 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lstat64(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = wrap___fxstatat64(_STAT_VER, AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);

/*	return rc;
 * }
 */
