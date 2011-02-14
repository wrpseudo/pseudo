/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fstat(int fd, struct stat *buf)
 *	int rc = -1;
 */

	rc = wrap___fxstat(_STAT_VER, fd, buf);

/*	return rc;
 * }
 */
