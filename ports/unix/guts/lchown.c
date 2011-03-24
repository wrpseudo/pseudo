/*
 * Copyright (c) 2008,2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int lchown(const char *path, uid_t owner, gid_t group)
 *	int rc = -1;
 */

	rc = wrap_chown(path, owner, group);

/*	return rc;
 * }
 */
