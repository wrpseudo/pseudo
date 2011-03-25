/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static gid_t
 * wrap_getegid(void) {
 *	gid_t rc = 0;
 */

	rc = pseudo_egid;

/*	return rc;
 * }
 */
