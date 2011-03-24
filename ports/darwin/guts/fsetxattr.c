/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fsetxattr(int filedes, const char *name, const void *value, size_t size, u_int32_t position, int options)
 *	int rc = -1;
 */

	rc = real_fsetxattr(filedes, name, value, size, position, options);

/*	return rc;
 * }
 */
