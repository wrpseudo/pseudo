/*
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * ssize_t fgetxattr(int filedes, const char *name, void *value, size_t size)
 *	ssize_t rc = -1;
 */

	/* suppress warnings */
	(void) filedes;
	(void) name;
	(void) value;
	(void) size;
	errno = ENOTSUP;

/*	return rc;
 * }
 */
