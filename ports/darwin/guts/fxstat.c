/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fxstat(int fd, struct stat *buf)
 *	int rc = -1;
 */

	rc = real_fxstat(fd, buf);

/*	return rc;
 * }
 */
