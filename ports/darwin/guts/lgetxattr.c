/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t lgetxattr(const char *pathname, const char *name, void *value, size_t size)
 *	ssize_t rc = -1;
 */

	rc = real_lgetxattr(pathname, name, value, size);

/*	return rc;
 * }
 */
