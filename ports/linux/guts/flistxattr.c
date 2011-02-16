/*
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t flistxattr(int filedes, char *list, size_t size)
 *	ssize_t rc = -1;
 */

	/* suppress warnings */
	(void) filedes;
	(void) list;
	(void) size;
	errno = ENOTSUP;

/*	return rc;
 * }
 */
