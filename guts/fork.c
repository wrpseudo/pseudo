/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fork(void) {
 *	int rc = -1;
 */
	if (real_fork) {
		rc = real_fork();
		/* special case: we may want to enable or disable
		 * pseudo in the child process
		 */
		if (rc == 0) {
			pseudo_setupenv();
			pseudo_client_reset();
		}
	} else {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("fork");
	}
/*	return rc;
 * }
 */
