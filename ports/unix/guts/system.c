/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int system(const char *command)
 *	int rc = -1;
 */
 	/* We want to ensure that the child process implicitly
	 * spawned has the right environment.  So...
	 */
	int pid;

	if (!command)
		return 1;

	pid = wrap_fork();

	if (pid) {
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			rc = WEXITSTATUS(status);
		} else {
			/* we naively assume that either it exited or
			 * got killed by a signal...
			 */
			rc = WTERMSIG(status) + 128;
		}
	} else {
		/* this involves ANOTHER fork, but it's much, much,
		 * simpler than trying to get all the details right.
		 */
		rc = real_system(command);
		exit(rc);
	}

/*	return rc;
 * }
 */
