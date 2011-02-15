/*
 * Copyright (c) 2011 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * int fremovexattr(int filedes, const char *name)
 *	int rc = -1;
 */

	rc = real_fremovexattr(filedes, name);

/*	return rc;
 * }
 */
