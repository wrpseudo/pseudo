/* 
 * static int
 * wrap_fchdir(int dirfd) {
 *	int rc = -1;
 */

	rc = real_fchdir(dirfd);

	if (rc != -1) {
		pseudo_client_op(OP_CHDIR, -1, dirfd, 0, 0);
	}

/*	return rc;
 * }
 */
