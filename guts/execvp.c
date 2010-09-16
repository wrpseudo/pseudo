/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_execvp(const char *file, char *const *argv) {
 *	int rc = -1;
 */

	/* note:  we don't canonicalize this, because we are intentionally
	 * NOT redirecting execs into the chroot environment.  If you try
	 * to execute /bin/sh, you get the actual /bin/sh, not
	 * <CHROOT>/bin/sh.  This allows use of basic utilities.  This
	 * design will likely be revisited.
	 */
	pseudo_client_op(OP_EXEC, PSA_EXEC, -1, -1, file, 0);

	if (!pseudo_get_value("PSEUDO_RELOADED"))
		pseudo_setupenv();
	else {
		pseudo_setupenv();
		pseudo_dropenv();
	}

	/* if exec() fails, we may end up taking signals unexpectedly...
	 * not much we can do about that.
	 */
	sigprocmask(SIG_SETMASK, &pseudo_saved_sigmask, NULL);
	rc = real_execvp(file, argv);

/*	return rc;
 * }
 */
