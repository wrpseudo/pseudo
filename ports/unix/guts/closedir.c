/*
 * Copyright (c) 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * static int
 * wrap_closedir(DIR *dirp) {
 *	int rc = -1;
 */
	if (!dirp) {
		errno = EFAULT;
		return -1;
	}

	int fd = dirfd(dirp);
	pseudo_client_op(OP_CLOSE, 0, fd, -1, 0, 0);
	rc = real_closedir(dirp);

/*	return rc;
 * }
 */
