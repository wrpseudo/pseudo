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
			if (!pseudo_get_value("PSEUDO_RELOADED")) {
                		pseudo_setupenv();
				pseudo_reinit_libpseudo();
 			} else {
				pseudo_setupenv();
				pseudo_dropenv();
			}
		}
	} else {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("fork");
	}
/*	return rc;
 * }
 */
