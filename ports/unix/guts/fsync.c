/*
 * Copyright (c) 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fsync(int fd)
 *	int rc = -1;
 */

	/* note: wrapper will never call this if PSEUDO_FORCE_ASYNC
	 * is defined.
	 */
	rc = real_fsync(fd);

/*	return rc;
 * }
 */
