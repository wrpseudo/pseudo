/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t flistxattr(int filedes, char *list, size_t size)
 *	ssize_t rc = -1;
 */

	rc = real_flistxattr(filedes, list, size);

/*	return rc;
 * }
 */
