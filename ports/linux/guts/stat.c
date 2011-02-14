/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int stat(const char *path, struct stat *buf)
 *	int rc = -1;
 */

	rc = wrap___fxstatat(_STAT_VER, AT_FDCWD, path, buf, 0);

/*	return rc;
 * }
 */
