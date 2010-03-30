/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_vfork(void) {
 *	int rc = -1;
 */

	/* like fakeroot, we really can't handle vfork's implications */
	rc = real_fork();
	if (rc == 0)
		pseudo_client_reset();

/*	return rc;
 * }
 */
