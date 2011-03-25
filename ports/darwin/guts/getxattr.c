/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t getxattr(const char *pathname, const char *name, void *value, size_t size, u_int32_t position, int options)
 *	ssize_t rc = -1;
 */

	rc = real_getxattr(pathname, name, value, size, position, options);

/*	return rc;
 * }
 */
