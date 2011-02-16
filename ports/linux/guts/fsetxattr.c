/*
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fsetxattr(int filedes, const char *name, const void *value, size_t size, int flags)
 *	int rc = -1;
 */

	/* suppress warnings */
	(void) filedes;
	(void) name;
	(void) value;
	(void) size;
	(void) flags;
	errno = ENOTSUP;

/*	return rc;
 * }
 */
