/*
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int removexattr(const char *pathname, const char *name)
 *	int rc = -1;
 */

	/* suppress warnings */
	(void) pathname;
	(void) name;
	errno = ENOTSUP;

/*	return rc;
 * }
 */
