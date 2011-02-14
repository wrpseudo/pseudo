/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_setgroups(size_t size, const gid_t *list) {
 *	int rc = -1;
 */

	/* let gcc know we're ignoring these */
	(void) size;
	(void) list;
	/* you always have all group privileges.  we're like magic! */
	rc = 0;

/*	return rc;
 * }
 */
