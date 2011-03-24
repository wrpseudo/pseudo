/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t fgetxattr(int filedes, const char *name, void *value, size_t size, u_int32_t position, int options)
 *	ssize_t rc = -1;
 */

	rc = real_fgetxattr(filedes, name, value, size, position, options);

/*	return rc;
 * }
 */
