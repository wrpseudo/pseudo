/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fcntl(int fd, int cmd, ... { struct flock *lock })
 *	int rc = -1;
 */

	rc = real_fcntl(fd, cmd, lock);

/*	return rc;
 * }
 */
