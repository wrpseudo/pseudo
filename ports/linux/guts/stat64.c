/*
 * Copyright (c) 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int stat64(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = wrap___fxstatat64(_STAT_VER, AT_FDCWD, path, buf, 0);

/*	return rc;
 * }
 */
