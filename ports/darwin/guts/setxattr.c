/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int setxattr(const char *pathname, const char *name, const void *value, size_t size, u_int32_t position, int options)
 *	int rc = -1;
 */

	rc = real_setxattr(pathname, name, value, size, position, options);

/*	return rc;
 * }
 */
