/*
 * Copyright (c) 2014 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fremovexattr(int filedes, const char *name, int options)
 *	int rc = -1;
 */

	rc = shared_removexattr(NULL, filedes, name, options);

/*	return rc;
 * }
 */
