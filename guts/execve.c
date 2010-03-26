/* 
 * static int
 * wrap_execve(const char *filename, char *const *argv, char *const *envp) {
 *	int rc = -1;
 */
	/* note:  we don't canonicalize this, because we are intentionally
	 * NOT redirecting execs into the chroot environment.  If you try
	 * to execute /bin/sh, you get the actual /bin/sh, not
	 * <CHROOT>/bin/sh.  This allows use of basic utilities.  This
	 * design will likely be revisited.
	 */
	pseudo_client_op(OP_EXEC, PSA_EXEC, 0, 0, filename, 0);
	rc = real_execve(filename, argv, envp);

/*	return rc;
 * }
 */
