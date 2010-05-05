/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execve(const char *filename, char *const *argv, char *const *envp) {
 *	int rc = -1;
 */
	char * const *new_environ;
	/* note:  we don't canonicalize this, because we are intentionally
	 * NOT redirecting execs into the chroot environment.  If you try
	 * to execute /bin/sh, you get the actual /bin/sh, not
	 * <CHROOT>/bin/sh.  This allows use of basic utilities.  This
	 * design will likely be revisited.
	 */
	pseudo_client_op(OP_EXEC, PSA_EXEC, -1, -1, filename, 0);
	if (!getenv("PSEUDO_RELOADED"))
		new_environ = pseudo_setupenv(envp, getenv("PSEUDO_OPTS"));
	else
		new_environ = envp;
	rc = real_execve(filename, argv, new_environ);

/*	return rc;
 * }
 */
