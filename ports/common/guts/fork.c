/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_fork(void) {
 *	int rc = -1;
 */
	rc = real_fork();
	/* special case: we may want to enable or disable
	 * pseudo in the child process
	 */
	if (rc == 0) {
		pseudo_setupenv();
		if (!pseudo_get_value("PSEUDO_UNLOAD")) {
			pseudo_reinit_libpseudo();
		} else {
			pseudo_dropenv();
		}
	}
/*	return rc;
 * }
 */
