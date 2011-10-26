/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execve(const char *file, char *const *argv, char *const *envp) {
 *	int rc = -1;
 */
	char * const *new_environ;
	/* note:  we don't canonicalize this, because we are intentionally
	 * NOT redirecting execs into the chroot environment.  If you try
	 * to execute /bin/sh, you get the actual /bin/sh, not
	 * <CHROOT>/bin/sh.  This allows use of basic utilities.  This
	 * design will likely be revisited.
	 */
        if (antimagic == 0) {
		char *path_guess = pseudo_exec_path(file, 0);
                pseudo_client_op(OP_EXEC, PSA_EXEC, -1, -1, path_guess, 0);
		free(path_guess);
        }

	new_environ = pseudo_setupenvp(envp);
	if (pseudo_get_value("PSEUDO_UNLOAD"))
		new_environ = pseudo_dropenvp(new_environ);

	/* if exec() fails, we may end up taking signals unexpectedly...
	 * not much we can do about that.
	 */
	sigprocmask(SIG_SETMASK, &pseudo_saved_sigmask, NULL);
	rc = real_execve(file, argv, new_environ);

/*	return rc;
 * }
 */
