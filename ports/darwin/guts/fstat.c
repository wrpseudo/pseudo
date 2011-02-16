/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fstat(int fd, struct stat *buf)
 *	int rc = -1;
 */

	rc = real_fstat(fd, buf);

/*	return rc;
 * }
 */
