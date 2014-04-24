/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fsetxattr(int filedes, const char *name, const void *value, size_t size, int xflags)
 *	int rc = -1;
 */
	rc = shared_setxattr(NULL, filedes, name, value, size, xflags);

/*	return rc;
 * }
 */
