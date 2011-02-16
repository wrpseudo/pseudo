/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * struct group *getgrgid(gid_t gid)
 *	struct group *rc = NULL;
 */

	rc = real_getgrgid(gid);

/*	return rc;
 * }
 */
