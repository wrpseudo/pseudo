/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t listxattr(const char *path, char *list, size_t size, int options)
 *	ssize_t rc = -1;
 */

	rc = shared_listxattr(path, -1, list, size, options);

/*	return rc;
 * }
 */
