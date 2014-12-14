/*
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fremovexattr(int filedes, const char *name)
 *	int rc = -1;
 */

	/* suppress warnings */
	(void) filedes;
	(void) name;
	errno = ENOTSUP;

/*	return rc;
 * }
 */
