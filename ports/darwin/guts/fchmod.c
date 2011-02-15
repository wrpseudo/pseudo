/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fchmod(int fd, mode_t mode)
 *	int rc = -1;
 */

	rc = real_fchmod(fd, mode);

/*	return rc;
 * }
 */
