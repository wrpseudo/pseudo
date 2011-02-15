/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fchown(int fd, uid_t owner, gid_t group)
 *	int rc = -1;
 */

	rc = real_fchown(fd, owner, group);

/*	return rc;
 * }
 */
