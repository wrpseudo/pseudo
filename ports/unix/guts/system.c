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

	pseudo_setupenv();
	rc = real_system(command);

/*	return rc;
 * }
 */
