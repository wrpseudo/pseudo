/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_dup2(int oldfd, int newfd) {
 *	int rc = -1;
 */
	int save_errno;

	/* close existing one first - this also causes the socket to the
	 * server to get moved around if someone tries to overwrite it. */
	pseudo_debug(PDBGF_CLIENT, "dup2: %d->%d\n", oldfd, newfd);
	pseudo_client_op(OP_CLOSE, 0, newfd, -1, 0, 0);
	rc = real_dup2(oldfd, newfd);
	save_errno = errno;
	pseudo_client_op(OP_DUP, 0, oldfd, newfd, 0, 0);
	errno = save_errno;

/*	return rc;
 * }
 */
