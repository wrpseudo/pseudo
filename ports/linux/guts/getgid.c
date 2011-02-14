/* 
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static gid_t
 * wrap_getgid(void) {
 *	gid_t rc = 0;
 */

	rc = pseudo_rgid;

/*	return rc;
 * }
 */
