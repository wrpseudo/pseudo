/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int setgroups(int size, const gid_t *list)
 *	int rc = -1;
 */

	rc = real_setgroups(size, list);

/*	return rc;
 * }
 */
