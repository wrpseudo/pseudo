/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_ulckpwdf(void) {
 *	int rc = -1;
 */

	/* lock is cleared automatically on close */
	rc = pseudo_pwd_lck_close();

/*	return rc;
 * }
 */
