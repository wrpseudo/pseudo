/*
 * Copyright (c) 2013 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned int flags)
 *	int rc = -1;
 */

	rc = real_sync_file_range(fd, offset, nbytes, flags);

/*	return rc;
 * }
 */
