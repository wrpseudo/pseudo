/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int getgroups(int size, gid_t *list)
 *	int rc = -1;
 */

	rc = real_getgroups(size, list);

/*	return rc;
 * }
 */
