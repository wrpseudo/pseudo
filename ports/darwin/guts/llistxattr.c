/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t llistxattr(const char *pathname, char *list, size_t size)
 *	ssize_t rc = -1;
 */

	rc = real_llistxattr(pathname, list, size);

/*	return rc;
 * }
 */
