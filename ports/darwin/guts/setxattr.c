/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int setxattr(const char *path, const char *name, const void *value, size_t size, u_int32_t position, int options)
 *	int rc = -1;
 */

	rc = shared_setxattr(path, -1, name, value, size, position, options);

/*	return rc;
 * }
 */
