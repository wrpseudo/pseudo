/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fchdir(int dirfd) {
 *	int rc = -1;
 */

	rc = real_fchdir(dirfd);

	if (rc != -1) {
		pseudo_client_op(OP_CHDIR, 0, -1, dirfd, 0, 0);
	}

/*	return rc;
 * }
 */
