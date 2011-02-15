/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fchdir(int dirfd)
 *	int rc = -1;
 */

	rc = real_fchdir(dirfd);

/*	return rc;
 * }
 */
