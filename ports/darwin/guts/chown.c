/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int chown(const char *path, uid_t owner, gid_t group)
 *	int rc = -1;
 */

	rc = real_chown(path, owner, group);

/*	return rc;
 * }
 */
